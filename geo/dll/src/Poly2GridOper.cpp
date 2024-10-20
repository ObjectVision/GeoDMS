// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
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
 * In 2017 a mayor reorganization and code optimization was performed
 * - All edges within the required y-range are ordered by first y and kept 
 *   in the heap currEdges while relevant for the current ray and that is ordered on the last y .
 * - TODO: edges on the left of the x-range within the y-range are coded at togglers at their start and stop row of open left
 * - TODO: edges on the right of the x-range within the y-range are coded at togglers at their start and stop row of open right
 * - edge incidence at x=0.5 or higher results at a pixel start at column 1 or higher, and lower is handled as incidental left toggler
 * - edge incidence at x=width-0.5 or higher results at a pixel start at column 1 or higher, and lower is handled as incidental left toggler
 */

namespace {
	typedef UInt32 rowcol_t;
	typedef SizeT  BurnValueVariant;
	typedef double coord_t;
	typedef UInt32 seq_index_t;
	typedef UInt32 part_index_t;

	typedef Point<rowcol_t> RasterSizeType;
	typedef Point<coord_t> point_t;
	const seq_index_t END_OF_LIST = -1;
};

typedef Couple<seq_index_t> poly_edge;
struct poly_edge_node {
	seq_index_t next_index;
	poly_edge   edge;
};

template <typename CoordType>
struct RLE_range
{
	CoordType m_Row, m_Start, m_End;
};

template <typename CoordType> using RLE_polygon = std::vector<RLE_range<CoordType>>;
template <typename CoordType> using RLE_polygon_tileset = std::vector<RLE_polygon<CoordType>>;
template <typename CoordType> using RLE_polygon_wall = std::vector<RLE_polygon_tileset<CoordType>>;

struct AbstrRasterizeInfo
{
	AbstrRasterizeInfo(RasterSizeType rasterSize)
		:	m_RasterSize(rasterSize)
	{}

	virtual void BurnLine(SizeT ny, SizeT nxStart, SizeT nxEnd, BurnValueVariant eBurnValue) =0;

	RasterSizeType  m_RasterSize;

	// buffers for ImageFilledPolygon to reuse internally. AbstrRasterInfo is assumend to be used thread specific.
};

