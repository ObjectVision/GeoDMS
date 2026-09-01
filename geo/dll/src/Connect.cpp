// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <atomic>
#include <map>

#include "mem/MyContainers.h"

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "geom/GeoDist.h"
#include "geom/SpatialIndex.h"
#include "geom/NeighbourIter.h"
#include "ptr/Resource.h"
#include "set/DataCompare.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexAssigner.h"
#include "OperSignature.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "IndexGetterCreator.h"
#include "LispTreeType.h"

CommonOperGroup cogCONNEIGH("connect_neighbour",   oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCON     ("connect",             oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCCON    ("capacitated_connect", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCONINFO ("connect_info", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTINFO("dist_info", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogIndex   ("spatialIndex", oper_policy::better_not_in_meta_scripting);

CommonOperGroup cogCON_EQ("connect_eq", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCON_NE("connect_ne", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCONINFO_EQ("connect_info_eq", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCONINFO_NE("connect_info_ne", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTINFO_EQ("dist_info_eq", oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTINFO_NE("dist_info_ne", oper_policy::better_not_in_meta_scripting);

using seq_index_type = UInt32;

enum class compare_type {
	none, eq, ne, count
};

// *****************************************************************************
//                          CutInfo for parallel connect
// *****************************************************************************

template <typename PointType, typename R = seq_index_type>
struct CutInfo
{
	row_id   pointIndex = -1;       // source point index (global, across tiles)
	R        arcIndex  = 0;         // original arc being cut
	UInt32   segmIndex = 0;         // segment within arc
	PointType srcPoint;             // the source point itself, so phase 3 need not re-read its tile
	PointType cutPoint;             // exact cut location
	bool     inArc    : 1 = false;  // needs splitting (not just connecting to existing node)
	bool     inSegm   : 1 = false;  // cut point is within segment (not at endpoint)
	bool     foundAny : 1 = false;  // whether a valid connection was found
	Float64  segmFraction = 0.0;    // position along segment [0,1] for deterministic ordering

	// For sorting: first by arc, then by segment, then by position along segment
	bool operator<(const CutInfo& rhs) const
	{
		if (arcIndex != rhs.arcIndex) return arcIndex < rhs.arcIndex;
		if (segmIndex != rhs.segmIndex) return segmIndex < rhs.segmIndex;
		return segmFraction < rhs.segmFraction;
	}
};

template<typename PointType, typename SpatialIndexType>
std::any CreateSpatialIndex(const AbstrOperGroup* og, const AbstrDataItem* arg1A, ResourceHandle& spi)
{
	auto arg1Data = const_array_cast<PointType>(arg1A)->GetDataRead();

	if (!arg1Data.size())
		return {};

	typename DataArray<PointType>::const_iterator
		destBegin= arg1Data.begin(),
		destEnd  = arg1Data.end();

	// test that arg1Data has only unique values
	{
		my_vec_t<PointType> sortedArg1Data(destBegin, destEnd);
		std::sort(sortedArg1Data.begin(), sortedArg1Data.end(), DataLessThanCompare<PointType>());
		if (std::adjacent_find(sortedArg1Data.begin(), sortedArg1Data.end()) != sortedArg1Data.end() )
			og->throwOperError("Multiple destinations with the same location found");
	}

	spi = makeResource<SpatialIndexType>(destBegin, destEnd, 0);
	return std::any(std::move(arg1Data));
}

// *****************************************************************************
//							ConnectNeighbourPointOperator
// *****************************************************************************

struct AbstrConnectNeighbourPointOperator : VariadicOperator
{
	AbstrConnectNeighbourPointOperator(const DataItemClass* argCls, bool withPartitioning)
		:	VariadicOperator(&cogCONNEIGH, AbstrDataItem::GetStaticClass(), withPartitioning ? 2 : 1)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = argCls;
		if (withPartitioning)
			*argClsIter++ = AbstrDataItem::GetStaticClass();
		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// batch E: connect_neighbour(points: attribute<Vp>(D)[; partitioning: attribute<P>(D)])
	// -> attribute<D>(D). The one def-time claim is the domain share between points
	// and partitioning (K1, CreateResult :133 is an unconditional UnifyDomain(UM_Throw),
	// no default/void escape), safe like the batch-D collect_by_cond K1. The value
	// classes stay member-unconstrained (no cross-class claim); the result relation
	// is deferred (§16 opaque-by-ruling).
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		sig_var D = sb.UnitVar("D");
		sb.ArgName(0, "points"); sb.ArgAttr(0, sb.UnitVar("Vp"), D, ValueComposition::Single);
		if (NrSpecifiedArgs() >= 2)
		{
			sb.ArgName(1, "partitioning"); sb.ArgAttr(1, sb.UnitVar("P"), D, ValueComposition::Single);
		}
		sb.ResultDeferred("attribute<D>(D): the nearest-neighbour relation");
		return true;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() >= 1);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = (args.size() >= 2) ? AsDataItem(args[1]) : nullptr;

		const AbstrUnit* pointUnit   = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointDomain = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* neighbourEntity = arg2A ? arg2A->GetAbstrValuesUnit() : nullptr;
		if (arg2A)
			pointDomain->UnifyDomain(arg2A->GetAbstrDomainUnit(), "v1", "e2", UM_Throw);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(pointDomain, pointDomain);
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle spi;
			auto arg1DataHolder = CreateIndex(arg1A, spi);

			if (spi)
			{
				SizeT count = res->GetAbstrDomainUnit()->GetCount();
				if (count)
				{
					std::unique_ptr<const IndexGetter> indexGetter;
					if (arg2A)
						indexGetter.reset( IndexGetterCreator::Create(arg2A, no_tile) );

					IndexAssigner32 indexAssigner(res, resLock.get(), no_tile, 0, count);

					Calculate(spi, indexAssigner.m_Indices, count, arg1A, no_tile, indexGetter.get());

					indexAssigner.Store();
				}
			}
			resLock.Commit();
		}
		return true;
	}

	virtual std::any CreateIndex(const AbstrDataItem* arg1, ResourceHandle& spi) const=0;
	virtual void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const AbstrDataItem* arg1A, tile_id t, const IndexGetter*) const=0;
};

template <typename T>
struct ConnectNeighbourPointOperator : AbstrConnectNeighbourPointOperator
{
	typedef T                              PointType;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef Float64                        SqrDistType;
	typedef Float64                        SqrtDistType;
	typedef Unit<PointType>                PointUnitType;

	typedef DataArray<PointType>           ArgType;

	using SpatialIndexType = SpatialIndex<CoordType, typename ArgType::const_iterator>;

	ConnectNeighbourPointOperator(bool withPartitioning)
		:	AbstrConnectNeighbourPointOperator(ArgType::GetStaticClass(), withPartitioning)
	{}

	std::any CreateIndex(const AbstrDataItem* arg1A, ResourceHandle& spi) const override
	{
		return CreateSpatialIndex<PointType, SpatialIndexType>(GetGroup(), arg1A, spi);
	}

	// Override Operator
	void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const AbstrDataItem* arg1A, tile_id t, const IndexGetter* indexGetter) const override
	{
		const ArgType* argPoints = const_array_cast<PointType>(arg1A);

		dms_assert(argPoints);
		dms_assert(resSize);

		SpatialIndexType& spIndex = GetAs<SpatialIndexType>(spi);
				
		auto pointData = argPoints->GetLockedDataRead(t);
		dms_assert(pointData.size() == resSize);

		auto destBegin = spIndex.first_leaf();
		dms_assert(pointData.begin() == destBegin);

		neighbour_iter<SpatialIndexType> iter(&spIndex);

		for (SizeT i=0; i!=resSize; ++i)
		{
			SizeT clusterID = i;
			if (indexGetter)
				clusterID = indexGetter->Get(i);

			const PointType& point = pointData[i];

			if (!IsDefined(point))
			{
				resBuffer[i] = UNDEFINED_VALUE(UInt32);
				continue;
			}
			typename DataArray<PointType>::const_iterator foundDestPointPtr = nullptr;

			iter.Reset(point);

			SizeT foundDestIndex = UNDEFINED_VALUE(SizeT);
			for (; iter; ++iter)
			{
				typename ArgType::const_iterator othPointPtr = iter.CurrLeaf().Value();
				SizeT thatIndex = othPointPtr - destBegin;

				SizeT thatClusterID = thatIndex;
				if (indexGetter)
					thatClusterID = indexGetter->Get(thatClusterID);

				if (clusterID != thatClusterID)
				{
					foundDestIndex = thatIndex;
					break;
				}
			}

			// store results 
			resBuffer[i] = foundDestIndex;
		}
	}
};

// *****************************************************************************
//									ConnectPointInfoOperator
// *****************************************************************************

struct AbstrConnectPointOperator : VariadicOperator
{
	bool isCapacitated;

	AbstrConnectPointOperator(const DataItemClass* argCls, bool isCapacitated_, compare_type CT)
		:	VariadicOperator(
				isCapacitated_ ? &cogCCON : &cogCON
			,	AbstrDataItem::GetStaticClass()
			,	2 + ( isCapacitated_ ? 2 : 0 ) + ( CT!=compare_type::none ? 2 : 0 )
			)
		,	isCapacitated(isCapacitated_)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = argCls;
		if (isCapacitated) *argClsIter++ = AbstrDataItem::GetStaticClass();
		if (CT != compare_type::none) *argClsIter++ = AbstrDataItem::GetStaticClass();

		*argClsIter++ = argCls;
		if (isCapacitated) *argClsIter++ = AbstrDataItem::GetStaticClass();
		if (CT != compare_type::none) *argClsIter++ = AbstrDataItem::GetStaticClass();
		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	// batch E: connect(point1: attribute<Vc>(D1); point2: attribute<Vc>(D2)) -> attribute<D1>(D2),
	// and capacitated_connect(point1; weight1; point2; weight2). Faithful, safe claims only:
	// the two coordinate value classes share (K16, CreateResult :300 is a plain UnifyValues(UM_Throw)
	// with NO default escape -- unlike union -- so class equality is a sound def-time under-approximation
	// of the class+metric reduction check), and the capacitated weights share their point domain (:303,
	// :304) and each other's value class (:306) -- all unconditional UM_Throw. The result relation is
	// deferred (§16). GUARD: skip the mis-registered eq/ne members (4-arg non-capacitated, 6-arg
	// capacitated) -- their CreateResult never implements the compare-key contract (it asserts 2||4 args
	// and treats any 4-arg as capacitated), so they must not be described.
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index n = NrSpecifiedArgs();
		bool describable = (n == 2 && !isCapacitated) || (n == 4 && isCapacitated);
		if (!describable)
			return false;
		sig_var Vc = sb.UnitVar("Vc"), D1 = sb.UnitVar("D1"), D2 = sb.UnitVar("D2");
		if (!isCapacitated)
		{
			sb.ArgName(0, "point1"); sb.ArgAttr(0, Vc, D1, ValueComposition::Single);
			sb.ArgName(1, "point2"); sb.ArgAttr(1, Vc, D2, ValueComposition::Single); // shared Vc: K16 coord class
		}
		else
		{
			sig_var Vw = sb.UnitVar("Vw");
			sb.ArgName(0, "point1");  sb.ArgAttr(0, Vc, D1, ValueComposition::Single);
			sb.ArgName(1, "weight1"); sb.ArgAttr(1, Vw, D1, ValueComposition::Single); // weight1.domain == point1.domain (:303)
			sb.ArgName(2, "point2");  sb.ArgAttr(2, Vc, D2, ValueComposition::Single); // shared Vc (:300)
			sb.ArgName(3, "weight2"); sb.ArgAttr(3, Vw, D2, ValueComposition::Single); // shared Vw (:306), weight2.domain == point2.domain (:304)
		}
		sb.ResultDeferred("attribute<D1>(D2): the point-connection relation");
		return true;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		bool isCapacitated = args.size() == 4;
		dms_assert(args.size() == 2 || isCapacitated);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg1W = isCapacitated ? AsDataItem(args[1]) : nullptr;
		const AbstrDataItem* arg2A = AsDataItem(args[isCapacitated ? 2 : 1]);
		const AbstrDataItem* arg2W = isCapacitated ? AsDataItem(args[3]) : nullptr;

		const AbstrUnit* point1Unit   = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* point2Unit   = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* point1Entity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* point2Entity = arg2A->GetAbstrDomainUnit();

		arg1A->GetAbstrValuesUnit()->UnifyValues(arg2A->GetAbstrValuesUnit(), "v1", isCapacitated ? "v3" : "v2", UM_Throw);
		if (isCapacitated)
		{
			point1Entity->UnifyDomain(arg1W->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			point2Entity->UnifyDomain(arg2W->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);

			arg1W->GetAbstrValuesUnit()->UnifyValues(arg2W->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(point2Entity, point1Entity);
			resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			ResourceHandle spi;
			auto arg1DataHolder = CreateIndex(arg1A, spi);

			std::unique_ptr<WeightGetter> weights1Getter(arg1W ? WeightGetterCreator::Create(arg1W) : nullptr);
			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item, also for an intermediate result

			parallel_tileloop(point2Entity->GetNrTiles(), [res, resObj = resLock.get(), arg2A, arg2W, &spi, &weights1Getter, &itemRef, this](tile_id t)->void
			{
				SizeT tileSize = resObj->GetTiledRangeData()->GetTileSize(t);
				if (!tileSize)
					return;

				IndexAssigner32 indexAssigner(res, resObj, t, 0, tileSize);
				if (!spi)
				{
					fast_undefine(indexAssigner.m_Indices, indexAssigner.m_Indices + tileSize);
				}
				else
				{
					std::unique_ptr<WeightGetter> weights2Getter(arg2W ? WeightGetterCreator::Create(arg2W, t) : nullptr);
					Calculate(spi, indexAssigner.m_Indices, tileSize, weights1Getter.get(), arg2A, t, weights2Getter.get(), itemRef.c_str());
				}

				indexAssigner.Store();
			});

			resLock.Commit();
		}
		return true;
	}

	virtual std::any CreateIndex(const AbstrDataItem* arg1, ResourceHandle& spi) const=0;
	virtual void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const WeightGetter* weights1, const AbstrDataItem* arg2A, tile_id t, const WeightGetter* weights2, CharPtr itemRef = "") const=0;
};

template <typename T, typename E = UInt32>
struct ConnectPointOperator : AbstrConnectPointOperator
{
	typedef T                              PointType;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef Float64                        SqrDistType;
	typedef Float64                        SqrtDistType;
	typedef Unit<PointType>                PointUnitType;

	typedef DataArray<PointType>           ArgType;

	typedef SpatialIndex<CoordType, typename ArgType::const_iterator> SpatialIndexType;

	ConnectPointOperator(bool isCapacitated, compare_type ct)
		:	AbstrConnectPointOperator(ArgType::GetStaticClass(), isCapacitated, ct)
	{}

	std::any CreateIndex(const AbstrDataItem* arg1A, ResourceHandle& spi) const override
	{
		return CreateSpatialIndex<PointType, SpatialIndexType>(GetGroup(), arg1A, spi);
	}

	// Override Operator
	void Calculate(ResourceHandle& spi, UInt32* resBuffer, SizeT resSize, const WeightGetter* weights1, const AbstrDataItem* arg2A, tile_id t, const WeightGetter* weights2, CharPtr itemRef = "") const override
	{
		Timer processTimer;

		const ArgType* arg2 = const_array_cast<PointType>(arg2A);

		assert(arg2);
		assert(resSize);
		assert(spi);

		SpatialIndexType& spIndex = GetAs<SpatialIndexType>(spi);
				
		auto arg2Data = arg2->GetLockedDataRead(t);
		dms_assert(arg2Data.size() == resSize);

		auto
			point2Begin = arg2Data.begin(),
			point2End   = arg2Data.end();

		auto
			destBegin = spIndex.first_leaf();

		auto reporter = [&processTimer, point2Begin, point2End, t, tn = arg2A->GetAbstrDomainUnit()->GetNrTiles(), itemRef](auto i) {
			if (processTimer.PassedSecs())
				reportF(SeverityTypeID::ST_MajorTrace, "{}Connect {} / {} points of tile {} / {} done"
					, itemRef
					, AsString(i), AsString(point2End - point2Begin)
					, AsString(t), AsString(tn));
		};

		parallel_for_if_separable<SizeT, UInt32>(0, point2End - point2Begin, [point2Begin, weights1, weights2, destBegin, resBuffer, &spIndex, &reporter](auto i)
		{
			if (IsDefined(point2Begin[i]))
			{
				Float64 minWeight = 0;
				if (weights1)
				{
					dms_assert(weights2);
					minWeight = weights2->Get(i);
				}
				neighbour_iter<SpatialIndexType> iter(&spIndex);
				iter.Reset(point2Begin[i]);
				for (; iter; ++iter)
				{
					SizeT pointIndex = (*iter) - destBegin;
					if (!weights1 || weights1->Get(pointIndex) >= minWeight)
					{
						resBuffer[i] = pointIndex;
						goto nextPoint;
					}
				}
			}
			resBuffer[i] = UNDEFINED_VALUE(UInt32);

		nextPoint:
			reporter(i);
		}
		);
	}
};

// *****************************************************************************
//									IndexedArcProjectionHandle
// *****************************************************************************

template <typename R, typename T, typename ResObjectPtr>
struct IndexedArcProjectionHandle : ArcProjectionHandleWithDist<R, T>
{
	template <typename SpatialIndexType>
	IndexedArcProjectionHandle(const Point<T>* p, const SpatialIndexType& spIndex, const R* optionalMaxSqrDistPtr, bool isPossiblyMultiPolygon)
		: ArcProjectionHandleWithDist<R, T>(p, spIndex.template GetSqrProximityUpperBound<R>(*p, 0xFFFFFFFF, optionalMaxSqrDistPtr), isPossiblyMultiPolygon)
	{
		assert(spIndex.size());		
		for (auto iter = spIndex.begin(Inflate(*p, Point<T>(this->m_Dist, this->m_Dist))); iter; ++iter)
		{
			ResObjectPtr streetPtr = (*iter)->get_ptr();
			if (Project2Arc(begin_ptr(*streetPtr), end_ptr(*streetPtr)))
			{
				this->m_ArcPtr = streetPtr;
				iter.RefineSearch( Inflate(*p, Point<T>(this->m_Dist, this->m_Dist)) );
			}
		}

		assert(!this->m_ArcPtr.is_null() || !this->m_FoundAny);
	}

	template <typename SpatialIndexType, typename Filter>
	IndexedArcProjectionHandle(Point<T> p, const SpatialIndexType& spIndex,  const Filter& filter, const R* optionalMaxSqrDistPtr, bool isPossiblyMultiPolygon)
	{
		UInt32 maxDepth = 0xFFFFFFFF;
		while (true) {
	
			ArcProjectionHandleWithDist<R, T> aph(p, spIndex.template GetSqrProximityUpperBound<R>(p, maxDepth, optionalMaxSqrDistPtr), isPossiblyMultiPolygon);
			assert(!aph.m_FoundAny);
			for (auto iter = spIndex.begin(Inflate(p, Point<T>(aph.m_Dist, aph.m_Dist))); iter; ++iter)
			{
				ResObjectPtr streetPtr = (*iter)->get_ptr();
				if (!filter(streetPtr))
					continue;
				if (aph.Project2Arc(begin_ptr(*streetPtr), end_ptr(*streetPtr)))
				{
					this->m_ArcPtr = streetPtr;
					iter.RefineSearch( Inflate(p, Point<T>(aph.m_Dist, aph.m_Dist)) );
				}
			}
			if (aph.m_FoundAny || !maxDepth)
			{
				assert(!this->m_ArcPtr.is_null());
				ArcProjectionHandleWithDist<R, T>::operator =(aph);
				break;
			}
			assert(!this->m_ArcPtr.is_null() || !this->m_FoundAny);
		}
	}

	ResObjectPtr m_ArcPtr;
};

// *****************************************************************************
//							IndexedPointProjectionHandle
// *****************************************************************************

// The search box of a reversed connect (#1228): the arc's bounding box grown by
// dist. The growing is done in Float64 and clamped to the coordinate type BEFORE
// the cast back, so an unbounded (widening round) distance cannot overflow an
// integer coordinate type.
//
// One extra unit is added on either side because the index tests a POINT leaf
// half-open -- IsIntersecting(Range, Point) admits the lower bound but not the
// upper one, so a point exactly on the upper edge would never be seen. The box
// only preselects candidates; each of them is still measured, so a box that is
// one unit too generous costs nothing.
template <typename T, typename R>
Range<Point<T>> InflatedSearchBox(const Range<Point<T>>& box, R dist)
{
	auto d = Float64(dist);
	auto lo = [](Float64 v) { return Max<Float64>(Float64(MinValue<T>()), v - 1.0); };
	auto hi = [](Float64 v) { return Min<Float64>(Float64(MaxValue<T>()), v + 1.0); };

	return Range<Point<T>>
	(	Point<T>(T(lo(std::floor(Float64(box.first .first ) - d))), T(lo(std::floor(Float64(box.first .second) - d))))
	,	Point<T>(T(hi(std::ceil (Float64(box.second.first ) + d))), T(hi(std::ceil (Float64(box.second.second) + d))))
	);
}

template <typename R>
R SqrtBet(R sqrDist)
{
	if (!(sqrDist > 0))
		return R(0);
	return SafeBet(sqrt(sqrDist));
}

// For ONE arc, the nearest of the indexed POINTS, measured to the nearest
// location on that arc: the #1228 mirror of IndexedArcProjectionHandle, which
// takes one point and searches an index of arcs. Here the spatial index is over
// the points, so the search box is the arc's bounding box inflated by the
// running best distance, narrowed (RefineSearch) as that distance drops.
//
// The seed radius is the tightest GetSqrProximityUpperBound over the arc's own
// vertices: the index guarantees a point within that radius of the vertex it was
// taken at, and that point lies in the inflated box, so ONE round finds
// something -- unless a maxSqrDist cuts the radius short (then there is nothing
// to be found within the allowed distance anyway), or an eq/ne filter rejects
// every candidate, which is what the widening rounds are for.
template <typename R, typename T>
struct IndexedPointProjectionHandle
{
	using PointType = Point<T>;
	using RectType = Range<PointType>;
	using ConstPointPtr = const PointType*;

	bool      m_FoundAny = false, m_InArc = false, m_InSegm = false;
	seq_elem_index_type m_SegmIndex = UNDEFINED_VALUE(seq_elem_index_type);
	PointType m_CutPoint;
	R         m_MinSqrDist = MaxValue<R>();
	R         m_Dist = 0;
	SizeT     m_PointIndex = UNDEFINED_VALUE(SizeT);

	template <typename SpatialIndexType, typename Filter>
	IndexedPointProjectionHandle(ConstPointPtr arcBegin, ConstPointPtr arcEnd, const RectType& arcBox
		, const SpatialIndexType& spIndex, ConstPointPtr pointBegin, const Filter& filter
		, const R* optionalMaxSqrDistPtr, bool isPossiblyMultiPolygon)
	{
		auto capBox = spIndex.GetBoundingBox();
		if (arcBegin == arcEnd || arcBox.inverted() || capBox.inverted())
			return; // an empty arc, or an index without a single defined point

		R sqrBound = optionalMaxSqrDistPtr ? *optionalMaxSqrDistPtr : MaxValue<R>();
		for (auto vertexPtr = arcBegin; vertexPtr != arcEnd; ++vertexPtr)
		{
			if (!IsDefined(*vertexPtr))
				continue; // a multi-linestring separator
			UInt32 maxDepth = 0xFFFFFFFF;
			MakeMin(sqrBound, spIndex.template GetSqrProximityUpperBound<R>(*vertexPtr, maxDepth, optionalMaxSqrDistPtr));
		}

		while (true) // m_FoundAny ends the loop, so sqrBound is this round's bound
		{
			auto searchBox = InflatedSearchBox<T>(arcBox, SqrtBet(sqrBound));
			bool isExhaustive = IsIncluding(searchBox, capBox);
			if (!searchBox.inverted())
				for (auto iter = spIndex.begin(searchBox); iter; ++iter)
				{
					ConstPointPtr pointPtr = (*iter)->get_ptr();
					if (!filter(pointPtr))
						continue;
					ArcProjectionHandleWithDist<R, T> aph(*pointPtr, m_FoundAny ? m_MinSqrDist : sqrBound, isPossiblyMultiPolygon);
					if (!aph.Project2Arc(arcBegin, arcEnd))
						continue;
					if (m_FoundAny && !(aph.m_MinSqrDist < m_MinSqrDist))
						continue; // a tie keeps the point with the lowest index, as connect does
					m_FoundAny   = true;
					m_InArc      = aph.m_InArc;
					m_InSegm     = aph.m_InSegm;
					m_SegmIndex  = aph.m_SegmIndex;
					m_CutPoint   = aph.m_CutPoint;
					m_MinSqrDist = aph.m_MinSqrDist;
					m_Dist       = aph.m_Dist;
					m_PointIndex = pointPtr - pointBegin;
					// monotone in the distance, which only drops, so the refinement is
					// always included in what the iterator still holds
					iter.RefineSearch(InflatedSearchBox<T>(arcBox, SqrtBet(m_MinSqrDist)));
				}
			if (m_FoundAny || isExhaustive || optionalMaxSqrDistPtr)
				break;
			// the filter rejected every candidate the seeded box held: widen. A zero
			// (or NaN) bound cannot be quadrupled into progress, so it goes all the
			// way -- which makes the next round the exhaustive one.
			if (sqrBound > 0 && sqrBound < MaxValue<R>() / 4)
				sqrBound *= 4;
			else
				sqrBound = MaxValue<R>();
		}
	}
};

// *****************************************************************************
//									ConnectInfoOperator
// *****************************************************************************

static StaticLateTokenID s_Dist("dist");
static StaticLateTokenID s_PointRel("point_rel"); // #1228: the reversed counterpart of token::arc_rel
static StaticLateTokenID s_ArcID("ArcID");
static StaticLateTokenID s_CutPoint("CutPoint");
static StaticLateTokenID s_InArc("InArc");
static StaticLateTokenID s_InSegm("InSegm");
static StaticLateTokenID s_SegmID("SegmID");

template <compare_type CT, bool HasMaxDist, bool HasMinDist>
using ConnectInfoBaseType = std::conditional_t < CT == compare_type::none,
	std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, QuaternaryOperator, TernaryOperator>, BinaryOperator>
	, std::conditional_t<HasMaxDist, std::conditional_t<HasMinDist, SexenaryOperator, QuinaryOperator>, QuaternaryOperator>>;

template <typename P, typename E = UInt32, compare_type CT = compare_type::none, typename SegmID = UInt32, typename SqrtDistType = Float64, bool HasMaxDist = false, bool HasMinDist = false, bool OnlyDistResult = false, bool Reversed = false>
class ConnectInfoOperator : ConnectInfoBaseType<CT, HasMaxDist, HasMinDist>
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	typedef Range<P>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef SqrtDistType                   SqrDistType;
	typedef Unit<PointType>                PointUnitType;
	typedef Unit<SqrtDistType>             DistUnitType;
	typedef Unit<Bool>                     BoolUnitType;
	typedef Unit<UInt32>                   SegmUnitType;

	typedef DataArray<PolygonType>         Arg1Type;
	typedef DataArray<PointType>           Arg2Type;

	typedef DataArray<SqrtDistType>        ResSubType1; // distance
//	typedef DataArray<E>                   ResSubType2; // arc-id
	typedef DataArray<PointType>           ResSubType3; // cut-point
	typedef DataArray<Bool>                ResSubType4; // in-arc
	typedef DataArray<Bool>                ResSubType5; // in-semg
	typedef DataArray<SegmID>              ResSubType6; // segm-id

	using SpatialIndexType = SpatialIndex<CoordType, typename Arg1Type::const_iterator>;
	using PointIndexType = SpatialIndex<CoordType, typename Arg2Type::const_iterator>;

	static auto cogInfo() { return OnlyDistResult ? &cogDISTINFO : &cogCONINFO; }
	static const Class* ResultCls()
	{
		if (OnlyDistResult)
			return  ResSubType1::GetStaticClass();
		return TreeItem::GetStaticClass();
	}

	// #1228: the reversed members take the points first and the arcs second, so
	// that each arc gets the nearest of the points instead of the other way round
	static const DataItemClass* GeoArgCls1() { return Reversed ? Arg2Type::GetStaticClass() : Arg1Type::GetStaticClass(); }
	static const DataItemClass* GeoArgCls2() { return Reversed ? Arg1Type::GetStaticClass() : Arg2Type::GetStaticClass(); }

public:
	ConnectInfoOperator()
		requires(CT == compare_type::none && !HasMinDist && !HasMaxDist)
		:	BinaryOperator(cogInfo(), ResultCls()
			,	GeoArgCls1(), GeoArgCls2()
			)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::none && !HasMinDist && HasMaxDist)
	:	TernaryOperator(cogInfo(), ResultCls()
			,	GeoArgCls1(), GeoArgCls2()
			,	DataArray<SqrtDistType>::GetStaticClass()
			)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::none && HasMinDist && HasMaxDist)
	:	QuaternaryOperator(cogInfo(), ResultCls()
			,	GeoArgCls1(), GeoArgCls2()
			,	DataArray<SqrtDistType>::GetStaticClass(), DataArray<SqrtDistType>::GetStaticClass()
			)
	{}


	ConnectInfoOperator()
		requires(CT == compare_type::eq && !HasMinDist && !HasMaxDist)
	:	QuaternaryOperator(OnlyDistResult ? &cogDISTINFO_EQ : &cogCONINFO_EQ, ResultCls()
			,	GeoArgCls1(), DataArray<E>::GetStaticClass()
			,	GeoArgCls2(), DataArray<E>::GetStaticClass()
			)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::eq && !HasMinDist && HasMaxDist)
	: QuinaryOperator(OnlyDistResult ? &cogDISTINFO_EQ : &cogCONINFO_EQ, ResultCls()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrtDistType>::GetStaticClass()
		)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::eq && HasMinDist && HasMaxDist)
	: SexenaryOperator(OnlyDistResult ? &cogDISTINFO_EQ : &cogCONINFO_EQ, ResultCls()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrtDistType>::GetStaticClass(), DataArray<SqrtDistType>::GetStaticClass()
		)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::ne && !HasMinDist && !HasMaxDist)
	:	QuaternaryOperator(OnlyDistResult ? &cogDISTINFO_NE : &cogCONINFO_NE, ResultCls()
			,	GeoArgCls1(), DataArray<E>::GetStaticClass()
			,	GeoArgCls2(), DataArray<E>::GetStaticClass()
			)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::ne && !HasMinDist && HasMaxDist)
	: QuinaryOperator(OnlyDistResult ? &cogDISTINFO_NE : &cogCONINFO_NE, ResultCls()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrtDistType>::GetStaticClass()
		)
	{}

	ConnectInfoOperator()
		requires(CT == compare_type::ne && HasMinDist && HasMaxDist)
	: SexenaryOperator(OnlyDistResult ? &cogDISTINFO_NE : &cogCONINFO_NE, ResultCls()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrtDistType>::GetStaticClass(), DataArray<SqrtDistType>::GetStaticClass()
		)
	{}

	// batch F: connect_info / dist_info (+ _eq/_ne, +maxdist/+mindist), deferred
	// from batch E. Mirrors the shipped FastConnect describe: the arc geometry
	// (Sequence|Polygon), the eq/ne join keys and the void-broadcasting distances
	// stay deferred prose; the points position carries fresh (single-use) vars --
	// no cross-argument unification claim. The one faithful upgrade is
	// dist_info's RESULT (OnlyDistResult): CreateResult below unconditionally
	// builds CreateCacheDataItem(pointEntity, the metric-LESS default dist unit)
	// -- ResultAttr(DefaultUnit, Dp) states the K3 domain identity and the class,
	// both discharged by CheckResultItem at reduction.
	// connect_info's container result is printer prose (ResultContainer).
	// #1228: a REVERSED member describes the same positions in the other order,
	// and defers its result -- everything it builds is keyed by the ARC domain,
	// which sits at a deferred position, so there is no variable to state that
	// domain identity in.
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index n = this->NrSpecifiedArgs(), i = 0; // this-> : dependent base in the ConnectInfo template
		sig_var Dp = sb.UnitVar("Dp"), Vpt = sb.UnitVar("Vpt");
		auto describeArcs = [&]
		{
			sb.ArgName(i, "arcs"); sb.ArgDeferred(i, "arc geometry (Sequence or Polygon)"); ++i;
			if (CT != compare_type::none) { sb.ArgName(i, "arcKey"); sb.ArgDeferred(i, "arc join key"); ++i; }
		};
		auto describePoints = [&]
		{
			if (auto ptCls = dynamic_cast<const DataItemClass*>(this->GetArgClass(i)))
				sb.MemberValueClass(Vpt, ptCls->GetValuesType());
			sb.ArgName(i, "points"); sb.ArgAttr(i, Vpt, Dp, ValueComposition::Single); ++i;
			if (CT != compare_type::none) { sb.ArgName(i, "pointKey"); sb.ArgDeferred(i, "point join key (shares values with arcKey)"); ++i; }
		};
		if constexpr (Reversed) { describePoints(); describeArcs(); }
		else                    { describeArcs(); describePoints(); }
		for (; i < n; ++i) { sb.ArgName(i, "distance"); sb.ArgDeferred(i, "max/min distance (may be a void-domain parameter)"); }
		sb.DeferredRelation("the arc and point coordinates share one value class (K16); eq/ne join keys share values");
		if constexpr (Reversed)
			sb.ResultDeferred(OnlyDistResult
				? "attribute(Da): the distance from each arc to the nearest of the points"
				: "container(Da): dist, point_rel, CutPoint, InArc, InSegm and SegmID, all keyed by the arc domain");
		else if (OnlyDistResult)
			sb.ResultAttr(sb.DefaultUnit(DataArray<SqrtDistType>::GetStaticClass()->GetValuesType()), Dp, ValueComposition::Single);
		else
			{
				// §12.8 slSubItemCall tranche: connect_info's CONTAINER members,
				// exactly as CreateResult builds them (:755-760) plus the
				// deprecated ArcID alias created at meta time (:765) -- ALL keyed by
				// the points' domain Dp (K3). Values: dist the metric-less default
				// dist class; CutPoint the point coordinate class (Vpt, class-level
				// -- a values-only var); InArc/InSegm Bool; SegmID UInt32; arc_rel
				// and ArcID hold the arc entity (a deferred arg, no var) so their
				// values stay unclaimed. The set is COMPLETE: CreateResult builds
				// exactly these, unconditionally. connect_info is cacheable, so
				// connect_info(...)/dist both types AND inline-reduces in a body.
				sig_var Dd = sb.DefaultUnit(DataArray<SqrtDistType>::GetStaticClass()->GetValuesType());
				sig_var Bf = sb.UnitVar("InFlag"); sb.FixedValueClass(Bf, DataArray<Bool>::GetStaticClass()->GetValuesType());
				sig_var Sg = sb.UnitVar("SegmId"); sb.FixedValueClass(Sg, DataArray<UInt32>::GetStaticClass()->GetValuesType());
				sb.ResultContainerMember("dist",     Dd,         Dp, ValueComposition::Single);
				sb.ResultContainerMember("arc_rel",  no_sig_var, Dp, ValueComposition::Single);
				sb.ResultContainerMember("ArcID",    no_sig_var, Dp, ValueComposition::Single);
				sb.ResultContainerMember("CutPoint", Vpt,        Dp, ValueComposition::Single);
				sb.ResultContainerMember("InArc",    Bf,         Dp, ValueComposition::Single);
				sb.ResultContainerMember("InSegm",   Bf,         Dp, ValueComposition::Single);
				sb.ResultContainerMember("SegmID",   Sg,         Dp, ValueComposition::Single);
				sb.ResultMembersComplete();
			}
		return true;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		UInt32 argCount = 0;

		const AbstrDataItem* argFstA = AsDataItem(args[argCount++]);
		const AbstrDataItem* argFst_ID = (CT != compare_type::none) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argSndA = AsDataItem(args[argCount++]);
		const AbstrDataItem* argSnd_ID = (CT != compare_type::none) ? AsDataItem(args[argCount++]) : nullptr;

		const AbstrDataItem* argMaxDist = (HasMaxDist) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argMinDist = (HasMinDist) ? AsDataItem(args[argCount++]) : nullptr;
		assert(args.size() == argCount);

		// #1228: a reversed member is given the points first and the arcs second;
		// from here on arg1A is the arc geometry and arg2A the points, whichever
		// position they came from.
		const AbstrDataItem* arg1A   = Reversed ? argSndA   : argFstA;
		const AbstrDataItem* arg1_ID = Reversed ? argSnd_ID : argFst_ID;
		const AbstrDataItem* arg2A   = Reversed ? argFstA   : argSndA;
		const AbstrDataItem* arg2_ID = Reversed ? argFst_ID : argSnd_ID;

		const AbstrUnit* polyUnit    = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointUnit   = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* polyEntity  = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointEntity = arg2A->GetAbstrDomainUnit();

		// the entity that gets a row of results: each point for a forward member,
		// each arc for a reversed one, with the relation pointing the other way
		const AbstrUnit* resEntity = Reversed ? polyEntity  : pointEntity;
		const AbstrUnit* relEntity = Reversed ? pointEntity : polyEntity;

		polyUnit->UnifyValues (pointUnit, "Values of polygon attribute", "Values of point attribute", UM_Throw);

		if (CT != compare_type::none)
		{
			polyEntity ->UnifyDomain(arg1_ID->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			pointEntity->UnifyDomain(arg2_ID->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);
			arg1_ID->GetAbstrValuesUnit()->UnifyValues(arg2_ID->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}
		if (HasMinDist)
			resEntity->UnifyDomain(argMinDist->GetAbstrDomainUnit(), "Domain of connected attribute", "Domain of Minimum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if (HasMaxDist)
			resEntity->UnifyDomain(argMaxDist->GetAbstrDomainUnit(), "Domain of connected attribute", "Domain of Maximum Distances", UnifyMode(UM_Throw| UM_AllowVoidRight));

		bool hasNonVoidMinDist = HasMinDist && !(argMinDist->HasVoidDomainGuarantee());
		bool hasNonVoidMaxDist = HasMaxDist && !(argMaxDist->HasVoidDomainGuarantee());

		const DistUnitType* distUnit = const_unit_cast<SqrtDistType>(DistUnitType::GetStaticClass()->CreateDefault());
		if (!resultHolder)
		{
			if (OnlyDistResult)
				resultHolder = CreateCacheDataItem(resEntity, distUnit);
			else
				resultHolder = TreeItem::CreateCacheRoot();
		}
		const BoolUnitType* boolUnit = OnlyDistResult ? nullptr : const_unit_cast<Bool  >( BoolUnitType::GetStaticClass()->CreateDefault() );
		const SegmUnitType* segmUnit = OnlyDistResult ? nullptr : const_unit_cast<UInt32>( SegmUnitType::GetStaticClass()->CreateDefault() );


		// the two static token types are unrelated, so name the TokenID first
		TokenID relNameID = Reversed ? static_cast<TokenID>(s_PointRel) : static_cast<TokenID>(token::arc_rel);

		AbstrDataItem* resSub1 = OnlyDistResult ? AsDataItem(resultHolder.GetNew()) : CreateDataItem(resultHolder.GetNew(), s_Dist, resEntity, distUnit).get(); // owned by resultHolder
		AbstrDataItem* resSub2 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder.GetNew(), relNameID, resEntity, relEntity).get(); // owned by resultHolder
		AbstrDataItem* resSub3 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder.GetNew(), s_CutPoint, resEntity, pointUnit ).get(); // owned by resultHolder
		AbstrDataItem* resSub4 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder.GetNew(), s_InArc,    resEntity, boolUnit  ).get(); // owned by resultHolder
		AbstrDataItem* resSub5 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder.GetNew(), s_InSegm,   resEntity, boolUnit  ).get(); // owned by resultHolder
		AbstrDataItem* resSub6 = OnlyDistResult ? nullptr : CreateDataItem(resultHolder.GetNew(), s_SegmID,   resEntity, segmUnit  ).get(); // owned by resultHolder

		if (resSub2 && !mustCalc)
		{
			resSub2->SetTSF(TSF_Categorical);
			if constexpr (!Reversed) // ArcID is the legacy name of arc_rel; point_rel never had one
			{
				auto resNrOrg_depreciated = CreateDataItem(resultHolder.GetNew(), s_ArcID, resEntity, relEntity);
				resNrOrg_depreciated->SetTSF(TSF_Categorical);
				resNrOrg_depreciated->SetTSF(TSF_Depreciated);
				resNrOrg_depreciated->SetReferredItem(resSub2);
			}
		}
		if (mustCalc)
		{
			Timer processTimer;
			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item, also for an intermediate result

			const Arg1Type* arg1 = const_array_cast<PolygonType>(arg1A);
			const Arg2Type* arg2 = const_array_cast<  PointType>(arg2A);

			bool isPossiblyMultiPolygon = arg1A->GetValueComposition() == ValueComposition::Polygon;

			assert(arg1);
			assert(arg2);

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg1_IdLock(arg1_ID);
			DataReadLock arg2_IdLock(arg2_ID);
			DataReadLock argMinDistLock(argMinDist);
			DataReadLock argMaxDistLock(argMaxDist);

			SizeT arg1Count = polyEntity->GetCount();
			SizeT arg2Count = pointEntity->GetCount();
			std::atomic<SizeT> nrArg2 = 0;

			DataWriteLock res1Lock(resSub1);
			DataWriteLock res2Lock(resSub2);
			DataWriteLock res3Lock(resSub3);
			DataWriteLock res4Lock(resSub4);
			DataWriteLock res5Lock(resSub5);
			DataWriteLock res6Lock(resSub6);

			if constexpr (!Reversed)
			{
			auto arg1Data = arg1->GetLockedDataRead();
			assert(arg1Count == arg1Data.size());
			SpatialIndexType spIndex(arg1Data.begin(), arg1Data.end(), 0);

			const E* polyIDsPtr = nullptr;
			typename DataArray<E>::locked_cseq_t polyIDs;  if (arg1_ID) { polyIDs = const_array_cast<E>(arg1_ID)->GetLockedDataRead(); polyIDsPtr = polyIDs.begin(); }

			parallel_tileloop(pointEntity->GetNrTiles(), [&, isPossiblyMultiPolygon, this](tile_id t)
				{
					auto arg2Data = arg2->GetLockedDataRead(t);

					const E* pointIDsPtr = nullptr;
					const SqrtDistType* minSqrDistPtr = nullptr;
					const SqrtDistType* maxSqrDistPtr = nullptr;
					typename DataArray<E>::locked_cseq_t pointIDs; if (arg2_ID) { pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead(t); pointIDsPtr = pointIDs.begin(); }
					typename DataArray<SqrtDistType>::locked_cseq_t minSqrDists; if (argMinDist) { minSqrDists = const_array_cast<SqrtDistType>(argMinDist)->GetLockedDataRead(hasNonVoidMinDist ? t : 0); minSqrDistPtr = minSqrDists.begin(); }
					typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists; if (argMaxDist) { maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0); maxSqrDistPtr = maxSqrDists.begin(); }

					auto arg2DataSize = arg2Data.size();
					if (!arg2DataSize)
						return;

					auto data1 = mutable_array_cast<SqrtDistType>(res1Lock)->GetWritableTile(t); auto r1 = data1.begin();

					AbstrDataObject* ado2 = OnlyDistResult ? nullptr : res2Lock.get();

					std::optional<WritableTileLock> arcIdDataLock;
					if (!OnlyDistResult)
						arcIdDataLock = WritableTileLock(ado2, t, dms_rw_mode::write_only_all);

					typename ResSubType3::locked_seq_t data3; typename ResSubType3::iterator r3;
					typename ResSubType4::locked_seq_t data4; typename ResSubType4::iterator r4;
					typename ResSubType5::locked_seq_t data5; typename ResSubType5::iterator r5;
					typename ResSubType6::locked_seq_t data6; typename ResSubType6::iterator r6;
					if (!OnlyDistResult)
					{
						data3 = mutable_array_cast<PointType>(res3Lock)->GetWritableTile(t); r3 = data3.begin();
						data4 = mutable_array_cast<Bool>     (res4Lock)->GetWritableTile(t); r4 = data4.begin();
						data5 = mutable_array_cast<Bool>     (res5Lock)->GetWritableTile(t); r5 = data5.begin();
						data6 = mutable_array_cast<SegmID>   (res6Lock)->GetWritableTile(t); r6 = data6.begin();
					}
					if (!arg1Count)
					{
						fast_fill(r1, r1 + arg2DataSize, MAX_VALUE(SqrtDistType)); //dist
						if (!OnlyDistResult)
						{
							ado2->FillWithUInt32Values(tile_loc(t, 0), arg2DataSize, UNDEFINED_VALUE(UInt32));
							fast_undefine(r3, r3 + arg2DataSize); // cut-point
							fast_undefine(r6, r6 + arg2DataSize); // segm-id
						}
					}
					else
					{
						auto streetBegin = arg1Data.begin();

						const PointType* pointBegin = arg2Data.begin();
						const PointType* pointEnd = arg2Data.end();
						const PointType* pointPtr = pointBegin;

						auto filter = [=, &pointPtr](typename Arg1Type::const_iterator streetPtr) ->bool
						{
							if constexpr (CT == compare_type::none)
								return true;
							else
							{
								seq_index_type streetIndex = streetPtr - streetBegin;
								dms_assert(streetIndex < arg1Count);
								E pointID = pointIDsPtr[pointPtr - pointBegin];
								if constexpr (CT == compare_type::eq)
								{
									if (pointID == polyIDsPtr[streetIndex])
										return true;
								}
								else
								{
									static_assert(CT == compare_type::ne);
									if (pointID != polyIDsPtr[streetIndex])
										return true;
								}
								//						return ((CT == compare_type::eq) == (pointID == polyIDsPtr[streetIndex])) || !IsDefined(pointID);
								return !IsDefined(pointID);
							}
						};

						SizeT nrUnreportedPoints = 0;
						SizeT currRow = 0;
						for (; pointPtr != pointEnd; ++r1, ++pointPtr)
						{
							auto point = *pointPtr;
							if (IsDefined(point))
							{
								IndexedArcProjectionHandle<SqrDistType, CoordType, typename Arg1Type::const_iterator> arcHnd(point, spIndex, filter, maxSqrDistPtr, isPossiblyMultiPolygon);
								if (arcHnd.m_FoundAny)
								{
									if (!maxSqrDistPtr || *maxSqrDistPtr > arcHnd.m_MinSqrDist)
										*r1 = Convert<SqrtDistType>(arcHnd.m_Dist);
									else
										MakeUndefined(*r1);
									if (!OnlyDistResult)
									{
										ado2->SetValueAsSizeT(currRow, arcHnd.m_ArcPtr - streetBegin, t);
										*r3 = arcHnd.m_CutPoint;
										*r4 = arcHnd.m_InArc;
										*r5 = arcHnd.m_InSegm;
										*r6 = arcHnd.m_SegmIndex;
									}
									goto pointProcessingCompleted;
								}
							}

							*r1 = MAX_VALUE(SqrtDistType);
							if (!OnlyDistResult)
							{
								ado2->SetValueAsSizeT(currRow, UNDEFINED_VALUE(SizeT), t);
								MakeUndefined(*r3);
								MakeUndefined(*r6);
							}

						pointProcessingCompleted:
							if (hasNonVoidMinDist)
								++minSqrDistPtr;
							if (hasNonVoidMaxDist)
								++maxSqrDistPtr;
							if (!OnlyDistResult)
							{
								++currRow;
								++r3, ++r4, ++r5, ++r6;
							}
							++nrUnreportedPoints;
							if (processTimer.PassedSecs())
							{
								nrArg2 += nrUnreportedPoints;
								nrUnreportedPoints = 0;
								reportF(SeverityTypeID::ST_MajorTrace, "{}{} {} / {} points done"
									, itemRef.c_str()
									, this->GetGroup()->GetName()
									, AsString(nrArg2), AsString(arg2Count));
							}
						}
						nrArg2 += nrUnreportedPoints;
					}
				});
			}
			else
			{
				// #1228: the index is over the POINTS and each ARC picks the nearest of
				// them, so here the points are read whole and the arcs per tile -- the
				// exact mirror of the forward branch above.
				auto arg2Data = arg2->GetLockedDataRead();
				assert(arg2Count == arg2Data.size());
				PointIndexType spIndex(arg2Data.begin(), arg2Data.end(), 0);

				const E* pointIDsPtr = nullptr;
				typename DataArray<E>::locked_cseq_t pointIDs;  if (arg2_ID) { pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead(); pointIDsPtr = pointIDs.begin(); }

				parallel_tileloop(polyEntity->GetNrTiles(), [&, isPossiblyMultiPolygon, this](tile_id t)
					{
						auto arcData = arg1->GetLockedDataRead(t);

						const E* polyIDsPtr = nullptr;
						const SqrtDistType* minSqrDistPtr = nullptr;
						const SqrtDistType* maxSqrDistPtr = nullptr;
						typename DataArray<E>::locked_cseq_t polyIDs; if (arg1_ID) { polyIDs = const_array_cast<E>(arg1_ID)->GetLockedDataRead(t); polyIDsPtr = polyIDs.begin(); }
						typename DataArray<SqrtDistType>::locked_cseq_t minSqrDists; if (argMinDist) { minSqrDists = const_array_cast<SqrtDistType>(argMinDist)->GetLockedDataRead(hasNonVoidMinDist ? t : 0); minSqrDistPtr = minSqrDists.begin(); }
						typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists; if (argMaxDist) { maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0); maxSqrDistPtr = maxSqrDists.begin(); }

						auto arcDataSize = arcData.size();
						if (!arcDataSize)
							return;

						auto data1 = mutable_array_cast<SqrtDistType>(res1Lock)->GetWritableTile(t); auto r1 = data1.begin();

						AbstrDataObject* ado2 = OnlyDistResult ? nullptr : res2Lock.get();

						std::optional<WritableTileLock> pointRelDataLock;
						if (!OnlyDistResult)
							pointRelDataLock = WritableTileLock(ado2, t, dms_rw_mode::write_only_all);

						typename ResSubType3::locked_seq_t data3; typename ResSubType3::iterator r3;
						typename ResSubType4::locked_seq_t data4; typename ResSubType4::iterator r4;
						typename ResSubType5::locked_seq_t data5; typename ResSubType5::iterator r5;
						typename ResSubType6::locked_seq_t data6; typename ResSubType6::iterator r6;
						if (!OnlyDistResult)
						{
							data3 = mutable_array_cast<PointType>(res3Lock)->GetWritableTile(t); r3 = data3.begin();
							data4 = mutable_array_cast<Bool>     (res4Lock)->GetWritableTile(t); r4 = data4.begin();
							data5 = mutable_array_cast<Bool>     (res5Lock)->GetWritableTile(t); r5 = data5.begin();
							data6 = mutable_array_cast<SegmID>   (res6Lock)->GetWritableTile(t); r6 = data6.begin();
						}
						if (!arg2Count)
						{
							fast_fill(r1, r1 + arcDataSize, MAX_VALUE(SqrtDistType)); //dist
							if (!OnlyDistResult)
							{
								ado2->FillWithUInt32Values(tile_loc(t, 0), arcDataSize, UNDEFINED_VALUE(UInt32));
								fast_undefine(r3, r3 + arcDataSize); // cut-point
								fast_undefine(r6, r6 + arcDataSize); // segm-id
							}
							return;
						}

						const PointType* pointBegin = arg2Data.begin();
						E arcID = UNDEFINED_VALUE(E);

						// the key of the feature that is being connected -- here the arc --
						// waives the comparison when it is null, as the point key does in
						// the forward direction
						auto filter = [=, &arcID](const PointType* pointPtr) ->bool
						{
							if constexpr (CT == compare_type::none)
								return true;
							else
							{
								SizeT pointIndex = pointPtr - pointBegin;
								assert(pointIndex < arg2Count);
								if constexpr (CT == compare_type::eq)
								{
									if (arcID == pointIDsPtr[pointIndex])
										return true;
								}
								else
								{
									static_assert(CT == compare_type::ne);
									if (arcID != pointIDsPtr[pointIndex])
										return true;
								}
								return !IsDefined(arcID);
							}
						};

						SizeT nrUnreportedArcs = 0;
						for (SizeT i = 0; i != arcDataSize; ++i, ++r1)
						{
							auto arcRef = arcData[i];
							if constexpr (CT != compare_type::none)
								arcID = polyIDsPtr[i];

							auto arcBegin = begin_ptr(arcRef);
							auto arcEnd = end_ptr(arcRef);
							auto arcBox = RangeFromSequence_SkipUndefined(arcBegin, arcEnd);

							IndexedPointProjectionHandle<SqrDistType, CoordType> pntHnd(arcBegin, arcEnd, arcBox, spIndex, pointBegin, filter, maxSqrDistPtr, isPossiblyMultiPolygon);
							if (pntHnd.m_FoundAny)
							{
								if (!maxSqrDistPtr || *maxSqrDistPtr > pntHnd.m_MinSqrDist)
									*r1 = Convert<SqrtDistType>(pntHnd.m_Dist);
								else
									MakeUndefined(*r1);
								if (!OnlyDistResult)
								{
									ado2->SetValueAsSizeT(i, pntHnd.m_PointIndex, t);
									*r3 = pntHnd.m_CutPoint;
									*r4 = pntHnd.m_InArc;
									*r5 = pntHnd.m_InSegm;
									*r6 = pntHnd.m_SegmIndex;
								}
							}
							else
							{
								*r1 = MAX_VALUE(SqrtDistType);
								if (!OnlyDistResult)
								{
									ado2->SetValueAsSizeT(i, UNDEFINED_VALUE(SizeT), t);
									MakeUndefined(*r3);
									*r4 = false;
									*r5 = false;
									MakeUndefined(*r6);
								}
							}
							if (hasNonVoidMinDist)
								++minSqrDistPtr;
							if (hasNonVoidMaxDist)
								++maxSqrDistPtr;
							if (!OnlyDistResult)
								++r3, ++r4, ++r5, ++r6;
							++nrUnreportedArcs;
							if (processTimer.PassedSecs())
							{
								nrArg2 += nrUnreportedArcs;
								nrUnreportedArcs = 0;
								reportF(SeverityTypeID::ST_MajorTrace, "{}{} {} / {} arcs done"
									, itemRef.c_str()
									, this->GetGroup()->GetName()
									, AsString(nrArg2), AsString(arg1Count));
							}
						}
						nrArg2 += nrUnreportedArcs;
					});
			}
			res1Lock.Commit();
			if (res2Lock) res2Lock.Commit();
			if (res3Lock) res3Lock.Commit();
			if (res4Lock) res4Lock.Commit();
			if (res5Lock) res5Lock.Commit();
			if (res6Lock) res6Lock.Commit();
			if (!OnlyDistResult)
				resultHolder->SetIsInstantiated();
		}
		return true;
	}
};

