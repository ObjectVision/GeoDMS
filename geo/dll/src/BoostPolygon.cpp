// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <numbers>

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "geo/AssocTower.h"
#include "geo/BoostPolygon.h"
#include "geo/SpatialIndex.h"
#include "mci/ValueWrap.h"
#include "ptr/Resource.h"
#include "ptr/ResourceArray.h"
#include "utl/mySPrintF.h"

#include "ParallelTiles.h"
#include "LispTreeType.h"
#include "LockLevels.h"
#include "TileChannel.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

//#pragma warning (disable: 4146 4267)
//#define MG_DEBUG_POLYGON

#include <ipolygon/polygon.hpp>

#include "BoostGeometry.h"
#include "CGAL_Traits.h"
#include "GEOS_Traits.h"

const Int32 MAX_COORD = (1 << 26);

#include "VersionComponent.h"
static VersionComponent s_Boost("boost " BOOST_LIB_VERSION);
static VersionComponent s_BoostPolygon("boost::polygon " BOOST_STRINGIZE(BOOST_POLYGON_VERSION));


template <typename T>
bool coords_using_more_than_25_bits(T coord)
	requires ( is_numeric_v<T> && (sizeof(T) < 4) )
{
	return false;
}

bool coords_using_more_than_25_bits(Int32 v)
{
	return v <= -MAX_COORD || v >= MAX_COORD;
}

bool coords_using_more_than_25_bits(UInt32 v)
{
	return v >= MAX_COORD;
}

template <gtl_point Point>
bool coords_using_more_than_25_bits(Point p)
{
	return coords_using_more_than_25_bits(boost::polygon::x(p)) || coords_using_more_than_25_bits(boost::polygon::y(p));
}

template <gtl_rect Rect>
bool coords_using_more_than_25_bits(const Rect& rect)
{
	return coords_using_more_than_25_bits(boost::polygon::ll(rect)) || coords_using_more_than_25_bits(boost::polygon::ur(rect));
}

// *****************************************************************************
//	boost::polygon serialization
// *****************************************************************************

template <class C>
FormattedOutStream& operator << (FormattedOutStream& os, const boost::polygon::point_data<C>& p)
{
	using namespace boost::polygon;

	os << "(" << x(p) << ", " << y(p) << ")";

	return os;
}

template <class C>
FormattedOutStream& operator << (FormattedOutStream& os, const boost::polygon::rectangle_data<C>& r)
{
	using namespace boost::polygon;

	os << "[" << ll(r) << ", " << ur(r) << "]";

	return os;
}

// *****************************************************************************
//	PolygonOverlay
// *****************************************************************************


static TokenID 
	s_tGM = token::geometry,
	s_tFR = token::first_rel,
	s_tSR = token::second_rel;

class AbstrPolygonOverlayOperator : public VariadicOperator
{
protected:
	using ResultingDomainType = UInt32;

