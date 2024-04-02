// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/RangeIndex.h"
#include "mth/Mathlib.h"

#include "AbstrUnit.h"
#include "DataArray.h"
#include "ParallelTiles.h"
#include "Param.h"
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
		dms_assert(ny < m_RasterSize.Y());
		dms_assert(nxStart <  m_RasterSize.X());
		dms_assert(nxEnd   <= m_RasterSize.X());
		dms_assert(nxStart <= nxEnd);

		pointer_type startLinePtr = m_ChunkBuf + ny * m_RasterSize.X();

		T v = eBurnValue;
		fast_fill(startLinePtr + nxStart, startLinePtr + nxEnd, v);
	}

	typename pointer_type m_ChunkBuf;
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
	dms_assert(rasterInfo);
	dms_assert(padf);
  
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
			dms_assert(part < nPartCount);
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
			dms_assert(y >= miny);
			dms_assert(y < maxy);
			auto& edgeListStart = edgeListStarts[y-miny];
			seq_index_t prevIndex = edgeListStart;
			edgeListStart = edgeLists.size();
			edgeLists.emplace_back(poly_edge_node{ prevIndex, (y1 < y2) ? poly_edge{ prev_i, i } : poly_edge{ i, prev_i } });
		}
	}
	dms_assert(part == nPartCount - 1);

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
				dms_assert(dy1 <= dy2);
				dms_assert(dy1 <= dy); // edge must have been activated
				dms_assert(dy2 > dy);  // edge cannot have been ended yet

				coord_t dx1 = padf[edge.first].X(), dx2 = padf[edge.second].X();

				dms_assert(dx1 > dminx || dx2 > dminx);
				dms_assert(dx1 <= dmaxx || dx2 <= dmaxx);
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

		dms_assert(!((e + int(open_left_here)) % 2));
		if ((e + int(open_left_here)) % 2) // avoid crash anyway
			continue; // by skipping this whole undisiplined row

		if (open_left_here)
		{
			dms_assert(e % 2 == 1); // follows from previous assert
			dms_assert(minx <= crossingCols[0]);
			dms_assert(crossingCols[0] <= maxx);
			rasterInfo->BurnLine(miny, minx, crossingCols[i++], eBurnValue);
		}
		dms_assert(!((e-i) % 2)); // follows from previous asserts

		for (; i < e; i += 2)
		{
			dms_assert(crossingCols[i] >= minx);
			dms_assert(crossingCols[i] <= crossingCols[i+1]);
			dms_assert(crossingCols[i+1] <= maxx);
			rasterInfo->BurnLine(miny, crossingCols[i], crossingCols[i + 1], eBurnValue);
		}
    }
	dms_assert(leftTogglePointsPtr == leftTogglePoints.end()-1);
	dms_assert(rightTogglePointsPtr == rightTogglePoints.end()-1);
	dms_assert(edgeListStartsPtr == edgeListStarts.end());
}

/************************************************************************/
/*                       gv_rasterize_one_shape()                       */
/************************************************************************/

