// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcTypeLists.h"
#include "RtcGeneratedVersion.h"

#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/TypeListOper.h"
#include "xct/DmsException.h"

#include "ParallelTiles.h"

#include "Unit.h"
#include "UnitClass.h"

#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/within.hpp>

#include "ipolygon/polygon.hpp"
#include "geo/BoostPolygon.h"
#include "RemoveAdjacentsAndSpikes.h"

#include "VersionComponent.h"
static VersionComponent s_BoostGeometry("boost::geometry " BOOST_STRINGIZE(BOOST_GEOMETRY_VERSION));

using bg_multi_point_t = boost::geometry::model::multi_point<DPoint>;
using bg_linestring_t = boost::geometry::model::linestring<DPoint>;
using bg_ring_t = boost::geometry::model::ring<DPoint>;
using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
using bg_multi_polygon_t = boost::geometry::model::multi_polygon<bg_polygon_t>;

namespace boost::geometry::traits
{
	// define Point<T> as model for concept boost::geometry::point
	template <typename T> struct tag<Point<T>> { using type = point_tag; };
	template <typename T> struct dimension<Point<T>> : ::dimension_of < Point<T>> {};
	template <typename T> struct coordinate_type<Point<T>> { using type = T; };
	template <typename T> struct coordinate_system<Point<T>> { using type = boost::geometry::cs::cartesian; };

	template <typename T, std::size_t Index>
	struct access<Point<T>, Index> {
		static_assert(Index < 2, "Out of range");
		using CoordinateType = T;
		static inline CoordinateType get(Point<T> const& p) { if constexpr (Index == 0) return p.X(); else return p.Y(); }
		static inline void set(Point<T>& p, CoordinateType const& value) { if constexpr (Index == 0) p.X() = value; else p.Y() = value; }
	};

} //  namespace boost::geometry::traits