	AbstrPolygonOverlayOperator(AbstrOperGroup& gr, const DataItemClass* polyAttrClass, bool mustCreateGeometries, bool onlyForwardMatches)
		: VariadicOperator(&gr, Unit<ResultingDomainType>::GetStaticClass(), onlyForwardMatches ? 1 : 2)
		,	m_MustCreateGeometries(mustCreateGeometries)
		,	m_OnlyForwardMatches(onlyForwardMatches)
	{
		m_ArgClasses[0] = polyAttrClass;
		if (onlyForwardMatches)
			return;
		m_ArgClasses[1] = polyAttrClass;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == (m_OnlyForwardMatches ? 1 : 2));

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = m_OnlyForwardMatches ? arg1A : AsDataItem(args[1]);
		assert(arg1A); 
		assert(arg2A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		values1Unit->UnifyValues(values2Unit, "v1", "v2", UM_Throw);

		AbstrUnit* res = Unit<ResultingDomainType>::GetStaticClass()->CreateResultUnit(resultHolder);
		resultHolder = res;

		AbstrDataItem* resG = (!m_MustCreateGeometries) ? nullptr : CreateDataItem(res, s_tGM, res, values1Unit, ValueComposition::Polygon);
		AbstrDataItem* res1 = e1IsVoid                  ? nullptr : CreateDataItem(res, s_tFR, res, domain1Unit);
		AbstrDataItem* res2 = e2IsVoid                  ? nullptr : CreateDataItem(res, s_tSR, res, domain2Unit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			ResourceHandle resData;
			ResourceHandle pointBoxDataHandle;

			for (tile_id t=0, te = domain1Unit->GetNrTiles(); t != te; ++t)
			{
				ReadableTileLock readPointLock (arg1A->GetCurrRefObj(), t);
				CreatePointHandle(arg1A, t, pointBoxDataHandle);
			}

			std::atomic<tile_id> intersectCount = 0;

			for (tile_id u=0, ue = domain2Unit->GetNrTiles(); u != ue; ++u)
			{
				ReadableTileLock readPolyLock (arg2A->GetCurrRefObj(), u);
				ResourceHandle polyInfoHandle;
				CreatePolyHandle(arg2A, u, polyInfoHandle);

// 				for (tile_id t=0, te = domainUnit->GetNrTiles(); t != te; ++t)
				leveled_critical_section resInsertSection(item_level_type(0), ord_level_type::SpecificOperatorGroup, "PolygonOverlay.InsertSection");

				parallel_tileloop(domain1Unit->GetNrTiles(), [this, &resInsertSection, arg1A, arg2A, u, &pointBoxDataHandle, &polyInfoHandle, &resData, &intersectCount](tile_id t)->void
					{
						if (this->IsIntersecting(t, u, pointBoxDataHandle, polyInfoHandle))
						{
							ReadableTileLock readPoly1Lock (arg1A->GetCurrRefObj(), t);

							Calculate(resData, resInsertSection, arg1A, arg2A, t, u, polyInfoHandle);

							++intersectCount;
						}
					}
				);
				reportF(SeverityTypeID::ST_MajorTrace, "%s with %s tiles of first argument and after matching %s / %s tiles of second argument, %s intersecting tiles were processed."
					, GetGroup()->GetNameStr()
					, AsString(domain1Unit->GetNrTiles())
					, AsString(u+1), AsString(ue)
					, AsString(intersectCount)
				);
			}

			StoreRes(res, resG, res1, res2, resData);
		}
		return true;
	}
	virtual void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const =0;
	virtual void CreatePointHandle(const AbstrDataItem* pointDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const =0;
	virtual bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const=0;
	virtual void Calculate(ResourceHandle& resData, leveled_critical_section& resInsertSection, const AbstrDataItem* poly1DataA, const AbstrDataItem* poly2DataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const=0;
	virtual void StoreRes(AbstrUnit* res, AbstrDataItem* resG, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resData) const=0;

	bool m_MustCreateGeometries = true, m_OnlyForwardMatches = false;
};

template <typename P, geometry_library GL, bool MustProduceGeometries>
class PolygonOverlayOperator : public AbstrPolygonOverlayOperator
{
	typedef P                       PointType;
	typedef std::vector<PointType>  PolygonType;
	typedef Unit<PointType>         PointUnitType;
	typedef Range<PointType>        BoxType;
	typedef std::vector<BoxType>    BoxArrayType;

	using ArgType = DataArray<PolygonType>;

	struct EmptyBase {};
	struct GeoBase 
	{
		PolygonType m_Geometry;
	};

	struct ResDataElemType : std::conditional_t<MustProduceGeometries, GeoBase, EmptyBase>
	{
		Point<SizeT> m_OrgRel = {SizeT(-1), SizeT(-1)};

		bool operator < (const ResDataElemType& oth) { return m_OrgRel < oth.m_OrgRel;  }
	};

	using ResTileDataType = std::vector<ResDataElemType>;
	using ResDataType     = std::map<Point<tile_id>, ResTileDataType>;

	using ScalarType = scalar_of_t<PointType>;
	using SpatialIndexType = SpatialIndex<ScalarType, typename ArgType::const_iterator>;
	using SpatialIndexArrayType = std::vector<SpatialIndexType>;

public:
	PolygonOverlayOperator(AbstrOperGroup& gr, bool unaryOperation)
		:	AbstrPolygonOverlayOperator(gr, ArgType::GetStaticClass(), MustProduceGeometries, unaryOperation)
	{}

	// Override Operator
	void CreatePolyHandle(const AbstrDataItem* polyDataA, tile_id u, ResourceHandle& polyInfoHandle) const override
	{
		auto polyData  = const_array_cast<PolygonType>(polyDataA);
		assert(polyData); 

		auto polyArray = polyData->GetTile(u);
		polyInfoHandle = makeResource<SpatialIndexType>(polyArray.begin(), polyArray.end(), 0);
	}

	void CreatePointHandle(const AbstrDataItem* polyDataA, tile_id t, ResourceHandle& pointBoxDataHandle) const override
	{
		if (!pointBoxDataHandle)
			pointBoxDataHandle = makeResource<BoxArrayType>(polyDataA->GetAbstrDomainUnit()->GetNrTiles());
		BoxArrayType& boxArray = GetAs<BoxArrayType>(pointBoxDataHandle);

		auto polyData = const_array_cast<PolygonType>(polyDataA);
		assert(polyData); 

		auto polyArray = polyData->GetTile(t);
		assert(t < boxArray.size());
		boxArray[t] = RangeFromSequence(polyArray.begin(), polyArray.end());
	}

	bool IsIntersecting(tile_id t, tile_id u, ResourceHandle& pointBoxDataHandle, ResourceHandle& polyInfoHandle) const override
	{
		BoxArrayType&     boxArray   = GetAs<BoxArrayType>(pointBoxDataHandle);
		SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		return ::IsIntersecting(spIndexPtr->GetBoundingBox(), boxArray[t]);
	}

	void Calculate(ResourceHandle& resDataHandle, leveled_critical_section& resInsertSection, const AbstrDataItem* poly1DataA, const AbstrDataItem* poly2DataA, tile_id t, tile_id u, const ResourceHandle& polyInfoHandle) const override
	{
		auto poly1Data = const_array_cast<PolygonType>(poly1DataA);
		auto poly2Data = const_array_cast<PolygonType>(poly2DataA);

		assert(poly1Data); 
		assert(poly2Data);

		auto poly1Array = poly1Data->GetTile(t);
		auto poly2Array = poly2Data->GetTile(u);

		SizeT p1Offset = poly1DataA->GetAbstrDomainUnit()->GetTileFirstIndex(t);
		SizeT p2Offset = poly2DataA->GetAbstrDomainUnit()->GetTileFirstIndex(u);

		const SpatialIndexType* spIndexPtr = GetOptional<SpatialIndexType>(polyInfoHandle);

		using box_iter_type =typename SpatialIndexType::template iterator<BoxType>;
		using coordinate_type = scalar_of_t<P>;
		using polygon_with_holes_type = bp::polygon_with_holes_data<coordinate_type>;
		using polygon_set_type = bp::polygon_set_data< coordinate_type >;

		using res_data_elem_type = ResDataElemType;

		ResTileDataType* resTileData = nullptr;
		if (!resTileData)
		{
			leveled_critical_section::scoped_lock resLock(resInsertSection); // free this lock after *resTileData has been created.

			if (!resDataHandle)
				resDataHandle = makeResource<ResDataType>();
			ResDataType& resData = GetAs<ResDataType>(resDataHandle);
			resTileData = &(resData[Point<tile_id>(t, u)]);
		}
		assert(resTileData);

		leveled_critical_section resLocalAdditionSection(item_level_type(0), ord_level_type::SpecificOperator, "Polygon.LocalAdditionSection");

		// avoid overhead of parallel_for context switch admin 
		bool onlyForwardMatches = this->m_OnlyForwardMatches;
		serial_for(SizeT(0), poly1Array.size(), [p1Offset, p2Offset, &poly1Array, &poly2Array, spIndexPtr, resTileData, &resLocalAdditionSection, onlyForwardMatches](SizeT i)->void
		{
			res_data_elem_type back;
			try {
				auto polyPtr = poly1Array.begin() + i;
				SizeT p1_rel = p1Offset + i;
				BoxType bbox = RangeFromSequence(polyPtr->begin(), polyPtr->end());
				if (!::IsIntersecting(spIndexPtr->GetBoundingBox(), bbox))
					return;

				if constexpr (GL == geometry_library::boost_polygon)
				{
					typename polygon_set_type::clean_resources psdCleanResources;
					polygon_set_type geometry;
					for (box_iter_type iter = spIndexPtr->begin(bbox); iter; ++iter)
					{
						back.m_OrgRel.first = p1_rel;
						back.m_OrgRel.second = p2Offset + (((*iter)->get_ptr()) - poly2Array.begin());
						if (onlyForwardMatches && back.m_OrgRel.first >= back.m_OrgRel.second)
							continue;
						using namespace bp::operators;
						bp::assign(geometry, *((*iter)->get_ptr()) & *polyPtr, psdCleanResources); // intersection

						if (!geometry.size(psdCleanResources))
							continue;

						if constexpr (MustProduceGeometries)
							bp_assign(back.m_Geometry, geometry, psdCleanResources);

						leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
						resTileData->push_back(std::move(back));
					}
				}
				else if constexpr (GL == geometry_library::boost_geometry)
				{
					box_iter_type iter = spIndexPtr->begin(bbox); if (!iter) return;

					bg_ring_t helperRing;
					bg_polygon_t helperPolygon;
					bg_multi_polygon_t currMP1, currMP2, resMP;

					assign_multi_polygon(currMP1, *polyPtr, true, helperPolygon, helperRing);
					if (currMP1.empty())
						return;

					for (; iter; ++iter)
					{
						back.m_OrgRel.first = p1_rel;
						back.m_OrgRel.second = p2Offset + (((*iter)->get_ptr()) - poly2Array.begin());
						if (onlyForwardMatches && back.m_OrgRel.first >= back.m_OrgRel.second)
							continue;

						assign_multi_polygon(currMP2, *((*iter)->get_ptr()), true, helperPolygon, helperRing);

						resMP.clear();
						boost::geometry::intersection(currMP1, currMP2, resMP);
						if (!resMP.empty())
						{
							if constexpr (MustProduceGeometries)
								bg_store_multi_polygon(back.m_Geometry, resMP);

							leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
							resTileData->push_back(std::move(back));
						}
					};

				}
				else if constexpr (GL == geometry_library::cgal)
				{
					CGAL_Traits::Polygon_set poly1;
					assign_multi_polygon(poly1, *polyPtr, true);

					for (box_iter_type iter = spIndexPtr->begin(bbox); iter; ++iter)
					{
						back.m_OrgRel.first = p1_rel;
						back.m_OrgRel.second = p2Offset + (((*iter)->get_ptr()) - poly2Array.begin());
						if (onlyForwardMatches && back.m_OrgRel.first >= back.m_OrgRel.second)
							continue;

						CGAL_Traits::Polygon_set poly2;
						assign_multi_polygon(poly2, *((*iter)->get_ptr()), true);

						CGAL_Traits::Polygon_set res;
						res.intersection(poly1, poly2);

						if (res.is_empty())
							continue;

						if constexpr (MustProduceGeometries)
							cgal_assign_polygon_set(back.m_Geometry, res);

						leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
						resTileData->push_back(std::move(back));
					}
				}
				else if constexpr (GL == geometry_library::geos)
				{
					auto mp1 = geos_create_polygons(*polyPtr);

					for (box_iter_type iter = spIndexPtr->begin(bbox); iter; ++iter)
					{
						back.m_OrgRel.first = p1_rel;
						back.m_OrgRel.second = p2Offset + (((*iter)->get_ptr()) - poly2Array.begin());
						if (onlyForwardMatches && back.m_OrgRel.first >= back.m_OrgRel.second)
							continue;

						CGAL_Traits::Polygon_set poly2;
						auto mp2 = geos_create_polygons(*((*iter)->get_ptr()));

						auto res = mp1->intersection(mp2.get());

						if (!res || res->isEmpty())
							continue;

						if constexpr (MustProduceGeometries)
							geos_assign_geometry(back.m_Geometry, res.get());

						leveled_critical_section::scoped_lock resLock(resLocalAdditionSection);
						resTileData->push_back(std::move(back));
					}
				}
			}
			catch (...)
			{
				auto errMsg = catchException(true);
				errMsg->TellExtraF("while processing intersection of row %d with row %d", back.m_OrgRel.first, back.m_OrgRel.second);
				throw DmsException(errMsg);
			}
		});
	}

	void StoreRes(AbstrUnit* res, AbstrDataItem* resG, AbstrDataItem* res1, AbstrDataItem* res2, ResourceHandle& resDataHandle) const override
	{
		ResDataType* resData = GetOptional<ResDataType>(resDataHandle);

		SizeT count = 0;
		if (resData)
			for (auto& resTiles : *resData) // ordered by (t, u)
				count += resTiles.second.size();
		res->SetCount(ThrowingConvert<ResultingDomainType>(count));

		locked_tile_write_channel<PolygonType> resGWriter(resG);
		locked_tile_write_channel<UInt32> res1Writer(res1);
		locked_tile_write_channel<UInt32> res2Writer(res2);

		if (resData)
		{
			for (auto& resTiles : *resData) // ordered by (t, u)
			{
				ResTileDataType& resTileData = resTiles.second;
				std::sort(resTileData.begin(), resTileData.end()); // sort on (m_OrgRel)

				for (auto& resElemData: resTileData)
				{
					if constexpr (MustProduceGeometries)
						if (resG) resGWriter.Write(std::move(resElemData.m_Geometry));
					if (res1) res1Writer.Write(resElemData.m_OrgRel.first);
					if (res2) res2Writer.Write(resElemData.m_OrgRel.second);
				}
			}
		}

		if constexpr (MustProduceGeometries)
			if (resG) { MG_CHECK(resGWriter.IsEndOfChannel()); resGWriter.Commit(); }
		if (res1) { MG_CHECK(res1Writer.IsEndOfChannel()); res1Writer.Commit(); }
		if (res2) { MG_CHECK(res2Writer.IsEndOfChannel()); res2Writer.Commit(); }
	}
};

// *****************************************************************************
//	UnionPolygon func
// *****************************************************************************

#include "IndexGetterCreator.h"

enum class PolygonFlags {
	none = 0,
	F_DoSplit = 1,
	F_DoUnion = 2,
	F_HasPartition = 4,
	F_DoPartUnion = 6,

	F_Inflate1 = 0x0010,
	F_Deflate1 = 0x0020,
	F_Filter1 = 0x0030,
	F_I4HV1 = 0x0040,
	F_I4D1 = 0x0050,
	F_I8D1 = 0x0060,
	F_I16D1 = 0x0070,
	F_IXHV1 = 0x0080,
	F_IXD1 = 0x0090,
	F_D4HV1 = 0x00A0,
	F_D4D1 = 0x00B0,
	F_D8D1 = 0x00C0,
	F_D16D1 = 0x00D0,
	F_DXHV1 = 0x00E0,
	F_DXD1 = 0x00F0,
	F_Mask1 = 0x00F0,

	F_Inflate2 = 0x0100,
	F_Deflate2 = 0x0200,
	F_Filter2 = 0x0300,
	F_I4HV2 = 0x0400,
	F_I4D2 = 0x0500,
	F_I8D2 = 0x0600,
	F_I16D2 = 0x0700,
	F_IXHV2 = 0x0800,
	F_IXD2 = 0x0900,
	F_D4HV2 = 0x0A00,
	F_D4D2 = 0x0B00,
	F_D8D2 = 0x0C00,
	F_D16D2 = 0x0D00,
	F_DXHV2 = 0x0E00,
	F_DXD2 = 0x0F00,
	F_Mask2 = 0x0F00,
};

constexpr PolygonFlags operator | (PolygonFlags a, PolygonFlags b)
{
	return static_cast<PolygonFlags>(static_cast<int>(a) | static_cast<int>(b));
}

bool operator & (PolygonFlags a, PolygonFlags b)
{
	return static_cast<int>(a) & static_cast<int>(b);
}

template <typename C>
boost::polygon::rectangle_data<C> get_enclosing_rectangle(const boost::polygon::rectangle_data<C>& a, const boost::polygon::rectangle_data<C>& b) {
	using namespace boost::polygon;
	C min_x = std::min(a.get(HORIZONTAL).get(LOW ), b.get(HORIZONTAL).get(LOW ));
	C min_y = std::min(a.get(VERTICAL  ).get(LOW ), b.get(VERTICAL  ).get(LOW ));
	C max_x = std::max(a.get(HORIZONTAL).get(HIGH), b.get(HORIZONTAL).get(HIGH));
	C max_y = std::max(a.get(VERTICAL  ).get(HIGH), b.get(VERTICAL  ).get(HIGH));

	return { boost::polygon::interval_data<C>(min_x, max_x), boost::polygon::interval_data<C>(min_y, max_y) };
}

template <typename C, typename GT2>
void dms_insert(bp::polygon_set_data<C>& lvalue, const GT2& rvalue)
{
	using traits_t = bp_union_poly_traits<C>;
	if (rvalue.size() == 0)
		return;


	auto rRange = Range<Point<C>>(rvalue.begin(), rvalue.end(), false, false);
	auto bpRect = boost::polygon::rectangle_data<C>(
		boost::polygon::interval_data<C>(rRange.first.X(), rRange.second.X())
		, boost::polygon::interval_data<C>(rRange.first.Y(), rRange.second.Y())
	);

	typename traits_t::rect_type bpLeftRect;
	if (lvalue.extents(bpLeftRect))
		bpRect = get_enclosing_rectangle(bpRect, bpLeftRect);

	bool mustTranslate = coords_using_more_than_25_bits(bpRect);
	if (mustTranslate)
	{
		typename traits_t::point_type p;
		// translate to zero to avoid numerical round-off errors
		bp::center(p, bpRect);
		typename traits_t::point_type mp(-x(p), -y(p));
		bp::convolve(bpRect, mp);
		if (coords_using_more_than_25_bits(bpRect))
			throwErrorF("boost::polygon::insert", "extent of objects is %s after moving it with %s, which requires more than 25 bits for coordinates"
				, AsString(bpRect)
				, AsString(mp)
			);
		lvalue.move(mp);

		using point_vector = std::vector < Point<C> >;
		point_vector rvalueCopy(rvalue.begin(), rvalue.end());
		for (auto& rvcp : rvalueCopy)
			boost::polygon::convolve(rvcp, mp);

		using vector_traits = bp::polygon_set_traits<point_vector>;
		lvalue.insert(vector_traits::begin(rvalueCopy), vector_traits::end(rvalueCopy));

		lvalue.move(p);
		return;
	}

	using GT2_traits = bp::polygon_set_traits<GT2>;
	lvalue.insert(GT2_traits::begin(rvalue), GT2_traits::end(rvalue));
}

template <typename C, typename GT2>
void dms_assign(bp::polygon_set_data<C>& lvalue, const GT2& rvalue)
{
	lvalue.clear();
	dms_insert(lvalue, rvalue);
}

Timer s_ProcessTimer;


template <typename P, typename SequenceType, typename MPT>
void UnionPolygon(ResourceArrayHandle& r, SizeT n, const AbstrDataItem* polyDataA, const AbstrDataItem* permDataA, tile_id t, const AbstrOperGroup* whosCalling)
{
	auto polyData = const_array_cast<SequenceType>(polyDataA);
	assert(polyData);
	assert(whosCalling);

	OwningPtr<IndexGetter> vg;

	if (permDataA)
		vg = IndexGetterCreator::Create(permDataA, t);

	auto polyArray = polyData->GetTile(t);

	if (!r)
		r.reset( ResourceArray<MPT>::create(n) );
	auto geometryTowerResourcePtr = debug_cast<ResourceArray<MPT>*>(r.get_ptr());
	assert(geometryTowerResourcePtr->size() == n);

	PolygonFlags unionPermState =
		(permDataA) ? PolygonFlags::F_DoPartUnion :
		(n==1)      ? PolygonFlags::F_DoUnion     : PolygonFlags::none;
	
//	typename polygon_set_data_type::clean_resources cleanResources;

	// insert each multi-polygon in polyArray in geometryPtr[part_rel] 
	for (auto pb=polyArray.begin(), pi=pb, pe=polyArray.end(); pi!=pe; ++pi)
	{
		auto geometryTowerPtr = geometryTowerResourcePtr->begin();
		if (unionPermState != PolygonFlags::F_DoUnion)
		{
			SizeT i = pi-pb;
			if (unionPermState != PolygonFlags::none)
			{
				assert(unionPermState == PolygonFlags::F_DoPartUnion);
				SizeT ri = vg->Get(i);	
				if (ri >= n)
				{
					if (!IsDefined(ri))
						continue;
					throwErrorF("Polygon", "Unexpected partion index %d at row %d", ri, i);
				}
				i = ri;
			}
			assert( i < n);
			geometryTowerPtr += i;
		}
		typename MPT::semi_group geometry;
		dms_assign(geometry, *pi);
		geometryTowerPtr->add(std::move(geometry));

		if (s_ProcessTimer.PassedSecs())
		{
			reportF(SeverityTypeID::ST_MajorTrace, "%s: processed %s / %s sequences of tile %s / %s"
				, whosCalling->GetNameStr()
				, AsString(pi - pb), AsString(pe - pb)
				, AsString(t), AsString(polyDataA->GetAbstrDomainUnit()->GetNrTiles())
			);
		}
	}
}

namespace std {
    inline UInt32 abs(UInt32 _X)
    {
        return _X;
    }

}

struct AbstrPolygonOperator : public VariadicOperator
{
/* Variants, each line represents 2*3 operator-groups:

	[split_][[partitioned_]union_]polygon                  (geometry[, partitioning]);
	[split_][[partitioned_]union_]polygon_filtered         (geometry[, partitioning], minArea);
	[split_][[partitioned_]union_]polygon_inflated         (geometry[, partitioning], increment);
	[split_][[partitioned_]union_]polygon_deflated         (geometry[, partitioning], decrement);
	[split_][[partitioned_]union_]polygon_filtered_inflated(geometry[, partitioning], minArea,   increment);
	[split_][[partitioned_]union_]polygon_deflated_filtered(geometry[, partitioning], decrement, minArea  );
	[split_][[partitioned_]union_]polygon_deflated_inflated(geometry[, partitioning], decrement, increment);
	[split_][[partitioned_]union_]polygon_inflated_deflated(geometry[, partitioning], increment, decrement);

*/
	static arg_index CalcNrArgs(PolygonFlags f)
	{
		dms_assert((f & PolygonFlags::F_DoUnion) || !(f & PolygonFlags::F_HasPartition));

		dms_assert((f & PolygonFlags::F_Mask1)  || !(f & PolygonFlags::F_Mask2));

		arg_index nrArgs = 1;

		if (f & PolygonFlags::F_HasPartition ) ++nrArgs;

		if (f & PolygonFlags::F_Mask1) ++nrArgs;
		if (f & PolygonFlags::F_Mask2) ++nrArgs;

		return nrArgs;
	}
	static const Class* ResultCls(const DataItemClass* polyAttrClass, PolygonFlags f)
	{
		if (f & PolygonFlags::F_DoSplit)
			return Unit<UInt32>::GetStaticClass();
		return polyAttrClass;
	}
protected:
	PolygonFlags m_Flags;

	AbstrPolygonOperator(AbstrOperGroup* og, const DataItemClass* polyAttrClass, const DataItemClass* numericAttrClass, PolygonFlags f)
		:	VariadicOperator(og, ResultCls(polyAttrClass, f), CalcNrArgs(f) )
		,	m_Flags(f)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = polyAttrClass;
		if (f & PolygonFlags::F_HasPartition ) *argClsIter++ = AbstrDataItem::GetStaticClass(); // partition attribute

		if (f & PolygonFlags::F_Mask1) *argClsIter++ = numericAttrClass;
		if (f & PolygonFlags::F_Mask2) *argClsIter++ = numericAttrClass;

		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
#if defined(MG_DEBUG_POLYGON)
		DBG_START("AbstrPolygon", "CreateResult", true);
		for (arg_index i = 0; i != args.size(); ++i)
		{
			DBG_TRACE(("arg %d: %s", i, args[i]->GetName().c_str()));
		}
#endif
		MG_CHECK(args.size() == CalcNrArgs(m_Flags));

		arg_index argCount = 0;
		const AbstrDataItem* argPoly = AsDataItem(args[argCount++]);
		const AbstrDataItem* argPart = (m_Flags & PolygonFlags::F_HasPartition) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argNum1 = (m_Flags & PolygonFlags::F_Mask1       ) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argNum2 = (m_Flags & PolygonFlags::F_Mask2       ) ? AsDataItem(args[argCount++]) : nullptr;

		assert(args.size() == argCount);

		assert(argPoly); 

		const AbstrUnit* domain1Unit = argPoly->GetAbstrDomainUnit();
		const AbstrUnit* values1Unit = argPoly->GetAbstrValuesUnit();

		const AbstrUnit* resDomain = (argPart) ? argPart->GetAbstrValuesUnit() : (m_Flags & PolygonFlags::F_DoUnion) ? Unit<Void>::GetStaticClass()->CreateDefault() : domain1Unit;

		if (argPart)
			domain1Unit->UnifyDomain(argPart->GetAbstrDomainUnit(), "Domain of polygon attribute", "Domain of partitioning attribute", UM_Throw);
		if (argNum1)
			resDomain  ->UnifyDomain(argNum1->GetAbstrDomainUnit(), "Domain of result", "Domain of Num1", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if (argNum2)
			resDomain  ->UnifyDomain(argNum2->GetAbstrDomainUnit(), "Domain of result", "Domain of Num2", UnifyMode(UM_Throw|UM_AllowVoidRight));

		AbstrDataItem* resGeometry;
		AbstrDataItem* resNrOrgEntity = nullptr;
		AbstrUnit*     resUnit;

		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			resUnit = Unit<UInt32>::GetStaticClass()->CreateResultUnit(resultHolder);
			resUnit->SetTSF(TSF_Categorical);

			resultHolder = resUnit;
			resGeometry = CreateDataItem(resUnit, token::geometry, resUnit, values1Unit, ValueComposition::Polygon);
			if (!resDomain->IsKindOf(Unit<Void>::GetStaticClass()))
			{
				resNrOrgEntity = CreateDataItem(resUnit, argPart ? token::part_rel : token::polygon_rel, resUnit, resDomain, ValueComposition::Single);
				resNrOrgEntity->SetTSF(TSF_Categorical);
			}
		}
		else
		{
			resUnit = nullptr;
			if (!resultHolder)
				resultHolder = CreateCacheDataItem(resDomain, values1Unit, ValueComposition::Polygon);
			resGeometry = AsDataItem(resultHolder.GetNew());
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(argPoly);
			DataReadLock arg2Lock(argPart);
			DataReadLock arg3Lock(argNum1);
			DataReadLock arg4Lock(argNum2);

			if (DoDelayStore())
			{
				SizeT domainCount = resDomain->GetCount();
				ResourceArrayHandle r;
				for (tile_id t=0, te = domain1Unit->GetNrTiles(); t != te; ++t)
				{
					ReadableTileLock readArg1Lock (argPoly->GetCurrRefObj(), t);
					ReadableTileLock readArg2Lock (argPart ? argPart->GetCurrRefObj() : nullptr, t);

					Calculate(r, domainCount, argPoly, argPart, t);
				}
				DataWriteLock resGeometryHandle; // will be assigned after establishing the count of resUnit
				Store(resUnit, resGeometry, resGeometryHandle, resNrOrgEntity, no_tile, 1, r, argNum1, argNum2);
				resGeometryHandle.Commit();
			}
			else
			{
				DataWriteLock resGeometryHandle(resGeometry);
				auto tn = domain1Unit->GetNrTiles();
				parallel_tileloop(tn, [this, &resultHolder, resUnit, resDomain, &resGeometryHandle, resNrOrgEntity, argPoly, argPart, argNum1, argNum2, tn](tile_id t) 
				{
					if (resultHolder.WasFailed(FR_Data))
						resultHolder.ThrowFail();
					ResourceArrayHandle r;
					ReadableTileLock readArg1Lock (argPoly->GetCurrRefObj(), t);
					ReadableTileLock readArg2Lock (argPart ? argPart->GetCurrRefObj() : nullptr, t);

					Calculate(r, resDomain->GetTileCount(t), argPoly, argPart, t);
					Store(resUnit, nullptr, resGeometryHandle, resNrOrgEntity, t, tn, r, argNum1, argNum2);
				});
				resGeometryHandle.Commit();
			}
		}
		return true;
	}
	bool DoDelayStore() const 
	{
		return m_Flags & (PolygonFlags::F_DoSplit | PolygonFlags::F_DoUnion);
	}
	void ProcessNumOper( ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id t, tile_id tn, PolygonFlags f) const
	{
		if (f == PolygonFlags::none)
			return;

		assert(r);
		assert(argNum);

		ReadableTileLock readNum1Lock (argNum ? argNum->GetCurrRefObj() : nullptr, !argNum || argNum->HasVoidDomainGuarantee() ? 0 : t);

		switch (f) {
			case PolygonFlags::F_Inflate1:
				reportF(SeverityTypeID::ST_MajorTrace, "%s.Inflate tile %d", GetGroup()->GetNameStr(), t);
				break;
			case PolygonFlags::F_Deflate1:
				reportF(SeverityTypeID::ST_MajorTrace, "%s.Deflate tile %d", GetGroup()->GetNameStr(), t);
				break;
			case PolygonFlags::F_Filter1:
				reportF(SeverityTypeID::ST_MajorTrace, "%s.Filter tile %d", GetGroup()->GetNameStr(), t);
				break;
			default:
				reportF(SeverityTypeID::ST_MajorTrace, "%s.Operation %d tile %d", GetGroup()->GetNameStr(), int(f) / int(PolygonFlags::F_Inflate1), t); // DEBUG
				break;
		}

		ProcessNumOperImpl(r, argNum, t, tn, f);
	}
	void Store (AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryHandle, AbstrDataItem* resNrOrgEntity, tile_id t, tile_id tn, ResourceArrayHandle& r, const AbstrDataItem* argNum1, const AbstrDataItem* argNum2) const
	{
		if (r)
		{
			constexpr int mask_ratio = int(PolygonFlags::F_Mask2) / int(PolygonFlags::F_Mask1);
			ProcessNumOper(r, argNum1, t, tn, PolygonFlags( int(m_Flags) & int(PolygonFlags::F_Mask1)));
			ProcessNumOper(r, argNum2, t, tn, PolygonFlags((int(m_Flags) & int(PolygonFlags::F_Mask2)) / mask_ratio));
		}
#if defined(MG_DEBUG_POLYGON)
		reportF(ST_MajorTrace, "UnionPolygon.Store %d", t);
#endif //defined(MG_DEBUG_POLYGON)
		StoreImpl(resUnit, resGeometry, resGeometryHandle, resNrOrgEntity, t, r);
	}
	virtual void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const=0;
	virtual void StoreImpl(AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const=0;
	virtual void ProcessNumOperImpl(ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id numT, tile_id tn, PolygonFlags f) const {}
};

template <typename C>
void SetKernel(typename bp_union_poly_traits<C>::ring_type& kernel, Float64 value, PolygonFlags f)
{
	using traits_t = bp_union_poly_traits<C>;

	if (f <= PolygonFlags::F_Filter1)
		return;

	kernel.clear();
	const Float64 c0 = 0;
	const Float64 csqrtHalf = std::numbers::sqrt2 / 2.0;

	switch (f) {
	case PolygonFlags::F_I4HV1:
	case PolygonFlags::F_D4HV1:
		kernel.reserve(5);
		kernel.push_back(traits_t::point_type(value, value));
		kernel.push_back(traits_t::point_type(value, -value));
		kernel.push_back(traits_t::point_type(-value, -value));
		kernel.push_back(traits_t::point_type(-value, value));
		kernel.push_back(kernel.front());
		break;
	case PolygonFlags::F_I4D1:
	case PolygonFlags::F_D4D1:
		kernel.reserve(5);
		kernel.push_back(traits_t::point_type(c0, value));
		kernel.push_back(traits_t::point_type(value, c0));
		kernel.push_back(traits_t::point_type(c0, -value));
		kernel.push_back(traits_t::point_type(-value, c0));
		kernel.push_back(kernel.front());
		break;
	case PolygonFlags::F_I8D1:
	case PolygonFlags::F_D8D1:
		kernel.reserve(9);
		kernel.push_back(traits_t::point_type(c0, value));
		kernel.push_back(traits_t::point_type(csqrtHalf * value, csqrtHalf * value));
		kernel.push_back(traits_t::point_type(value, c0));
		kernel.push_back(traits_t::point_type(csqrtHalf * value, -csqrtHalf * value));
		kernel.push_back(traits_t::point_type(c0, -value));
		kernel.push_back(traits_t::point_type(-csqrtHalf * value, -csqrtHalf * value));
		kernel.push_back(traits_t::point_type(-value, c0));
		kernel.push_back(traits_t::point_type(-csqrtHalf * value, csqrtHalf * value));
		kernel.push_back(kernel.front());
		break;

	case PolygonFlags::F_I16D1:
	case PolygonFlags::F_D16D1:
		kernel.reserve(17);
		kernel.push_back(traits_t::point_type(c0, value));
		kernel.push_back(traits_t::point_type(0.3826834 * value, 0.9238795 * value));
		kernel.push_back(traits_t::point_type(csqrtHalf * value, csqrtHalf * value));
		kernel.push_back(traits_t::point_type(0.9238795 * value, 0.3826834 * value));
		kernel.push_back(traits_t::point_type(value, c0));
		kernel.push_back(traits_t::point_type(0.9238795 * value, -0.3826834 * value));
		kernel.push_back(traits_t::point_type(csqrtHalf * value, -csqrtHalf * value));
		kernel.push_back(traits_t::point_type(0.3826834 * value, -0.9238795 * value));
		kernel.push_back(traits_t::point_type(c0, -value));
		kernel.push_back(traits_t::point_type(-0.3826834 * value, -0.9238795 * value));
		kernel.push_back(traits_t::point_type(-csqrtHalf * value, -csqrtHalf * value));
		kernel.push_back(traits_t::point_type(-0.9238795 * value, -0.3826834 * value));
		kernel.push_back(traits_t::point_type(-value, c0));
		kernel.push_back(traits_t::point_type(-0.9238795 * value, 0.3826834 * value));
		kernel.push_back(traits_t::point_type(-csqrtHalf * value, csqrtHalf * value));
		kernel.push_back(traits_t::point_type(-0.3826834 * value, 0.9238795 * value));
		kernel.push_back(kernel.front());
		break;

	case PolygonFlags::F_IXHV1:
	case PolygonFlags::F_DXHV1:
		kernel.reserve(9);
		kernel.push_back(traits_t::point_type(0.1 * value, 0.1 * value));
		kernel.push_back(traits_t::point_type(+value, c0));
		kernel.push_back(traits_t::point_type(0.1 * value, -0.1 * value));
		kernel.push_back(traits_t::point_type(c0, -value));
		kernel.push_back(traits_t::point_type(-0.1 * value, -0.1 * value));
		kernel.push_back(traits_t::point_type(-value, c0));
		kernel.push_back(traits_t::point_type(-0.1 * value, 0.1 * value));
		kernel.push_back(traits_t::point_type(c0, +value));
		kernel.push_back(kernel.front());
		break;

	case PolygonFlags::F_IXD1:
	case PolygonFlags::F_DXD1:
		kernel.reserve(9);
		kernel.push_back(traits_t::point_type(value, value));
		kernel.push_back(traits_t::point_type(0.1 * value, c0));
		kernel.push_back(traits_t::point_type(value, -value));
		kernel.push_back(traits_t::point_type(c0, -0.1 * value));
		kernel.push_back(traits_t::point_type(-value, -value));
		kernel.push_back(traits_t::point_type(-0.1 * value, c0));
		kernel.push_back(traits_t::point_type(-value, value));
		kernel.push_back(traits_t::point_type(c0, 0.1 * value));
		kernel.push_back(kernel.front());
		break;
	}
}

template<typename C>
struct union_bp_polygonsets
{
	using traits_t = bp_union_poly_traits<C>;
	using polygon_set_data = typename traits_t::polygon_set_data_type;
	void operator()(polygon_set_data& lvalue, polygon_set_data&& rvalue) const
	{
		typename traits_t::rect_type bpRightRect;
		if (!rvalue.extents(bpRightRect))
			return; // nothing to merge

		typename traits_t::rect_type bpRect;
		if (lvalue.extents(bpRect))
		{
			bpRect = get_enclosing_rectangle(bpRect, bpRightRect);

			bool mustTranslate = coords_using_more_than_25_bits(bpRect);
			if (mustTranslate)
			{
				typename traits_t::point_type p;
				// translate to zero to avoid numerical round-off errors
				bp::center(p, bpRect);
				typename traits_t::point_type mp(-x(p), -y(p));
				bp::convolve(bpRect, mp);
				if (coords_using_more_than_25_bits(bpRect))
					throwErrorF("union_bp_polygonsets", "extent of objects is %s after moving it with %s, which requires more than 25 bits for coordinates"
						, AsString(bpRect)
						, AsString(mp)
					);
				lvalue.move(mp);
				rvalue.move(mp);

				lvalue.insert(std::move(rvalue));

				lvalue.move(p);
				return;
			}


		}
		lvalue.insert(std::move(rvalue));
	}
};

template <typename P>
class BpPolygonOperator : public AbstrPolygonOperator
{
	using SequenceType = typename sequence_traits<P>::container_type;
	using traits_t = bp_union_poly_traits<scalar_of_t<P>>;

	using ScalarType = typename traits_t::coordinate_type;
	using PointType = typename traits_t::point_type;
	using RingType = typename traits_t::ring_type;
	using MultiPolygonType = typename traits_t::multi_polygon_type;
	using PolygonSetDataType = typename traits_t::polygon_set_data_type;
	using PolygonSetTower = assoc_tower<PolygonSetDataType, union_bp_polygonsets<ScalarType> >;
	using NumType = Float64;

	typedef DataArray<SequenceType> ArgPolyType;
	typedef DataArray<NumType>  ArgNumType;

public:
	BpPolygonOperator(AbstrOperGroup* aog, PolygonFlags flags)
		:	AbstrPolygonOperator(aog, ArgPolyType::GetStaticClass(), ArgNumType::GetStaticClass(), flags)
	{}

	void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const override
	{
		UnionPolygon<P, SequenceType, PolygonSetTower>(r, domainCount, polyDataA, partitionDataA, t, GetGroup());
	}

	void ProcessNumOperImpl(ResourceArrayHandle& r, const AbstrDataItem* argNum, tile_id t, tile_id tn, PolygonFlags flag) const override
	{
		assert(r);
		assert(argNum);
		assert(flag != PolygonFlags::none);

		auto geometryDataTowerResourcePtr = debug_cast<ResourceArray<PolygonSetTower>*>(r.get_ptr());
		auto geometryDataTowerPtr = geometryDataTowerResourcePtr->begin();

		auto argNumData = const_array_cast<NumType>(argNum)->GetTile(argNum->HasVoidDomainGuarantee() ? 0 : t);

		bool isParam = argNumData.size() == 1;
		SizeT domainCount = r->size();
		assert(isParam || argNumData.size() == domainCount);

		typename traits_t::ring_type               kernel;
		typename traits_t::multi_polygon_type      geometry;
		typename traits_t::polygon_with_holes_type polygon;

		NumType value;
		if (isParam)
		{
			value = argNumData[0];
			SetKernel<scalar_of_t<P>>(kernel, value, flag);
		}

		typename traits_t::polygon_set_data_type::convolve_resources convolveResources;
		typename traits_t::polygon_set_data_type::clean_resources& cleanResources = convolveResources.cleanResources;

		for (SizeT i = 0; i!=domainCount; ++i, ++geometryDataTowerPtr)
		{
			typename traits_t::polygon_set_data_type geometryData = geometryDataTowerPtr->get_result();

			if (empty(geometryData, cleanResources))
				continue;

			if (!isParam)
			{
				value = argNumData[i];
				SetKernel<scalar_of_t<P>>(kernel, value, flag);
			}

			typename traits_t::rect_type rectangle;
			typename traits_t::point_type p;
			geometryData.extents(rectangle); // , cleanResources);

			bool mustTranslate = coords_using_more_than_25_bits(rectangle);
			if (mustTranslate)
			{
				/* translate to zero to avoid numerical round-off errors */
				bp::center(p, rectangle);
				typename traits_t::point_type mp(-x(p), -y(p));
				bp::convolve(rectangle, mp);
				if (coords_using_more_than_25_bits(rectangle))
					throwErrorF("boost::polygon::ProcessSuffix", "extent of object is %s after moving it with %s, which requires more than 25 bits for coordinates"
						, AsString(rectangle)
						, AsString(mp)
					);

				geometryData.move(mp);
			}
			geometryData.clean(cleanResources);

			switch (flag) {
				case PolygonFlags::F_Inflate1:
					resize(geometryData, value, true, 8, convolveResources); //geometryData += value;
					break;
				case PolygonFlags::F_Deflate1:
					resize(geometryData, -value, true, 8, convolveResources); //geometryData -= value;
					break;
				case PolygonFlags::F_Filter1:
					bp::keep(geometryData, 
						value, MAX_VALUE(traits_t::area_type), 
						0, MAX_VALUE(traits_t::unsigned_area_type), 
						0, MAX_VALUE(traits_t::unsigned_area_type)
					, cleanResources
					);
					break;
				default:
					dms_assert(flag > PolygonFlags::F_Filter1 && flag <= PolygonFlags::F_DXD1);
					geometry.clear();
					geometryData.get(geometry, cleanResources);

					bp::detail::minkowski_offset<ScalarType>::convolve_kernel_with_polygon_set(geometryData, geometry, kernel.begin(), kernel.end(), (flag >= PolygonFlags::F_D4HV1));
			}
			/* translate result back to where it came from */
			geometryData.clean(cleanResources); // remove internal edges
			if (mustTranslate)
				geometryData.move(p);

			if (s_ProcessTimer.PassedSecs())
			{
				reportF(SeverityTypeID::ST_MajorTrace, "%s: processed %s / %s sequences of tile %s / %s"
					, GetGroup()->GetName()
					, AsString(i), AsString(domainCount)
					, AsString(t), AsString(tn)
				);
			}
			geometryDataTowerPtr->add(std::move(geometryData));
		}
	}

	void StoreImpl (AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const override
	{
		SizeT domainCount = 0;
		OwningPtrSizedArray<typename traits_t::multi_polygon_type> geometryPtr;
		if (r)
		{
			auto geometryDataTowerResourcePtr = debug_cast<ResourceArray<PolygonSetTower>*>(r.get_ptr());
			domainCount = geometryDataTowerResourcePtr->size();
			auto geometryDataTowerPtr = geometryDataTowerResourcePtr->begin();

			assert(geometryDataTowerPtr);

			geometryPtr = OwningPtrSizedArray<typename traits_t::multi_polygon_type>(domainCount, value_construct MG_DEBUG_ALLOCATOR_SRC("BoostPolygon: geometryPtr"));

			typename traits_t::polygon_set_data_type::clean_resources cleanResources;
			for (SizeT i = 0; i != domainCount; ++i, ++geometryDataTowerPtr) // TODO G8: parallel_for and cleanResources in a threadLocal thing
			{
				typename traits_t::polygon_set_data_type geometryData = geometryDataTowerPtr->get_result();
				geometryData.get(geometryPtr[i], cleanResources);
				geometryData = typename traits_t::polygon_set_data_type(); // free no longer required resources

			}
			r.reset();
		}
		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			dms_assert(resUnit);
			SizeT splitCount = 0;
			for (SizeT i = 0; i != domainCount; ++i)
				splitCount += geometryPtr[i].size();
			resUnit->SetCount(splitCount); // we must be in delayed store now

			if (resNrOrgEntity)
			{
				DataWriteLock resRelLock(resNrOrgEntity);
				SizeT splitCount2 = 0;
				for (SizeT i = 0; i != domainCount; ++i)
				{
					SizeT nrSplits = geometryPtr[i].size();
					SizeT nextCount = splitCount2 + nrSplits;
					while (splitCount2 != nextCount)
						resRelLock->SetValueAsSizeT(splitCount2++, i);
				}
				resRelLock.Commit();
			}
		}
		if (DoDelayStore())
		{
			assert(resGeometry);
			resGeometryLock = DataWriteHandle(resGeometry, dms_rw_mode::write_only_all);
		}
		assert(resGeometryLock);

		auto resArray = mutable_array_cast<SequenceType>(resGeometryLock)->GetLockedDataWrite(t, dms_rw_mode::write_only_all); // t may be no_tile
		auto resIter = resArray.begin();

		for (SizeT i = 0; i!=domainCount; ++i)
		{
			if (m_Flags & PolygonFlags::F_DoSplit)
			{
				resIter = bp_split_assign(resIter, geometryPtr[i]);
			}
			else
			{
				bp_assign_mp(*resIter, geometryPtr[i]);
				++resIter;
			}
			geometryPtr[i].clear();
		}
		assert(resIter == resArray.end() || domainCount == 0);
	}
};

// *****************************************************************************
//	Boost Geometry PolygonOverlay
// *****************************************************************************

struct union_bg_multi_polygon
{
	void operator()(bg_multi_polygon_t& lvalue, bg_multi_polygon_t&& rvalue) const
	{
		bg_multi_polygon_t result;
		boost::geometry::union_(lvalue, rvalue, result);
		result.swap(lvalue);
	}
};

template <typename P>
class BgPolygonOperator : public AbstrPolygonOperator
{
	using SequenceType = std::vector<P>;

	using traits_t = bg_union_poly_traits<P>;

	using ScalarType  = traits_t::coordinate_type;
	using PointType   = traits_t::point_type;
	using RingType    = traits_t::ring_type;
	using PolygonType = traits_t::polygon_with_holes_type;
	using MultiPolygonType = traits_t::multi_polygon_type;
	using MultiPolygonTower = assoc_tower<MultiPolygonType, union_bg_multi_polygon>;
	using NumType     = Float64;

	typedef DataArray<SequenceType>   ArgPolyType;
	typedef DataArray<NumType>        ArgNumType;

public:
	BgPolygonOperator(AbstrOperGroup* aog, PolygonFlags flags)
		: AbstrPolygonOperator(aog, ArgPolyType::GetStaticClass(), ArgNumType::GetStaticClass(), flags)
	{}

	void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const override
	{
		UnionPolygon<P, SequenceType, MultiPolygonTower>(r, domainCount, polyDataA, partitionDataA, t, GetGroup());
	}

	void StoreImpl(AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const override
	{
		SizeT domainCount = 0;
		auto geometryTowerResourcePtr = debug_cast<ResourceArray<MultiPolygonTower>*>(r.get_ptr());
		MultiPolygonTower* geometryTowerPtr = nullptr;
		if (geometryTowerResourcePtr)
		{
			domainCount = geometryTowerResourcePtr->size();
			geometryTowerPtr = geometryTowerResourcePtr->begin();
		}

		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			assert(resUnit);
			SizeT splitCount = 0;
			auto geometryTowerIter = geometryTowerPtr;
			for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
			{
				if (!geometryTowerIter->empty())
				{
					splitCount += geometryTowerIter->front().size(); // geometryPtr 
				}
			}
			resUnit->SetCount(splitCount); // we must be in delayed store now
			if (resNrOrgEntity)
			{
				DataWriteLock resRelLock(resNrOrgEntity);
				SizeT splitCount2 = 0;
				geometryTowerIter = geometryTowerPtr;
				for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
				{
					if (!geometryTowerIter->empty())
					{
						SizeT nrSplits = geometryTowerIter->front().size();
						SizeT nextCount = splitCount2 + nrSplits;
						while (splitCount2 != nextCount)
							resRelLock->SetValueAsSizeT(splitCount2++, i);
					}
				}
				resRelLock.Commit();
			}
		}
		if (DoDelayStore())
		{
			assert(resGeometry);
			resGeometryLock = DataWriteHandle(resGeometry, dms_rw_mode::write_only_all);
		}
		assert(resGeometryLock);

		auto resArray = mutable_array_cast<SequenceType>(resGeometryLock)->GetLockedDataWrite(t, dms_rw_mode::write_only_all); // t may be no_tile
		auto resIter = resArray.begin();

		for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerPtr)
		{
			if (m_Flags & PolygonFlags::F_DoSplit)
			{
				resIter = bg_split_assign(resIter, geometryTowerPtr->get_result());
			}
			else
			{
				bg_assign(*resIter, geometryTowerPtr->get_result());
				++resIter;
			}
		}
		assert(resIter == resArray.end() || domainCount == 0);
	}
};

// *****************************************************************************
//	CGAL PolygonOverlay
// *****************************************************************************

struct union_cgal_multi_polygon
{
	void operator()(CGAL_Traits::Polygon_set& lvalue, const CGAL_Traits::Polygon_set& rvalue) const
	{
		lvalue.join(rvalue);
	}
};


template <typename P>
class CGAL_PolygonOperator : public AbstrPolygonOperator
{
	using SequenceType = std::vector<P>;

