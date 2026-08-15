// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Operator: the base class of all operators — argument binding and
// applicability checks, and the performance estimation used by the
// scheduler.

#include "Operator.h"

// For the argument-materialization term in EstimatePerformance: it asks each argument's data object
// for its regime and tiling, so the complete type is needed here (§8.1.16).
#include "AbstrDataObject.h"
#include "TiledRangeData.h"

// *****************************************************************************
// Section:     Operator
// *****************************************************************************

// Operator Constructors

Operator::Operator(AbstrOperGroup* group, ClassCPtr resultCls)
	:	m_Group(group)
	,	m_ResultClass(resultCls)
{
	dms_assert(resultCls);

	group->Register(this);
}

Operator::Operator(AbstrOperGroup* group, ClassCPtr resultCls, const ClassCPtr* argClsList, UInt32 nrArgs)
	:	m_ArgClassesBegin(argClsList)
	,	m_ArgClassesEnd  (argClsList+nrArgs)
	,	m_Group      (group)
	,	m_ResultClass(resultCls)
	,	m_NrOptionalArgs(0)
{
	assert(resultCls);
	assert(group);
	group->Register(this);
}

Operator::~Operator()
{}

bool Operator::CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const
{
	throwIllegalAbstract(MG_POS, "Operator::CreateResult");
}

auto Operator::GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const -> oper_arg_policy
{
	return GetGroup()->GetArgPolicy(argNr, firstArgValue);
}

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "act/MainThread.h"
#include "stg/AbstrStorageManager.h" // IsInMMD
#include "utl/Environment.h"        // IsMultiThreaded3, IsPerformanceLogging
#include "dbg/Check.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"

TIC_CALL CharPtr AsString(materialization m)
{
	switch (m) {
	case materialization::meta: return "meta";
	case materialization::eager: return "eager";
	case materialization::deferred: return "deferred";
	case materialization::streaming: return "streaming";
	case materialization::spilled: return "spilled";
	}
	return "?";
}

TIC_CALL CharPtr AsString(estimate_confidence c)
{
	switch (c) {
	case estimate_confidence::measured: return "measured";
	case estimate_confidence::derived: return "derived";
	case estimate_confidence::declared: return "declared";
	case estimate_confidence::bounded: return "bounded";
	case estimate_confidence::assumed: return "assumed";
	}
	return "?";
}

// A domain's element count, with how much that number can be trusted. Robust by construction: at
// schedule time a cache result's domain is often not computed yet, and asking an uncomputed unit
// for its count -- or evaluating a declared size rule -- throws, which must degrade this one field
// and never the whole estimate.
static auto EstimateDomainCount(const AbstrUnit* domain) -> AbstrUnit::CountEstimate
{
	try {
		return domain->EstimateCount();
	}
	catch (...) {
		// A declared SizeExpectation/SizeUpperbound that cannot be evaluated where the estimate is
		// taken is a silent loss of the modeller's knowledge, and the reason is what tells us where
		// it *can* be evaluated. Everything here is inside the guard: probing for the properties can
		// itself need meta-info, so even asking whether one exists may throw.
		auto reason = catchException(false);
		try {
			if (IsPerformanceLogging())
				reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
					, "size estimate for {}: unavailable here: {}"
					, domain->GetFullName(), reason ? reason->GetAsText() : SharedStr("(no reason)"));
		}
		catch (...) {}
		return { 0, 0, estimate_confidence::assumed };
	}
}

// Which materialization regime this result will be produced in. Mirrors the predicate the operator
// families apply when they choose between CreateFutureTileFunctor and an eager parallel_tileloop
// (clc/dll/include/OperAttr*.h and friends), so the estimate describes what will actually happen.
// Families whose gate carries extra terms refine this.
//
// KeepData is deliberately NOT tested here: of the seven gated families only the casted-unary one
// consults GetKeepDataState(), so applying it to all of them mislabelled a KeepData result of a
// unary/binary/ternary/point/lookup operator as eager when it really gets a FutureTileFunctor.
// The casted-unary family adds the term in its own override. See doc/tile-data-retainment.md §4.7.
static auto PredictMaterialization(const AbstrDataItem* res, tile_id nrTiles) -> materialization
{
	// A result in a memory-mapped store becomes a FileTileArray: its data goes to a cache file and
	// RAM holds only the mapped views. Charging it the whole array (as folding it into 'eager' did)
	// over-reserves by the array size and would make a ledger refuse work it could have run.
	if (IsInMMD(res))
		return materialization::spilled;
	if (!IsMultiThreaded3() || nrTiles <= 1)
		return materialization::eager;
	return res->GetLazyCalculatedState() ? materialization::streaming : materialization::deferred;
}

