// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <numbers>

#include "BoostGeometry.h"

#include "geo/BoostPolygon.h"

#include "CGAL_Traits.h"
#include "GEOS_Traits.h"

#include <geos/simplify/DouglasPeuckerSimplifier.h>

static VersionComponent s_BoostGeometry("boost::geometry " BOOST_STRINGIZE(BOOST_GEOMETRY_VERSION));

// *****************************************************************************
//	more conversion functions
// *****************************************************************************

template <typename P>
void bp_load_multi_linestring(std::vector<std::vector<bp::point_data<scalar_of_t<P>>>>& mls, SA_ConstReference<P> multiLineStringRef, std::vector<bp::point_data<scalar_of_t<P>>>& helperLineString)
{
	mls.clear();

	auto lineStringBegin = begin_ptr(multiLineStringRef)
		, sequenceEnd = end_ptr(multiLineStringRef);

	while (lineStringBegin != sequenceEnd)
	{
		while (!IsDefined(*lineStringBegin))
			if (++lineStringBegin == sequenceEnd)
				return;

		auto lineStringEnd = lineStringBegin + 1;
		while (lineStringEnd != sequenceEnd && IsDefined(*lineStringEnd))
			++lineStringEnd;

		helperLineString.assign(lineStringBegin, lineStringEnd);
		if (!helperLineString.empty())
			mls.emplace_back(std::move(helperLineString));

		lineStringBegin = lineStringEnd;
	}
}

template <typename P>
void bg_load_multi_linestring(bg_multi_linestring_t& mls, SA_ConstReference<P> multiLineStringRef, bg_linestring_t& helperLineString)
{
	mls.clear();

	auto lineStringBegin = begin_ptr(multiLineStringRef)
		, sequenceEnd = end_ptr(multiLineStringRef);

	while (lineStringBegin != sequenceEnd)
	{
		while (!IsDefined(*lineStringBegin))
			if (++lineStringBegin == sequenceEnd)
				return;

		auto lineStringEnd = lineStringBegin + 1;
		while (lineStringEnd != sequenceEnd && IsDefined(*lineStringEnd))
			++lineStringEnd;

		helperLineString.assign(lineStringBegin, lineStringEnd);
		if (!helperLineString.empty())
			mls.emplace_back(std::move(helperLineString));

		lineStringBegin = lineStringEnd;
	}
}

// *****************************************************************************
//	operation groups
// *****************************************************************************