template <typename T>
struct RasterizeInfo : AbstrRasterizeInfo
{
	typedef typename sequence_traits<T>::pointer pointer_type;
	RasterizeInfo(RasterSizeType rasterSize, pointer_type chunkBuf)
		:	AbstrRasterizeInfo(rasterSize)
		,	m_ChunkBuf(chunkBuf)
	{
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

template <typename TE>
struct RLE_Info : AbstrRasterizeInfo
{
	RLE_Info(RasterSizeType rasterSize)
		: AbstrRasterizeInfo(rasterSize)
	{}

	void BurnLine(SizeT ny, SizeT nxStart, SizeT nxEnd, BurnValueVariant eBurnValue) override
	{
		assert(ny < m_RasterSize.Y());
		assert(nxStart < m_RasterSize.X());
		assert(nxEnd <= m_RasterSize.X());
		assert(nxStart <= nxEnd);

		result.emplace_back(ny, nxStart, nxEnd);
	}
	RLE_polygon<TE> result;
};

struct IFP_resouces {
	std::vector<rowcol_t> rows, cols;
	std::vector<seq_index_t> edgeListStarts;
	std::vector<poly_edge_node> edgeLists;
	std::vector<poly_edge> currEdges;
	std::vector<bool> leftTogglePoints, rightTogglePoints;
	std::vector<rowcol_t> crossingCols;
};

void ImageFilledPolygon(AbstrRasterizeInfo* rasterInfo, point_t* padf, part_index_t nPartCount, seq_index_t* panPartSize, BurnValueVariant eBurnValue, IFP_resouces& ifpResources)
{
	assert(rasterInfo);
	assert(padf);
  
	// count nr of edges
    seq_index_t n = 0;
    for(part_index_t part = 0; part < nPartCount; part++ )
        n += panPartSize[part];
	if (!n)
		return;

	rowcol_t miny = 0, maxy = rasterInfo->m_RasterSize.Y();
	rowcol_t minx = 0, maxx = rasterInfo->m_RasterSize.X();
	coord_t dminx = minx+0.5, dmaxx = (maxx-1)+0.5;
	coord_t dminy = miny+0.5, dmaxy = (maxy-1)+0.5;

	// determine start and end rows of each edge
	auto& rows = ifpResources.rows;	rows.clear(); rows.reserve(n);
	auto& cols = ifpResources.cols; cols.clear(); cols.reserve(n);
	for (seq_index_t i = 0; i < n; i++) {
		coord_t dx = padf[i].X();
		coord_t dy = padf[i].Y();
		cols.push_back((dx <= dminx) ? minx : (dx > dmaxx) ? maxx : RoundPositiveHalfOpen<sizeof(rowcol_t)>(dx));
		rows.push_back((dy <= dminy) ? miny : (dy > dmaxy) ? maxy : RoundPositiveHalfOpen<sizeof(rowcol_t)>(dy));
	}
	auto xRange = minmax_element(cols.begin(), cols.end()); minx = *xRange.first; maxx = *xRange.second; rowcol_t nrx = maxx - minx; if (!nrx) return;
	auto yRange = minmax_element(rows.begin(), rows.end()); miny = *yRange.first; maxy = *yRange.second; rowcol_t nry = maxy - miny; if (!nry) return;
	dminx = minx + 0.5, dmaxx = (maxx - 1) + 0.5;

	// fill heap-queue with edge set change events, ordered by y.
	auto& edgeListStarts = ifpResources.edgeListStarts;
	auto& edgeLists = ifpResources.edgeLists;
	auto& leftTogglePoints = ifpResources.leftTogglePoints;
	auto& rightTogglePoints = ifpResources.rightTogglePoints;
	edgeLists.clear(); // initialize reusable resource
	vector_fill_n(edgeListStarts, nry, END_OF_LIST);
	vector_fill_n(leftTogglePoints, nry+1, false);
	vector_fill_n(rightTogglePoints, nry+1, false);

	part_index_t part = 0;
	seq_index_t partoffset = 0, partend = panPartSize[part];

	// the edges form a set of rings such that each row crosses an even number of edges,
	// were 'cross' means that one end is has an y-coordinate less or equal to that row and another y-coordinate strictly greater that the row-coordinate

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

		if (y1 == y2) continue; // this edge will not cross any y-row

		rowcol_t x1 = cols[prev_i], x2 = cols[i];
		if (x1 <= minx && x2 <= minx) // the crossings of this edge will be less or equal to the first x-column at dminx+0.5
		{
			leftTogglePoints[y1-miny].flip();
			leftTogglePoints[y2-miny].flip();
		}
		else if (x1 >= maxx && x2 >= maxx) // the crossings of this edge will be greater than the last x-column at (dmaxx-1.0)+0.5
		{
			rightTogglePoints[y1-miny].flip();
			rightTogglePoints[y2-miny].flip();
		}
		else
		{
			rowcol_t y = Min<rowcol_t>(y1, y2); // row number of the starting point of this edge.
			assert(y >= miny);
			assert(y < maxy);
			auto& edgeListStart = edgeListStarts[y-miny];
			seq_index_t prevIndex = edgeListStart;
			edgeListStart = edgeLists.size();
			edgeLists.emplace_back(poly_edge_node{ prevIndex, (y1 < y2) ? poly_edge{ prev_i, i } : poly_edge{ i, prev_i } });
		}
	}
	assert(part == nPartCount - 1);

	auto leftTogglePointsPtr = leftTogglePoints.begin();
	auto rightTogglePointsPtr = rightTogglePoints.begin();
	auto edgeListStartsPtr = edgeListStarts.begin();

	std::vector<poly_edge>& currEdges = ifpResources.currEdges;
	currEdges.clear(); // initialize reusable resource

	bool open_left = false, open_right = false;
	for (; miny < maxy; miny++)
	{
		coord_t dy = miny + 0.5; /* center height of line*/

		if (*leftTogglePointsPtr++)
			open_left = !open_left;

		if (*rightTogglePointsPtr++)
			open_right = !open_right;

		// remove obsolete edges
		auto currEdgesEnd = currEdges.end();
		currEdges.erase(std::remove_if(currEdges.begin(), currEdgesEnd, [rowsPtr = rows.begin(), miny](const poly_edge& e) {return rowsPtr[e.second] <= miny; }), currEdgesEnd);

		// insert new edges
		for (seq_index_t currEdgeIndex = *edgeListStartsPtr++; currEdgeIndex != END_OF_LIST; currEdgeIndex = edgeLists[currEdgeIndex].next_index)
			currEdges.push_back(edgeLists[currEdgeIndex].edge);

		std::vector<rowcol_t>& crossingCols = ifpResources.crossingCols;
		crossingCols.clear(); // initialize reusable resource

		// push_back found crossings.
		bool open_left_here = open_left, open_right_here = open_right;
		for (auto edge: currEdges) {
			rowcol_t x1 = cols[edge.first], x2 = cols[edge.second];
			if (x1 != x2) {
				coord_t dy1 = padf[edge.first].Y(), dy2 = padf[edge.second].Y();
				assert(dy1 <= dy2);
				assert(dy1 <= dy); // edge must have been activated
				assert(dy2 > dy);  // edge cannot have been ended yet

				coord_t dx1 = padf[edge.first].X(), dx2 = padf[edge.second].X();

				assert(dx1 > dminx || dx2 > dminx);
				assert(dx1 <= dmaxx || dx2 <= dmaxx);
				coord_t intersect = (dy - dy1) * (dx2 - dx1) / (dy2 - dy1) + dx1;
				if (intersect <= dminx)
				{
					open_left_here = !open_left_here;
					continue;
				}
				else if (intersect > dmaxx) {
					open_right_here = !open_right_here;
					continue;
				}
				x1 = RoundPositiveHalfOpen<sizeof(rowcol_t)>(intersect);
			}
			crossingCols.push_back(x1);
		}

		std::sort(crossingCols.begin(), crossingCols.end());
		if (open_right_here)
			crossingCols.emplace_back(maxx);
		SizeT i=0, e = crossingCols.size();

		assert(!((e + int(open_left_here)) % 2));
		if ((e + int(open_left_here)) % 2) // avoid crash anyway
			continue; // by skipping this whole undisiplined row

		if (open_left_here)
		{
			assert(e % 2 == 1); // follows from previous assert
			assert(minx <= crossingCols[0]);
			assert(crossingCols[0] <= maxx);
			rasterInfo->BurnLine(miny, minx, crossingCols[i++], eBurnValue);
		}
		assert(!((e-i) % 2)); // follows from previous asserts

		for (; i < e; i += 2)
		{
			assert(crossingCols[i] >= minx);
			assert(crossingCols[i] <= crossingCols[i+1]);
			assert(crossingCols[i+1] <= maxx);
			rasterInfo->BurnLine(miny, crossingCols[i], crossingCols[i + 1], eBurnValue);
		}
    }
	assert(leftTogglePointsPtr == leftTogglePoints.end()-1);
	assert(rightTogglePointsPtr == rightTogglePoints.end()-1);
	assert(edgeListStartsPtr == edgeListStarts.end());
}

/************************************************************************/
/*                       gv_rasterize_one_shape()                       */
/************************************************************************/

void rasterize_one_shape(AbstrRasterizeInfo* rasterInfo, std::vector<DPoint>& poShape, BurnValueVariant eBurnValue, IFP_resouces& ifpResources)
{
	assert(rasterInfo);

    if (poShape.empty())
        return;

	seq_index_t poNrValues = poShape.size();

	ImageFilledPolygon(rasterInfo, &(poShape[0]), 1, &poNrValues, eBurnValue, ifpResources);
}

// *****************************************************************************
//									BoundsArray(s)
// *****************************************************************************

#include "BoundingBoxCache.h"

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
	struct AbstrSequenceGetter
	{
		virtual void OpenTile(const AbstrDataObject* polyData, tile_id t) = 0;
		virtual void GetValue(SizeT i, std::vector<DPoint>& dPoints)   = 0;
		virtual ~AbstrSequenceGetter() {}
	};

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