// True scalar width in bits, or 0 when there is none (variable width arrives as the UInt32(-1)
// marker -- see EstimateDataBytes). Local twin of AbstrDataItem.cpp's FixedWidthInBits.
static SizeT ScalarBitsOf(const ValueClass* vc)
{
	if (!vc)
		return 0;
	auto scalarClass = vc->GetScalarClass();
	if (!scalarClass)
		return 0;
	auto bitSize = SizeT(scalarClass->GetBitSize());
	return (bitSize == SizeT(UInt32(-1))) ? 0 : bitSize;
}

// Carry a derived per-element width from an argument to the result, for the families whose result
// elements ARE their argument's elements (lookup, collect_by_cond, collect_by_org_rel, ...).
//
// Guards keep this honest. Identical ValueComposition means the element really is carried over
// rather than rebuilt, and the result is left alone once it has its own width, so an operator that
// publishes a precise figure is never overwritten by an inherited one. Between SEQUENCES whose
// scalar widths are both known, the value type may differ: a point-type conversion (mm integer
// points -> float points) keeps the points-per-row and scales the bytes by the scalar ratio --
// without that, the BAG chains that pass through the mm stage kept estimating 21x under
// (SS8.1.19/8.1.21). Everything else still requires the exact same value type. The max over
// arguments is the conservative choice for an admission charge.
static void InheritEstimatedElementWidth(const AbstrDataItem* adi, const ArgRefs& args) noexcept
{
	try {
		if (adi->GetEstimatedBytesPerElement())
			return; // already knows better than anything we could copy in
		auto valuesUnit = adi->GetAbstrValuesUnit();
		if (!valuesUnit)
			return;
		auto valuesType = valuesUnit->GetValueType();
		auto composition = adi->GetValueComposition();
		bool isSeq = composition != ValueComposition::Single;
		if (!isSeq && valuesType && valuesType->GetBitSize() != UInt32(-1))
			return; // fixed width: EstimateDataBytes is already exact
		auto myScalarBits = isSeq ? ScalarBitsOf(valuesType) : 0;

		SizeT widest = 0;
		for (const auto& argRef : args)
			if (auto argItem = GetItem(argRef); argItem && IsDataItem(argItem))
			{
				auto argAdi = AsDataItem(argItem);
				if (argAdi->GetValueComposition() != composition)
					continue;
				auto argValuesUnit = argAdi->GetAbstrValuesUnit();
				if (!argValuesUnit)
					continue;
				SizeT argWidth = argAdi->GetEstimatedBytesPerElement();
				if (!argWidth)
					continue;
				auto argType = argValuesUnit->GetValueType();
				if (argType != valuesType)
				{
					// sequence-to-sequence with a scalar conversion: same element count per row,
					// bytes scale with the scalar width
					auto argScalarBits = isSeq ? ScalarBitsOf(argType) : SizeT(0);
					if (!myScalarBits || !argScalarBits)
						continue;
					argWidth = (argWidth * myScalarBits) / argScalarBits;
				}
				MakeMax(widest, argWidth);
			}
		adi->SetEstimatedBytesPerElement(widest);
	}
	catch (...) {} // an unresolvable values unit just means: keep the generic guess
}