static CommonOperGroup grBgSimplify_multi_polygon("bg_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_polygon      ("bg_simplify_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_linestring   ("bg_simplify_linestring", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grGeosSimplify_multi_polygon("geos_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBgIntersect   ("bg_intersect" ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgUnion       ("bg_union"     ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgXOR         ("bg_xor"       ,   oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgDifference  ("bg_difference",   oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grcgalIntersect ("cgal_intersect",  oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalUnion     ("cgal_union",      oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalXOR       ("cgal_xor",        oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalDifference("cgal_difference", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grgeosIntersect ("geos_intersect",  oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosUnion     ("geos_union",      oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosXOR       ("geos_xor",        oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grgeosDifference("geos_difference", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBpBuffer_point        ("bp_buffer_point",         oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBpBuffer_multi_point  ("bp_buffer_multi_point",   oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBgBuffer_point        ("bg_buffer_point",         oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_point  ("bg_buffer_multi_point",   oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grCgalBuffer_point      ("cgal_buffer_point",       oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grCgalBuffer_multi_point("cgal_buffer_multi_point", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grGeosBuffer_point      ("geos_buffer_point",       oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_multi_point("geos_buffer_multi_point", oper_policy::better_not_in_meta_scripting);


#if DMS_VERSION_MAJOR < 15
static Obsolete<CommonOperGroup> grBgBuffer_polygon("use bg_buffer_single_polygon", "bg_buffer_polygon", oper_policy::better_not_in_meta_scripting|oper_policy::depreciated);
#endif

static CommonOperGroup grBgBuffer_single_polygon("bg_buffer_single_polygon", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBpBuffer_linestring("bp_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_linestring("bg_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_linestring("geos_buffer_linestring", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grCgalBuffer_linestring("cgal_buffer_linestring", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBpBuffer_multi_polygon("bp_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_polygon("bg_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grGeosBuffer_multi_polygon("geos_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);

#if DMS_VERSION_MAJOR < 15
static Obsolete<CommonOperGroup> grOuter_polygon("use bg_outer_single_polygon", "outer_polygon", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated);
static Obsolete<CommonOperGroup> grOuter_multi_polygon("use bg_outer_multi_polygon", "outer_multi_polygon", oper_policy::better_not_in_meta_scripting | oper_policy::depreciated);
#endif

static CommonOperGroup grBgOuter_single_polygon("bg_outer_single_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgOuter_multi_polygon("bg_outer_multi_polygon", oper_policy::better_not_in_meta_scripting);

// *****************************************************************************
//	map algebraic operations on boost geometry polygons
// *****************************************************************************

template <typename P> using sequence_t = std::vector<P>;
template <typename P> using BinaryMapAlgebraicOperator = BinaryAttrOper<sequence_t<P>, sequence_t<P>, sequence_t<P>>;

template <typename P, typename BinaryBgMpOper>
struct BgMultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using ArgType = DataArray<PolygonType>;

	BgMultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}
	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);


		bg_ring_t helperRing;
		bg_polygon_t helperPolygon;
		bg_multi_polygon_t currMP1, currMP2, resMP;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			assign_multi_polygon(currMP1, arg1Data[0], true, helperPolygon, helperRing);
		if (domain2IsVoid)
			assign_multi_polygon(currMP2, arg2Data[0], true, helperPolygon, helperRing);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				assign_multi_polygon(currMP1, arg1Data[i], true, helperPolygon, helperRing);
			if (!domain2IsVoid)
				assign_multi_polygon(currMP2, arg2Data[i], true, helperPolygon, helperRing);
			resMP.clear();
			m_Oper(currMP1, currMP2, resMP);
			bg_store_multi_polygon(resData[i], resMP);
		}
	}
	BinaryBgMpOper m_Oper;
};

// *****************************************************************************
//	map algebraic operations on CGAL polygons
// *****************************************************************************

template <typename P, typename BinaryBgMpOper>
struct CGAL_MultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using ArgType = DataArray<PolygonType>;

	CGAL_MultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}
	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		CGAL_Traits::Ring helperRing;
		CGAL_Traits::Polygon_with_holes helperPolygon;
		CGAL_Traits::Polygon_set currMP1, currMP2, resMP;
		std::vector<DPoint> helperPointArray;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			assign_multi_polygon(currMP1, arg1Data[0], true, std::move(helperPolygon), std::move(helperRing));
		if (domain2IsVoid)
			assign_multi_polygon(currMP2, arg2Data[0], true, std::move(helperPolygon), std::move(helperRing));

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				assign_multi_polygon(currMP1, arg1Data[i], true, std::move(helperPolygon), std::move(helperRing));
			if (!domain2IsVoid)
				assign_multi_polygon(currMP2, arg2Data[i], true, std::move(helperPolygon), std::move(helperRing));
			resMP.clear();
			m_Oper(currMP1, currMP2, resMP);
			cgal_assign_polygon_set(resData[i], resMP);
		}
	}
	BinaryBgMpOper m_Oper;
};

// *****************************************************************************
//	map algebraic operations on GEOS polygons
// *****************************************************************************

template <typename P, typename BinaryBgMpOper>
struct GEOS_MultiPolygonOperator : BinaryMapAlgebraicOperator<P>
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using ArgType = DataArray<PolygonType>;

	GEOS_MultiPolygonOperator(AbstrOperGroup& gr, BinaryBgMpOper&& oper = BinaryBgMpOper())
		: BinaryMapAlgebraicOperator<P>(&gr, compatible_simple_values_unit_creator, ValueComposition::Polygon)
	{}
	using st = sequence_traits<PolygonType>;
	using seq_t = typename st::seq_t;
	using cseq_t = typename st::cseq_t;

	void CalcTile(seq_t resData, cseq_t arg1Data, cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		tile_offset n1 = arg1Data.size();
		tile_offset n2 = arg2Data.size();
		tile_offset n = std::max(n1, n2);
		assert(n1 == n || (af & AF1_ISPARAM));
		assert(n2 == n || (af & AF2_ISPARAM));
		assert(resData.size() == n);

		std::unique_ptr<geos::geom::Geometry> currMP1, currMP2, resMP;

		bool domain1IsVoid = (af & AF1_ISPARAM);
		bool domain2IsVoid = (af & AF2_ISPARAM);
		if (domain1IsVoid)
			currMP1 = geos_create_polygons(arg1Data[0]);
		if (domain2IsVoid)
			currMP2 = geos_create_polygons(arg2Data[0]);

		for (SizeT i = 0; i != n; ++i)
		{
			if (!domain1IsVoid)
				currMP1 = geos_create_polygons(arg1Data[i]);
			if (!domain2IsVoid)
				currMP2 = geos_create_polygons(arg2Data[i]);
			m_Oper(std::move(currMP1), std::move(currMP2), resMP);
			geos_assign_geometry(resData[i], resMP.get());
		}
	}
	BinaryBgMpOper m_Oper;
};

// *****************************************************************************
//	simplify
// *****************************************************************************