	struct p2g_DispatcherTileData
	{
		p2g_DispatcherTileData(AbstrDataObject* resObj, const AbstrUnit* resDomain, const AbstrDataItem* polyAttr, WeakPtr<const AbstrBoundingBoxCache> boxesArrays, tile_id tg)
			:	m_ResObj(resObj)
			,	m_PolyAttr(polyAttr)
			,	m_BoxesArrays(boxesArrays)
			,	m_RasterTileId(tg)
			,	m_ViewPortInfo(polyAttr, resDomain, tg, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, false, countcolor_t(-1), false)
		{
			m_SequenceGetter = CreateSequenceGetter(m_PolyAttr->GetAbstrValuesUnit());
		}


		// constant for all tiles
		AbstrDataObject*                     m_ResObj;
		WeakPtr<const AbstrDataItem>         m_PolyAttr;
		WeakPtr<const AbstrBoundingBoxCache> m_BoxesArrays;

		// tile and sub-tile dependent
		ViewPortInfoEx<Int32>          m_ViewPortInfo;
		tile_id                        m_RasterTileId;
		OwningPtr<AbstrSequenceGetter> m_SequenceGetter;

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

			CrdTransformation transForm = m_ViewPortInfo.Inverse();
			transForm -= base;
			std::vector<DPoint> dPoints;
			IFP_resouces ifpResources;

