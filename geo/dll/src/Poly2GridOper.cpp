// Copyright (C) 1998-2024 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// File: Poly2GridOper.cpp
// Purpose:
//   - Rasterize polygonal geometry (multi-ring) to grid tiles (scanline fill).
//   - Provide both direct grid-writing and run-length encoded (RLE) results.
//   - Integrates with tiling, viewport transforms, and per-tile bounding boxes.
// Highlights:
//   - Robust scanline conversion that uses double precision intersection math.
//   - Handles polygons with multiple parts (rings) and edges outside the viewport
//     using left/right "toggle" sentinels to keep winding parity correct.
//   - Re-usable, pre-allocated resources to minimize per-scanline allocations.
//   - Two main flows:
//       1) poly2grid: writes span-filled raster lines into a tile buffer.
//       2) poly2allgrids: produces RLE spans per polygon per tile.
//
// Threading:
//   - Rasterization resources (IFP_resouces) are allocated per call-site/
//     per-thread and re-used within a scan to avoid frequent allocations.
//   - Operators support parallel tile loops.
//
// Important invariants:
//   - A pixel is considered inside the polygon if its center lies inside.
//   - Row/column rounding uses RoundPositiveHalfOpen for consistent scanline
//     grid coverage.
//   - For each scanline, crossing count (after considering toggles) must be even.
//
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/RangeIndex.h"
#include "mth/Mathlib.h"
#include "xct/DmsException.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "ParallelTiles.h"
#include "Param.h"
#include "tilechannel.h"
#include "TreeItemClass.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "Operator.h"

#include "ViewPortInfoEx.h"
#include "RemoveAdjacentsAndSpikes.h"

/************************************************************************/
/*                       dllImageFilledPolygon()                        */
/*                                                                      */
/*      Perform scanline conversion of the passed multi-ring            */
/*      polygon.  Note the polygon does not need to be explicitly       */
/*      closed.  The scanline function will be called with              */
/*      horizontal scanline chunks which may not be entirely            */
/*      contained within the valid raster area (in the X                */
/*      direction).                                                     */
/*                                                                      */
/*      NEW: Nodes' coordinate are kept as double  in order             */
/*      to compute accurately the intersections with the lines          */
/*                                                                      */
/*         A pixel is considered inside a polygon if its center         */
/*         falls inside the polygon. This is somehow more robust unless */
/*         the nodes are placed in the center of the pixels in which    */
/*         case, due to numerical inaccuracies, it's hard to predict    */
/*         if the pixel will be considered inside or outside the shape. */
/************************************************************************/

/*
 * NOTE: This code was originally adapted from the gdImageFilledPolygon()
 * function in libgd.
 *
 * http://www.boutell.com/gd/
 *
 * It was later adapted for direct inclusion in GDAL and relicensed under
 * the GDAL MIT/X license (pulled from the OpenEV distribution).
 *
 * Adapted to GeoDMS in 2012/2013
 * In 2017 a major reorganization and code optimization was performed
 * - All edges within the required y-range are ordered by first y and kept
 *   in the heap currEdges while relevant for the current ray and that is ordered on the last y .
 * - TODO: edges on the left of the x-range within the y-range are coded at togglers at their start and stop row of open left
 * - TODO: edges on the right of the x-range within the y-range are coded at togglers at their start and stop row of open right
 * - edge incidence at x=0.5 or higher results at a pixel start at column 1 or higher, and lower is handled as incidental left toggler
 * - edge incidence at x=width-0.5 or higher results at a pixel start at column 1 or higher, and lower is handled as incidental left toggler
 */

namespace {
	// Row/column and index typedefs used throughout scanline rasterization.
	typedef UInt32 rowcol_t;
	typedef SizeT  BurnValueVariant;  // Variant type used for "burn" value written to grid.
	typedef double coord_t;           // Coordinate precision for intersections.
	typedef UInt32 seq_index_t;       // Index within a polygon sequence.
	typedef UInt32 part_index_t;      // Polygon part/ring counter.

	typedef Point<rowcol_t> RasterSizeType; // <cols, rows> size in raster space.
	typedef Point<coord_t> point_t;         // polygon coordinate type (double).
	const seq_index_t END_OF_LIST = -1;     // Linked list terminator for edge starts.
};

// Single edge represented by indices into point array.
typedef Couple<seq_index_t> poly_edge;

// Node for a linked list of edges starting at a given scanline (by start row).
struct poly_edge_node {
	seq_index_t next_index; // Next node in bucket.
	poly_edge   edge;       // Directed edge (start->end) with y(start) < y(end)
};

// Generic RLE range over a row: [m_Start, m_End) within row m_Row.
template <typename CoordType>
struct RLE_range
{
	CoordType m_Row, m_Start, m_End;
};

// Convenience aliases for RLE collections.
template <typename CoordType> using RLE_polygon = std::vector<RLE_range<CoordType>>;
template <typename CoordType> using RLE_polygon_tileset = std::vector<RLE_polygon<CoordType>>;
template <typename CoordType> using RLE_polygon_wall = std::vector<RLE_polygon_tileset<CoordType>>;

// Abstract sink for scanline spans. Implementations:
//  - RasterizeInfo<T>: write spans directly into a raster buffer.
//  - RLE_Info<T>: collect spans as RLE ranges for later processing.
struct AbstrRasterizeInfo
{
	AbstrRasterizeInfo(RasterSizeType rasterSize)
		:	m_RasterSize(rasterSize)
	{}