// *****************************************************************************
//									FastConnectOperator
// *****************************************************************************

template <class T, class R = seq_index_type, compare_type CT = compare_type::none, typename E= UInt32, typename SqrtDistType = Float64, bool HasMaxDist = false, bool HasMinDist = false, bool Reversed = false>
class FastConnectOperator : ConnectInfoBaseType<CT, HasMaxDist, HasMinDist>
{
	using PointType = T;
	using PolygonType = sequence_traits<PointType>::container_type;
	typedef Range<T>                       RangeType;
	typedef typename PointType::field_type CoordType;
	typedef SqrtDistType                   SqrDistType;
	typedef Unit<PointType>                PointUnitType;

	typedef Unit<R>                    ResultUnitType;
	typedef DataArray<PolygonType>     ResultSubType;
	typedef DataArray<PolygonType>     Arg1Type;
	typedef DataArray<PointType>       Arg2Type;

	typedef SpatialIndex<CoordType, sequence_array_index<PointType> > SpatialIndexType;
	using PointIndexType = SpatialIndex<CoordType, typename Arg2Type::const_iterator>;

	// #1228: the reversed members take the points first and the arcs second, so
	// that each arc gets the nearest of the points instead of the other way round
	static const DataItemClass* GeoArgCls1() { return Reversed ? Arg2Type::GetStaticClass() : Arg1Type::GetStaticClass(); }
	static const DataItemClass* GeoArgCls2() { return Reversed ? Arg1Type::GetStaticClass() : Arg2Type::GetStaticClass(); }

