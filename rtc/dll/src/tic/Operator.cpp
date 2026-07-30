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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "Operator.h"

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
#include "TiledRangeData.h"
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
	if (!IsMultiThreaded3() || nrTiles <= 1 || IsInMMD(res))
		return materialization::eager;
	return res->GetLazyCalculatedState() ? materialization::streaming : materialization::deferred;
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
	result.resultingMemory = EstimateDataBytes(adi, domainCount.expected);
	result.resultingMemoryUpperBound = EstimateDataBytes(adi, domainCount.upperBound);

	// Tiling, then the regime it enables, then what is actually resident under that regime.
	tile_offset maxTileSize = 0;
	try {
		result.nrChores = domain->GetNrTiles();
		if (auto trd = domain->GetTiledRangeData())
			maxTileSize = trd->GetMaxTileSize();
	}
	catch (...) {}
	result.extraTasks = UInt16(Min<SizeT>(result.nrChores, MAX_VALUE(UInt16)));
	result.choreMemory = maxTileSize ? EstimateDataBytes(adi, maxTileSize)
		: (result.nrChores ? result.resultingMemory / result.nrChores : result.resultingMemory);

	result.regime = PredictMaterialization(adi, result.nrChores);
	if (result.regime == materialization::streaming)
	{
		// Only the tiles being pulled right now exist; a released tile is freed and recomputed if
		// pulled again. The bound is concurrency x one tile, never the whole array.
		auto inflight = Min<SizeT>(result.nrChores, MaxConcurrentTreads());
		result.residentMemory = result.choreMemory * inflight;
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
			result.inputSize += EstimateDataBytes(argAdi, argCount.expected);
			MakeMax(nrElemOps, argCount.expected);
			MakeMax(result.confidence, argCount.confidence); // only as good as its worst input
		}
	result.expectedCalcTime = calc_time_t(nrElemOps) * GetGroup()->GetCalcFactor();

	return result;
}