	// Called for each horizontal span [nxStart, nxEnd) at raster row ny.
	virtual void BurnLine(SizeT ny, SizeT nxStart, SizeT nxEnd, BurnValueVariant eBurnValue) =0;

	RasterSizeType  m_RasterSize;

	// Note: AbstrRasterizeInfo is assumed to be used per-thread; any internal
	// buffers are not shared across threads.
};

// Direct raster writer; burn spans into a provided chunk buffer.
template <typename T>
struct RasterizeInfo : AbstrRasterizeInfo
{
	typedef typename sequence_traits<T>::pointer pointer_type;
	RasterizeInfo(RasterSizeType rasterSize, pointer_type chunkBuf)
		:	AbstrRasterizeInfo(rasterSize)
		,	m_ChunkBuf(chunkBuf)
	{
		// Initialize with 0/undefined.
		fast_fill(m_ChunkBuf, m_ChunkBuf+Cardinality(rasterSize), UNDEFINED_OR_ZERO(T));
	}

	void BurnLine(SizeT ny, SizeT nxStart, SizeT nxEnd, BurnValueVariant eBurnValue) override
	{
		assert(ny < m_RasterSize.Y());
		assert(nxStart <  m_RasterSize.X());
		assert(nxEnd   <= m_RasterSize.X());
		assert(nxStart <= nxEnd);

		pointer_type startLinePtr = m_ChunkBuf + ny * m_RasterSize.X();

		T v = eBurnValue;
		fast_fill(startLinePtr + nxStart, startLinePtr + nxEnd, v);
	}

	typename pointer_type m_ChunkBuf;
};

// RLE collector; collect spans as rows of start/end for later usage.
template <typename TE>
struct RLE_Info : AbstrRasterizeInfo
{
	RLE_Info(RasterSizeType rasterSize)
		: AbstrRasterizeInfo(rasterSize)
	{}

	void BurnLine(SizeT ny, SizeT nxStart, SizeT nxEnd, BurnValueVariant /*eBurnValue*/) override
	{
		assert(ny < m_RasterSize.Y());
		assert(nxStart < m_RasterSize.X());
		assert(nxEnd <= m_RasterSize.X());
		assert(nxStart <= nxEnd);

		result.emplace_back(ny, nxStart, nxEnd);
	}
	RLE_polygon<TE> result;
};

// Re-usable scratch memory to avoid reallocations within ImageFilledPolygon.
struct IFP_resouces {
	std::vector<rowcol_t> rows, cols;             // rounded row/col for each input point
	std::vector<seq_index_t> edgeListStarts;      // per-row start indices into edgeLists
	std::vector<poly_edge_node> edgeLists;        // buckets of edges keyed by start row
	std::vector<poly_edge> currEdges;             // active edge list for current scanline
	std::vector<bool> leftTogglePoints, rightTogglePoints; // toggles for edges fully left/right
	std::vector<rowcol_t> crossingCols;           // intersection columns per scanline
};