	// Position along the cut segment, for the deterministic ordering of multiple
	// cuts on one arc.
	//
	// !inSegm and inArc  => the cut is on the segment END vertex arc[segmIndex+1] -> 1.0.
	// !inSegm and !inArc => the cut is on the arc's terminal/begin vertex -> 0.0 (and such
	// cuts are excluded from cutsPerArc, so this only orders correctly-by-construction).
	//
	// The old 'inArc ? 0.0 : 1.0' put the END-vertex cut at 0.0, mis-sorting it
	// before co-segment interior cuts; processed end-to-beginning the interior cut
	// overwrote that vertex, collapsing the vertex cut to tailSize==1 (skipped), so
	// the cut point never became a node and the connector dead-ended (#1138/#1136).
	template <typename ArcRef>
	static Float64 CalcSegmFraction(ArcRef arc, const CutInfo<PointType, R>& ci)
	{
		if (!ci.inSegm)
			return ci.inArc ? 1.0 : 0.0;
		if (ci.segmIndex + 1 >= arc.size())
			return 1.0;
		auto p1 = arc[ci.segmIndex];
		auto p2 = arc[ci.segmIndex + 1];
		auto segLen = sqrt(SqrDist<Float64>(p1, p2));
		return (segLen > 0) ? sqrt(SqrDist<Float64>(p1, ci.cutPoint)) / segLen : 0.0;
	}

public:
	FastConnectOperator()
		requires(CT == compare_type::none && !HasMinDist && !HasMaxDist)
	:	BinaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			,	GeoArgCls1()
			,	GeoArgCls2()
			)
	{}
	FastConnectOperator()
		requires(CT == compare_type::none && !HasMinDist && HasMaxDist)
	: TernaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			, GeoArgCls1()
			, GeoArgCls2()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}
	FastConnectOperator()
		requires(CT == compare_type::none && HasMinDist && HasMaxDist)
	: QuaternaryOperator(&cogCON, ResultUnitType::GetStaticClass()
			, GeoArgCls1()
			, GeoArgCls2()
			, DataArray<SqrDistType>::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}

	FastConnectOperator()
		requires(CT == compare_type::eq && !HasMinDist && !HasMaxDist)
	:	QuaternaryOperator(&cogCON_EQ, ResultUnitType::GetStaticClass()
			,	GeoArgCls1(), DataArray<E>::GetStaticClass()
			,	GeoArgCls2(), DataArray<E>::GetStaticClass()
		)
	{}
	FastConnectOperator()
		requires(CT == compare_type::eq && !HasMinDist && HasMaxDist)
	: QuinaryOperator(&cogCON_EQ, ResultUnitType::GetStaticClass()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}
	FastConnectOperator()
		requires(CT == compare_type::ne && !HasMinDist && !HasMaxDist)
	: QuaternaryOperator(&cogCON_NE, ResultUnitType::GetStaticClass()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
		)
	{}
	FastConnectOperator()
		requires(CT == compare_type::ne && !HasMinDist && HasMaxDist)
	: QuinaryOperator(&cogCON_NE, ResultUnitType::GetStaticClass()
			, GeoArgCls1(), DataArray<E>::GetStaticClass()
			, GeoArgCls2(), DataArray<E>::GetStaticClass()
			, DataArray<SqrDistType>::GetStaticClass()
		)
	{}

	// batch E: connect / connect_eq / connect_ne (arc -> network). This is a
	// FRESH-UNIT operator (K6): CreateResult builds a new Unit<UInt32> result
	// carrying geometry + arc_rel sub-items (:970-977). The one faithful def-time
	// claim is that fresh result unit (ResultUnit(GeneratedUnit) -- the proven-safe
	// batch-D pattern: a fresh flexible node, LispPtr-memoized, never rigid). The
	// arc geometry (composition Sequence|Polygon, not member-fixed), the K16
	// coordinate-class share (:955, plain UM_Throw), the eq/ne join keys (:960),
	// and the void-broadcasting min/max distances (:963,:965, UM_AllowVoidRight)
	// are recorded as deferred prose (§16 opaque) -- no cross-argument unification.
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index n = this->NrSpecifiedArgs(), i = 0; // this-> : dependent base in the FastConnect template
		auto describeArcs = [&]
		{
			sb.ArgName(i, "arcs"); sb.ArgDeferred(i, "arc geometry (Sequence or Polygon)"); ++i;
			if (CT != compare_type::none) { sb.ArgName(i, "arcKey"); sb.ArgDeferred(i, "arc join key"); ++i; }
		};
		auto describePoints = [&]
		{
			sb.ArgName(i, "points"); sb.ArgAttr(i, sb.UnitVar("Vpt"), sb.UnitVar("Dp"), ValueComposition::Single); ++i; // fresh vars: no cross-arg claim
			if (CT != compare_type::none) { sb.ArgName(i, "pointKey"); sb.ArgDeferred(i, "point join key (shares values with arcKey)"); ++i; }
		};
		if constexpr (Reversed) { describePoints(); describeArcs(); } // #1228
		else                    { describeArcs(); describePoints(); }
		for (; i < n; ++i) { sb.ArgName(i, "distance"); sb.ArgDeferred(i, "max/min distance (may be a void-domain parameter)"); }
		sb.DeferredRelation("the arc and point coordinates share one value class (K16); eq/ne join keys share values");
		sig_var U = sb.GeneratedUnit("connected_network");
		sb.ResultUnit(U);
		// §12.7 slSubItemCall tranche: the two sub-items CreateResult makes.
		// Their values units (the arc coordinates / the arcs' domain) belong to
		// DEFERRED positions, so only the domain identity and the geometry's
		// member-fixed Sequence composition are claimed
		sb.ResultContainerMember("geometry", no_sig_var, U, ValueComposition::Sequence);
		sb.ResultContainerMember("arc_rel", no_sig_var, U, ValueComposition::Single);
		sb.ResultMembersComplete();
		return true;
	}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		arg_index argCount = 0;

		const AbstrDataItem* argFstA = debug_valcast<const AbstrDataItem*>(args[argCount++]);
		const AbstrDataItem* argFst_ID = (CT != compare_type::none) ? debug_valcast<const AbstrDataItem*>(args[argCount++]) : nullptr;
		const AbstrDataItem* argSndA = debug_valcast<const AbstrDataItem*>(args[argCount++]);
		const AbstrDataItem* argSnd_ID = (CT != compare_type::none) ? debug_valcast<const AbstrDataItem*>(args[argCount++]) : nullptr;

		const AbstrDataItem* argMaxDist = (HasMaxDist) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argMinDist = (HasMinDist) ? AsDataItem(args[argCount++]) : nullptr;
		dms_assert(args.size() == argCount);

		// #1228: a reversed member is given the points first and the arcs second;
		// from here on arg1A is the arc geometry and arg2A the points, whichever
		// position they came from.
		const AbstrDataItem* arg1A   = Reversed ? argSndA   : argFstA;
		const AbstrDataItem* arg1_ID = Reversed ? argSnd_ID : argFst_ID;
		const AbstrDataItem* arg2A   = Reversed ? argFstA   : argSndA;
		const AbstrDataItem* arg2_ID = Reversed ? argFst_ID : argSnd_ID;

		const AbstrUnit* polyUnit = arg1A->GetAbstrValuesUnit();
		const AbstrUnit* pointUnit = arg2A->GetAbstrValuesUnit();
		const AbstrUnit* polyEntity = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* pointEntity = arg2A->GetAbstrDomainUnit();

		// the entity whose features each get one connection: each point for a
		// forward member, each arc for a reversed one
		const AbstrUnit* resEntity = Reversed ? polyEntity : pointEntity;
		polyUnit->UnifyValues(pointUnit, "polygon coordinates", "points", UM_Throw);
		if (CT != compare_type::none)
		{
			polyEntity->UnifyDomain(arg1_ID->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			pointEntity->UnifyDomain(arg2_ID->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);
			arg1_ID->GetAbstrValuesUnit()->UnifyValues(arg2_ID->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}
		if (HasMinDist)
			resEntity->UnifyDomain(argMinDist->GetAbstrDomainUnit(), "Domain of connected features", "Domain of Minimum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if (HasMaxDist)
			resEntity->UnifyDomain(argMaxDist->GetAbstrDomainUnit(), "Domain of connected features", "Domain of Maximum Distances", UnifyMode(UM_Throw | UM_AllowVoidRight));

		bool hasNonVoidMinDist = HasMinDist && !(argMinDist->HasVoidDomainGuarantee());
		bool hasNonVoidMaxDist = HasMaxDist && !(argMaxDist->HasVoidDomainGuarantee());

		auto resDomain_owner = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder.GetNew());
		ResultUnitType* resDomain = mutable_unit_cast<R>(resDomain_owner.get());
		dms_assert(resDomain);
		bool createNewResult = !resultHolder;
		resultHolder = resDomain;

		AbstrDataItem* resSub   = CreateDataItem(resDomain, token::geometry, resDomain, polyUnit, ValueComposition::Sequence).get(); // owned by resDomain
		AbstrDataItem* resNrOrg = CreateDataItem(resDomain, token::arc_rel, resDomain, arg1A->GetAbstrDomainUnit()).get(); // owned by resDomain

		resNrOrg->SetTSF(TSF_Categorical);

		MG_PRECONDITION(resSub);

		if (mustCalc)
		{
			Timer processTimer;
			auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item, also for an intermediate result

			bool isPossiblyMultiPolygon = arg1A->GetValueComposition() == ValueComposition::Polygon;

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg1IDLock(arg1_ID);
			DataReadLock arg2IDLock(arg2_ID);
			DataReadLock argMinDistLock(argMinDist);
			DataReadLock argMaxDistLock(argMaxDist);

			R arg1Count = arg1A->GetAbstrDomainUnit()->GetCount(); 
			R arg2Count = arg2A->GetAbstrDomainUnit()->GetCount();

			if(!arg1Count) 
			{
				resDomain->SetCount(0);
				DataWriteLock(resSub  ).Commit();
				DataWriteLock(resNrOrg).Commit();
				return true;
			}

			// ============================================================
			// PHASE 0: Copy original arcs and build read-only spatial index
			// ============================================================

			auto arg1Data = const_array_cast<PolygonType>(arg1A)->GetLockedDataRead();
			assert(arg1Count == arg1Data.size());

			const E* polyIDsPtr = nullptr;
			typename DataArray<E>::locked_cseq_t polyIDs;
			if (arg1_ID) {
				polyIDs = const_array_cast<E>(arg1_ID)->GetLockedDataRead();
				polyIDsPtr = polyIDs.begin();
			}

			// ============================================================
			// PHASE 1: Parallel Discovery - find cut info for each point
			//          (#1228, for a reversed member: for each arc)
			// ============================================================

			using CutInfoType = CutInfo<PointType, R>;
			my_vec_t<CutInfoType> allCutInfos;
			std::atomic<SizeT> nrProcessedPoints = 0;

			if constexpr (!Reversed)
			{
			using OriginalSpatialIndexType = SpatialIndex<CoordType, typename Arg1Type::const_iterator>;
			OriginalSpatialIndexType spIndexOriginal(arg1Data.begin(), arg1Data.end(), 0);

			tile_id nrTiles = arg2A->GetAbstrDomainUnit()->GetNrTiles();

			// Per-tile cut info vectors: the inner vectors jointly hold one CutInfo per input point,
			// which is this phase's real working set -- census-visible via my_vec_t.
			std::vector<my_vec_t<CutInfoType>> perTileCutInfos(nrTiles);
			std::vector<SizeT> tileOffsets(nrTiles + 1, 0);

			// Calculate tile offsets for global point indexing
			for (tile_id t = 0; t < nrTiles; ++t)
				tileOffsets[t + 1] = tileOffsets[t] + arg2A->GetAbstrDomainUnit()->GetTileCount(t);

			parallel_tileloop(nrTiles, [&, isPossiblyMultiPolygon, this](tile_id t)
			{
				auto arg2Data = const_array_cast<PointType>(arg2A)->GetLockedDataRead(t);
				auto tileSize = arg2Data.size();
				if (!tileSize)
					return;

				const E* pointIDsPtr = nullptr;
				typename DataArray<E>::locked_cseq_t pointIDs;
				if (arg2_ID) {
					pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead(t);
					pointIDsPtr = pointIDs.begin();
				}

				const SqrDistType* maxSqrDistPtr = nullptr;
				typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists;
				if (argMaxDist) {
					maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0);
					maxSqrDistPtr = maxSqrDists.begin();
				}

				auto streetBegin = arg1Data.begin();
				row_id globalOffset = tileOffsets[t];

				my_vec_t<CutInfoType>& tileCutInfos = perTileCutInfos[t];
				tileCutInfos.reserve(tileSize);

				for (SizeT i = 0; i < tileSize; ++i)
				{
					auto point = arg2Data[i];
					CutInfoType cutInfo;
					cutInfo.pointIndex = globalOffset + i;
					cutInfo.foundAny = false;

					if (IsDefined(point))
					{
						auto filter = [&](typename Arg1Type::const_iterator streetPtr) -> bool
						{
							if constexpr (CT == compare_type::none)
								return true;
							else
							{
								R streetIndex = streetPtr - streetBegin;
								assert(streetIndex < arg1Count);
								E pointID = pointIDsPtr[i];
								if constexpr (CT == compare_type::eq)
									return pointID == polyIDsPtr[streetIndex] || !IsDefined(pointID);
								else
									return pointID != polyIDsPtr[streetIndex] || !IsDefined(pointID);
							}
						};

						const SqrDistType* currMaxDistPtr = hasNonVoidMaxDist ? (maxSqrDistPtr + i) : maxSqrDistPtr;
						IndexedArcProjectionHandle<SqrDistType, CoordType, typename Arg1Type::const_iterator> arcHnd(
							point, spIndexOriginal, filter, currMaxDistPtr, isPossiblyMultiPolygon);

						if (arcHnd.m_FoundAny)
						{
							cutInfo.foundAny = true;
							cutInfo.arcIndex = arcHnd.m_ArcPtr - streetBegin;
							cutInfo.segmIndex = arcHnd.m_SegmIndex;
							cutInfo.srcPoint = point;
							cutInfo.cutPoint = arcHnd.m_CutPoint;
							cutInfo.inArc = arcHnd.m_InArc;
							cutInfo.inSegm = arcHnd.m_InSegm;

							// Calculate fraction along segment for deterministic ordering
							MG_CHECK(cutInfo.arcIndex < arg1Count);
							cutInfo.segmFraction = CalcSegmFraction(arg1Data[cutInfo.arcIndex], cutInfo);
						}
					}
					tileCutInfos.push_back(cutInfo);
					nrProcessedPoints += 1;

					if (processTimer.PassedSecs())
					{
						reportF(SeverityTypeID::ST_MajorTrace, "{}Connect discovery: {} / {} points done"
							, itemRef.c_str()
							, AsString(nrProcessedPoints.load()), AsString(arg2Count));
					}
				}
			});

			allCutInfos.reserve(arg2Count);
			for (tile_id t = 0; t < nrTiles; ++t)
				for (auto& ci : perTileCutInfos[t])
					allCutInfos.push_back(ci);
			}
			else
			{
				// #1228: the index is over the POINTS and each ARC picks the nearest of
				// them, so exactly one cut is discovered per arc.
				auto arg2Data = const_array_cast<PointType>(arg2A)->GetLockedDataRead();
				assert(arg2Count == arg2Data.size());
				PointIndexType spIndexPoints(arg2Data.begin(), arg2Data.end(), 0);

				const E* pointIDsPtr = nullptr;
				typename DataArray<E>::locked_cseq_t pointIDs;
				if (arg2_ID) {
					pointIDs = const_array_cast<E>(arg2_ID)->GetLockedDataRead();
					pointIDsPtr = pointIDs.begin();
				}

				tile_id nrArcTiles = arg1A->GetAbstrDomainUnit()->GetNrTiles();
				std::vector<my_vec_t<CutInfoType>> perTileCutInfos(nrArcTiles);
				std::vector<SizeT> tileOffsets(nrArcTiles + 1, 0);
				for (tile_id t = 0; t < nrArcTiles; ++t)
					tileOffsets[t + 1] = tileOffsets[t] + arg1A->GetAbstrDomainUnit()->GetTileCount(t);

				parallel_tileloop(nrArcTiles, [&, isPossiblyMultiPolygon, this](tile_id t)
				{
					auto arcData = const_array_cast<PolygonType>(arg1A)->GetLockedDataRead(t);
					auto tileSize = arcData.size();
					if (!tileSize)
						return;

					const SqrDistType* maxSqrDistPtr = nullptr;
					typename DataArray<SqrtDistType>::locked_cseq_t maxSqrDists;
					if (argMaxDist) {
						maxSqrDists = const_array_cast<SqrtDistType>(argMaxDist)->GetLockedDataRead(hasNonVoidMaxDist ? t : 0);
						maxSqrDistPtr = maxSqrDists.begin();
					}

					const PointType* pointBegin = arg2Data.begin();
					R globalOffset = tileOffsets[t];
					E arcID = UNDEFINED_VALUE(E);

					// the key of the feature that is being connected -- here the arc --
					// waives the comparison when it is null, as the point key does in the
					// forward direction
					auto filter = [&](const PointType* pointPtr) -> bool
					{
						if constexpr (CT == compare_type::none)
							return true;
						else
						{
							SizeT pointIndex = pointPtr - pointBegin;
							assert(pointIndex < arg2Count);
							if constexpr (CT == compare_type::eq)
								return arcID == pointIDsPtr[pointIndex] || !IsDefined(arcID);
							else
								return arcID != pointIDsPtr[pointIndex] || !IsDefined(arcID);
						}
					};

					my_vec_t<CutInfoType>& tileCutInfos = perTileCutInfos[t];
					tileCutInfos.reserve(tileSize);

					for (SizeT i = 0; i < tileSize; ++i)
					{
						auto arcRef = arcData[i];
						CutInfoType cutInfo;
						cutInfo.arcIndex = globalOffset + i;
						cutInfo.foundAny = false;

						if constexpr (CT != compare_type::none)
							arcID = polyIDsPtr[cutInfo.arcIndex];

						auto arcBegin = begin_ptr(arcRef);
						auto arcEnd = end_ptr(arcRef);
						auto arcBox = RangeFromSequence_SkipUndefined(arcBegin, arcEnd);
						const SqrDistType* currMaxDistPtr = hasNonVoidMaxDist ? (maxSqrDistPtr + i) : maxSqrDistPtr;

						IndexedPointProjectionHandle<SqrDistType, CoordType> pntHnd(
							arcBegin, arcEnd, arcBox, spIndexPoints, pointBegin, filter, currMaxDistPtr, isPossiblyMultiPolygon);

						if (pntHnd.m_FoundAny)
						{
							cutInfo.foundAny = true;
							cutInfo.pointIndex = pntHnd.m_PointIndex;
							cutInfo.segmIndex = pntHnd.m_SegmIndex;
							cutInfo.srcPoint = pointBegin[pntHnd.m_PointIndex];
							cutInfo.cutPoint = pntHnd.m_CutPoint;
							cutInfo.inArc = pntHnd.m_InArc;
							cutInfo.inSegm = pntHnd.m_InSegm;

							MG_CHECK(cutInfo.arcIndex < arg1Count);
							cutInfo.segmFraction = CalcSegmFraction(arg1Data[cutInfo.arcIndex], cutInfo);
						}
						tileCutInfos.push_back(cutInfo);
						nrProcessedPoints += 1;

						if (processTimer.PassedSecs())
						{
							reportF(SeverityTypeID::ST_MajorTrace, "{}Connect discovery: {} / {} arcs done"
								, itemRef.c_str()
								, AsString(nrProcessedPoints.load()), AsString(arg1Count));
						}
					}
				});

				allCutInfos.reserve(arg1Count);
				for (tile_id t = 0; t < nrArcTiles; ++t)
					for (auto& ci : perTileCutInfos[t])
						allCutInfos.push_back(ci);
			}

			// ============================================================
			// PHASE 2: Consolidation - group cuts by arc, sort, assign indices
			// ============================================================

			R nrValidConnections = 0;
			for (const auto& ci : allCutInfos)
				if (ci.foundAny)
					++nrValidConnections;

			// Group cuts that need splitting by original arc
			my_map_t<R, my_vec_t<CutInfoType*>> cutsPerArc;
			for (auto& ci : allCutInfos)
			{
				if (ci.foundAny && ci.inArc)
					cutsPerArc[ci.arcIndex].push_back(&ci);
			}

			// Sort cuts within each arc by position (segment index, then fraction)
			for (auto& [arcIdx, cuts] : cutsPerArc)
			{
				std::sort(cuts.begin(), cuts.end(), [](const CutInfoType* a, const CutInfoType* b) {
					if (a->segmIndex != b->segmIndex) return a->segmIndex < b->segmIndex;
					return a->segmFraction < b->segmFraction;
				});
			}

			// Count total number of new tail arcs (one per split point, but accounting for multiple cuts on same arc)
			R nrNewTails = 0;
			for (auto& [arcIdx, cuts] : cutsPerArc)
				nrNewTails += cuts.size();

			// ============================================================
			// PHASE 3: Build result geometry
			// ============================================================

			R maxResCount = arg1Count + nrValidConnections + nrNewTails;

			typename sequence_traits<typename ResultSubType::value_type>::container_type
				resultSubData(maxResCount MG_DEBUG_ALLOCATOR_SRC("Connect: resultSubData.indices"));

			// Calculate data size for reservation
			SizeT actualDataSize = 0;
			for (tile_id t = 0; t < arg1A->GetAbstrDomainUnit()->GetNrTiles(); ++t)
				actualDataSize += const_array_cast<PolygonType>(arg1A)->GetLockedDataRead(t).get_sa().actual_data_size();
			actualDataSize += nrValidConnections * 2; // connection edges have 2 points each
			actualDataSize += nrNewTails * 4; // rough estimate for tail points

			resultSubData.data_reserve(actualDataSize MG_DEBUG_ALLOCATOR_SRC("Connect: resultSubData.sequences"));

			// Copy original arcs (they will be modified for splits)
			auto resIter = resultSubData.begin();
			for (tile_id t = 0; t < arg1A->GetAbstrDomainUnit()->GetNrTiles(); ++t)
			{
				auto arg1TileData = const_array_cast<PolygonType>(arg1A)->GetLockedDataRead(t);
				resIter = std::copy(arg1TileData.begin(), arg1TileData.end(), resIter);
			}
			auto resOriginalArcsEnd = resIter;

			// Reserve space for connection edges
			auto resConnectionsBegin = resIter;
			resIter += nrValidConnections;
			auto resConnectionsEnd = resIter;

			// Reserve space for tail arcs
			auto resTailsBegin = resIter;

			// Prepare arc_rel data
			OwningPtrSizedArray<R> nrOrgEntityData(nrNewTails, dont_initialize MG_DEBUG_ALLOCATOR_SRC("Connect: nrOrgEntityData"));

			// Process cuts per arc and create tails (sequential, maintains deterministic order)
			R tailIndex = 0;
			for (auto& [originalArcIdx, cuts] : cutsPerArc)
			{
				// Process cuts in spatial order along the arc
				typename ResultSubType::reference arcRef = resultSubData[originalArcIdx];

				for (SizeT cutIdx = cuts.size(); cutIdx > 0; --cutIdx)
				{
					// Process from end to beginning to preserve segment indices
					CutInfoType* ci = cuts[cutIdx - 1];

					if (ci->segmIndex + 1 >= arcRef.size())
						continue; // Invalid segment index

					// Create tail from cut point to end
					auto tailIter = resTailsBegin + tailIndex;
					SizeT tailSize = arcRef.size() - ci->segmIndex - (ci->inSegm ? 0 : 1);
					if (tailSize > 1)
					{
						tailIter->resize_uninitialized(tailSize MG_DEBUG_ALLOCATOR_SRC("Connect tail"));
						auto tailPtr = tailIter->begin();

						if (ci->inSegm)
							*tailPtr++ = ci->cutPoint;

						auto arcCut = arcRef.begin() + ci->segmIndex + 1;
						auto arcEnd = arcRef.end();
						fast_copy(arcCut, arcEnd, tailPtr);

						// Truncate original arc
						arcRef.erase(arcCut + 1, arcEnd);
						*(arcRef.begin() + ci->segmIndex + 1) = ci->cutPoint;

						nrOrgEntityData[tailIndex] = originalArcIdx;
						++tailIndex;
					}
				}
			}
			auto resTailsEnd = resTailsBegin + tailIndex;

			// Create connection edges (can be done in parallel)
			R connectionIndex = 0;
			for (auto& ci : allCutInfos)
			{
				if (ci.foundAny)
				{
					auto connEdgeIter = resConnectionsBegin + connectionIndex;
					auto& connEdge = *connEdgeIter;
					connEdge.resize_uninitialized(2 MG_DEBUG_ALLOCATOR_SRC("Connect edge"));

					connEdge[0] = ci.srcPoint; // kept by the discovery phase, which had the point at hand
					connEdge[1] = ci.cutPoint;
					++connectionIndex;
				}
			}

			// Set result counts
			R actualNrTails = tailIndex;
			resDomain->SetCount(arg1Count + nrValidConnections + actualNrTails);

			// Write results
			DataWriteLock resLock(resSub);
			auto resSubData = mutable_array_cast<PolygonType>(resLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);

			resSubData.get_sa().data_reserve(resultSubData.actual_data_size() MG_DEBUG_ALLOCATOR_SRC("Connect: resSubData.data_reserve"));

			auto ri = resSubData.begin();
			ri = fast_copy(resultSubData.begin(), resOriginalArcsEnd, ri); // Original arcs (modified)
			ri = fast_copy(resConnectionsBegin, resConnectionsEnd, ri);     // Connection edges
			ri = fast_copy(resTailsBegin, resTailsEnd, ri);                 // Tail arcs

			resLock.Commit();

			// Write arc_rel results
			DataWriteLock resNrOrgLock(resNrOrg);

			tile_id t = no_tile;
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor(IdAssigner(resNrOrgLock.get(), t, 0, 0, arg1Count));
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor(NullAssigner(resNrOrgLock.get(), t, arg1Count, nrValidConnections));
			arg1A->GetAbstrDomainUnit()->InviteUnitProcessor(IndexAssigner32(resNrOrg, resNrOrgLock.get(), t, SizeT(arg1Count) + nrValidConnections, actualNrTails, nrOrgEntityData.begin()));

			resNrOrgLock.Commit();
		}
		return true;
	}
	compare_type m_CompareType = compare_type::none;
};