	using traits_t = CGAL_Traits;

//	using ScalarType = traits_t::coordinate_type;
	using PointType = traits_t::Point;
	using RingType = traits_t::Ring;
	using PolygonType = traits_t::Polygon_with_holes;
	using MultiPolygonType = traits_t::Polygon_set;
	using MultiPolygonTower = assoc_tower<MultiPolygonType, union_cgal_multi_polygon>;
	using NumType = Float64;

	using ArgPolyType = DataArray<SequenceType>;
	using ArgNumType = DataArray<NumType>;

public:
	CGAL_PolygonOperator(AbstrOperGroup* aog, PolygonFlags flags)
		: AbstrPolygonOperator(aog, ArgPolyType::GetStaticClass(), ArgNumType::GetStaticClass(), flags)
	{}

	void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const override
	{
		UnionPolygon<P, SequenceType, MultiPolygonTower>(r, domainCount, polyDataA, partitionDataA, t, GetGroup());
	}

	void StoreImpl(AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const override
	{
		SizeT domainCount = 0;
		auto geometryTowerResourcePtr = debug_cast<ResourceArray<MultiPolygonTower>*>(r.get_ptr());
		MultiPolygonTower* geometryTowerPtr = nullptr;
		if (geometryTowerResourcePtr)
		{
			domainCount = geometryTowerResourcePtr->size();
			geometryTowerPtr = geometryTowerResourcePtr->begin();
		}

		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			assert(resUnit);
			SizeT splitCount = 0;
			auto geometryTowerIter = geometryTowerPtr;
			for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
			{
				if (!geometryTowerIter->empty())
				{
					std::vector<traits_t::Polygon_with_holes> geometryList;
					geometryTowerIter->front().polygons_with_holes(std::back_inserter(geometryList));
					splitCount += geometryList.size();
				}
			}
			resUnit->SetCount(splitCount); // we must be in delayed store now
			if (resNrOrgEntity)
			{
				DataWriteLock resRelLock(resNrOrgEntity);
				SizeT splitCount2 = 0;
				geometryTowerIter = geometryTowerPtr;
				for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
				{
					if (!geometryTowerIter->empty())
					{
						std::vector<traits_t::Polygon_with_holes> geometryList;
						geometryTowerIter->front().polygons_with_holes(std::back_inserter(geometryList));
						SizeT nrSplits = geometryList.size();
						SizeT nextCount = splitCount2 + nrSplits;
						while (splitCount2 != nextCount)
							resRelLock->SetValueAsSizeT(splitCount2++, i);
					}
				}
				resRelLock.Commit();
			}
		}
		if (DoDelayStore())
		{
			assert(resGeometry);
			resGeometryLock = DataWriteHandle(resGeometry, dms_rw_mode::write_only_all);
		}
		assert(resGeometryLock);

		auto resArray = mutable_array_cast<SequenceType>(resGeometryLock)->GetLockedDataWrite(t, dms_rw_mode::write_only_all); // t may be no_tile
		auto resIter = resArray.begin();

		for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerPtr)
		{
			if (m_Flags & PolygonFlags::F_DoSplit)
			{
				resIter = cgal_split_assign_polygon_set(resIter, geometryTowerPtr->get_result());
			}
			else
			{
				cgal_assign_polygon_set(*resIter, geometryTowerPtr->get_result());
				++resIter;
			}
		}
		assert(resIter == resArray.end() || domainCount == 0);
	}
};