void rasterize_one_shape(AbstrRasterizeInfo* rasterInfo, std::vector<DPoint>& poShape, BurnValueVariant eBurnValue, IFP_resouces& ifpResources)
{
	dms_assert(rasterInfo);

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

	struct DispatcherTileData
	{
		DispatcherTileData(AbstrDataObject* resObj, const AbstrUnit* resDomain, const AbstrDataItem* polyAttr, WeakPtr<const AbstrBoundingBoxCache> boxesArrays, tile_id tg)
			:	m_ResObj(resObj)
			,	m_PolyAttr(polyAttr)
			,	m_BoxesArrays(boxesArrays)
			,	m_RasterTileId(tg)
			,	m_ViewPortInfo(polyAttr, resDomain, tg, AsUnit(polyAttr->GetAbstrValuesUnit()->GetCurrRangeItem()), no_tile, nullptr, false, false, countcolor_t(-1), false)
		{
			CreateSequenceGetter();
		}

		void CreateSequenceGetter();

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
			dms_assert(abstrPolyDomain);
			const Unit<E>* polyDomain = dynamic_cast<const Unit<E>*>(abstrPolyDomain); // could be nullptr

			DRect clipRect = m_ViewPortInfo.GetViewPortInGrid();

			CrdTransformation transForm = m_ViewPortInfo.Inverse();
			transForm -= base;
			std::vector<DPoint> dPoints;
			IFP_resouces ifpResources;

			auto res = mutable_array_cast<E>(m_ResObj)->GetDataWrite(m_RasterTileId, dms_rw_mode::write_only_all);
			MG_CHECK(res.size() == Cardinality(size)); // or at least at least
			OwningPtr<AbstrRasterizeInfo> rasterInfoPtr;
			rasterInfoPtr.assign( new RasterizeInfo<E>(Convert<RasterSizeType>(size), res.begin()) );

			dms_assert(m_BoxesArrays);

			for (tile_id t = 0, te = abstrPolyDomain->GetNrTiles(); t!=te; ++t)
			{
				typename Unit<E>::range_t tileIndexRange = polyDomain ? polyDomain->GetTileRange(t) : indexRange;

				if (!IsIntersecting(clipRect, m_BoxesArrays->GetTileBounds(t)))
					continue;

				AbstrSequenceGetter* sg = m_SequenceGetter;
				dms_assert(sg);
				sg->OpenTile(polyData, t);

				for (tile_offset i = 0, e = abstrPolyDomain->GetTileCount(t); i != e; ++i)
				{
					if (!(i % AbstrBoundingBoxCache::c_BlockSize))
						while (!IsIntersecting(clipRect, m_BoxesArrays->GetBlockBounds(t, i / AbstrBoundingBoxCache::c_BlockSize)))
						{
							i += AbstrBoundingBoxCache::c_BlockSize;
							if (!(i < e))
								goto end_of_tile_loop;
						}

					if (!IsIntersecting(clipRect, m_BoxesArrays->GetBounds(t, i)))
						continue;
					sg->GetValue(i, dPoints);

					remove_adjacents_and_spikes(dPoints);
					if (dPoints.size() < 3)
						continue;

					for (auto pi = dPoints.begin(), pe = dPoints.end(); pi != pe; ++pi)
						transForm.InplApply(*pi);

					E eBurnValueSource = Range_GetValue_naked(tileIndexRange, i);

					rasterize_one_shape(rasterInfoPtr, dPoints,eBurnValueSource, ifpResources);
				}
			end_of_tile_loop:;
			}
		}
	};

	struct DispatcherBase : UnitProcessor
	{
		template <typename E>
		void VisitImpl(const Unit<E>* inviter) const
		{
			dms_assert(m_Data);
			m_Data->GetBitmap<E>(inviter->GetRange()); // corresponding with the indices of m_PolyData
		}
		void VisitImpl(const Unit<Void>* inviter) const
		{
			dms_assert(m_Data);
			m_Data->GetBitmap<Bool>(Unit<Bool>::range_t(1, 2)); // corresponding with the indices of m_PolyData
		}
		WeakPtr<DispatcherTileData> m_Data;
	};

	struct Dispatcher : boost::mpl::fold<typelists::domain_int_types, DispatcherBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
	{};

	struct SequenceGetterGeneratorBase : UnitProcessor
	{
		template <typename P>
		void VisitImpl(const Unit<P>* inviter) const
		{
			dms_assert(m_Data);
			m_Data->m_SequenceGetter = new SequenceGetter<P>;
		}
		WeakPtr<DispatcherTileData> m_Data;
	};

	struct SequenceGetterGenerator : boost::mpl::fold<typelists::seq_points, SequenceGetterGeneratorBase, VisitorImpl<Unit<boost::mpl::_2>, boost::mpl::_1> >::type
	{};

	void DispatcherTileData::CreateSequenceGetter()
	{
		dms_assert(!m_SequenceGetter);

		SequenceGetterGenerator gen;
		gen.m_Data = this;
		m_PolyAttr->GetAbstrValuesUnit()->InviteUnitProcessor(gen);

		dms_assert(m_SequenceGetter);
	}
}

// *****************************************************************************
//									Poly2GridOper (PolygonAttr, GridSet)
// *****************************************************************************

CommonOperGroup cog_poly2grid("poly2grid", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cog_poly2grid_untiled("poly2grid_untiled", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

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
		dms_assert(args.size() >= 2);
		if (args.size() > 2 && !resultHolder)
			// throwErrorD("poly2grid", "Obsolete third argument used; replace by poly2grid(polygons, gridDomain)");
			reportD(SeverityTypeID::ST_Warning, "poly2grid: Obsolete third argument used; replace by poly2grid(polygons, gridDomain)");
		const AbstrDataItem* polyAttr = AsDataItem(args[0]); // can be segmented
		const AbstrUnit* gridDomainUnit = AsUnit(args[1]); // can be tiled

		dms_assert(polyAttr);
		dms_assert(gridDomainUnit);

		const AbstrUnit* polyDomainUnit = polyAttr->GetAbstrDomainUnit();
		dms_assert(polyDomainUnit);
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
			poly2grid::DispatcherTileData dispatcherTileData(resObj, resDomain, polyAttr, boxesArray, tg);

			poly2grid::Dispatcher disp;
			disp.m_Data = &dispatcherTileData;

			polyAttr->GetAbstrDomainUnit()->InviteUnitProcessor(disp);
		};

		if (doUntiled)
			poly2gridFunctor(no_tile);
		else
			parallel_tileloop(resObj->GetTiledRangeData()->GetNrTiles(), poly2gridFunctor);
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

		DomainInst(const DataItemClass* polygonDataClass)
			: m_TiledOper  (polygonDataClass, DomainUnitType::GetStaticClass(), false)
			, m_UntiledOper(polygonDataClass, DomainUnitType::GetStaticClass(), true)
		{}
	};

	template <typename CrdPoint>
	struct Poly2gridOperators
	{
		typedef std::vector<CrdPoint> PolygonType;
		typedef DataArray<PolygonType> PolygonDataType;

		tl_oper::inst_tuple_templ<typelists::domain_points, DomainInst, const DataItemClass*> m_AllDomainsInst = PolygonDataType::GetStaticClass();
	};

namespace 
{
	tl_oper::inst_tuple_templ<typelists::seq_points, Poly2gridOperators> operInstances;
}