// *****************************************************************************
//									SpatialIndexOper
// *****************************************************************************

// Level      NrQuad  =[4^(Level+1)-1]/3
//     0           1
//     1           5 (Representable by UInt4)
//     2          21
//     3          85 (Representable by (U)Int8)
//     4         341
//     5        1365
//...
//     15 1431655765 = (2^32-1)/3 (Representable by (U)Int32)
//..UInt4 ->  UInt32
//..UInt2 ->  UInt8
//  UInt1 ->  UInt4


template <typename RangeType, typename PointIter>
RangeType GetBounds(PointIter lbFirst, PointIter lbLast, PointIter ubFirst, RangeType bounds)
{
	for (; lbFirst!=lbLast; ++ubFirst, ++lbFirst)
		bounds |= RangeType(*lbFirst, *ubFirst);
	return bounds;
}


template <typename PointType>
UInt32 CalcSpatialIndex(const Range<PointType>& thisRange, Range<PointType> boundingBox, UInt32 level)
{
	dms_assert(IsIncluding(boundingBox, thisRange));
	dms_assert(level <= 15);

	UInt32 result      = 0;
	UInt32 levelWeight = 1;
	for (; level; levelWeight*=4, --level)
	{
		PointType mid = Center(boundingBox);
		UInt32 offset = SpatialIndexImpl::GetQuadrantOffset(1, thisRange, mid); // returns 0 if not in any quadrant
		switch (offset)
		{
			case 0: return result;
			case 1: boundingBox.second.first = mid.first; boundingBox.second.second = mid.second; break;
			case 2: boundingBox.second.first = mid.first; boundingBox.first .second = mid.second; break;
			case 3: boundingBox.first .first = mid.first; boundingBox.second.second = mid.second; break;
			case 4: boundingBox.first .first = mid.first; boundingBox.first .second = mid.second; break;
		}
		result *= 4;
		result += offset;
	}
	return result;
}