// *****************************************************************************
//	GEOS PolygonOverlay
// *****************************************************************************

template <typename P>
class GEOS_PolygonOperator : public AbstrPolygonOperator
{
	using SequenceType = std::vector<P>;

	using traits_t = geos_union_poly_traits<P>;

	using ScalarType = traits_t::coordinate_type;
	using PointType = traits_t::point_type;
	using RingType = traits_t::ring_type;
	using PolygonType = traits_t::polygon_with_holes_type;
	using MultiPolygonType = traits_t::multi_polygon_type;
	using MultiPolygonTower = assoc_tower<std::unique_ptr<geos::geom::Geometry>, union_geos_multi_polygon>;
	using NumType = Float64;

	typedef DataArray<SequenceType>   ArgPolyType;
	typedef DataArray<NumType>        ArgNumType;

public:
	GEOS_PolygonOperator(AbstrOperGroup* aog, PolygonFlags flags)
		: AbstrPolygonOperator(aog, ArgPolyType::GetStaticClass(), ArgNumType::GetStaticClass(), flags)
	{}

	void Calculate(ResourceArrayHandle& r, SizeT domainCount, const AbstrDataItem* polyDataA, const AbstrDataItem* partitionDataA, tile_id t) const override
	{
		UnionPolygon<P, SequenceType, MultiPolygonTower>(r, domainCount, polyDataA, partitionDataA, t, GetGroup());
	}