			auto res = mutable_array_cast<E>(m_ResObj)->GetDataWrite(m_RasterTileId, dms_rw_mode::write_only_all);
			MG_CHECK(res.size() == Cardinality(size)); // or at least at least
			auto rasterInfo = RasterizeInfo<E>(Convert<RasterSizeType>(size), res.begin());

			assert(m_BoxesArrays);

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
						if (!(i % AbstrBoundingBoxCache::c_BlockSize))
							while (!IsIntersecting(clipRect, m_BoxesArrays->GetBlockBounds(tp, i / AbstrBoundingBoxCache::c_BlockSize)))
							{
								i += AbstrBoundingBoxCache::c_BlockSize;
								if (!(i < e))
									goto end_of_tile_loop;
							}

						if (!IsIntersecting(clipRect, m_BoxesArrays->GetBounds(tp, i)))
							continue;
						sg->GetValue(i, dPoints);

						remove_adjacents_and_spikes(dPoints);
						if (dPoints.size() < 3)
							continue;

						for (auto pi = dPoints.begin(), pe = dPoints.end(); pi != pe; ++pi)
							transForm.InplApply(*pi);

						BurnValueVariant eBurnValueSource = Range_GetValue_naked(tileIndexRange, i);

						rasterize_one_shape(&rasterInfo, dPoints, eBurnValueSource, ifpResources);
					}
					catch (DmsException& x)
					{
						x.AsErrMsg()->TellExtraF("\nin poly2grid at tile %d and index %d", tp, i);
						throw;
					}
				}
			end_of_tile_loop:;
			}
		}

		template <>
		void GetBitmap<Void>(typename Unit<Void>::range_t indexRange)
		{
			GetBitmap<Bool>(Unit<Bool>::range_t(1, 2));
		}

	};

	struct p2ag_DispatcherTileData
	{
		p2ag_DispatcherTileData(const AbstrUnit* resDomain, const AbstrDataItem* polyAttr)
			: m_PolyAttr(polyAttr)
			, m_ViewPortInfo(polyAttr, resDomain, no_tile, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, false, countcolor_t(-1), false)
		{}


		WeakPtr<const AbstrDataItem>   m_PolyAttr;
		ViewPortInfoEx<Int32>          m_ViewPortInfo;


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

			OwningPtr<AbstrSequenceGetter> sg = CreateSequenceGetter(m_PolyAttr->GetAbstrValuesUnit());
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

					rasterize_one_shape(&rleInfo, dPoints, BurnValueVariant(), ifpResources);
					result[i] = std::move(rleInfo.result);
//					MG_CHECK(rleInfo.result.size() == 0);
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
		bool isAllDefined = polyAttr->HasUndefinedValues();

		// TODO G8: Avoid double work of polygons than intersect with multiple tiles: when going throuh the list, take neighbouring tiles into account such as with an ordered heap

		auto poly2gridFunctor = [resObj, resDomain, polyAttr, boxesArray](tile_id tg)
		{
			poly2grid::p2g_DispatcherTileData dispatcherTileData(resObj, resDomain, polyAttr, boxesArray, tg);

			visit<typelists::domain_int_types>(polyAttr->GetAbstrDomainUnit(),
				[&dispatcherTileData]<typename E>(const Unit<E>* polyDomain)
				{
					dispatcherTileData.GetBitmap<E>(polyDomain->GetRange());
				}
			);
		};

		if (doUntiled)
			poly2gridFunctor(no_tile);
		else
			parallel_tileloop(resObj->GetTiledRangeData()->GetNrTiles(), poly2gridFunctor);
	}
};