// Core scanline polygon rasterizer.
// padf: pointer to polygon vertices (may contain multiple rings, contiguous).
// nPartCount/panPartSize: partitioning of padf into rings.
// eBurnValue: value to write per-span (opaque to raster sink).
// ifpResources: temporary buffers for performance (re-usable).
//
// Algorithm:
//   - Clamp vertices to raster Y/X and round to rows/cols (for bucketization).
//   - Compute min/max ranges; early out if empty.
//   - Build buckets of edges starting at each integer row, and track edges that
//     lie completely left/right of viewport using parity toggles.
//   - For each scanline row:
//       - Flip open_left/open_right if toggles fire.
//       - Remove expired edges; add new starting edges.
//       - For each active edge compute intersection X at row center (y+0.5).
//         If < minX or > maxX, treat as toggle. Else add rounded col.
//       - Sort crossing columns; if right parity is open, append maxX.
//       - If left parity open: burn [minX, first crossing)
//         Then burn alternating spans between crossing pairs.
void ImageFilledPolygon(AbstrRasterizeInfo* rasterInfo, point_t* padf, part_index_t nPartCount, seq_index_t* panPartSize, BurnValueVariant eBurnValue, IFP_resouces& ifpResources)
{
	assert(rasterInfo);
	assert(padf);
  
	// count nr of edges (also equals number of vertices)
    seq_index_t n = 0;
    for(part_index_t part = 0; part < nPartCount; part++ )
        n += panPartSize[part];
	if (!n)
		return;

	// Full raster bounds
	rowcol_t miny = 0, maxy = rasterInfo->m_RasterSize.Y();
	rowcol_t minx = 0, maxx = rasterInfo->m_RasterSize.X();
	coord_t dminx = minx+0.5, dmaxx = (maxx-1)+0.5;
	coord_t dminy = miny+0.5, dmaxy = (maxy-1)+0.5;

	// Round vertices to integer rows/cols with half-open rounding.
	// This prepares for edge bucketing by start row and quick out-of-range filtering.
	auto& rows = ifpResources.rows;	rows.clear(); rows.reserve(n);
	auto& cols = ifpResources.cols; cols.clear(); cols.reserve(n);
	for (seq_index_t i = 0; i < n; i++) {
		coord_t dx = padf[i].X();
		coord_t dy = padf[i].Y();
		cols.push_back((dx <= dminx) ? minx : (dx > dmaxx) ? maxx : RoundPositiveHalfOpen<sizeof(rowcol_t)>(dx));
		rows.push_back((dy <= dminy) ? miny : (dy > dmaxy) ? maxy : RoundPositiveHalfOpen<sizeof(rowcol_t)>(dy));
	}

	// Compute effective raster sub-extent touched by the polygon vertices.
	// If width/height is zero in rounded space, nothing to rasterize.
	auto xRange = minmax_element(cols.begin(), cols.end()); minx = *xRange.first; maxx = *xRange.second; rowcol_t nrx = maxx - minx; if (!nrx) return;
	auto yRange = minmax_element(rows.begin(), rows.end()); miny = *yRange.first; maxy = *yRange.second; rowcol_t nry = maxy - miny; if (!nry) return;
	dminx = minx + 0.5, dmaxx = (maxx - 1) + 0.5;

	// Prepare edge change events ordered by row (bucketed by start row).
	auto& edgeListStarts = ifpResources.edgeListStarts;
	auto& edgeLists = ifpResources.edgeLists;
	auto& leftTogglePoints = ifpResources.leftTogglePoints;
	auto& rightTogglePoints = ifpResources.rightTogglePoints;
	edgeLists.clear(); // initialize reusable resource
	vector_fill_n(edgeListStarts, nry, END_OF_LIST);
	vector_fill_n(leftTogglePoints, nry+1, false);
	vector_fill_n(rightTogglePoints, nry+1, false);

	// Track current ring (part) to pair edges correctly across rings.
	part_index_t part = 0;
	seq_index_t partoffset = 0, partend = panPartSize[part];

	// Edges are produced by consecutive point pairs within a ring, including
	// the closing edge (last->first). For each edge, direct it so y1<y2.
	// Cases:
	//   - Edge fully at left of viewport -> toggle left parity at start/stop rows.
	//   - Edge fully at right of viewport -> toggle right parity at start/stop rows.
	//   - Otherwise -> bucket edge into list for min(y1, y2).
	for (seq_index_t i = 0; i < n; i++) {
		while (i == partend) {
			assert(part < nPartCount);
			partoffset = partend;
			partend += panPartSize[part];
			part++;
		}

		seq_index_t prev_i;
		if (i == partoffset)
			prev_i = partend - 1;
		else
			prev_i = i - 1;
		rowcol_t y1 = rows[prev_i];
		rowcol_t y2 = rows[i];

		if (y1 == y2) continue; // horizontal edge does not cross any scanline row center

		rowcol_t x1 = cols[prev_i], x2 = cols[i];
		if (x1 <= minx && x2 <= minx) // entire edge is left of viewport, handled via left toggles
		{
			leftTogglePoints[y1-miny].flip();
			leftTogglePoints[y2-miny].flip();
		}
		else if (x1 >= maxx && x2 >= maxx) // entire edge is right of viewport, handled via right toggles
		{
			rightTogglePoints[y1-miny].flip();
			rightTogglePoints[y2-miny].flip();
		}
		else
		{
			rowcol_t y = Min<rowcol_t>(y1, y2); // row number where this edge becomes active.
			assert(y >= miny);
			assert(y < maxy);
			auto& edgeListStart = edgeListStarts[y-miny];
			seq_index_t prevIndex = edgeListStart;
			edgeListStart = edgeLists.size();
			edgeLists.emplace_back(poly_edge_node{ prevIndex, (y1 < y2) ? poly_edge{ prev_i, i } : poly_edge{ i, prev_i } });
		}
	}
	assert(part == nPartCount - 1);

	// Iterators helpful for row-stepping.
	auto leftTogglePointsPtr = leftTogglePoints.begin();
	auto rightTogglePointsPtr = rightTogglePoints.begin();
	auto edgeListStartsPtr = edgeListStarts.begin();

	// Active edge set for current scanline.
	std::vector<poly_edge>& currEdges = ifpResources.currEdges;
	currEdges.clear(); // initialize reusable resource

	// open_left/open_right keep parity across edges lying strictly outside viewport.
	bool open_left = false, open_right = false;
	for (; miny < maxy; miny++)
	{
		coord_t dy = miny + 0.5; /* center height of line*/

		// Apply toggles for this row.
		if (*leftTogglePointsPtr++)
			open_left = !open_left;

		if (*rightTogglePointsPtr++)
			open_right = !open_right;

		// Remove edges that ended before or at this row.
		auto currEdgesEnd = currEdges.end();
		currEdges.erase(std::remove_if(currEdges.begin(), currEdgesEnd, [rowsPtr = rows.begin(), miny](const poly_edge& e) {return rowsPtr[e.second] <= miny; }), currEdgesEnd);

		// Insert edges starting at this row.
		for (seq_index_t currEdgeIndex = *edgeListStartsPtr++; currEdgeIndex != END_OF_LIST; currEdgeIndex = edgeLists[currEdgeIndex].next_index)
			currEdges.push_back(edgeLists[currEdgeIndex].edge);

		// Compute intersections for active edges; store as rounded columns.
		std::vector<rowcol_t>& crossingCols = ifpResources.crossingCols;
		crossingCols.clear(); // initialize reusable resource

		bool open_left_here = open_left, open_right_here = open_right;
		for (auto edge: currEdges) {
			rowcol_t x1 = cols[edge.first], x2 = cols[edge.second];
			if (x1 != x2) {
				// Compute intersection X at dy, with the directed edge (y1<y2).
				coord_t dy1 = padf[edge.first].Y(), dy2 = padf[edge.second].Y();
				assert(dy1 <= dy2);
				assert(dy1 <= dy); // edge must have been activated already
				assert(dy2 > dy);  // edge not ended yet

				coord_t dx1 = padf[edge.first].X(), dx2 = padf[edge.second].X();

				assert(dx1 > dminx || dx2 > dminx);
				assert(dx1 <= dmaxx || dx2 <= dmaxx);
				coord_t intersect = (dy - dy1) * (dx2 - dx1) / (dy2 - dy1) + dx1;
				if (intersect <= dminx)
				{
					// Intersection strictly left of viewport; flip left parity.
					open_left_here = !open_left_here;
					continue;
				}
				else if (intersect > dmaxx) {
					// Intersection strictly right of viewport; flip right parity.
					open_right_here = !open_right_here;
					continue;
				}
				// Round to half-open integer column.
				x1 = RoundPositiveHalfOpen<sizeof(rowcol_t)>(intersect);
			}
			crossingCols.push_back(x1);
		}

		// Sort crossing columns; enforce even-odd parity.
		std::sort(crossingCols.begin(), crossingCols.end());
		if (open_right_here)
			crossingCols.emplace_back(maxx);
		SizeT i=0, e = crossingCols.size();

		// If total crossings plus left-open parity is odd, the polygon is ill-formed.
		assert(!((e + int(open_left_here)) % 2));
		if ((e + int(open_left_here)) % 2) // avoid crash anyway
			continue; // by skipping this whole undisiplined row

		// If left-open, burn from viewport minx to first crossing.
		if (open_left_here)
		{
			assert(e % 2 == 1); // follows from previous assert
			assert(minx <= crossingCols[0]);
			assert(crossingCols[0] <= maxx);
			rasterInfo->BurnLine(miny, minx, crossingCols[i++], eBurnValue);
		}
		assert(!((e-i) % 2)); // follows from previous asserts

		// Burn alternating spans between crossing pairs [i, i+1).
		for (; i < e; i += 2)
		{
			assert(crossingCols[i] >= minx);
			assert(crossingCols[i] <= crossingCols[i+1]);
			assert(crossingCols[i+1] <= maxx);
			rasterInfo->BurnLine(miny, crossingCols[i], crossingCols[i + 1], eBurnValue);
		}
    }

	// Ensure pointers walked to expected ends (sanity).
	assert(leftTogglePointsPtr == leftTogglePoints.end()-1);
	assert(rightTogglePointsPtr == rightTogglePoints.end()-1);
	assert(edgeListStartsPtr == edgeListStarts.end());
}

