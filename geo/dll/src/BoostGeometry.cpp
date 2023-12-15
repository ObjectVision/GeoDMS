//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcTypeLists.h"

#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/TypeListOper.h"

#include "ParallelTiles.h"

#include "Unit.h"
#include "UnitClass.h"

#include <boost/geometry.hpp>

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
	dms_assert(ring.front() == ring.back());
	remove_adjacents_and_spikes(ring);
	if (ring.size() < 3)
	{
		ring.clear();
		return false;
	}
	dms_assert(ring.front() != ring.back());
	ring.emplace_back(ring.front());
	return true;
}

void move(bg_ring_t& ring, DPoint displacement)
{
	dms_assert(ring.front() == ring.back());
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
				break; // don't start on 2nd polygon
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
		dms_assert((*ri).begin() != (*ri).end());
		dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

		helperRing.assign((*ri).begin(), (*ri).end());
		if (!clean(helperRing))
		{
			continue;
		}

		dms_assert(helperRing.begin() != helperRing.end());
		dms_assert(helperRing.begin()[0] == helperRing.end()[-1]); // closed ?
		bool currOrientation = (boost::geometry::area(helperRing) > 0);
		if (ri == rb || currOrientation == outerOrientation)
		{
			if (ri != rb)
				resMP.emplace_back(helperPolygon);
			helperPolygon.clear(); dms_assert(helperPolygon.outer().empty() && helperPolygon.inners().empty());
			helperPolygon.outer() = bg_ring_t(helperRing.begin(), helperRing.end());
			outerOrientation = currOrientation;
		}
		else if (mustInsertInnerRings)
		{
			helperPolygon.inners().emplace_back(helperRing);
		}
	}
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
	dms_assert(ring.begin()[0] == ring.end()[-1]); // closed ?
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


static CommonOperGroup grBgSimplify_multi_polygon("bg_simplify_multi_polygon");
static CommonOperGroup grBgSimplify_polygon      ("bg_simplify_polygon");
static CommonOperGroup grBgSimplify_linestring   ("bg_simplify_linestring");

static CommonOperGroup grBgIntersect("bg_intersect");
static CommonOperGroup grBgUnion    ("bg_union");
static CommonOperGroup grBgXOR      ("bg_xor");
static CommonOperGroup grBgSubtr     ("bg_subtr");

static CommonOperGroup grBgBuffer_point        ("bg_buffer_point");
static CommonOperGroup grBgBuffer_multi_point  ("bg_buffer_multi_point");
static CommonOperGroup grBgBuffer_polygon      ("bg_buffer_polygon");
static CommonOperGroup grBgBuffer_multi_polygon("bg_buffer_multi_polygon");
static CommonOperGroup grBgBuffer_linestring   ("bg_buffer_linestring");