static TokenID s_PolygonRelTokenID = GetTokenID_st("polygon_rel");
static TokenID s_GridRelTokenID    = GetTokenID_st("grid_rel");

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

		resultHolder = Unit< DomainType>::GetStaticClass()->CreateResultUnit(resultHolder);
		auto resDomain = AsUnit(resultHolder.GetNew()); assert(resDomain);
		AbstrDataItem* resPolyRelAttr = nullptr;
		if (polyDomainUnit->GetValueType() != ValueWrap<Void>::GetStaticClass())
			resPolyRelAttr = CreateDataItem(resDomain, s_PolygonRelTokenID, resDomain, polyDomainUnit, ValueComposition::Single);
		AbstrDataItem* resGridRelAttr = CreateDataItem(resDomain, s_GridRelTokenID, resDomain, gridDomainUnit, ValueComposition::Single);

		if (!mustCalc)
		{
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
		bool isAllDefined = polyAttr->HasUndefinedValues();

		// TODO G8: Avoid double work of polygons than intersect with multiple tiles: when going throuh the list, take neighbouring tiles into account such as with an ordered heap

		auto tpn = polyAttr->GetAbstrDomainUnit()->GetTiledRangeData()->GetNrTiles();

		visit<typelists::int_points>(rasterDomain, [resDomain, resPolyRelAttr, resGridRelAttr, polyAttr, tpn]<typename RT >(const Unit<RT>*rasterDomain)
			{
				auto resultingWall = RLE_polygon_wall<int>(tpn);
				auto resultingPointCount = std::vector<SizeT>(tpn);

				auto dispatcherTileData = poly2grid::p2ag_DispatcherTileData(rasterDomain, polyAttr);

				auto poly2allgridsFunctor = [&dispatcherTileData, &resultingWall, &resultingPointCount](tile_id tp)
				{
					resultingWall[tp] = dispatcherTileData.GetTileResults<int>(tp);
					SizeT pointCount = 0;
					for (const auto& resultingRLE_area : resultingWall[tp])
						for (const auto& resultingRLE : resultingRLE_area)
							pointCount += (resultingRLE.m_End - resultingRLE.m_Start);
					resultingPointCount[tp] = pointCount;
				};

				parallel_tileloop(tpn, poly2allgridsFunctor);

				SizeT totalPointCount = 0;
				for (auto pointCount : resultingPointCount)
					totalPointCount += pointCount;
				resDomain->SetCount(totalPointCount);
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
		using PolygonType = std::vector<CrdPoint>;
		using PolygonDataType = DataArray<PolygonType>;

		tl_oper::inst_tuple_templ<typelists::domain_points, DomainInst, const DataItemClass*> m_AllDomainsInst = PolygonDataType::GetStaticClass();
	};

namespace 
{
	tl_oper::inst_tuple_templ<typelists::seq_points, Poly2gridOperators> operInstances;
}