/************************************************************************/
/*                       gv_rasterize_one_shape()                       */
/*   Convenience wrapper to rasterize a single multi-ring polygon.      */
/************************************************************************/
void rasterize_one_shape(AbstrRasterizeInfo* rasterInfo, std::vector<DPoint>& poShape, BurnValueVariant eBurnValue, IFP_resouces& ifpResources)
{
	assert(rasterInfo);

    if (poShape.empty())
        return;

	seq_index_t poNrValues = poShape.size();

	// Single part polygon (one ring) with all vertices.
	ImageFilledPolygon(rasterInfo, &(poShape[0]), 1, &poNrValues, eBurnValue, ifpResources);
}

// *****************************************************************************
//									BoundsArray(s)
// *****************************************************************************
//
// Provide cached bounding boxes for polygon sequences. Used to quickly reject
// polygons/blocks/tiles when they don't intersect the viewport.
//
#include "BoundingBoxCache.h"

// Get bounding box cache over sequence values for the polygon attribute.
// This is templated over the point type in the value unit.
std::shared_ptr<const AbstrBoundingBoxCache> GetSequenceBounds(const AbstrDataItem* polyAttr, bool mustPrepare)
{
	return visit_and_return_result<typelists::seq_points, std::shared_ptr<const AbstrBoundingBoxCache> >(
		polyAttr->GetAbstrValuesUnit(),
		[polyAttr, mustPrepare]<typename P >(const Unit<P>*)
		{
			return GetSequenceBoundingBoxCache<scalar_of_t<P>>(polyAttr, mustPrepare);
		}
	);
}


namespace poly2grid
{
	// Abstract getter to fetch polygon sequences per-tile, independent of
	// concrete point type.
	struct AbstrSequenceGetter
	{
		virtual void OpenTile(const AbstrDataObject* polyData, tile_id t) = 0;
		virtual void GetValue(SizeT i, std::vector<DPoint>& dPoints)   = 0;
		virtual ~AbstrSequenceGetter() {}
	};

	// Concrete typed sequence getter for polygon data arrays.
	template <typename P>
	struct SequenceGetter : AbstrSequenceGetter
	{
		typedef typename sequence_traits<P>::container_type PolyType;
		typedef typename DataArray<PolyType>::locked_cseq_t locked_cseq_t;

		void OpenTile(const AbstrDataObject* polyData, tile_id t) override
		{
			m_Seq = const_array_cast<PolyType>(polyData)->GetTile(t);
		}