	void StoreImpl(AbstrUnit* resUnit, AbstrDataItem* resGeometry, DataWriteHandle& resGeometryLock, AbstrDataItem* resNrOrgEntity, tile_id t, ResourceArrayHandle& r) const override
	{
		SizeT domainCount = 0;
		auto geometryTowerResourcePtr = debug_cast<ResourceArray<MultiPolygonTower>*>(r.get_ptr());
		MultiPolygonTower* geometryTowerPtr = nullptr;
		if (geometryTowerResourcePtr)
		{
			domainCount = geometryTowerResourcePtr->size();
			geometryTowerPtr = geometryTowerResourcePtr->begin();
		}

		if (m_Flags & PolygonFlags::F_DoSplit)
		{
			assert(resUnit);
			SizeT splitCount = 0;
			auto geometryTowerIter = geometryTowerPtr;
			for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
			{
				if (!geometryTowerIter->empty())
				{
					splitCount += geometryTowerIter->front()->getNumGeometries(); // geometryPtr 
				}
			}
			resUnit->SetCount(splitCount); // we must be in delayed store now
			if (resNrOrgEntity)
			{
				DataWriteLock resRelLock(resNrOrgEntity);
				SizeT splitCount2 = 0;
				geometryTowerIter = geometryTowerPtr;
				for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerIter)
				{
					if (!geometryTowerIter->empty())
					{
						SizeT nrSplits = geometryTowerIter->front()->getNumGeometries();
						SizeT nextCount = splitCount2 + nrSplits;
						while (splitCount2 != nextCount)
							resRelLock->SetValueAsSizeT(splitCount2++, i);
					}
				}
				resRelLock.Commit();
			}
		}
		if (DoDelayStore())
		{
			assert(resGeometry);
			resGeometryLock = DataWriteHandle(resGeometry, dms_rw_mode::write_only_all);
		}
		assert(resGeometryLock);