class AbstrSimplifyOperator : public BinaryOperator
{
protected:
	AbstrSimplifyOperator(AbstrOperGroup& gr, const DataItemClass* polyAttrClass)
		:	BinaryOperator(&gr, polyAttrClass
			,	polyAttrClass
			,	DataArray<Float64>::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A); 
		dms_assert(arg2A); 

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw| UM_AllowVoidRight));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, arg1A->GetValueComposition());

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			Float64 maxError = const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0];
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A, maxError](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock (arg1A->GetCurrRefObj(), t);

					Calculate(resObj, arg1A, maxError, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const = 0;
};

template <typename P, geometry_library GL>
struct SimplifyMultiPolygonOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyMultiPolygonOperator(AbstrOperGroup& aog)
		: AbstrSimplifyOperator(aog, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		if constexpr (GL == geometry_library::boost_geometry)
		{
			bg_ring_t  currRing, resRing;
			std::vector<DPoint> ringClosurePoints;

			for (SizeT i = 0, n = polyData.size(); i != n; ++i)
			{
				auto polyDataElem = polyData[i];
				P lb = MaxValue<P>();
				for (auto p : polyDataElem)
					MakeLowerBound(lb, p);

				ringClosurePoints.clear();
				SA_ConstRingIterator<PointType> rb(polyDataElem, 0), re(polyDataElem, -1);
				auto ri = rb;
				dbg_assert(ri != re);
				if (ri == re)
					continue;
				for (; ri != re; ++ri)
				{
					assert((*ri).begin() != (*ri).end());
					assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

					currRing.assign((*ri).begin(), (*ri).end());
					assert(currRing.begin() != currRing.end());
					assert(currRing.begin()[0] == currRing.end()[-1]); // closed ?
					move(currRing, -DPoint(lb));
					if (empty(currRing))
						continue;

					boost::geometry::simplify(currRing, resRing, maxError);
					move(resRing, DPoint(lb));

					if (empty(resRing))
						continue;

					assert(resRing.begin()[0] == resRing.end()[-1]); // closed ?
					resData[i].append(resRing.begin(), resRing.end());
					ringClosurePoints.emplace_back(resRing.end()[-1]);
				}
				if (ringClosurePoints.empty())
					continue;
				ringClosurePoints.pop_back();
				while (!ringClosurePoints.empty())
				{
					resData[i].emplace_back(ringClosurePoints.back());
					ringClosurePoints.pop_back();
				}
			}
		}
		else if constexpr (GL == geometry_library::geos)
		{
			for (SizeT i = 0, n = polyData.size(); i != n; ++i)
			{
				auto polyDataElem = polyData[i];
				auto currGeom = geos_create_polygons(polyDataElem);

				geos::simplify::DouglasPeuckerSimplifier simplifier(currGeom.get());
				simplifier.setDistanceTolerance(maxError);

				auto resGeom = simplifier.getResultGeometry();

				geos_assign_geometry(resData[i], resGeom.get());
			}

		}
	}
};

template <typename P>
struct SimplifyPolygonOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyPolygonOperator()
		: AbstrSimplifyOperator(grBgSimplify_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		bg_ring_t currRing, resRing;
		std::vector<DPoint> ringClosurePoints;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto lb = MaxValue<PointType>();
			for (auto p : polyData[i])
				MakeLowerBound(lb, p);

			ringClosurePoints.clear();
			SA_ConstRingIterator<PointType> rb(polyData[i], 0), re(polyData[i], -1);
			auto ri = rb;
			dbg_assert(ri != re);
			if (ri == re)
				continue;
			for (; ri != re; ++ri)
			{
				assert((*ri).begin() != (*ri).end()); // non-empty ring, must be guaranteed by boost::polygon::SA_ConstRingIterator
//				dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

				currRing.assign((*ri).begin(), (*ri).end());
				if ((*ri).begin()[0] != (*ri).end()[-1])
					currRing.emplace_back(currRing.front());
				assert(currRing.begin()[0] == currRing.end()[-1]); // closed !
				move(currRing, -DPoint(lb));

				boost::geometry::simplify(currRing, resRing, maxError);
				move(resRing, DPoint(lb));

				if (empty(resRing))
				{
					if (ri == rb) // if first ring is empty, assume all further rings are inner rings inside it (this is supposed not to be a multi_polygon)
						break;
					continue;
				}

				dms_assert(resRing.begin()[0] == resRing.end()[-1]); // closed ?
				resData[i].append(resRing.begin(), resRing.end());
				ringClosurePoints.emplace_back(resRing.end()[-1]);
			}
			if (ringClosurePoints.empty())
				continue;
			ringClosurePoints.pop_back();
			while (!ringClosurePoints.empty())
			{
				resData[i].emplace_back(ringClosurePoints.back());
				ringClosurePoints.pop_back();
			}
		}
	}
};

template <typename P>
struct SimplifyLinestringOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyLinestringOperator()
		: AbstrSimplifyOperator(grBgSimplify_linestring, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto lineStringData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(lineStringData.size() == resData.size());

		assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

		bg_multi_linestring_t currGeometry, resGeometry;
		bg_linestring_t helperLineString;

		for (SizeT i = 0, n = lineStringData.size(); i != n; ++i)
		{
			bg_load_multi_linestring(currGeometry, lineStringData[i], helperLineString);
			resGeometry.resize(0);
			if (!currGeometry.empty())
			{
				auto lb = MaxValue<DPoint>();
				MakeLowerBound(lb, currGeometry);
				move(currGeometry, -lb);

				boost::geometry::simplify(currGeometry, resGeometry, maxError);
				move(resGeometry, DPoint(lb));

			}
			bg_store_multi_linestring(resData[i], resGeometry);
		}
	}
};