TIC_CALL auto Operator::EstimatePerformance(TreeItemDualRef& resultHolder, const ArgRefs& args) const -> PerformanceEstimationData
{
	// Only materialize the skeleton when there isn't one: this is the very condition
	// CreateResultCaller early-returns on, but checking it here also keeps overrides that require
	// the meta thread (PhaseContainer's MG_CHECK) out of the way when re-estimating from a worker.
	if (!resultHolder || resultHolder.IsTmp())
		CreateResultCaller(resultHolder, args);

	auto result = PerformanceEstimationData();

	// Never GetNew() here: it MG_CHECKs !IsOld(), so it throws for every operator whose result is an
	// EXISTING item (oper_policy::existing -- subitem and friends), which silently cost those their
	// whole estimate. GetUlt() is also the item RunOperator measures, so estimate and actual agree.
	auto resultItem = resultHolder.GetUlt();
	if (!resultItem)
		resultItem = resultHolder.GetOld();
	if (!resultItem || !IsDataItem(resultItem))
	{
		result.regime = materialization::meta;
		result.confidence = estimate_confidence::derived; // a unit or container result: zero data cost, exactly
		return result;
	}

	auto adi = AsDataItem(resultItem);
	auto domain = adi->GetAbstrDomainUnit();

	auto domainCount = EstimateDomainCount(domain);
	result.resultingNrElements = domainCount.expected;
	result.confidence = domainCount.confidence;

	// Before charging for a variable-width result, see whether an argument already carries a derived
	// per-element width. The selection/permutation families (lookup, collect_by_cond,
	// collect_by_org_rel, recollect_by_cond, ...) copy argument elements verbatim, so the result's
	// elements are exactly as wide as the argument's -- whereas EstimateDataBytes would otherwise
	// fall back to ASSUMED_SEQ_LENGTH and lose whatever the producer knew. Guarded on identical
	// composition AND identical value type, so this only fires where an element really is carried
	// over unchanged; an operator that builds different elements publishes its own width instead.
	InheritEstimatedElementWidth(adi, args);

	result.resultingMemory = EstimateDataBytes(adi, domainCount.expected);
	result.resultingMemoryUpperBound = EstimateDataBytes(adi, domainCount.upperBound);

	// Tiling, then the regime it enables, then what is actually resident under that regime.
	// Read the tiling only where it already exists: an estimator must not force anything, and
	// GetNrTiles MG_CHECKs a not-yet-calculated range -- the catch that used to sit here recovered
	// from that in Release, but throwCheckFailed asserts before throwing, so any Debug run through
	// here died (#1181). Probing the range item also reads the tiling where it lives: on a
	// delegating unit, GetTiledRangeData on the unit itself answers from its own, empty, slot.
	tile_offset maxTileSize = 0;
	if (auto rangeItem = domain->GetCurrRangeItem())
		if (auto trd = AsUnit(rangeItem.get())->GetTiledRangeData())
		{
			result.nrChores = trd->GetNrTiles();
			maxTileSize = trd->GetMaxTileSize();
		}
	result.extraTasks = UInt16(Min<SizeT>(result.nrChores, MAX_VALUE(UInt16)));
	result.choreMemory = maxTileSize ? EstimateDataBytes(adi, maxTileSize)
		: (result.nrChores ? result.resultingMemory / result.nrChores : result.resultingMemory);

	result.regime = PredictMaterialization(adi, result.nrChores);
	if (result.regime == materialization::streaming || result.regime == materialization::spilled)
	{
		// Only the tiles in flight occupy RAM: streaming frees a released tile (and recomputes it if
		// pulled again), spilled unmaps it (and pages it back in). Either way the bound is
		// concurrency x one tile, never the whole array.
		auto inflight = Min<SizeT>(result.nrChores, MaxConcurrentTreads());
		result.residentMemory = result.choreMemory * inflight;
		if (result.regime == materialization::spilled)
			result.ioBytes = result.resultingMemory; // the volume that goes to, and comes back from, the cache file
	}
	else
		// eager writes it all at once; deferred accumulates its tiles and keeps them, so both end
		// up holding the whole array for as long as the result is of interest.
		result.residentMemory = result.resultingMemory;

	// Work scales with the widest domain involved, not with the result's own: an aggregation
	// visits every input element to produce one. Per-family refinements sharpen this (§4.3 of
	// doc/development/schedule-with-lookahead.md); this is the honest default.
	SizeT nrElemOps = result.resultingNrElements;
	for (const auto& argRef : args)
		if (auto argItem = GetItem(argRef); argItem && IsDataItem(argItem))
		{
			auto argAdi = AsDataItem(argItem);
			auto argCount = EstimateDomainCount(argAdi->GetAbstrDomainUnit());
			auto argBytes = EstimateDataBytes(argAdi, argCount.expected);
			result.inputSize += argBytes;
			// Inputs this op is the last consumer of are released by its completion. Our own
			// supplier interest is still held at estimation time, so a count of 1 means: only us.
			// Conservative by construction: config items, kept items and shared inputs count 0.
			if (argItem->IsCacheItem() && !argItem->GetKeepDataState() && argItem->GetInterestCount() <= 1)
				result.reclaimableInputMemory += argBytes;

			// An argument that has not been materialized yet is materialized by US, when we pull it,
			// inside our own CancelableFrame -- so its allocation belongs to this operation, not to
			// the producer that only built the tile functor. That is what makes an aggregation's
			// measured peak track its input rather than its result (§8.1.16).
			// Only knowable once the argument HAS a data object, i.e. from the run-time estimate that
			// RefreshEstimateForAdmission takes; at schedule time this term is legitimately 0.
			if (auto argObj = argAdi->GetCurrRefObj())
				switch (argObj->GetMaterialization())
				{
				case materialization::deferred:
					// tiles are kept once pulled, so pulling it through costs its whole volume
					result.argMaterializationMemory += argBytes;
					break;
				case materialization::streaming:
					// a released tile is freed again, so only the tiles in flight are resident --
					// the same bound the result side applies to a streaming result
					if (auto argTiles = argObj->GetTiledRangeData(); argTiles && argTiles->GetNrTiles())
						result.argMaterializationMemory +=
							(argBytes / argTiles->GetNrTiles()) * Min<SizeT>(argTiles->GetNrTiles(), MaxConcurrentTreads());
					break;
				default: // eager / spilled / meta: already materialized, charged to whoever made it
					break;
				}

			MakeMax(nrElemOps, argCount.expected);
			MakeMax(result.confidence, argCount.confidence); // only as good as its worst input
		}
	result.expectedCalcTime = calc_time_t(nrElemOps) * GetGroup()->GetCalcFactor();

	return result;
}