		auto resArray = mutable_array_cast<SequenceType>(resGeometryLock)->GetLockedDataWrite(t, dms_rw_mode::write_only_all); // t may be no_tile
		auto resIter = resArray.begin();

		for (SizeT i = 0; i != domainCount; ++i, ++geometryTowerPtr)
		{
			if (m_Flags & PolygonFlags::F_DoSplit)
			{
				resIter = geos_split_assign_geometry(resIter, geometryTowerPtr->get_result().get());
			}
			else
			{
				geos_assign_geometry(*resIter, geometryTowerPtr->get_result().get());
				++resIter;
			}
		}
		assert(resIter == resArray.end() || domainCount == 0);
	}
};

// *****************************************************************************
//	PolygonOverlay
// *****************************************************************************

#include "IndexAssigner.h"

CommonOperGroup cogPC("polygon_connectivity", oper_policy::better_not_in_meta_scripting);

static TokenID tF1 = GetTokenID_st("F1"), tF2 = GetTokenID_st("F2");

class AbstrPolygonConnectivityOperator : public UnaryOperator
{
protected:
	using ResultingDomainType = UInt32;

	AbstrPolygonConnectivityOperator(AbstrOperGroup* aog, const DataItemClass* polyAttrClass)
		:	UnaryOperator(aog, Unit<ResultingDomainType>::GetStaticClass(),	polyAttrClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit();

		AbstrUnit* res = Unit<ResultingDomainType>::GetStaticClass()->CreateResultUnit(resultHolder);
		res->SetTSF(TSF_Categorical);

		resultHolder = res;

		AbstrDataItem* resF1 = CreateDataItem(res, tF1, res, domain1Unit);
		AbstrDataItem* resF2 = CreateDataItem(res, tF2, res, domain1Unit);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);

			Calculate(res, resF1, resF2, arg1A);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem*arg1A) const=0;
};

template <typename P>
class PolygonConnectivityOperator : public AbstrPolygonConnectivityOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;

	using Arg1Type = DataArray<PolygonType>;
	using ScalarType = scalar_of_t<PointType>;