// *****************************************************************************
//	buffer
// *****************************************************************************

class AbstrBufferOperator : public TernaryOperator
{
protected:
	AbstrBufferOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass, const DataItemClass* argAttrClass = nullptr)
		: TernaryOperator(&og, polyAttrClass
			, argAttrClass ? argAttrClass : polyAttrClass
			, DataArray<Float64>::GetStaticClass()
			, DataArray<UInt8>::GetStaticClass()
		)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		assert(arg1A);
		assert(arg2A);
		assert(arg3A);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		const AbstrUnit* domain3Unit = arg3A->GetAbstrDomainUnit(); bool e3IsVoid = domain3Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
//		const AbstrUnit* values3Unit = arg3A->GetAbstrValuesUnit();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw | UM_AllowVoidRight));
		domain1Unit->UnifyDomain(domain3Unit, "e1", "e3", UnifyMode(UM_Throw | UM_AllowVoidRight));

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			Float64 bufferDistance = e2IsVoid ? const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0] : 0;
			UInt8 nrPointsInCircle = e3IsVoid ? const_array_cast<UInt8  >(arg3A)->GetLockedDataRead()[0] : 0;

			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [=, this, resObj = resLock.get()](tile_id t)->void
				{
					this->Calculate(resObj, arg1A
						, e2IsVoid, arg2A, bufferDistance
						, e3IsVoid, arg3A, nrPointsInCircle
						, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resObj
		, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const = 0;
};

template <typename CoordType>
auto bp_circle(double radius, int pointsPerCircle) -> std::vector<bp::point_data<CoordType> >
{
	if (pointsPerCircle < 3)
		pointsPerCircle = 3;
	using Point = bp::point_data<CoordType>;
	std::vector<Point> points;
	points.reserve(pointsPerCircle + 1);
	auto anglePerPoint = 2.0 * std::numbers::pi_v<double> / pointsPerCircle;
	for (int i = 0; i < pointsPerCircle; ++i) {
		double angle = i * anglePerPoint;
		int x = static_cast<int>(radius * std::cos(angle));
		int y = static_cast<int>(radius * std::sin(angle));
		points.emplace_back(x, y);
	}
	points.emplace_back(points.front());
	return points;
}

template <typename P, geometry_library GL>
struct BufferPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PointType>;
	using ResultType = DataArray<PolygonType>;

	BufferPointOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* pointItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		auto pointData = const_array_cast<PointType>(pointItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData     = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem    )->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(pointData.size() == resData.size());

		SizeT i=0, n = pointData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				std::vector<PointType> ringClosurePoints;
				assert(pointItem->GetValueComposition() == ValueComposition::Single);

				using bg_polygon_t = boost::geometry::model::polygon<DPoint>;

				boost::geometry::model::multi_polygon<bg_polygon_t> resMP;
				boost::geometry::buffer(DPoint(0, 0), resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);

				boost::geometry::model::ring<DPoint> resRing = resMP[0].outer();
				boost::geometry::model::ring<DPoint> movedRing;

				do {
					movedRing = resRing;
					move(movedRing, DPoint(pointData[i]));
					bg_store_ring(resData[i], movedRing);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::boost_polygon)
			{
				auto resRing = bp_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					auto movedRing = resRing;
					move<CoordType>(movedRing, pointData[i]);

					bp_assign_ring(resData[i], movedRing.begin(), movedRing.end());

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::cgal)
			{
				auto cgalCircle = cgal_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					// Define the translation vector (dx, dy)
					CGAL_Traits::Kernel::Vector_2 translation_vector(pointData[i].X(), pointData[i].Y());

					// Define the affine transformation for translation
					CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translate(CGAL::TRANSLATION, translation_vector);

					// Create a new polygon for the translated version
					CGAL::Polygon_2<CGAL_Traits::Kernel> translated_polygon;

					// Apply the translation to each vertex of the original polygon
					for (auto vertex : cgalCircle)
						translated_polygon.push_back(translate.transform(vertex));

					// Store the translated polygon
					cgal_assign_ring(resData[i], translated_polygon);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				const auto& dmsPoint = pointData[i];
				auto geosPoint = geos_factory()->createPoint(geos::geom::Coordinate(dmsPoint.X(), dmsPoint.Y()));
				auto bufferGeometry = geosPoint->buffer(bufferDistance, (pointsPerCircle + 3) / 4);

				geos_assign_geometry(resData[i], bufferGeometry.get());

				// move to nextPoint
				if (++i == n)
					return;
			}
		}
	}
};