		void GetValue(SizeT i, std::vector<DPoint>& dPoints) override
		{
			typename DataArray<PolyType>::const_reference seq = m_Seq[i];
			dPoints.clear();
			dPoints.reserve(seq.size());
			dPoints.insert(dPoints.begin(), seq.begin(), seq.end());
		}

		locked_cseq_t m_Seq;
	};

	// Factory for polymorphic sequence getter based on values unit.
	auto CreateSequenceGetter(const AbstrUnit* polygonCoordinateUnit)
	{
		return visit_and_return_result<typelists::seq_points, AbstrSequenceGetter*>(
			polygonCoordinateUnit,
			[]<typename P >(const Unit<P>*)
		{
			return new SequenceGetter<P>;
		}
		);
	}

	// Dispatcher per raster tile: transforms polygon coordinates into tile-local
	// grid space and burns polygons into the result buffer.
	struct p2g_DispatcherTileData
	{
		p2g_DispatcherTileData(AbstrDataObject* resObj, const AbstrUnit* resDomain, const AbstrDataItem* polyAttr, WeakPtr<const AbstrBoundingBoxCache> boxesArrays, tile_id tg)
			:	m_ResObj(resObj)
			,	m_PolyAttr(polyAttr)
			,	m_BoxesArrays(boxesArrays)
			,	m_RasterTileId(tg)
			,	m_ViewPortInfo(polyAttr, resDomain, tg, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, false, countcolor_t(-1), false)
		{
			m_SequenceGetter.reset( CreateSequenceGetter(m_PolyAttr->GetAbstrValuesUnit()) );
		}


		// constant for all tiles
		AbstrDataObject*                     m_ResObj;
		WeakPtr<const AbstrDataItem>         m_PolyAttr;
		WeakPtr<const AbstrBoundingBoxCache> m_BoxesArrays;

		// tile and sub-tile dependent
		ViewPortInfoEx<Int32>          m_ViewPortInfo;
		tile_id                        m_RasterTileId;
		std::unique_ptr<AbstrSequenceGetter> m_SequenceGetter;

		// Template over domain E: map polygon domain indices to burn values and write into raster tile.
		template <typename E>
		void GetBitmap(typename Unit<E>::range_t indexRange) // corresponding with the indices of m_PolyData
		{
			RasterSizeType size = m_ViewPortInfo.GetViewPortSize();
			IPoint base = m_ViewPortInfo.GetViewPortOrigin();

			const AbstrDataItem* polyAttr = m_PolyAttr;
			const AbstrDataObject* polyData = polyAttr->GetCurrRefObj();
			const AbstrUnit* abstrPolyDomain = polyAttr->GetAbstrDomainUnit(); // could be void domain.
			assert(abstrPolyDomain);
			const Unit<E>* polyDomain = dynamic_cast<const Unit<E>*>(abstrPolyDomain); // could be nullptr

			DRect clipRect = m_ViewPortInfo.GetViewPortInGrid();

			// Create transform from layer/world coords to tile-local grid coords.
			CrdTransformation transForm = m_ViewPortInfo.Inverse();
			transForm -= base;
			std::vector<DPoint> dPoints;
			IFP_resouces ifpResources; // per-call scratch

			// Acquire write buffer for tile and wrap as RasterizeInfo sink.
			auto res = mutable_array_cast<E>(m_ResObj)->GetDataWrite(m_RasterTileId, dms_rw_mode::write_only_all);
			MG_CHECK(res.size() == Cardinality(size)); // or at least at least
			auto rasterInfo = RasterizeInfo<E>(Convert<RasterSizeType>(size), res.begin());

			assert(m_BoxesArrays);

			// Iterate polygon tiles and features, reject with tile/block/feature bounds.
			for (tile_id tp = 0, te = abstrPolyDomain->GetNrTiles(); tp!=te; ++tp)
			{
				typename Unit<E>::range_t tileIndexRange = polyDomain ? polyDomain->GetTileRange(tp) : indexRange;

				if (!IsIntersecting(clipRect, m_BoxesArrays->GetTileBounds(tp)))
					continue;

				AbstrSequenceGetter* sg = m_SequenceGetter.get();
				assert(sg);
				sg->OpenTile(polyData, tp);

				for (tile_offset i = 0, e = abstrPolyDomain->GetTileCount(tp); i != e; ++i)
				{
					try
					{
						// Skip whole blocks when not intersecting clipRect.
						if (!(i % AbstrBoundingBoxCache::c_BlockSize))
							while (!IsIntersecting(clipRect, m_BoxesArrays->GetBlockBounds(tp, i / AbstrBoundingBoxCache::c_BlockSize)))
							{
								i += AbstrBoundingBoxCache::c_BlockSize;
								if (!(i < e))
									goto end_of_tile_loop;
							}

						// Fine-grained feature rejection.
						if (!IsIntersecting(clipRect, m_BoxesArrays->GetBounds(tp, i)))
							continue;
						sg->GetValue(i, dPoints);

						// Clean degenerate vertices (adjacent duplicates/spikes).
						remove_adjacents_and_spikes(dPoints);
						if (dPoints.size() < 3)
							continue;

						// Transform to tile-local grid coords.
						for (auto pi = dPoints.begin(), pe = dPoints.end(); pi != pe; ++pi)
							transForm.InplApply(*pi);

						// Compute burn value per-feature (domain dependent).
						BurnValueVariant eBurnValueSource = Range_GetValue_naked(tileIndexRange, i);

						// Rasterize polygon to tile buffer.
						rasterize_one_shape(&rasterInfo, dPoints, eBurnValueSource, ifpResources);
					}
					catch (DmsException& x)
					{
						x.AsErrMsg()->TellExtraF("\nin poly2grid at tile %d and index %d", tp, i);
						throw;
					}
				}
			end_of_tile_loop: ;
			}
		}

