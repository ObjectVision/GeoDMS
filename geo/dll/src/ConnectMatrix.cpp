// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// #1236: connect_matrix / dist_matrix -- for each feature, ALL points within a
// maximum distance, instead of the single nearest one that connect_info gives.
//
// The result is a sparse matrix: a fresh domain of (feature, point) PAIRS, so
// unlike connect_info it is keyed by neither argument. No new geometry is built,
// which is why there is no separate connect_/connect_info_ pair of names here.

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include <algorithm>
#include <atomic>

#include "mem/MyContainers.h"

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "mci/CompositeCast.h"
#include "geom/GeoDist.h"
#include "geom/SpatialIndex.h"
#include "geom/SpatialSearchBox.h"
#include "ptr/Resource.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "IndexAssigner.h"
#include "OperSignature.h"
#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "LispTreeType.h"

CommonOperGroup cogCONMATRIX    ("connect_matrix",    oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCONMATRIX_EQ ("connect_matrix_eq", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogCONMATRIX_NE ("connect_matrix_ne", oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTMATRIX   ("dist_matrix",       oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTMATRIX_EQ("dist_matrix_eq",    oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);
CommonOperGroup cogDISTMATRIX_NE("dist_matrix_ne",    oper_policy::dynamic_result_class | oper_policy::better_not_in_meta_scripting);

namespace {

enum class match_type { none, eq, ne };

static StaticLateTokenID sm_Dist("dist");
static StaticLateTokenID sm_PointRel("point_rel");
static StaticLateTokenID sm_CutPoint("CutPoint");
static StaticLateTokenID sm_InArc("InArc");
static StaticLateTokenID sm_InSegm("InSegm");
static StaticLateTokenID sm_SegmID("SegmID");

// One accepted (feature, point) pair. The whole working set of the discovery
// phase is a vector of these per tile, which the max_nr_matches bound caps at
// k per feature -- census-visible through my_vec_t.
template <typename PointType, typename SqrDistType, typename R>
struct MatchRec
{
	R           featureIndex = 0;
	UInt32      pointIndex = 0;
	SqrDistType sqrDist = 0;
	PointType   cutPoint;
	UInt32      segmIndex = 0;
	bool        inArc = false, inSegm = false;
};

// Ordering within one feature: nearest first, ties by the lower point index, so
// that the result is deterministic. Used both as the heap's ordering (the top is
// then the worst kept match, the one that a better candidate evicts) and as the
// final sort.
template <typename M>
struct MatchWorseThan
{
	bool operator()(const M& a, const M& b) const
	{
		if (a.sqrDist != b.sqrDist) return a.sqrDist < b.sqrDist;
		return a.pointIndex < b.pointIndex;
	}
};

template <typename P, typename E = UInt32, match_type CT = match_type::none, typename SqrtDistType = Float64, bool OnlyDistResult = false, bool HasMaxCount = false>
class ConnectMatrixOperator : public VariadicOperator
{
	using PointType = P;
	using PolygonType = sequence_traits<PointType>::container_type;
	using CoordType = typename PointType::field_type;
	using SqrDistType = SqrtDistType;
	using R = UInt32;

	using PointUnitType = Unit<PointType>;
	using DistUnitType = Unit<SqrtDistType>;
	using BoolUnitType = Unit<Bool>;
	using SegmUnitType = Unit<UInt32>;
	using ResultUnitType = Unit<R>;

	using PointsArrayType = DataArray<PointType>;
	using FeatsArrayType = DataArray<PolygonType>;

	using PointIndexType = SpatialIndex<CoordType, typename PointsArrayType::const_iterator>;
	using MatchType = MatchRec<PointType, SqrDistType, R>;

	static AbstrOperGroup* MatrixGroup()
	{
		if constexpr (CT == match_type::eq) return OnlyDistResult ? &cogDISTMATRIX_EQ : &cogCONMATRIX_EQ;
		else if constexpr (CT == match_type::ne) return OnlyDistResult ? &cogDISTMATRIX_NE : &cogCONMATRIX_NE;
		else return OnlyDistResult ? &cogDISTMATRIX : &cogCONMATRIX;
	}
	static constexpr arg_index NrKeyArgs() { return CT == match_type::none ? 0 : 2; }
	static constexpr arg_index NrArgs() { return 4 + NrKeyArgs() + (HasMaxCount ? 1 : 0); }

public:
	ConnectMatrixOperator()
		: VariadicOperator(MatrixGroup(), ResultUnitType::GetStaticClass(), NrArgs())
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();
		*argClsIter++ = PointsArrayType::GetStaticClass();
		if constexpr (CT != match_type::none) *argClsIter++ = DataArray<E>::GetStaticClass();
		*argClsIter++ = DataArray<SqrtDistType>::GetStaticClass();
		*argClsIter++ = FeatsArrayType::GetStaticClass();
		if constexpr (CT != match_type::none) *argClsIter++ = DataArray<E>::GetStaticClass();
		*argClsIter++ = DataArray<SqrtDistType>::GetStaticClass();
		if constexpr (HasMaxCount) *argClsIter++ = DataArray<UInt32>::GetStaticClass();
		assert(m_ArgClassesEnd == argClsIter);
	}

	// A FRESH-UNIT operator (K6): the result is a new pair domain carrying
	// point_rel / arc_rel / dist (+ the cut-point members for connect_matrix).
	// Their domain is that fresh unit, so it can be claimed; their values units
	// belong to deferred argument positions and stay unclaimed, as in connect.
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		arg_index i = 0;
		sb.ArgName(i, "points"); sb.ArgAttr(i, sb.UnitVar("Vpt"), sb.UnitVar("Dp"), ValueComposition::Single); ++i;
		if constexpr (CT != match_type::none) { sb.ArgName(i, "pointKey"); sb.ArgDeferred(i, "point join key"); ++i; }
		sb.ArgName(i, "maxSqrDistPoint"); sb.ArgDeferred(i, "the point's squared cutoff (may be a void-domain parameter)"); ++i;
		sb.ArgName(i, "features"); sb.ArgDeferred(i, "feature geometry (Sequence or Polygon)"); ++i;
		if constexpr (CT != match_type::none) { sb.ArgName(i, "featureKey"); sb.ArgDeferred(i, "feature join key (shares values with pointKey)"); ++i; }
		sb.ArgName(i, "maxSqrDistFeature"); sb.ArgDeferred(i, "the feature's squared cutoff (may be a void-domain parameter)"); ++i;
		if constexpr (HasMaxCount) { sb.ArgName(i, "max_nr_matches"); sb.ArgDeferred(i, "at most this many nearest points per feature"); ++i; }
		sb.DeferredRelation("the point and feature coordinates share one value class (K16); eq/ne join keys share values");

		sig_var U = sb.GeneratedUnit("connect_matrix");
		sb.ResultUnit(U);
		sb.ResultContainerMember("point_rel", no_sig_var, U, ValueComposition::Single);
		sb.ResultContainerMember("arc_rel", no_sig_var, U, ValueComposition::Single);
		sb.ResultContainerMember("dist", sb.DefaultUnit(DataArray<SqrtDistType>::GetStaticClass()->GetValuesType()), U, ValueComposition::Single);
		if constexpr (!OnlyDistResult)
		{
			sig_var Bf = sb.UnitVar("InFlag"); sb.FixedValueClass(Bf, DataArray<Bool>::GetStaticClass()->GetValuesType());
			sig_var Sg = sb.UnitVar("SegmId"); sb.FixedValueClass(Sg, DataArray<UInt32>::GetStaticClass()->GetValuesType());
			sb.ResultContainerMember("CutPoint", no_sig_var, U, ValueComposition::Single);
			sb.ResultContainerMember("InArc", Bf, U, ValueComposition::Single);
			sb.ResultContainerMember("InSegm", Bf, U, ValueComposition::Single);
			sb.ResultContainerMember("SegmID", Sg, U, ValueComposition::Single);
		}
		sb.ResultMembersComplete();
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		arg_index argCount = 0;
		const AbstrDataItem* argPoints = AsDataItem(args[argCount++]);
		const AbstrDataItem* argPointKey = (CT != match_type::none) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argDp = AsDataItem(args[argCount++]);
		const AbstrDataItem* argFeats = AsDataItem(args[argCount++]);
		const AbstrDataItem* argFeatKey = (CT != match_type::none) ? AsDataItem(args[argCount++]) : nullptr;
		const AbstrDataItem* argDf = AsDataItem(args[argCount++]);
		const AbstrDataItem* argMaxCount = HasMaxCount ? AsDataItem(args[argCount++]) : nullptr;
		assert(args.size() == argCount);

		const AbstrUnit* pointUnit = argPoints->GetAbstrValuesUnit();
		const AbstrUnit* featUnit = argFeats->GetAbstrValuesUnit();
		const AbstrUnit* pointEntity = argPoints->GetAbstrDomainUnit();
		const AbstrUnit* featEntity = argFeats->GetAbstrDomainUnit();

		featUnit->UnifyValues(pointUnit, "Values of feature attribute", "Values of point attribute", UM_Throw);

		if constexpr (CT != match_type::none)
		{
			pointEntity->UnifyDomain(argPointKey->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
			featEntity->UnifyDomain(argFeatKey->GetAbstrDomainUnit(), "e3", "e4", UM_Throw);
			argPointKey->GetAbstrValuesUnit()->UnifyValues(argFeatKey->GetAbstrValuesUnit(), "v2", "v4", UM_Throw);
		}
		// each cutoff belongs to its own geometry, or is one value for all of them
		pointEntity->UnifyDomain(argDp->GetAbstrDomainUnit(), "Domain of points", "Domain of the point cutoff", UnifyMode(UM_Throw | UM_AllowVoidRight));
		featEntity->UnifyDomain(argDf->GetAbstrDomainUnit(), "Domain of features", "Domain of the feature cutoff", UnifyMode(UM_Throw | UM_AllowVoidRight));
		if constexpr (HasMaxCount)
			argMaxCount->GetAbstrDomainUnit()->UnifyDomain(Unit<Void>::GetStaticClass()->CreateDefault(), "Domain of max_nr_matches", "void", UM_Throw);

		bool hasNonVoidDp = !argDp->HasVoidDomainGuarantee();
		bool hasNonVoidDf = !argDf->HasVoidDomainGuarantee();

		auto resDomain_owner = ResultUnitType::GetStaticClass()->CreateResultUnit(resultHolder.GetNew());
		ResultUnitType* resDomain = mutable_unit_cast<R>(resDomain_owner.get());
		assert(resDomain);
		resultHolder = resDomain;

		const DistUnitType* distUnit = const_unit_cast<SqrtDistType>(DistUnitType::GetStaticClass()->CreateDefault());

		AbstrDataItem* resPointRel = CreateDataItem(resDomain, sm_PointRel, resDomain, pointEntity).get();
		AbstrDataItem* resArcRel = CreateDataItem(resDomain, token::arc_rel, resDomain, featEntity).get();
		AbstrDataItem* resDist = CreateDataItem(resDomain, sm_Dist, resDomain, distUnit).get();
		resPointRel->SetTSF(TSF_Categorical);
		resArcRel->SetTSF(TSF_Categorical);

		AbstrDataItem* resCutPoint = nullptr;
		AbstrDataItem* resInArc = nullptr;
		AbstrDataItem* resInSegm = nullptr;
		AbstrDataItem* resSegmID = nullptr;
		if constexpr (!OnlyDistResult)
		{
			const BoolUnitType* boolUnit = const_unit_cast<Bool>(BoolUnitType::GetStaticClass()->CreateDefault());
			const SegmUnitType* segmUnit = const_unit_cast<UInt32>(SegmUnitType::GetStaticClass()->CreateDefault());
			resCutPoint = CreateDataItem(resDomain, sm_CutPoint, resDomain, pointUnit).get();
			resInArc = CreateDataItem(resDomain, sm_InArc, resDomain, boolUnit).get();
			resInSegm = CreateDataItem(resDomain, sm_InSegm, resDomain, boolUnit).get();
			resSegmID = CreateDataItem(resDomain, sm_SegmID, resDomain, segmUnit).get();
		}

		if (!mustCalc)
			return true;

		Timer processTimer;
		auto itemRef = resultHolder.GetProgressPrefix(); // #795: names the config item

		bool isPossiblyMultiPolygon = argFeats->GetValueComposition() == ValueComposition::Polygon;

		DataReadLock pointsLock(argPoints);
		DataReadLock featsLock(argFeats);
		DataReadLock pointKeyLock(argPointKey);
		DataReadLock featKeyLock(argFeatKey);
		DataReadLock dpLock(argDp);
		DataReadLock dfLock(argDf);
		DataReadLock maxCountLock(argMaxCount);

		SizeT pointCount = pointEntity->GetCount();
		SizeT featCount = featEntity->GetCount();

		SizeT maxNrMatches = SizeT(-1);
		if constexpr (HasMaxCount)
		{
			auto mc = const_array_cast<UInt32>(argMaxCount)->GetLockedDataRead();
			if (mc.size() && IsDefined(mc[0]))
				maxNrMatches = mc[0];
		}

		// the points go into the index and are therefore read whole; the features
		// are iterated and stream per tile (#1236: with a cutoff on both sides the
		// candidate set is symmetric, so this choice is a memory/tiling one)
		auto pointData = const_array_cast<PointType>(argPoints)->GetLockedDataRead();
		assert(pointCount == pointData.size());
		PointIndexType spIndex(pointData.begin(), pointData.end(), 0);
		const PointType* pointBegin = pointData.begin();

		typename DataArray<SqrtDistType>::locked_cseq_t dpData = const_array_cast<SqrtDistType>(argDp)->GetLockedDataRead();
		const SqrtDistType* dpPtr = dpData.begin();

		const E* pointKeyPtr = nullptr;
		typename DataArray<E>::locked_cseq_t pointKeys;
		if (argPointKey) { pointKeys = const_array_cast<E>(argPointKey)->GetLockedDataRead(); pointKeyPtr = pointKeys.begin(); }

		// the widest radius any point allows: the query box of a feature can be no
		// larger than that, whatever its own cutoff says
		SqrDistType globalMaxDp = 0;
		if (hasNonVoidDp)
		{
			for (SizeT i = 0; i != pointCount; ++i)
				if (IsDefined(dpPtr[i]))
					MakeMax(globalMaxDp, dpPtr[i]);
		}
		else if (dpData.size() && IsDefined(dpPtr[0]))
			globalMaxDp = dpPtr[0];

		tile_id nrFeatTiles = featEntity->GetNrTiles();
		std::vector<my_vec_t<MatchType>> perTileMatches(nrFeatTiles);
		std::vector<SizeT> tileOffsets(nrFeatTiles + 1, 0);
		for (tile_id t = 0; t != nrFeatTiles; ++t)
			tileOffsets[t + 1] = tileOffsets[t] + featEntity->GetTileCount(t);

		std::atomic<SizeT> nrProcessedFeatures = 0;

		parallel_tileloop(nrFeatTiles, [&, isPossiblyMultiPolygon, this](tile_id t)
		{
			auto featData = const_array_cast<PolygonType>(argFeats)->GetLockedDataRead(t);
			auto tileSize = featData.size();
			if (!tileSize)
				return;

			const SqrtDistType* dfPtr = nullptr;
			typename DataArray<SqrtDistType>::locked_cseq_t dfData;
			dfData = const_array_cast<SqrtDistType>(argDf)->GetLockedDataRead(hasNonVoidDf ? t : 0);
			dfPtr = dfData.begin();

			const E* featKeyPtr = nullptr;
			typename DataArray<E>::locked_cseq_t featKeys;
			if (argFeatKey) { featKeys = const_array_cast<E>(argFeatKey)->GetLockedDataRead(t); featKeyPtr = featKeys.begin(); }

			my_vec_t<MatchType>& tileMatches = perTileMatches[t];
			my_vec_t<MatchType> heap; // the k best of ONE feature
			R globalOffset = tileOffsets[t];
			MatchWorseThan<MatchType> worseThan;

			SizeT nrUnreported = 0;
			for (SizeT i = 0; i != tileSize; ++i)
			{
				auto featRef = featData[i];
				auto featBegin = begin_ptr(featRef);
				auto featEnd = end_ptr(featRef);
				auto featBox = RangeFromSequence_SkipUndefined(featBegin, featEnd);

				SqrDistType df = (hasNonVoidDf ? dfPtr[i] : dfPtr[0]);
				E featKey = featKeyPtr ? featKeyPtr[i] : UNDEFINED_VALUE(E);

				heap.clear();
				if (featBegin != featEnd && !featBox.inverted() && IsDefined(df) && df >= 0 && maxNrMatches)
				{
					auto radius = Min<SqrDistType>(df, globalMaxDp);
					auto box = InflatedSearchBox<CoordType>(featBox, SqrtBet(radius));
					if (!box.inverted())
						for (auto iter = spIndex.begin(box); iter; ++iter)
						{
							const PointType* pointPtr = (*iter)->get_ptr();
							SizeT pi = pointPtr - pointBegin;
							assert(pi < pointCount);

							if constexpr (CT != match_type::none)
							{
								// exactly the rule connect_eq / connect_ne already use: the
								// comparison must hold, and a null POINT key waives it. A
								// null feature key is not a wildcard -- under _eq only
								// null-key points reach it, under _ne every point does.
								E pointKey = pointKeyPtr[pi];
								bool admit = (CT == match_type::eq)
									? (featKey == pointKey)
									: (featKey != pointKey);
								if (!admit && IsDefined(pointKey))
									continue;
							}

							SqrDistType dp = hasNonVoidDp ? dpPtr[pi] : dpPtr[0];
							if (!IsDefined(dp) || !(dp >= 0))
								continue;
							auto limit = Min<SqrDistType>(df, dp);

							// a handle seeded with the pair's own cutoff returns true
							// exactly when the feature is within it, and then carries the
							// true distance and the cut point
							ArcProjectionHandleWithDist<SqrDistType, CoordType> aph(*pointPtr, limit, isPossiblyMultiPolygon);
							if (!aph.Project2Arc(featBegin, featEnd))
								continue;

							MatchType m;
							m.featureIndex = globalOffset + i;
							m.pointIndex = pi;
							m.sqrDist = aph.m_MinSqrDist;
							m.cutPoint = aph.m_CutPoint;
							m.segmIndex = aph.m_SegmIndex;
							m.inArc = aph.m_InArc;
							m.inSegm = aph.m_InSegm;

							if (heap.size() < maxNrMatches)
							{
								heap.push_back(m);
								std::push_heap(heap.begin(), heap.end(), worseThan);
							}
							else if (worseThan(m, heap.front()))
							{
								// the heap is full and this one is nearer than its worst
								std::pop_heap(heap.begin(), heap.end(), worseThan);
								heap.back() = m;
								std::push_heap(heap.begin(), heap.end(), worseThan);
							}
						}
				}

				std::sort(heap.begin(), heap.end(), worseThan); // nearest first within the feature
				for (const auto& m : heap)
					tileMatches.push_back(m);

				++nrUnreported;
				if (processTimer.PassedSecs())
				{
					nrProcessedFeatures += nrUnreported;
					nrUnreported = 0;
					reportF(SeverityTypeID::ST_MajorTrace, "{}{} {} / {} features done"
						, itemRef.c_str()
						, this->GetGroup()->GetName()
						, AsString(nrProcessedFeatures.load()), AsString(featCount));
				}
			}
			nrProcessedFeatures += nrUnreported;
		});

		SizeT nrMatches = 0;
		for (tile_id t = 0; t != nrFeatTiles; ++t)
			nrMatches += perTileMatches[t].size();

		resDomain->SetCount(nrMatches);

		DataWriteLock pointRelLock(resPointRel);
		DataWriteLock arcRelLock(resArcRel);
		DataWriteLock distLock(resDist);
		DataWriteLock cutPointLock(resCutPoint);
		DataWriteLock inArcLock(resInArc);
		DataWriteLock inSegmLock(resInSegm);
		DataWriteLock segmIDLock(resSegmID);

		OwningPtrSizedArray<UInt32> pointRelData(nrMatches, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ConnectMatrix: point_rel"));
		OwningPtrSizedArray<UInt32> arcRelData(nrMatches, dont_initialize MG_DEBUG_ALLOCATOR_SRC("ConnectMatrix: arc_rel"));

		auto distData = mutable_array_cast<SqrtDistType>(distLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
		auto d = distData.begin();

		typename DataArray<PointType>::locked_seq_t cutPointData; typename DataArray<PointType>::iterator cp;
		typename DataArray<Bool>::locked_seq_t inArcData; typename DataArray<Bool>::iterator ia;
		typename DataArray<Bool>::locked_seq_t inSegmData; typename DataArray<Bool>::iterator is;
		typename DataArray<UInt32>::locked_seq_t segmIDData; typename DataArray<UInt32>::iterator sg;
		if constexpr (!OnlyDistResult)
		{
			cutPointData = mutable_array_cast<PointType>(cutPointLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero); cp = cutPointData.begin();
			inArcData = mutable_array_cast<Bool>(inArcLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero); ia = inArcData.begin();
			inSegmData = mutable_array_cast<Bool>(inSegmLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero); is = inSegmData.begin();
			segmIDData = mutable_array_cast<UInt32>(segmIDLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero); sg = segmIDData.begin();
		}

		SizeT w = 0;
		for (tile_id t = 0; t != nrFeatTiles; ++t)
			for (const auto& m : perTileMatches[t])
			{
				pointRelData[w] = m.pointIndex;
				arcRelData[w] = m.featureIndex;
				*d++ = Convert<SqrtDistType>(sqrt(m.sqrDist));
				if constexpr (!OnlyDistResult)
				{
					*cp++ = m.cutPoint;
					*ia++ = m.inArc;
					*is++ = m.inSegm;
					*sg++ = m.segmIndex;
				}
				++w;
			}
		assert(w == nrMatches);

		// the relations are typed by their target domain, so they go in through the
		// index assigner rather than a per-element SetValueAsSizeT
		pointEntity->InviteUnitProcessor(IndexAssigner32(resPointRel, pointRelLock.get(), no_tile, 0, nrMatches, pointRelData.begin()));
		featEntity->InviteUnitProcessor(IndexAssigner32(resArcRel, arcRelLock.get(), no_tile, 0, nrMatches, arcRelData.begin()));

		pointRelLock.Commit();
		arcRelLock.Commit();
		distLock.Commit();
		if (cutPointLock) cutPointLock.Commit();
		if (inArcLock) inArcLock.Commit();
		if (inSegmLock) inSegmLock.Commit();
		if (segmIDLock) segmIDLock.Commit();

		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

template <typename PointType>
struct ConnectMatrixOperators
{
	// ConnectMatrixOperator <P, E, CT, SqrtDistType, OnlyDistResult, HasMaxCount>
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float64, false, false> cm64;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float64, false, true > cm64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float32, false, false> cm32;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float32, false, true > cm32k;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float64, true, false> dm64;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float64, true, true > dm64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float32, true, false> dm32;
	ConnectMatrixOperator<PointType, UInt32, match_type::none, Float32, true, true > dm32k;

	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float64, false, false> cm_eq64;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float64, false, true > cm_eq64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float32, false, false> cm_eq32;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float32, false, true > cm_eq32k;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float64, true, false> dm_eq64;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float64, true, true > dm_eq64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float32, true, false> dm_eq32;
	ConnectMatrixOperator<PointType, UInt32, match_type::eq, Float32, true, true > dm_eq32k;

	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float64, false, false> cm_ne64;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float64, false, true > cm_ne64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float32, false, false> cm_ne32;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float32, false, true > cm_ne32k;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float64, true, false> dm_ne64;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float64, true, true > dm_ne64k;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float32, true, false> dm_ne32;
	ConnectMatrixOperator<PointType, UInt32, match_type::ne, Float32, true, true > dm_ne32k;
};

tl_oper::inst_tuple_templ<typelists::seq_points, ConnectMatrixOperators> connectMatrixOperatorInstances;

} // namespace

/******************************************************************************/