static CommonOperGroup grOuter_polygon("outer_polygon");
static CommonOperGroup grOuter_multi_polygon("outer_multi_polygon");


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
				dms_assert((*ri).begin() != (*ri).end());
				dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

				currRing.assign((*ri).begin(), (*ri).end());
				dms_assert(currRing.begin() != currRing.end());
				dms_assert(currRing.begin()[0] == currRing.end()[-1]); // closed ?
				move(currRing, -DPoint(lb));

				boost::geometry::simplify(currRing, resRing, maxError);
				move(resRing, DPoint(lb));

				if (empty(resRing))
					continue;

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
				dms_assert((*ri).begin() != (*ri).end());
				dms_assert((*ri).begin()[0] == (*ri).end()[-1]); // closed ?

				currRing.assign((*ri).begin(), (*ri).end());
				dms_assert(currRing.begin() != currRing.end());
				dms_assert(currRing.begin()[0] == currRing.end()[-1]); // closed ?
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
		dms_assert(polyData.size() == resData.size());

		dms_assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

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
		dms_assert(args.size() == 3);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		const AbstrDataItem* arg3A = AsDataItem(args[2]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		dms_assert(arg3A);

		const AbstrUnit* domain1Unit = arg1A->GetAbstrDomainUnit(); bool e1IsVoid = domain1Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values1Unit = arg1A->GetAbstrValuesUnit();

		const AbstrUnit* domain2Unit = arg2A->GetAbstrDomainUnit(); bool e2IsVoid = domain2Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* values2Unit = arg2A->GetAbstrValuesUnit();

		const AbstrUnit* domain3Unit = arg3A->GetAbstrDomainUnit(); bool e3IsVoid = domain3Unit->GetValueType() == ValueWrap<Void>::GetStaticClass();
//		const AbstrUnit* values3Unit = arg2A->GetAbstrValuesUnit();

		domain1Unit->UnifyDomain(domain2Unit, "e1", "e2", UnifyMode(UM_Throw | UM_AllowVoidRight));
		domain1Unit->UnifyDomain(domain3Unit, "e1", "e3", UnifyMode(UM_Throw | UM_AllowVoidRight));

		MG_CHECK(e2IsVoid);
		MG_CHECK(e3IsVoid);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(domain1Unit, values1Unit, ValueComposition::Polygon);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			Float64 bufferDistance = const_array_cast<Float64>(arg2A)->GetLockedDataRead()[0];
			UInt8 nrPointsInCircle = const_array_cast<UInt8  >(arg3A)->GetLockedDataRead()[0];
			auto resItem = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(resItem, dms_rw_mode::write_only_mustzero);

			parallel_tileloop(domain1Unit->GetNrTiles(), [this, resObj = resLock.get(), arg1A, bufferDistance, nrPointsInCircle](tile_id t)->void
				{
					ReadableTileLock readPoly1Lock(arg1A->GetCurrRefObj(), t);

					Calculate(resObj, arg1A, bufferDistance, nrPointsInCircle, t);
				}
			);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const = 0;
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

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* pointItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const override
	{
		auto pointData = const_array_cast<PointType>(pointItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(pointData.size() == resData.size());

		boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
		boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::side_straight               sideStrategy;

		std::vector<PointType> ringClosurePoints;
		dms_assert(pointItem->GetValueComposition() == ValueComposition::Single);

		using bg_polygon_t = boost::geometry::model::polygon<DPoint>;

		boost::geometry::model::multi_polygon<bg_polygon_t> resMP;
		boost::geometry::buffer(DPoint(0, 0), resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
		boost::geometry::model::ring<DPoint> resRing = resMP[0].outer(), movedRing;

		for (SizeT i = 0, n = pointData.size(); i != n; ++i)
		{
			movedRing = resRing;
			move(movedRing, DPoint(pointData[i]));
			store_ring(resData[i], movedRing);
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

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
		boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::side_straight               sideStrategy;

		std::vector<DPoint> ringClosurePoints;
		dms_assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

		boost::geometry::model::multi_point<DPoint> currGeometry;
		bg_multi_polygon_t resMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			resMP.clear();
			currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));

			auto p = MaxValue<DPoint>();
			MakeLowerBound(p, currGeometry);
			move(currGeometry, -p);

			boost::geometry::buffer(currGeometry, resMP, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
			move(resMP, p);

			store_multi_polygon(resData[i], resMP, ringClosurePoints);
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

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const override
	{
		dms_assert(polyItem->GetValueComposition() == ValueComposition::Sequence);

		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
		boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::side_straight               sideStrategy;

		std::vector<DPoint> ringClosurePoints;

		bg_linestring_t currGeometry;
		bg_multi_polygon_t resMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			resMP.clear();
			currGeometry.assign(begin_ptr(polyData[i]), end_ptr(polyData[i]));

			auto p = MaxValue<DPoint>();
			MakeLowerBound(p, currGeometry);
			move(currGeometry, -p);

			boost::geometry::buffer(currGeometry, resMP,	distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
			move(resMP, p);

			store_multi_polygon(resData[i], resMP, ringClosurePoints);
		}
	}
};

template <typename P>
struct BufferMultiPolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferMultiPolygonOperator()
		: AbstrBufferOperator(grBgBuffer_multi_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		boost::geometry::strategy::buffer::distance_symmetric<Float64> distStrategy(bufferDistance);
		boost::geometry::strategy::buffer::join_round                  joinStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::end_round                   endStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::point_circle                circleStrategy(pointsPerCircle);
		boost::geometry::strategy::buffer::side_straight               sideStrategy;

		std::vector<DPoint> helperPointArray;
		bg_ring_t helperRing;

		bg_polygon_t helperPolygon;
		bg_multi_polygon_t currMP, resMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_multi_polygon(currMP, polyData[i], true, helperPolygon, helperRing);

			auto lb = MaxValue<DPoint>();
			MakeLowerBound(lb, currMP);
			move(currMP, -lb);

			boost::geometry::buffer(currMP, resMP
				, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
			move(resMP, lb);

			store_multi_polygon(resData[i], resMP, helperPointArray);
		}
	}
};

template <typename P>
struct BufferPolygonOperator : public AbstrBufferOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	BufferPolygonOperator()
		: AbstrBufferOperator(grBgBuffer_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, Float64 bufferDistance, UInt8 pointsPerCircle, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

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

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_polygon(currPoly, polyData[i], true, helperRing);
			if (currPoly.outer().empty())
				continue;

			auto lb = MaxValue<DPoint>();
			MakeLowerBound(lb, currPoly);
			move(currPoly, -lb);

			boost::geometry::buffer(currPoly, resMP
				, distStrategy, sideStrategy, joinStrategy, endStrategy, circleStrategy);
			move(resMP, lb);

			store_multi_polygon(resData[i], resMP, ringClosurePoints);
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

	OuterMultiPolygonOperator()
		: AbstrOuterOperator(grOuter_multi_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resItem, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resItem)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		std::vector<DPoint> ringClosurePoints;
		bg_ring_t currRing;

		using bg_polygon_t = boost::geometry::model::polygon<DPoint>;
		bg_polygon_t currPoly;
		bg_multi_polygon_t currMP, resMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_multi_polygon(currMP, polyData[i], false, currPoly, currRing);
			store_multi_polygon(resData[i], resMP, ringClosurePoints);
		}
	}
};