public:
	PolygonConnectivityOperator()
		:	AbstrPolygonConnectivityOperator(&cogPC, Arg1Type::GetStaticClass())
	{}
	void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A) const override
	{
		Timer processTimer;

		auto polyData = const_array_cast<PolygonType>(arg1A)->GetDataRead();
		SizeT nrVertices = polyData.size();
		SizeT nrEdges = 0;
		std::vector<std::set<int> > graph;
		std::vector<SizeT> domain_rel; domain_rel.reserve(nrVertices);

		if (nrVertices)
		{
			bp::connectivity_extraction<ScalarType> ce;
			typename bp::connectivity_extraction<ScalarType>::insert_resources ceInsertResources;

			bp::polygon_set_data<ScalarType> polygon;
			typename bp::polygon_set_data<ScalarType>::clean_resources psdCleanResources;
			for (SizeT i=0; i!=nrVertices; ++i)
			{
				if (polyData[i].size() > 2)
				{
					bp::assign(polygon, polyData[i], psdCleanResources);
					int result = ce.insert(polygon, ceInsertResources);
					assert(result == domain_rel.size());
					domain_rel.emplace_back(i);
				}
				if (processTimer.PassedSecs())
					reportF(SeverityTypeID::ST_MajorTrace, "%s: inserted %s / %s sequences"
						, GetGroup()->GetNameStr()
						, AsString(i), AsString(nrVertices)
					);
			}

			if (domain_rel.size())
			{
				typename bp::connectivity_extraction<ScalarType>::extract_resources ceExtractResources;
				graph.resize(domain_rel.size());
				ce.extract(graph, ceExtractResources);

				for (auto gi = graph.begin(), ge = graph.end(); gi != ge; ++gi)
					nrEdges += gi->size();

				assert(nrEdges % 2 == 0);
				nrEdges /= 2;
			}
		}
		res->SetCount(ThrowingConvert< ResultingDomainType>(nrEdges));
		DataWriteLock resF1Lock(resF1);
		DataWriteLock resF2Lock(resF2);

		IndexAssigner32 indexAssigner1(resF1, resF1Lock.get(), no_tile, 0, nrEdges);
		IndexAssigner32 indexAssigner2(resF2, resF2Lock.get(), no_tile, 0, nrEdges);

		SizeT i=0, e=0;
		for (auto gi=graph.begin(), ge=graph.end(); gi!=ge; ++i, ++gi)
		{
			for (auto vsi = gi->begin(), vse=gi->end(); vsi!=vse; ++vsi)
				if (i < *vsi)
				{
					assert(e < nrEdges);
					assert(*vsi < domain_rel.size());
					indexAssigner1.m_Indices[e] = domain_rel[i];
					indexAssigner2.m_Indices[e] = domain_rel[ * vsi ];
					++e;
				}
			if (processTimer.PassedSecs())
				reportF(SeverityTypeID::ST_MajorTrace, "%s: extracted %s / %s sequences"
					, GetGroup()->GetNameStr()
					, AsString(gi - graph.begin()), AsString(graph.size())
				);
		}
		indexAssigner1.Store();
		indexAssigner2.Store();

		MG_CHECK(e == nrEdges);
		resF2Lock.Commit();
		resF1Lock.Commit();
	}
};

CommonOperGroup cogBC("box_connectivity", oper_policy::better_not_in_meta_scripting);

class AbstrBoxConnectivityOperator : public BinaryOperator
{
protected:
	using ResultingDomainType = UInt32;

	AbstrBoxConnectivityOperator(const DataItemClass* polyAttrClass)
		: BinaryOperator(&cogBC, Unit<ResultingDomainType>::GetStaticClass()
			, polyAttrClass, polyAttrClass
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]); assert(arg1A);
		const AbstrDataItem* arg2A = AsDataItem(args[1]); assert(arg1A);

		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();
		if (!mustCalc)
			e->UnifyDomain(arg2A->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);


		AbstrUnit* res = Unit<ResultingDomainType>::GetStaticClass()->CreateResultUnit(resultHolder);
		res->SetTSF(TSF_Categorical);

		resultHolder = res;

		AbstrDataItem* resF1 = CreateDataItem(res, tF1, res, e);
		AbstrDataItem* resF2 = CreateDataItem(res, tF2, res, e);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			Calculate(res, resF1, resF2, arg1A, arg2A);
		}
		return true;
	}
	virtual void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const = 0;
};