#include "OperAttrTer.h"
#include "UnitCreators.h"

template <class T, class LevelType, class QuadIdType>
struct SpatialIndexOper : TernaryAttrOper< QuadIdType, T, T, LevelType>
{
	using PointType = T;
	using RangeType = Range<T>;
	using CoordType = PointType::field_type;

	SpatialIndexOper(AbstrOperGroup* gr)
		: TernaryAttrOper< QuadIdType, T, T, LevelType>(gr, default_unit_creator<QuadIdType>, ValueComposition::Single,	false)
	{}

	void CalcTile(sequence_traits<QuadIdType>::seq_t resData, sequence_traits<T>::cseq_t arg1Data, sequence_traits<T>::cseq_t ubData, sequence_traits<LevelType>::cseq_t levelData, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		bool e1Void = af & AF1_ISPARAM;
		bool e2Void = af & AF2_ISPARAM;
		bool e3Void = af & AF3_ISPARAM;

		// TODO: Make Tile aware
		// ISSUE: GetBounds should analyse all tiles to get the boundingBox before indexing can start.
		if (e1Void != e2Void)
			this->GetGroup()->throwOperError("LowerBounds and UpperBounds are required to have the same domain");

		auto
			lbIter = arg1Data.begin()
		,	lbEnd  = arg1Data.end();
		auto ubIter = ubData.begin();

		auto levelIter = levelData.begin();

		UInt32 level = 0; if (e3Void) level = LevelType(*levelIter);

		auto resIter = resData.begin();

		RangeType boundingBox = GetBounds<RangeType>(lbIter, lbEnd, ubIter, RangeType()); // TODO: bring Outside tile stuff by implementing PreCalculate for ternary operators

		for (;lbIter != lbEnd; ++resIter, ++ubIter, ++lbIter)
		{
			if (!e3Void) { level = LevelType(*levelIter); ++levelIter; }
			*resIter = CalcSpatialIndex<PointType>(RangeType(*lbIter, *ubIter), boundingBox, level);
		}
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************


namespace 
{
	template <typename PointType>
	struct ConnectOperators
	{
		ConnectOperators()
			:	spatialIndex4(&cogIndex)
			,	spatialIndex2(&cogIndex)
			,	spatialIndex1(&cogIndex)
			,	connectNPT(false)
			,	connectNPP(true)
			,	cp(false, compare_type::none)
			,	ccp(true, compare_type::none)
			,	cp_eq(false, compare_type::eq)
			,	ccp_eq(true, compare_type::eq)
			,	cp_ne(false, compare_type::ne)
			,	ccp_ne(true, compare_type::ne)
		{}

		// FastConnectOperator <T, R, CT, E, SqrtDistType, HasMaxDist, HasMinDist, Reversed>
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32> fc;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true> fcmd64;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true> fcmdmd64;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true> fcmd32;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true> fcmdmd32;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32> fc_eq;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true> fc_eq_md64;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true> fc_eq_md32;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32> fc_ne;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true> fc_ne_md64;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true> fc_ne_md32;

		// #1228: the same members with the points first and the arcs second, which
		// connects each arc to the nearest of the points. No minSqrDist variants:
		// that argument is not implemented in either direction.
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, false, false, true> rfc;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, false, true> rfcmd64;
		FastConnectOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, false, true> rfcmd32;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, false, false, true> rfc_eq;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true, false, true> rfc_eq_md64;
		FastConnectOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true, false, true> rfc_eq_md32;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, false, false, true> rfc_ne;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true, false, true> rfc_ne_md64;
		FastConnectOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true, false, true> rfc_ne_md32;

		ConnectPointOperator<PointType> cp, ccp, cp_eq, ccp_eq, cp_ne, ccp_ne;
		ConnectNeighbourPointOperator<PointType> connectNPT;
		ConnectNeighbourPointOperator<PointType> connectNPP;
		// ConnectInfoOperator <P, E, CT, SegmID, SqrtDistType, HasMaxDist, HasMinDist, OnlyDistResult, Reversed>
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32> ci;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true> cimd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true> cimdmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true> cimd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true> cimdmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32> ci_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true> ci_eq_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true> ci_eq_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32> ci_ne;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true> ci_ne_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true> ci_ne_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, false, false, true> dc;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, false, true> dcmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, true, true> dcmdmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, false, true> dcmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, true, true> dcmdmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, false, false, true> dc_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true, false, true> dc_eq_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true, false, true> dc_eq_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, false, false, true> dc_ne;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true, false, true> dc_ne_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true, false, true> dc_ne_md32;

		// #1228: connect_info / dist_info with the points first and the arcs second,
		// so that each arc reports the nearest of the points
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, false, false, false, true> rci;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, false, false, true> rcimd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, false, false, true> rcimd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, false, false, false, true> rci_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true, false, false, true> rci_eq_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true, false, false, true> rci_eq_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, false, false, false, true> rci_ne;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true, false, false, true> rci_ne_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true, false, false, true> rci_ne_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, false, false, true, true> rdc;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float64, true, false, true, true> rdcmd64;
		ConnectInfoOperator <PointType, UInt32, compare_type::none, UInt32, Float32, true, false, true, true> rdcmd32;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, false, false, true, true> rdc_eq;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float64, true, false, true, true> rdc_eq_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::eq, UInt32, Float32, true, false, true, true> rdc_eq_md32;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, false, false, true, true> rdc_ne;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float64, true, false, true, true> rdc_ne_md64;
		ConnectInfoOperator <PointType, UInt32, compare_type::ne, UInt32, Float32, true, false, true, true> rdc_ne_md32;

		SpatialIndexOper<PointType, UInt4, UInt32>    spatialIndex4;
		SpatialIndexOper<PointType, UInt2, UInt8>     spatialIndex2;
		SpatialIndexOper<PointType, Bool,  UInt4>     spatialIndex1;
	};

	tl_oper::inst_tuple_templ<typelists::seq_points, ConnectOperators > connectOperatorInstances;
}

/******************************************************************************/