		// Specialization for Void domain: map to Bool domain with trivial range.
		template <>
		void GetBitmap<Void>(typename Unit<Void>::range_t /*indexRange*/)
		{
			GetBitmap<Bool>(Unit<Bool>::range_t(1, 2));
		}

	};

	// Dispatcher producing RLE results instead of direct raster writes.
	struct p2ag_DispatcherTileData
	{
		p2ag_DispatcherTileData(const AbstrUnit* resDomain, const AbstrDataItem* polyAttr)
			: m_PolyAttr(polyAttr)
			, m_ViewPortInfo(polyAttr, resDomain, no_tile, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, false, countcolor_t(-1), false)
		{}


		WeakPtr<const AbstrDataItem>   m_PolyAttr;
		ViewPortInfoEx<Int32>          m_ViewPortInfo;


		// Gather RLE spans for all features in a polygon tile.
		template <typename RT>
		auto GetTileResults(tile_id tp) -> RLE_polygon_tileset<scalar_of_t<RT>>
		{
			RasterSizeType size = m_ViewPortInfo.GetViewPortSize();
			IPoint base = m_ViewPortInfo.GetViewPortOrigin();

			const AbstrDataItem* polyAttr = m_PolyAttr;
			const AbstrDataObject* polyData = polyAttr->GetCurrRefObj();
			const AbstrUnit* abstrPolyDomain = polyAttr->GetAbstrDomainUnit(); // could be void domain.
			assert(abstrPolyDomain);

		//	const Unit<E>* polyDomain = dynamic_cast<const Unit<E>*>(abstrPolyDomain); // could be nullptr

			DRect clipRect = m_ViewPortInfo.GetViewPortInGrid();

			CrdTransformation transForm = m_ViewPortInfo.Inverse();
			transForm -= base;
			std::vector<DPoint> dPoints;
			IFP_resouces ifpResources;

			auto rleInfo = RLE_Info<scalar_of_t<RT>>(Convert<RasterSizeType>(size));

			// Per-tile sequence getter.
			auto sg = std::unique_ptr<AbstrSequenceGetter>( CreateSequenceGetter(m_PolyAttr->GetAbstrValuesUnit()) );
			assert(sg);
			sg->OpenTile(polyData, tp);

			tile_offset i=0, te = abstrPolyDomain->GetTileCount(tp);
			auto result = RLE_polygon_tileset<scalar_of_t<RT>>(te);

			try
			{
				for (; i != te; ++i)
				{
					sg->GetValue(i, dPoints);
					auto polyBounds = DRect(dPoints.begin(), dPoints.end(), false, false);
					if (!IsIntersecting(clipRect, polyBounds))
						continue;

					remove_adjacents_and_spikes(dPoints);
					if (dPoints.size() < 3)
						continue;

					for (auto pi = dPoints.begin(), pe = dPoints.end(); pi != pe; ++pi)
						transForm.InplApply(*pi);

					// Burn to RLE collector; burn value not used here.
					rasterize_one_shape(&rleInfo, dPoints, BurnValueVariant(), ifpResources);
					result[i] = std::move(rleInfo.result);
					// rleInfo.result is intentionally moved; object reused for next feature.
				}
			}
			catch (DmsException& x)
			{
				x.AsErrMsg()->TellExtraF("\nin poly2allgrids at tile %d and index %d", tp, i);
				throw;
			}
			return result;
		}
	};

}