template <typename P, geometry_library GL>
struct BufferMultiPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPointOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];

			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

				boost::geometry::model::multi_point<DPoint> currGeometry;
				bg_multi_polygon_t resMP;

				do {
					resMP.clear();
					currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));

					auto p = MaxValue<DPoint>();
					MakeLowerBound(p, currGeometry);
					move(currGeometry, -p);

					boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
					move(resMP, p);

					bg_store_multi_polygon(resData[i], resMP);

					// move to next geometry
							// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr(GL == geometry_library::boost_polygon)
			{
				typename bp::polygon_set_data<CoordType>::clean_resources cleanResources;
				typename bp_union_poly_traits<CoordType>::polygon_set_data_type resMP;
				auto resRing = bp_circle<CoordType>(bufferDistance, pointsPerCircle);

				do {
					resMP.clear();
					for (const auto& dmsPoint : polyData[i])
					{
						auto movedRing = resRing;
						move<CoordType>(movedRing, dmsPoint);
						resMP.insert(movedRing, false);
					}

					bp_assign(resData[i], resMP, cleanResources);
					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);

			}
			else if constexpr (GL == geometry_library::cgal)
			{
				auto cgalCircle = cgal_circle<CoordType>(bufferDistance, pointsPerCircle);
				do {
					CGAL_Traits::Polygon_set result;
					for (const auto& p : polyData[i])
					{
						// Define the translation vector (dx, dy)
						CGAL_Traits::Kernel::Vector_2 translation_vector(p.X(), p.Y());

						// Define the affine transformation for translation
						CGAL::Aff_transformation_2<CGAL_Traits::Kernel> translate(CGAL::TRANSLATION, translation_vector);

						// Create a new polygon for the translated version
						CGAL::Polygon_2<CGAL_Traits::Kernel> translated_polygon;

						// Apply the translation to each vertex of the original polygon
						for (auto vertex : cgalCircle)
							translated_polygon.push_back(translate.transform(vertex));
						result.join( translated_polygon );
					}
					// Store the translated polygon
					cgal_assign_polygon_set(resData[i], result);

					// move to nextPoint
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				std::unique_ptr<geos::geom::Geometry> geosResult;
				for (const auto& p : polyData[i])
				{
					auto point = geos_factory()->createPoint(geos::geom::Coordinate(p.X(), p.Y()));
					auto bufferGeometry = point->buffer(bufferDistance, (pointsPerCircle + 3) / 4);

					if (!geosResult)
						geosResult = std::move(bufferGeometry);
					else
						geosResult = geosResult->Union(bufferGeometry.get());
				}

				geos_assign_geometry(resData[i], geosResult.get());

				// move to next geometry
				if (++i == n)
					return;
			}
		}
	}
};

template <typename CoordType>
void bp_bufferLineString(bp::polygon_set_data<CoordType>& result, const std::vector<bp::point_data< CoordType>>& lineString, const std::vector<bp::point_data< CoordType>>& circle)
{
	using Point = bp::point_data<CoordType>;
	using bp_convolution = boost::polygon::detail::template minkowski_offset<CoordType>;
	bp_convolution::convolve_two_point_sequences(result, lineString.begin(), lineString.end(), circle.begin(), circle.end(), false);
}


template <typename P, geometry_library GL>
struct BufferLineStringOperator : public AbstrBufferOperator
{
	using PointType = P;
	using CoordType = scalar_of_t<PointType>;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferLineStringOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* lineStringItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		assert(lineStringItem->GetValueComposition() == ValueComposition::Sequence);

		auto lineStringData = const_array_cast<PolygonType>(lineStringItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);

		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(lineStringData.size() == resData.size());

		SizeT i = 0, n = lineStringData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr (GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;


				bg_linestring_t helperLineString;
				bg_multi_linestring_t currGeometry;
				bg_multi_polygon_t resMP;

				do {
					resMP.clear();
					bg_load_multi_linestring(currGeometry, lineStringData[i], helperLineString);

					if (!currGeometry.empty())
					{
						auto p = MaxValue<DPoint>();
						MakeLowerBound(p, currGeometry);
						move(currGeometry, -p);

						boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
						move(resMP, p);

					}
					bg_store_multi_polygon(resData[i], resMP);

					// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else if constexpr (GL == geometry_library::geos)
			{
				auto lineStringRef = lineStringData[i];
				auto lineString = geos_create_multi_linestring<P>(lineStringRef.begin(), lineStringRef.end());
				auto bufferedLineString = lineString->buffer(bufferDistance, (pointsPerCircle + 3) / 4);
				geos_assign_geometry(resData[i], bufferedLineString.get());
			}
			else if constexpr (GL == geometry_library::boost_polygon)
			{
				auto circle = bp_circle<CoordType>(bufferDistance, pointsPerCircle);

				using traits_t = bp_union_poly_traits<CoordType>;
				using bp_linestring = typename traits_t::ring_type;
				bp_linestring helperLineString;
				typename bp::polygon_set_data<CoordType>::clean_resources cleanResources;
				std::vector<bp_linestring> lineStrings;

				do {
					lineStrings.clear();
					bp_load_multi_linestring<P>(lineStrings, lineStringData[i], helperLineString);

					bp::polygon_set_data<CoordType> resMP;
					for (const auto& ls : lineStrings)
					{
						bp_bufferLineString(resMP, ls, circle);
					}

					bp_assign(resData[i], resMP, cleanResources);

					// move to next geometry
					if (++i == n)
						return;
				} while (e2IsVoid && e3IsVoid);
			}
			else
			{
//				thisGlIsNotYetImplemented; 
			}
		}
	}
};