// TODO: return_lower_bound obv concepts overloaden

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::ring<P>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::linestring<P>& ls)
{
	for (const auto& p : ls)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::multi_point<P>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

template<typename P>
void MakeLowerBound(P& lb, const boost::geometry::model::polygon<P>& mp)
{
	// asume all inner rings are inside the envelope of the outer ring
	MakeLowerBound(lb, mp.outer());
}

template<typename P, typename Polygon>
void MakeLowerBound(P& lb, const boost::geometry::model::multi_polygon<Polygon>& mp)
{
	for (const auto& p : mp)
		MakeLowerBound(lb, p);
}

bool clean(bg_ring_t& ring)
{
	assert(ring.empty() || ring.front() == ring.back());
	remove_adjacents_and_spikes(ring);
	if (ring.size() < 3)
	{
		ring.clear();
		return false;
	}
	assert(ring.front() != ring.back());
	ring.emplace_back(ring.front());
	return true;
}

void move(bg_ring_t& ring, DPoint displacement)
{
	assert(ring.empty() || ring.front() == ring.back());
	for (auto& p : ring)
		boost::geometry::add_point(p, displacement);
	clean(ring);
}

void move(bg_linestring_t& ls, DPoint displacement)
{
	for (auto& p : ls)
		boost::geometry::add_point(p, displacement);
	if (ls.size() < 2)
		return;
	remove_adjacents(ls);
	dms_assert(!ls.empty());
}

void move(bg_multi_point_t& ms, DPoint displacement)
{
	for (auto& p : ms)
		boost::geometry::add_point(p, displacement);
}

void move(bg_polygon_t& polygon, DPoint displacement)
{
	move(polygon.outer(), displacement);
	if (polygon.outer().empty())
	{
		polygon.inners().clear();
		return;
	}
	for (auto& ir : polygon.inners())
		move(ir, displacement);
}

template<typename P, typename Polygon>
void move(boost::geometry::model::multi_polygon<Polygon>& mp, P displacement)
{
	for (auto& polygon : mp)
		move(polygon, displacement);
}

template <typename P>
bool empty(boost::geometry::model::ring<P>& ring)
{
	if (ring.size() > 3)
		return false;
	if (ring.size() < 3)
		return true;

	dms_assert(ring.size() == 3);
	return ring[0] == ring[2];
}

template <typename DmsPointType>
void assign_polygon(bg_polygon_t& resPoly, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings, bg_ring_t& helperRing)
{
	resPoly.clear(); dms_assert(resPoly.outer().empty() and resPoly.inners().empty());

	boost::polygon::SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
	auto ri = rb;
	//			dbg_assert(ri != re);
	if (ri == re)
		return;

	bool outerOrientation = true;
	for (; ri != re; ++ri)
	{
		dms_assert((*ri).begin() != (*ri).end());
		dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRing.assign((*ri).begin(), (*ri).end());
		if (!clean(helperRing))
		{
			if (ri == rb)
				break;
			else
				continue;
		}

		dms_assert(helperRing.begin() != helperRing.end());
		dms_assert(helperRing.begin()[0] == helperRing.end()[-1]); // closed ?

		bool currOrientation = (boost::geometry::area(helperRing) > 0);
		if (ri == rb)
		{
			resPoly.outer() = helperRing;
			outerOrientation = currOrientation;
		}
		else
		{
			if (outerOrientation == currOrientation)
				// don't start on 2nd polygon
				throwErrorD("assign_polygon", "second ring with same orientation detected as first ring; "
					"consider using an operation that supports multi_polygons (bg_buffer_multi_polygon or outer_multi_polygon)"
				);

			if (mustInsertInnerRings)
				resPoly.inners().emplace_back(helperRing);
		}
	}
}

template <typename DmsPointType>
void assign_multi_polygon(bg_multi_polygon_t& resMP, SA_ConstReference<DmsPointType> polyRef, bool mustInsertInnerRings, bg_polygon_t& helperPolygon, bg_ring_t& helperRing)
{
	resMP.clear();
	
	boost::polygon::SA_ConstRingIterator<DmsPointType>
		rb(polyRef, 0),
		re(polyRef, -1);
	auto ri = rb;
	//			dbg_assert(ri != re);
	if (ri == re)
		return;
	bool outerOrientation = true;
	for (; ri != re; ++ri)
	{
		assert((*ri).begin() != (*ri).end());
		assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRing.assign((*ri).begin(), (*ri).end());
		if (!clean(helperRing))
		{
			continue;
		}

		assert(helperRing.begin() != helperRing.end());
		assert(helperRing.begin()[0] == helperRing.end()[-1]); // closed ?
		bool currOrientation = (boost::geometry::area(helperRing) > 0);
		if (ri == rb || currOrientation == outerOrientation)
		{
			if (ri != rb && !helperPolygon.outer().empty())
				resMP.emplace_back(helperPolygon);
			helperPolygon.clear(); assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
			helperPolygon.outer() = bg_ring_t(helperRing.begin(), helperRing.end());
			outerOrientation = currOrientation;

			// skip outer rings that intersect with a previous outer ring if innerRings are skipped
			if (!mustInsertInnerRings)
			{ 
				SizeT polygonIndex = 0;
				while (polygonIndex < resMP.size())
				{
					auto currPolygon = resMP.begin() + polygonIndex;
					if (boost::geometry::intersects(currPolygon->outer(), helperPolygon.outer()))
					{
						MG_CHECK(!boost::geometry::overlaps(currPolygon->outer(), helperPolygon.outer()));
						if (boost::geometry::within(currPolygon->outer(), helperPolygon.outer()))
						{
							resMP.erase(currPolygon);
							continue;
						}
						MG_CHECK(boost::geometry::within(helperPolygon.outer(), currPolygon->outer()));
						helperPolygon.clear();
						assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
						break;
					}
					polygonIndex++;
				}
			}
		}
		else if (mustInsertInnerRings)
		{
			helperPolygon.inners().emplace_back(helperRing);
		}
	}
	if (!helperPolygon.outer().empty())
		resMP.emplace_back(helperPolygon);
}

template <typename Numeric>
auto sqr(Numeric x)
{
	return x * x;
}

template <typename DmsPointType>
void store_ring(SA_Reference<DmsPointType> resDataElem, const bg_ring_t& ring)
{
	assert(ring.begin()[0] == ring.end()[-1]); // closed ?
	resDataElem.append(ring.begin(), ring.end());
}

template <typename DmsPointType>
void store_multi_polygon(SA_Reference<DmsPointType> resDataElem, bg_multi_polygon_t& resMP, std::vector<DPoint>& ringClosurePoints)
{
	ringClosurePoints.clear();
	for (const auto& resPoly : resMP)
	{
		const auto& resRing = resPoly.outer();
		store_ring(resDataElem, resRing);
		ringClosurePoints.emplace_back(resRing.end()[-1]);

		for (const auto& resLake : resPoly.inners())
		{
			store_ring(resDataElem, resLake);
			ringClosurePoints.emplace_back(resLake.end()[-1]);
		}
	}
	if (!ringClosurePoints.empty())
	{
		ringClosurePoints.pop_back();
		while (!ringClosurePoints.empty())
		{
			resDataElem.emplace_back(ringClosurePoints.back());
			ringClosurePoints.pop_back();
		}
	}
}

// *****************************************************************************
//	simplify
// *****************************************************************************


static CommonOperGroup grBgSimplify_multi_polygon("bg_simplify_multi_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_polygon      ("bg_simplify_polygon", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSimplify_linestring   ("bg_simplify_linestring", oper_policy::better_not_in_meta_scripting);

static CommonOperGroup grBgIntersect("bg_intersect", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgUnion    ("bg_union", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgXOR      ("bg_xor", oper_policy::better_not_in_meta_scripting);
static CommonOperGroup grBgSubtr     ("bg_subtr", oper_policy::better_not_in_meta_scripting);

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
			boost::polygon::SA_ConstRingIterator<PointType>
				rb(polyDataElem, 0),
				re(polyDataElem, -1);
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
			boost::polygon::SA_ConstRingIterator<PointType>
				rb(polyData[i], 0),
				re(polyData[i], -1);
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
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		assert(polyData.size() == resData.size());

		assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

		bg_linestring_t currGeometry, resGeometry;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			auto lb = MaxValue<PointType>();
			for (auto p : polyData[i])
				MakeLowerBound(lb, p);

			currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));
			move(currGeometry, -DPoint(lb));

			resGeometry.resize(0);
			boost::geometry::simplify(currGeometry, resGeometry, maxError);
			move(resGeometry, DPoint(lb));

			auto resDataElem = resData[i];
			resDataElem.reserve(resGeometry.size());
			for (auto p : resGeometry)
				resDataElem.emplace_back(p);
		}
	}
};

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

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem
		, bool e2IsVoid, const AbstrDataItem* bufDistItem, Float64 bufferDistance
		, bool e3IsVoid, const AbstrDataItem* ppcItem, UInt8 pointsPerCircle
		, tile_id t) const override
	{
		dms_assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

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

			std::vector<DPoint> ringClosurePoints;

			bg_linestring_t currGeometry;
			bg_multi_polygon_t resMP;

	nextPointWithSameResRing:

			resMP.clear();
			currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));
			if (!currGeometry.empty())
			{
				auto p = MaxValue<DPoint>();
				MakeLowerBound(p, currGeometry);
				move(currGeometry, -p);

				boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
				move(resMP, p);

				store_multi_polygon(resData[i], resMP, ringClosurePoints);
			}
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
	tl_oper::inst_tuple_templ<typelists::points, SimplifyLinestringOperator> simplifyLineStringOperators;
	tl_oper::inst_tuple_templ<typelists::points, SimplifyMultiPolygonOperator> simplifyMultiPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, SimplifyPolygonOperator> simplifyPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferPointOperator> bufferPointOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferMultiPointOperator> bufferMultiPointOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferLineStringOperator> bufferLineStringOperators;

#if DMS_VERSION_MAJOR < 15
	tl_oper::inst_tuple_templ<typelists::points, BufferSinglePolygonOperator, AbstrOperGroup&> bg_bufferPolygonOperators(grBgBuffer_polygon);
#endif
	tl_oper::inst_tuple_templ<typelists::points, BufferSinglePolygonOperator, AbstrOperGroup&> bg_buffersinglePolygonOperators(grBgBuffer_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, BufferMultiPolygonOperator, AbstrOperGroup&> bg_bufferMultiPolygonOperators(grBgBuffer_multi_polygon);

#if DMS_VERSION_MAJOR < 15
	tl_oper::inst_tuple_templ<typelists::points, OuterSingePolygonOperator, AbstrOperGroup&> outerPolygonOperators(grOuter_polygon);
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator, AbstrOperGroup&> outerMultiPolygonOperators(grOuter_multi_polygon);
#endif
	tl_oper::inst_tuple_templ<typelists::points, OuterSingePolygonOperator, AbstrOperGroup&> bg_outerSinglePolygonOperators(grBgOuter_single_polygon);
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator, AbstrOperGroup&> bg_outerMultiPolygonOperators(grBgOuter_multi_polygon);
}