// *****************************************************************************
//									Poly2GridOper (PolygonAttr, GridSet)
// *****************************************************************************
//
// Operator that burns polygon attributes into a raster grid domain. Supports
// both tiled and untiled execution.
//
CommonOperGroup cog_poly2grid("poly2grid", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cog_poly2grid_untiled("poly2grid_untiled", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cog_poly2grid32("poly2allgrids", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cog_poly2grid64("poly2allgrids_uint64", oper_policy::better_not_in_meta_scripting);

struct Poly2GridOperator : public BinaryOperator
{
	bool doUntiled;

	Poly2GridOperator(const DataItemClass* polyAttrClass, const UnitClass* gridSetType, bool doUntiled_)
		: BinaryOperator(doUntiled_ ? &cog_poly2grid_untiled : &cog_poly2grid, AbstrDataItem::GetStaticClass()
			, polyAttrClass
			, gridSetType
		)
		, doUntiled(doUntiled_)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() >= 2);
		if (args.size() > 2 && !resultHolder)
			// throwErrorD("poly2grid", "Obsolete third argument used; replace by poly2grid(polygons, gridDomain)");
			reportD(SeverityTypeID::ST_Warning, "poly2grid: Obsolete third argument used; replace by poly2grid(polygons, gridDomain)");
		const AbstrDataItem* polyAttr = AsDataItem(args[0]); // can be segmented
		const AbstrUnit* gridDomainUnit = AsUnit(args[1]); // can be tiled

		assert(polyAttr);
		assert(gridDomainUnit);

		const AbstrUnit* polyDomainUnit = polyAttr->GetAbstrDomainUnit();
		assert(polyDomainUnit);
		if (polyDomainUnit->GetValueType() == ValueWrap<Void>::GetStaticClass())
			polyDomainUnit = Unit<Bool>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(gridDomainUnit, polyDomainUnit);

		if (!mustCalc)
		{
			// Validate transform compatibility early (no execution).
			ViewPortInfoEx<Int32> viewPortInfoCheck(polyAttr, gridDomainUnit, no_tile, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, true, countcolor_t(-1), false);
		}
		else
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			DataReadLock arg1Lock(polyAttr);

			auto bounds = GetSequenceBounds(polyAttr, false);

			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			Calculate(resLock.get(), res->GetAbstrDomainUnit(), polyAttr, bounds.get());

			resLock.Commit();
		}
		return true;
	}

	// resObj: has gridDomain and domain of polyAttr as valuesUnit
	// polyAttr can be segmented
	void Calculate(AbstrDataObject* resObj, const AbstrUnit* resDomain, const AbstrDataItem* polyAttr, const AbstrBoundingBoxCache* boxesArray) const
	{
		bool isAllDefined = polyAttr->HasUndefinedValues(); // note: result not used; kept for future optimizations

		// TODO G8: Avoid double work for polygons that intersect multiple tiles:
		//          e.g. with an ordered heap processing neighboring tiles together.

		auto poly2gridFunctor = [resObj, resDomain, polyAttr, boxesArray](tile_id tg)
		{
			poly2grid::p2g_DispatcherTileData dispatcherTileData(resObj, resDomain, polyAttr, boxesArray, tg);

			// Dispatch on polygon domain type to get proper index range mapping.
			visit<typelists::domain_int_types>(polyAttr->GetAbstrDomainUnit(),
				[&dispatcherTileData]<typename E>(const Unit<E>* polyDomain)
				{
					dispatcherTileData.GetBitmap<E>(polyDomain->GetRange());
				}
			);
		};

		// Execute either as a single untiled run or parallel over tiles.
		if (doUntiled)
			poly2gridFunctor(no_tile);
		else
			parallel_tileloop(resObj->GetTiledRangeData()->GetNrTiles(), poly2gridFunctor);
	}
};

static TokenID s_PolygonRelTokenID = GetTokenID_st("polygon_rel");
static TokenID s_GridRelTokenID    = GetTokenID_st("grid_rel");

// *****************************************************************************
//									Poly2AllGridsOperator
// *****************************************************************************
// Produces two outputs within a synthetic domain:
//  - polygon_rel: relation to original polygon domain (optional for Void)
//  - grid_rel: relation to raster grid cells covered by polygons
// Uses RLE spans gathered per-tile to enumerate all covered raster coordinates.
//
template<typename DomainType = UInt32>
struct Poly2AllGridsOperator : public BinaryOperator
{
	Poly2AllGridsOperator(AbstrOperGroup& aog, const DataItemClass* polyAttrClass, const UnitClass* gridSetType)
		: BinaryOperator(&aog, Unit<DomainType>::GetStaticClass()
			, polyAttrClass
			, gridSetType
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);
		const AbstrDataItem* polyAttr = AsDataItem(args[0]); // can be segmented
		const AbstrUnit* gridDomainUnit = AsUnit(args[1]); // can be tiled

		assert(polyAttr);
		assert(gridDomainUnit);

		const AbstrUnit* polyDomainUnit = polyAttr->GetAbstrDomainUnit();
		assert(polyDomainUnit);

		// Setup synthetic domain and result attributes.
		resultHolder = Unit< DomainType>::GetStaticClass()->CreateResultUnit(resultHolder).release();
		auto resDomain = AsUnit(resultHolder.GetNew()); assert(resDomain);
		AbstrDataItem* resPolyRelAttr = nullptr;
		if (polyDomainUnit->GetValueType() != ValueWrap<Void>::GetStaticClass())
			resPolyRelAttr = CreateDataItem(resDomain, s_PolygonRelTokenID, resDomain, polyDomainUnit, ValueComposition::Single);
		AbstrDataItem* resGridRelAttr = CreateDataItem(resDomain, s_GridRelTokenID, resDomain, gridDomainUnit, ValueComposition::Single);