template <typename P, geometry_library GL>
struct BufferMultiPolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPolygonOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			if (!e2IsVoid)
				bufferDistance = bufDistData[i];
			if (!e3IsVoid)
				pointsPerCircle = ppcData[i];
			if constexpr(GL == geometry_library::boost_geometry)
			{
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				bg_ring_t helperRing;

				bg_polygon_t helperPolygon;
				bg_multi_polygon_t currMP, resMP;

				bool takeSmallLoop = e2IsVoid && e3IsVoid;
				do
				{


					assign_multi_polygon(currMP, polyData[i], true, helperPolygon, helperRing);
					if (!currMP.empty())
					{
						auto lb = MaxValue<DPoint>();
						MakeLowerBound(lb, currMP);
						move(currMP, -lb);

						boost::geometry::buffer(currMP, resMP
							, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
						move(resMP, lb);

						bg_store_multi_polygon(resData[i], resMP);
					}

					if (s_ProcessTimer.PassedSecs(5))
					{
						reportF(SeverityTypeID::ST_MajorTrace, "%s: processed %d/%d sequences of tile %d/%d"
							, GetGroup()->GetNameStr()
							, i, n
							, t, resItem->GetTiledRangeData()->GetNrTiles()
						);
					}
					++i;
				} while (i != n && takeSmallLoop);
				if (i == n)
					break;
			}
			else if constexpr (GL == geometry_library::geos)
			{
				auto mp = geos_create_polygons(polyData[i]);
				if (mp && !mp->isEmpty())
				{
					auto resMP = mp->buffer(bufferDistance, pointsPerCircle);
					geos_assign_geometry(resData[i], resMP.get());
				}
				if (++i == n)
					break;
			}
		}
	}
};

template <typename P>
struct BufferSinglePolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferSinglePolygonOperator(AbstrOperGroup& gr)
		: AbstrBufferOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto bufDistData = e2IsVoid ? DataArray<Float64>::locked_cseq_t{} : const_array_cast<Float64>(bufDistItem)->GetTile(t);
		auto ppcData = e3IsVoid ? DataArray<UInt8  >::locked_cseq_t{} : const_array_cast<UInt8>  (ppcItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		SizeT i = 0, n = polyData.size(); if (!n) return;

		while (true)
		{
			try {
				if (!e2IsVoid)
					bufferDistance = bufDistData[i];
				if (!e3IsVoid)
					pointsPerCircle = ppcData[i];
				boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
				boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
				boost::geometry::strategy::buffer::side_straight               sideStrategy;

				boost::geometry::model::ring<DPoint> helperRing;

				using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
				bg_polygon_t currPoly;
				boost::geometry::model::multi_polygon<bg_polygon_t> resMP;

			nextPointWithSameResRing:

				assign_polygon(currPoly, polyData[i], true, helperRing);
				if (!currPoly.outer().empty())
				{

					auto lb = MaxValue<DPoint>();
					MakeLowerBound(lb, currPoly);
					move(currPoly, -lb);

					boost::geometry::buffer(currPoly, resMP
						, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
					move(resMP, lb);

					bg_store_multi_polygon(resData[i], resMP);
				}

				// move to nextPoint
				if (++i == n)
					break;
				if (e2IsVoid && e3IsVoid)
					goto nextPointWithSameResRing;
			}
			catch (DmsException& e)
			{
				e.AsErrMsg()->TellExtraF("BufferSinglePolygonOperator::Calculate tile %d, offset %d", t, i);
				throw;
			}
		}
	}
};

// *****************************************************************************
//	outer
// *****************************************************************************

class AbstrOuterOperator : public UnaryOperator
{
protected:
	AbstrOuterOperator(AbstrOperGroup& og, const DataItemClass* polyAttrClass)
		: UnaryOperator(&og, polyAttrClass, polyAttrClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj(), t);

					Calculate(resObj, arg1A, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const = 0;
};

template <typename P>
struct OuterMultiPolygonOperator : public AbstrOuterOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	OuterMultiPolygonOperator(AbstrOperGroup& gr)
		: AbstrOuterOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		bg_ring_t currRing;

		using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
		bg_polygon_t currPoly;
		bg_multi_polygon_t currMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_multi_polygon(currMP, polyData[i], false, currPoly, currRing);

			if (!currMP.empty())
				bg_store_multi_polygon(resData[i], currMP);
		}
	}
};