template <typename P>
struct OuterPolygonOperator : public AbstrOuterOperator
{
	using PointType = P;
	using PolygonType = std::vector<PointType>;
	using Arg1Type = DataArray<PolygonType>;

	OuterPolygonOperator()
		: AbstrOuterOperator(grOuter_polygon, Arg1Type::GetStaticClass())
	{}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* polyItem, tile_id t) const override
	{
		auto polyData = const_array_cast<PolygonType>(polyItem)->GetTile(t);
		auto resData = mutable_array_cast<PolygonType>(resObj)->GetWritableTile(t);
		dms_assert(polyData.size() == resData.size());

		std::vector<DPoint> ringClosurePoints;
		bg_ring_t helperRing;

		bg_polygon_t  currPoly;
		bg_multi_polygon_t resMP;

		for (SizeT i = 0, n = polyData.size(); i != n; ++i)
		{
			assign_polygon(currPoly, polyData[i], false, helperRing);

			store_multi_polygon(resData[i], resMP, ringClosurePoints);
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
	tl_oper::inst_tuple_templ<typelists::points, BufferPolygonOperator> bufferPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, BufferMultiPolygonOperator> bufferMultiPolygonOperators;

	tl_oper::inst_tuple_templ<typelists::points, OuterPolygonOperator> outerPolygonOperators;
	tl_oper::inst_tuple_templ<typelists::points, OuterMultiPolygonOperator> outerMultiPolygonOperators;
}