		if (!mustCalc)
		{
			// Validation only.
			ViewPortInfoEx<Int32> viewPortInfoCheck(polyAttr, gridDomainUnit, no_tile, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, true, countcolor_t(-1), false);
		}
		else
		{
			DataReadLock arg1Lock(polyAttr);

			Calculate(resDomain, resPolyRelAttr, resGridRelAttr, polyAttr, gridDomainUnit);

		}
		return true;
	}

	// resObj: has gridDomain and domain of polyAttr as valuesUnit
	// polyAttr can be segmented
	void Calculate(AbstrUnit* resDomain, AbstrDataItem* resPolyRelAttr, AbstrDataItem* resGridRelAttr, const AbstrDataItem* polyAttr, const AbstrUnit* rasterDomain) const
	{
		bool isAllDefined = polyAttr->HasUndefinedValues(); // note: retained for future use

		// TODO G8: Avoid double work of polygons that intersect multiple tiles.

		auto tpn = polyAttr->GetAbstrDomainUnit()->GetTiledRangeData()->GetNrTiles();

		// Raster-domain dependent output for grid_rel encoding (point type).
		visit<typelists::int_points>(rasterDomain, [resDomain, resPolyRelAttr, resGridRelAttr, polyAttr, tpn]<typename RT >(const Unit<RT>*rasterDomain)
			{
				// Collect per-polygon RLE results for all tiles.
				auto resultingWall = RLE_polygon_wall<int>(tpn);
				auto resultingPointCount = std::vector<SizeT>(tpn);

				auto dispatcherTileData = poly2grid::p2ag_DispatcherTileData(rasterDomain, polyAttr);

				// Compute RLE per tile and count total points covered.
				auto poly2allgridsFunctor = [&dispatcherTileData, &resultingWall, &resultingPointCount](tile_id tp)
				{
					resultingWall[tp] = dispatcherTileData.GetTileResults<int>(tp);
					ASyncContinueCheck();

					SizeT pointCount = 0;
					for (const auto& resultingRLE_area : resultingWall[tp])
						for (const auto& resultingRLE : resultingRLE_area)
							pointCount += (resultingRLE.m_End - resultingRLE.m_Start);
					resultingPointCount[tp] = pointCount;
				};

				parallel_tileloop(tpn, poly2allgridsFunctor);

				// Set resulting synthetic domain size.
				SizeT totalPointCount = 0;
				for (auto pointCount : resultingPointCount)
					totalPointCount += pointCount;
				resDomain->SetCount(totalPointCount);

				// Write polygon_rel if polygon domain is not Void.
				visit<typelists::domain_types>(polyAttr->GetAbstrDomainUnit(), [resPolyRelAttr, tpn, &resultingWall]<typename E>(const Unit<E>* domain)
				{
					if constexpr (!std::is_same_v<E, Void>)
					{
						assert(resPolyRelAttr);
						auto resPolyRelLock = DataWriteLock(resPolyRelAttr, dms_rw_mode::write_only_all);
						auto resPolyRelWriter = tile_write_channel<E>(mutable_array_cast<E>(resPolyRelLock.get()));
						for (tile_id tp = 0; tp < tpn; tp++)
						{
							auto tileRange = domain->GetTileRange(tp);
							tile_offset ti = 0;
							for (const auto& resultingRLE_area : resultingWall[tp])
							{
								auto e = Range_GetValue_naked(tileRange, ti++);
								for (const auto& resultingRLE : resultingRLE_area)
									for (auto col = resultingRLE.m_Start; col != resultingRLE.m_End; col++)
										resPolyRelWriter.Write(e);
							}
						}
						resPolyRelLock.Commit();
					}
				});

				// Write grid_rel: absolute raster locations for each covered cell.
				auto resGridRelLock = DataWriteLock(resGridRelAttr, dms_rw_mode::write_only_all);
				auto resGridRelWriter = tile_write_channel<RT>(mutable_array_cast<RT>(resGridRelLock.get()));
				auto vpTopLeft = dispatcherTileData.m_ViewPortInfo.GetViewPortOrigin();
				for (tile_id tp = 0; tp < tpn; tp++)
					for (const auto& resultingRLE_area : resultingWall[tp])
						for (const auto& resultingRLE : resultingRLE_area)
						{
							auto row = resultingRLE.m_Row + vpTopLeft.Row();
							auto col = resultingRLE.m_Start + vpTopLeft.Col();
							auto colEnd = resultingRLE.m_End + vpTopLeft.Col();
							for (; col != colEnd; ++col)
								resGridRelWriter.Write(shp2dms_order<scalar_of_t<RT>>(col, row));
						}
				resGridRelLock.Commit();
			}
		);
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************
//
// Instantiate operators for all supported polygon coordinate types (seq_points)
// and raster domain point types (domain_points).
//
	template <typename DomPoint>
	struct DomainInst
	{
		using DomainUnitType = Unit<DomPoint>;

		Poly2GridOperator m_TiledOper, m_UntiledOper;
		Poly2AllGridsOperator<UInt32> m_P2AG32;
		Poly2AllGridsOperator<UInt64> m_P2AG64;

		DomainInst(const DataItemClass* polygonDataClass)
			: m_TiledOper  (polygonDataClass, DomainUnitType::GetStaticClass(), false)
			, m_UntiledOper(polygonDataClass, DomainUnitType::GetStaticClass(), true)
			, m_P2AG32(cog_poly2grid32, polygonDataClass, DomainUnitType::GetStaticClass())
			, m_P2AG64(cog_poly2grid64, polygonDataClass, DomainUnitType::GetStaticClass())
		{}
	};

	template <typename CrdPoint>
	struct Poly2gridOperators
	{
		using PointType = CrdPoint;
		using PolygonType = sequence_traits<PointType>::container_type;
		using PolygonDataType = DataArray<PolygonType>;

		tl_oper::inst_tuple_templ<typelists::domain_points, DomainInst> m_AllDomainsInst = PolygonDataType::GetStaticClass();
	};

namespace
{
	tl_oper::inst_tuple_templ<typelists::seq_points, Poly2gridOperators> operInstances;
}