template <typename P>
struct OuterSingePolygonOperator : public AbstrOuterOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	OuterSingePolygonOperator(AbstrOperGroup& gr)
		: AbstrOuterOperator(gr, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		bg_ring_t helperRing;

		bg_polygon_t  currPoly;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			try {
				assign_polygon(currPoly, polyData[i], false, helperRing);

				if (!currPoly.outer().empty())
				{
					resData[i].reserve(currPoly.outer().size());
					bg_store_ring(resData[i], currPoly.outer());
				}
			}
			catch (DmsException& e)
			{
				e.AsErrMsg()->TellExtraF("OuterSingePolygonOperator::Calculate tile %d, offset %d", t, i);
				throw;
			}

		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	template <typename P> using BgSimplifyMultiPolygonOperator = SimplifyMultiPolygonOperator<P, geometry_library::boost_geometry>;
	template <typename P> using GeosSimplifyMultiPolygonOperator = SimplifyMultiPolygonOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, SimplifyLinestringOperator> simplifyLineStringOperators;
	tl_oper::inst_tuple_templ<typelists::points, BgSimplifyMultiPolygonOperator, AbstrOperGroup&> bg_simplifyMultiPolygonOperators(grBgSimplify_multi_polygon);
	tl_oper::inst_tuple_templ<typelists::points, GeosSimplifyMultiPolygonOperator, AbstrOperGroup&> geos_simplifyMultiPolygonOperators(grGeosSimplify_multi_polygon);
	tl_oper::inst_tuple_templ<typelists::points, SimplifyPolygonOperator> simplifyPolygonOperators;

	template <typename P> using BpBufferPointOperator = BufferPointOperator<P, geometry_library::boost_polygon>;
	template <typename P> using BgBufferPointOperator = BufferPointOperator<P, geometry_library::boost_geometry>;
	template <typename P> using CgalBufferPointOperator = BufferPointOperator<P, geometry_library::cgal>;
	template <typename P> using GeosBufferPointOperator = BufferPointOperator<P, geometry_library::geos>;

	template <typename P> using BpBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::boost_polygon>;
	template <typename P> using BgBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::boost_geometry>;
	template <typename P> using CgalBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::cgal>;
	template <typename P> using GeosBufferMultiPointOperator = BufferMultiPointOperator<P, geometry_library::geos>;

	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferPointOperator, AbstrOperGroup&> bpBufferPointOperators(grBpBuffer_point);
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferMultiPointOperator, AbstrOperGroup&> bpBufferMultiPointOperators(grBpBuffer_multi_point);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferPointOperator, AbstrOperGroup&> bgBufferPointOperators(grBgBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferMultiPointOperator, AbstrOperGroup&> bgBufferMultiPointOperators(grBgBuffer_multi_point);
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferPointOperator, AbstrOperGroup&> cgalBufferPointOperators(grBgBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferMultiPointOperator, AbstrOperGroup&> cgalBufferMultiPointOperators(grCgalBuffer_multi_point);
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferPointOperator, AbstrOperGroup&> geosBufferPointOperators(grBgBuffer_point);
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferMultiPointOperator, AbstrOperGroup&> geosBeosBufferMultiPointOperators(grGeosBuffer_multi_point);



	template <typename P> using BpBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::boost_polygon>;
	template <typename P> using BgBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::boost_geometry>;
	template <typename P> using GeosBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::geos>;
	template <typename P> using CgalBufferLineStringOperator = BufferLineStringOperator<P, geometry_library::cgal>;
	tl_oper::inst_tuple_templ<typelists::int_points, BpBufferLineStringOperator, AbstrOperGroup&> bpBufferLineStringOperators(grBpBuffer_linestring);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferLineStringOperator, AbstrOperGroup&> bgBufferLineStringOperators(grBgBuffer_linestring);
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferLineStringOperator, AbstrOperGroup&> geosBufferLineStringOperators(grGeosBuffer_linestring);
	tl_oper::inst_tuple_templ<typelists::points, CgalBufferLineStringOperator, AbstrOperGroup&> cgalBufferLineStringOperators(grCgalBuffer_linestring);


	template <typename P> using BgIntersectMultiPolygonOperator = BgMultiPolygonOperator < P, bg_intersection> ;
	tl_oper::inst_tuple_templ<typelists::points, BgIntersectMultiPolygonOperator, AbstrOperGroup&> bgIntersectMultiPolygonOperatorsNamed(grBgIntersect);
	tl_oper::inst_tuple_templ<typelists::float_points, BgIntersectMultiPolygonOperator, AbstrOperGroup&> bgIntersectMultiPolygonOperatorsMul(cog_mul);
	tl_oper::inst_tuple_templ<typelists::float_points, BgIntersectMultiPolygonOperator, AbstrOperGroup&> bgIntersectMultiPolygonOperatorsBitAnd(cog_bitand);

	template <typename P> using BgUnionMultiPolygonOperator = BgMultiPolygonOperator<P, bg_union>;
	tl_oper::inst_tuple_templ<typelists::points, BgUnionMultiPolygonOperator, AbstrOperGroup&> bgUnionMultiPolygonOperatorsNamed(grBgUnion);
	tl_oper::inst_tuple_templ<typelists::float_points, BgUnionMultiPolygonOperator, AbstrOperGroup&> bgUnionMultiPolygonOperatorsAdd(cog_add);
	tl_oper::inst_tuple_templ<typelists::float_points, BgUnionMultiPolygonOperator, AbstrOperGroup&> bgUnionMultiPolygonOperatorsBitOr(cog_bitor);

	template <typename P> using BgDifferenceMultiPolygonOperator = BgMultiPolygonOperator<P, bg_difference>;
	tl_oper::inst_tuple_templ<typelists::points, BgDifferenceMultiPolygonOperator, AbstrOperGroup&> bgDifferenceMultiPolygonOperatorsNamed(grBgDifference);
	tl_oper::inst_tuple_templ<typelists::float_points, BgDifferenceMultiPolygonOperator, AbstrOperGroup&> bgDifferenceMultiPolygonOperatorsSub(cog_sub);

	template <typename P> using BgSymmetricDifferenceMultiPolygonOperator = BgMultiPolygonOperator<P, bg_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, BgSymmetricDifferenceMultiPolygonOperator, AbstrOperGroup&> bgSymmetricDifferenceMultiPolygonOperatorsNamed(grBgXOR);
	tl_oper::inst_tuple_templ<typelists::float_points, BgSymmetricDifferenceMultiPolygonOperator, AbstrOperGroup&> bgSymmetricDifferenceMultiPolygonOperatorsBitXOR(cog_bitxor);


	template <typename P> using BgBufferMultiPolygonOperator = BufferMultiPolygonOperator<P, geometry_library::boost_geometry>;
	template <typename P> using GeosBufferMultiPolygonOperator = BufferMultiPolygonOperator<P, geometry_library::geos>;
	tl_oper::inst_tuple_templ<typelists::points, BufferSinglePolygonOperator, AbstrOperGroup&> bg_buffersinglePolygonOperators(grBgBuffer_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, BgBufferMultiPolygonOperator, AbstrOperGroup&> bg_bufferMultiPolygonOperators(grBgBuffer_multi_polygon);
	tl_oper::inst_tuple_templ<typelists::points, GeosBufferMultiPolygonOperator, AbstrOperGroup&> geos_bufferMultiPolygonOperators(grGeosBuffer_multi_polygon);

	tl_oper::inst_tuple_templ<typelists::points, OuterSingePolygonOperator, AbstrOperGroup&> bg_outerSinglePolygonOperators(grBgOuter_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator, AbstrOperGroup&> bg_outerMultiPolygonOperators(grBgOuter_multi_polygon);

	template <typename P> using CGAL_IntersectMultiPolygonOperator = CGAL_MultiPolygonOperator < P, cgal_intersection>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_IntersectMultiPolygonOperator, AbstrOperGroup&> cgalIntersectMultiPolygonOperatorsNamed(grcgalIntersect);

	template <typename P> using CGAL_UnionMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_union>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_UnionMultiPolygonOperator, AbstrOperGroup&> cgalUnionMultiPolygonOperatorsNamed(grcgalUnion);

	template <typename P> using CGAL_DifferenceMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_difference>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_DifferenceMultiPolygonOperator, AbstrOperGroup&> cgalDifferenceMultiPolygonOperatorsNamed(grcgalDifference);

	template <typename P> using CGAL_SymmetricDifferenceMultiPolygonOperator = CGAL_MultiPolygonOperator<P, cgal_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, CGAL_SymmetricDifferenceMultiPolygonOperator, AbstrOperGroup&> cgalSymmetricDifferenceMultiPolygonOperatorsNamed(grcgalXOR);


	template <typename P> using GEOS_IntersectMultiPolygonOperator = GEOS_MultiPolygonOperator < P, geos_intersection>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_IntersectMultiPolygonOperator, AbstrOperGroup&> geosIntersectMultiPolygonOperatorsNamed(grgeosIntersect);

	template <typename P> using GEOS_UnionMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_union>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_UnionMultiPolygonOperator, AbstrOperGroup&> geosUnionMultiPolygonOperatorsNamed(grgeosUnion);

	template <typename P> using GEOS_DifferenceMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_difference>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_DifferenceMultiPolygonOperator, AbstrOperGroup&> geosDifferenceMultiPolygonOperatorsNamed(grgeosDifference);

	template <typename P> using GEOS_SymmetricDifferenceMultiPolygonOperator = GEOS_MultiPolygonOperator<P, geos_sym_difference>;
	tl_oper::inst_tuple_templ<typelists::points, GEOS_SymmetricDifferenceMultiPolygonOperator, AbstrOperGroup&> geosSymmetricDifferenceMultiPolygonOperatorsNamed(grgeosXOR);

}