template <typename P>
class BoxConnectivityOperator : public AbstrBoxConnectivityOperator
{
	using PointType = P;
	using ScalarType = scalar_of_t<P>;
	using RectType = Range<PointType>;
	using RectTypeCPtr = const RectType*;
	using Arg1Type = DataArray<PointType>;
	using SpatialIndexType = SpatialIndex<ScalarType, RectTypeCPtr>;

public:
	BoxConnectivityOperator()
		: AbstrBoxConnectivityOperator(Arg1Type::GetStaticClass())
	{}
	void Calculate(AbstrUnit* res, AbstrDataItem* resF1, AbstrDataItem* resF2, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const override
	{
		auto lowerBoundData = const_array_cast<PointType>(arg1A)->GetDataRead();
		auto upperBoundData = const_array_cast<PointType>(arg2A)->GetDataRead();

		std::vector<RectType> rects; rects.reserve(lowerBoundData.size());
		for (SizeT i = 0, n= lowerBoundData.size(); i !=n; ++i)
			rects.emplace_back(lowerBoundData[i], upperBoundData[i]);
		lowerBoundData = {};
		upperBoundData = {};

		auto rects_begin = begin_ptr(rects);
		auto rects_beyond = end_ptr(rects);
		auto spatialIndex = SpatialIndexType(rects_begin, rects_beyond);
		
		// counting
		SizeT nrEdges = 0;
		for (const auto& rect : rects)
		{
			SizeT index = &rect - begin_ptr(rects);
			for (auto iter = spatialIndex.begin(rect); iter; ++iter)
			{
				auto currRectPtr = (*iter)->get_ptr();
				if (IsTouching(*currRectPtr, rect))
				{
					SizeT index2 = currRectPtr - rects_begin;
					if (index < index2)
						++nrEdges;
				}
			}
		}
		res->SetCount(ThrowingConvert<ResultingDomainType>(nrEdges));
		DataWriteLock resF1Lock(resF1);
		DataWriteLock resF2Lock(resF2);

		IndexAssigner32 indexAssigner1(resF1, resF1Lock.get(), no_tile, 0, nrEdges);
		IndexAssigner32 indexAssigner2(resF2, resF2Lock.get(), no_tile, 0, nrEdges);
		
		SizeT e = 0;
		for (const auto& rect : rects)
		{
			SizeT index = &rect - begin_ptr(rects);
			for (auto iter = spatialIndex.begin(rect); iter; ++iter)
			{
				auto currRectPtr = (*iter)->get_ptr();
				if (IsTouching(*currRectPtr, rect))
				{
					SizeT index2 = currRectPtr - rects_begin;
					if (index < index2)
					{
						assert(e < nrEdges);
						indexAssigner1.m_Indices[e] = index;
						indexAssigner2.m_Indices[e] = index2;
						++e;
					}
				}
			}
		}
		indexAssigner1.Store();
		indexAssigner2.Store();

		MG_CHECK(e == nrEdges);
		resF2Lock.Commit();
		resF1Lock.Commit();
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	struct BpPolyOperatorGroup : CommonOperGroup
	{
		BpPolyOperatorGroup(CharPtr name, PolygonFlags flags)
			: CommonOperGroup(name)
			, m_Instances(this, flags)
		{
			SetBetterNotInMetaScripting();
		}

		tl_oper::inst_tuple_templ<typelists::sint_points, BpPolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct BpPartionedAlternatives
	{
		BpPartionedAlternatives(AbstrOperGroup* cog, PolygonFlags flags)
			: m_Instances(cog, flags)
		{}

		tl_oper::inst_tuple_templ<typelists::sint_points, BpPolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct BgPolyOperatorGroup : CommonOperGroup
	{
		BgPolyOperatorGroup(CharPtr name, PolygonFlags flags)
			: CommonOperGroup(name)
			, m_Instances(this, flags)
		{
			SetBetterNotInMetaScripting();
		}

		tl_oper::inst_tuple_templ<typelists::seq_points, BgPolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct BgPartitionedAlternatives
	{
		BgPartitionedAlternatives(AbstrOperGroup* cog, PolygonFlags flags)
			: m_Instances(cog, flags)
		{}

		tl_oper::inst_tuple_templ<typelists::seq_points, BgPolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct CGAL_PolyOperatorGroup : CommonOperGroup
	{
		CGAL_PolyOperatorGroup(CharPtr name, PolygonFlags flags)
			: CommonOperGroup(name)
			, m_Instances(this, flags)
		{
			SetBetterNotInMetaScripting();
		}

		tl_oper::inst_tuple_templ<typelists::seq_points, CGAL_PolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct CGAL_PartitionedAlternatives
	{
		CGAL_PartitionedAlternatives(AbstrOperGroup* cog, PolygonFlags flags)
			: m_Instances(cog, flags)
		{}

		tl_oper::inst_tuple_templ<typelists::seq_points, CGAL_PolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct GEOS_PolyOperatorGroup : CommonOperGroup
	{
		GEOS_PolyOperatorGroup(CharPtr name, PolygonFlags flags)
			: CommonOperGroup(name)
			, m_Instances(this, flags)
		{
			SetBetterNotInMetaScripting();
		}

		tl_oper::inst_tuple_templ<typelists::seq_points, GEOS_PolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct GEOS_PartitionedAlternatives
	{
		GEOS_PartitionedAlternatives(AbstrOperGroup* cog, PolygonFlags flags)
			: m_Instances(cog, flags)
		{}

		tl_oper::inst_tuple_templ<typelists::seq_points, GEOS_PolygonOperator, AbstrOperGroup*, PolygonFlags>
			m_Instances;
	};

	struct PolyOperatorGroups
	{
		BpPolyOperatorGroup simplePO, unionPO, partitionedPO;

		PolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(mySSPrintF(nameTempl.c_str(), "partitioned_union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};
	struct BpPolyOperatorGroups
	{
		BpPolyOperatorGroup simplePO, unionPO;
		BpPartionedAlternatives partitionedPO;

		BpPolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(&unionPO, PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};
	struct BgPolyOperatorGroups
	{
		BgPolyOperatorGroup simplePO, unionPO;
		BgPartitionedAlternatives partitionedPO;

		BgPolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(&unionPO, PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};
	struct CGAL_PolyOperatorGroups
	{
		CGAL_PolyOperatorGroup simplePO, unionPO;
		CGAL_PartitionedAlternatives partitionedPO;

		CGAL_PolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(&unionPO, PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};
	struct GEOS_PolyOperatorGroups
	{
		GEOS_PolyOperatorGroup simplePO, unionPO;
		GEOS_PartitionedAlternatives partitionedPO;

		GEOS_PolyOperatorGroups(WeakStr nameTempl, PolygonFlags flags)
			: simplePO(mySSPrintF(nameTempl.c_str(), "").c_str(), flags)
			, unionPO(mySSPrintF(nameTempl.c_str(), "union_").c_str(), PolygonFlags(flags | PolygonFlags::F_DoUnion))
			, partitionedPO(&unionPO, PolygonFlags(flags | PolygonFlags::F_DoPartUnion))
		{}
	};
	struct PolyOperatorGroupss
	{
		PolyOperatorGroups simple, split;
		PolyOperatorGroupss(CharPtr suffix, PolygonFlags flags)
			: simple(SharedStr("%spolygon") + suffix, flags)
			, split(SharedStr("split_%spolygon") + suffix, PolygonFlags(flags | PolygonFlags::F_DoSplit))
		{}
	};
	struct BpPolyOperatorGroupss
	{
		BpPolyOperatorGroups 
			simple = BpPolyOperatorGroups(SharedStr("bp_%spolygon"), PolygonFlags())
		,   split = BpPolyOperatorGroups(SharedStr("bp_split_%spolygon"), PolygonFlags::F_DoSplit)
		;
	};
	struct BgPolyOperatorGroupss
	{
		BgPolyOperatorGroups
			simple = BgPolyOperatorGroups(SharedStr("bg_%spolygon"), PolygonFlags())
		,	split = BgPolyOperatorGroups(SharedStr("bg_split_%spolygon"), PolygonFlags::F_DoSplit)
		;
	};
	struct CGAL_PolyOperatorGroupss
	{
		CGAL_PolyOperatorGroups
			simple = CGAL_PolyOperatorGroups(SharedStr("cgal_%spolygon"), PolygonFlags())
			, split = CGAL_PolyOperatorGroups(SharedStr("cgal_split_%spolygon"), PolygonFlags::F_DoSplit)
			;
	};
	struct GEOS_PolyOperatorGroupss
	{
		GEOS_PolyOperatorGroups
			simple = GEOS_PolyOperatorGroups(SharedStr("geos_%spolygon"), PolygonFlags())
			, split = GEOS_PolyOperatorGroups(SharedStr("geos_split_%spolygon"), PolygonFlags::F_DoSplit)
			;
	};


	struct PolyOperatorGroupsss
	{
		PolyOperatorGroupss f1, f2, f3, f4, f5, f6, f7, f8, f9, fa, fb, fc, fd, fe, ff;

		PolyOperatorGroupsss(WeakStr s2, PolygonFlags f)
			: f1(("_filtered" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Filter1))
			, f2(("_inflated" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Inflate1))
			, f3(("_deflated" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_Deflate1))
			, f4(("_i4HV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I4HV1))
			, f5(("_i4D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I4D1))
			, f6(("_i8D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I8D1))
			, f7(("_i16D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_I16D1))
			, f8(("_iXHV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_IXHV1))
			, f9(("_iXD" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_IXD1))
			, fa(("_d4HV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D4HV1))
			, fb(("_d4D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D4D1))
			, fc(("_d8D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D8D1))
			, fd(("_d16D" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_D16D1))
			, fe(("_dXHV" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_DXHV1))
			, ff(("_dXD" + s2).c_str(), PolygonFlags(f | PolygonFlags::F_DXD1))
		{}
	};

	static CommonOperGroup grOverlayPolygon("overlay_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grBgOverlayPolygon("bg_overlay_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grBpOverlayPolygon("bp_overlay_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grCGALOverlayPolygon("cgal_overlay_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grGEOSOverlayPolygon("geos_overlay_polygon", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grBgPolygonConnectivity  ("bg_polygon_connectivity", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grBpPolygonConnectivity  ("bp_polygon_connectivity", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grCGALPolygonConnectivity("cgal_polygon_connectivity", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
	static CommonOperGroup grGEOSPolygonConnectivity("geos_polygon_connectivity", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);


	template <typename P> using BoostPolygonOverlayOperator  = PolygonOverlayOperator<P, geometry_library::boost_polygon, true>;
	template <typename P> using BoostGeometryOverlayOperator = PolygonOverlayOperator<P, geometry_library::boost_geometry, true>;
	template <typename P> using CGAL_OverlayOperator = PolygonOverlayOperator<P, geometry_library::cgal, true>;
	template <typename P> using GEOS_OverlayOperator = PolygonOverlayOperator<P, geometry_library::geos, true>;
	template <typename P> using BoostPolygonConnectivityOperator = PolygonOverlayOperator<P, geometry_library::boost_polygon, false>;
	template <typename P> using BoostGeometryConnectivityOperator = PolygonOverlayOperator<P, geometry_library::boost_geometry, false>;
	template <typename P> using CGAL_ConnectivityOperator = PolygonOverlayOperator<P, geometry_library::cgal, false>;
	template <typename P> using GEOS_ConnectivityOperator = PolygonOverlayOperator<P, geometry_library::geos, false>;

	tl_oper::inst_tuple_templ<typelists::sint_points , BoostPolygonOverlayOperator , AbstrOperGroup&, bool> boostPolygonOverlayOperators   (grOverlayPolygon, false);
	tl_oper::inst_tuple_templ<typelists::sint_points , BoostPolygonOverlayOperator , AbstrOperGroup&, bool> boostPolygonBpOverlayOperators (grBpOverlayPolygon, false);
	tl_oper::inst_tuple_templ<typelists::float_points, BoostGeometryOverlayOperator, AbstrOperGroup&, bool> boostGeometryOverlayOperators  (grOverlayPolygon, false);
	tl_oper::inst_tuple_templ<typelists::points      , BoostGeometryOverlayOperator, AbstrOperGroup&, bool> boostGeometryBgOverlayOperators(grBgOverlayPolygon, false);
	tl_oper::inst_tuple_templ<typelists::points,       CGAL_OverlayOperator, AbstrOperGroup&, bool> cgalOverlayOperators(grCGALOverlayPolygon, false);
	tl_oper::inst_tuple_templ<typelists::points,       GEOS_OverlayOperator, AbstrOperGroup&, bool> geosOverlayOperators(grGEOSOverlayPolygon, false);

	tl_oper::inst_tuple_templ<typelists::sint_points, PolygonConnectivityOperator>	polygonConnectivityOperators;
	tl_oper::inst_tuple_templ<typelists::points,      BoxConnectivityOperator>	    boxConnectivityOperators;

	tl_oper::inst_tuple_templ<typelists::sint_points, BoostPolygonConnectivityOperator, AbstrOperGroup&, bool> boostPolygonConnectivityOperators(grBpPolygonConnectivity, false);
	tl_oper::inst_tuple_templ<typelists::points, BoostGeometryConnectivityOperator, AbstrOperGroup&, bool> boostGeometryConnectivityOperators(grBgPolygonConnectivity, false);
	tl_oper::inst_tuple_templ<typelists::points, CGAL_ConnectivityOperator, AbstrOperGroup&, bool> cgalConnectivityOperators(grCGALPolygonConnectivity, false);
	tl_oper::inst_tuple_templ<typelists::points, GEOS_ConnectivityOperator, AbstrOperGroup&, bool> geosConnectivityOperators(grGEOSPolygonConnectivity, false);

	tl_oper::inst_tuple_templ<typelists::sint_points, BoostPolygonOverlayOperator, AbstrOperGroup&, bool> boostPolygonBpOverlay1Operators(grBpOverlayPolygon, true);
	tl_oper::inst_tuple_templ<typelists::points, BoostGeometryOverlayOperator, AbstrOperGroup&, bool> boostGeometryBgOverlay1Operators(grBgOverlayPolygon, true);
	tl_oper::inst_tuple_templ<typelists::points, CGAL_OverlayOperator, AbstrOperGroup&, bool> cgalOverlay1Operators(grCGALOverlayPolygon, true);
	tl_oper::inst_tuple_templ<typelists::points, GEOS_OverlayOperator, AbstrOperGroup&, bool> geosOverlay1Operators(grGEOSOverlayPolygon, true);
	tl_oper::inst_tuple_templ<typelists::sint_points, BoostPolygonConnectivityOperator, AbstrOperGroup&, bool> boostPolygonConnectivity1Operators(grBpPolygonConnectivity, true);
	tl_oper::inst_tuple_templ<typelists::points, BoostGeometryConnectivityOperator, AbstrOperGroup&, bool> boostGeometryConnectivity1Operators(grBgPolygonConnectivity, true);
	tl_oper::inst_tuple_templ<typelists::points, CGAL_ConnectivityOperator, AbstrOperGroup&, bool> cgalConnectivity1Operators(grCGALPolygonConnectivity, true);
	tl_oper::inst_tuple_templ<typelists::points, GEOS_ConnectivityOperator, AbstrOperGroup&, bool> geosConnectivity1Operators(grGEOSPolygonConnectivity, true);


	PolyOperatorGroupss simple("", PolygonFlags());
	BpPolyOperatorGroupss bp_simple;
	BgPolyOperatorGroupss bg_simple;
	CGAL_PolyOperatorGroupss cgal_simple;
	GEOS_PolyOperatorGroupss geos_simple;

	PolyOperatorGroupsss f2 (SharedStr(""),          PolygonFlags());
	PolyOperatorGroupsss f21(SharedStr("_filtered"), PolygonFlags::F_Filter2);
	PolyOperatorGroupsss f22(SharedStr("_inflated"), PolygonFlags::F_Inflate2);
	PolyOperatorGroupsss f23(SharedStr("_deflated"), PolygonFlags::F_Deflate2);
	PolyOperatorGroupsss f24(SharedStr("_i4HV"), PolygonFlags::F_I4HV2);
	PolyOperatorGroupsss f25(SharedStr("_i4D"), PolygonFlags::F_I4D2);
	PolyOperatorGroupsss f26(SharedStr("_i8D"), PolygonFlags::F_I8D2);
	PolyOperatorGroupsss f27(SharedStr("_i16D"), PolygonFlags::F_I16D2);
	PolyOperatorGroupsss f28(SharedStr("_iXHV"), PolygonFlags::F_IXHV2);
	PolyOperatorGroupsss f29(SharedStr("_iXD"), PolygonFlags::F_IXD2);
	PolyOperatorGroupsss f2a(SharedStr("_d4HV"), PolygonFlags::F_D4HV2);
	PolyOperatorGroupsss f2b(SharedStr("_d4D"), PolygonFlags::F_D4D2);
	PolyOperatorGroupsss f2c(SharedStr("_d8D"), PolygonFlags::F_D8D2);
	PolyOperatorGroupsss f2d(SharedStr("_d16D"), PolygonFlags::F_D16D2);
	PolyOperatorGroupsss f2e(SharedStr("_dXHV"), PolygonFlags::F_DXHV2);
	PolyOperatorGroupsss f2f(SharedStr("_dXD"), PolygonFlags::F_DXD2);
}

/******************************************************************************/
