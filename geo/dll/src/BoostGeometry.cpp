// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "BoostGeometry.h"

#include "CGAL_Traits.h"
#include <CGAL/Boolean_set_operations_2.h>

static VersionComponent s_BoostGeometry("boost::geometry " BOOST_STRINGIZE(BOOST_GEOMETRY_VERSION));

// *****************************************************************************
//	operation groups
// *****************************************************************************


static CommonOperGroup grBgSimplify_multi_polygon("bg_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_polygon      ("bg_simplify_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_linestring   ("bg_simplify_linestring", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBgIntersect ("bg_intersect" , oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgUnion     ("bg_union"     , oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgXOR       ("bg_xor"       , oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgDifference("bg_difference", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grcgalIntersect("cgal_intersect", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalUnion("cgal_union", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalXOR("cgal_xor", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grcgalDifference("cgal_difference", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBgBuffer_point        ("bg_buffer_point", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_point  ("bg_buffer_multi_point", oper_policy::better_not_in_meta_scripting);

#if DMS_VERSION_MAJOR < 15
static Obsolete<CommonOperGroup> grBgBuffer_polygon("use bg_buffer_single_polygon", "bg_buffer_polygon", oper_policy::better_not_in_meta_scripting|oper_policy::depreciated);
#endif

static CommonOperGroup grBgBuffer_single_polygon("bg_buffer_single_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_multi_polygon("bg_buffer_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgBuffer_linestring   ("bg_buffer_linestring", oper_policy::better_not_in_meta_scripting);

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
		std::vector<DPoint> helperPointArray;

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
			store_multi_polygon(resData[i], resMP, helperPointArray);
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
			cgal_assign(resData[i], resMP);
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

template <typename P>
struct SimplifyMultiPolygonOperator : public AbstrSimplifyOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	SimplifyMultiPolygonOperator()
		: AbstrSimplifyOperator(grBgSimplify_multi_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 maxError, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		bg_ring_t  currRing, resRing;
		std::vector<DPoint> ringClosurePoints;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto polyDataElem = polyData[i];
			P lb = MaxValue<P>();
			for (auto p: polyDataElem)
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
			load_multi_linestring(currGeometry, lineStringData[i], helperLineString);
			resGeometry.resize(0);
			if (!currGeometry.empty())
			{
				auto lb = MaxValue<DPoint>();
				MakeLowerBound(lb, currGeometry);
				move(currGeometry, -lb);

				boost::geometry::simplify(currGeometry, resGeometry, maxError);
				move(resGeometry, DPoint(lb));

			}
			store_multi_linestring(resData[i], resGeometry);
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

template <typename P>
struct BufferPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PointType>;
	using ResultType = DataArray<PolygonType>;

	BufferPointOperator()
		: AbstrBufferOperator(grBgBuffer_point, ResultType::GetStaticClass(), Arg1Type::GetStaticClass())
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

		nextPointWithSameResRing:
			movedRing = resRing;
			move(movedRing, DPoint(pointData[i]));
			store_ring(resData[i], movedRing);
			if (++i == n)
				break;
			if (e2IsVoid && e3IsVoid)
				goto nextPointWithSameResRing;
		}
	}
};

template <typename P>
struct BufferMultiPointOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPointOperator()
		: AbstrBufferOperator(grBgBuffer_multi_point, Arg1Type::GetStaticClass())
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

			boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
			boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::side_straight               sideStrategy;

			std::vector<DPoint> ringClosurePoints;
			assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

			boost::geometry::model::multi_point<DPoint> currGeometry;
			bg_multi_polygon_t resMP;

		nextPointWithSameResRing:
			resMP.clear();
			currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));

			auto p = MaxValue<DPoint>();
			MakeLowerBound(p, currGeometry);
			move(currGeometry, -p);

			boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
			move(resMP, p);

			store_multi_polygon(resData[i], resMP, ringClosurePoints);

			if (++i == n)
				break;
			if (e2IsVoid && e3IsVoid)
				goto nextPointWithSameResRing;
		}
	}
};

template <typename P>
struct BufferLineStringOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferLineStringOperator()
		: AbstrBufferOperator(grBgBuffer_linestring, Arg1Type::GetStaticClass())
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

			boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
			boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::side_straight               sideStrategy;

			std::vector<DPoint> ringClosurePoints;

			bg_linestring_t helperLineString;
			bg_multi_linestring_t currGeometry;
			bg_multi_polygon_t resMP;

	nextPointWithSameResRing:

			resMP.clear();
			load_multi_linestring(currGeometry, lineStringData[i], helperLineString);

			if (!currGeometry.empty())
			{
				auto p = MaxValue<DPoint>();
				MakeLowerBound(p, currGeometry);
				move(currGeometry, -p);

				boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
				move(resMP, p);

			}
			store_multi_polygon(resData[i], resMP, ringClosurePoints);
			if (++i == n)
				break;
			if (e2IsVoid && e3IsVoid)
				goto nextPointWithSameResRing;
		}
	}
};

template <typename P>
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
			boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
			boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
			boost::geometry::strategy::buffer::side_straight               sideStrategy;

			std::vector<DPoint> helperPointArray;
			bg_ring_t helperRing;

			bg_polygon_t helperPolygon;
			bg_multi_polygon_t currMP, resMP;

		nextPointWithSameResRing:

			assign_multi_polygon(currMP, polyData[i], true, helperPolygon, helperRing);
			if (!currMP.empty())
			{
				auto lb = MaxValue<DPoint>();
				MakeLowerBound(lb, currMP);
				move(currMP, -lb);

				boost::geometry::buffer(currMP, resMP
					, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
				move(resMP, lb);

				store_multi_polygon(resData[i], resMP, helperPointArray);
			}
			if (++i == n)
				break;
			if (e2IsVoid && e3IsVoid)
				goto nextPointWithSameResRing;
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

				std::vector<DPoint> ringClosurePoints;
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

					store_multi_polygon(resData[i], resMP, ringClosurePoints);
				}
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

		std::vector<DPoint> ringClosurePoints;
		bg_ring_t currRing;

		using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
		bg_polygon_t currPoly;
		bg_multi_polygon_t currMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_multi_polygon(currMP, polyData[i], false, currPoly, currRing);

			if (!currMP.empty())
				store_multi_polygon(resData[i], currMP, ringClosurePoints);
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
					store_ring(resData[i], currPoly.outer());
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
	struct bg_intersection {
		void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::intersection(a, b, r); }
	};

	struct bg_union {
		void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::union_(a, b, r); }
	};

	struct bg_difference {
		void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::difference(a, b, r); }
	};

	struct bg_sym_difference {
		void operator ()(const auto& a, const auto& b, auto& r) const { boost::geometry::sym_difference(a, b, r); }
	};

	struct cgal_intersection {
		void operator ()(const auto& a, const auto& b, auto& r) const { r.intersection(a, b); }
	};

	struct cgal_union {
		void operator ()(const auto& a, const auto& b, auto& r) const { r.join(a, b); }
	};

	struct cgal_difference {
		void operator ()(const auto& a, const auto& b, auto& r) const { r.difference(a, b); }
	};

	struct cgal_sym_difference {
		void operator ()(const auto& a, const auto& b, auto& r) const { r.symmetric_difference(a, b); }
	};

	tl_oper::inst_tuple_templ<typelists::points, SimplifyLinestringOperator> simplifyLineStringOperators;
	tl_oper::inst_tuple_templ<typelists::points, SimplifyMultiPolygonOperator> simplifyMultiPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, SimplifyPolygonOperator> simplifyPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferPointOperator> bufferPointOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferMultiPointOperator> bufferMultiPointOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferLineStringOperator> bufferLineStringOperators;

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

	tl_oper::inst_tuple_templ<typelists::points, BufferSinglePolygonOperator, AbstrOperGroup&> bg_buffersinglePolygonOperators(grBgBuffer_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, BufferMultiPolygonOperator, AbstrOperGroup&> bg_bufferMultiPolygonOperators(grBgBuffer_multi_polygon);

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


}

