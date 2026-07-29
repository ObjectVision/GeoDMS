// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "PerfMeasurement.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "ItemLocks.h"
#include "TreeItem.h"
#include "dbg/Check.h"
#include "dbg/SeverityType.h"
#include "utl/mySPrintF.h"

namespace {

	CharPtr AsString(estimate_confidence c)
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

	// actual/estimate as a printable factor. Both zero (a void domain, say) is a match, not a 0x miss.
	Float64 Residual(SizeT actual, SizeT estimate)
	{
		if (!estimate)
			return actual ? 0.0 : 1.0;
		return Float64(actual) / Float64(estimate);
	}

	// Cache results are parentless, so GetFullName can be empty; the source name still identifies them.
	SharedStr ItemLabel(const TreeItem* item)
	{
		if (!item)
			return SharedStr("<no result>");
		auto name = item->GetFullName();
		return name.empty() ? item->GetSourceName() : name;
	}

} // anonymous namespace

TIC_CALL SizeT ResolvedNrElements(const TreeItem* item)
{
	if (!item || !IsDataItem(item))
		return UNDEFINED_VALUE(SizeT);
	auto domain = AsDataItem(item)->GetAbstrDomainUnit();
	if (!domain || !IsDataReady(domain))
		return UNDEFINED_VALUE(SizeT);
	return domain->GetCount();
}

TIC_CALL auto EstimateOperPerformance(const Operator* oper, TreeItemDualRef& resultHolder
	, const ArgRefs& args) -> PerformanceEstimationData
{
	assert(IsPerformanceLogging());

	try {
		return oper->EstimatePerformance(resultHolder, args);
	}
	catch (...)
	{
		// A SizeEstimator rule can fail or a domain can refuse to be counted; that is a fact about
		// the estimate, not about the operation, so swallow it and report zero confidence.
		return PerformanceEstimationData();
	}
}

TIC_CALL void ReportOperPerformance(CharPtr operName, const TreeItem* result
	, const PerformanceEstimationData& scheduleEstimate, const PerformanceEstimationData& runEstimate
	, Float64 elapsedMSec, SizeT actualNrElements)
{
	assert(IsPerformanceLogging()); // callers gate; keep the formatting off the hot path

	// The run-time estimate is the better one and the one a gate would use; the schedule-time
	// figure is reported alongside only where it disagreed, which is the interesting case.
	const auto& estimate = runEstimate;
	auto resultLabel = ItemLabel(result);
	auto scheduleStr = (scheduleEstimate.resultingNrElements == runEstimate.resultingNrElements
		&& scheduleEstimate.regime == runEstimate.regime)
		? SharedStr()
		: mySSPrintF("; at schedule time: est n {}, {}, {} confidence"
			, scheduleEstimate.resultingNrElements, AsString(scheduleEstimate.regime)
			, AsString(scheduleEstimate.confidence));

	if (!result || !IsDataItem(result))
	{
		// A unit or container result: the operator only did meta work, so there is nothing to
		// compare. Still worth timing -- meta work on the meta thread delays everything behind it.
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "oper {}: {} meta result, no data cost; took {:.1f} ms", operName, resultLabel, elapsedMSec);
		return;
	}

	// Naming the regime is what keeps these lines comparable. Under deferred/streaming the elapsed
	// time is tile-functor construction, not compute -- the element work is charged to whoever
	// pulls the tiles -- and residentMemory is what a ledger would book, which for streaming is
	// concurrency x one tile rather than the whole array (§4.4 of the plan; /C3 for eager numbers).
	auto regimeStr = (estimate.regime == materialization::eager)
		? mySSPrintF("eager, resident {}", estimate.residentMemory)
		: mySSPrintF("{}, {} chores of {} B, resident {} (element work deferred to tile pull)"
			, AsString(estimate.regime), estimate.nrChores, estimate.choreMemory, estimate.residentMemory);

	if (!IsDefined(actualNrElements))
	{
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "oper {}: {} took {:.1f} ms; actual size not established; est n {}, est bytes {}, est {} confidence; {}"
			, operName, resultLabel
			, elapsedMSec
			, estimate.resultingNrElements, estimate.resultingMemory, AsString(estimate.confidence)
			, regimeStr + scheduleStr
		);
		return;
	}

	auto actualBytes = EstimateDataBytes(AsDataItem(result), actualNrElements);
	reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
		, "oper {}: {} took {:.1f} ms; n {} vs est {} ({:.2f}x); bytes {} vs est {} ({:.2f}x); est {} confidence, est {:.0f} elem-ops; {}"
		, operName, resultLabel
		, elapsedMSec
		, actualNrElements, estimate.resultingNrElements, Residual(actualNrElements, estimate.resultingNrElements)
		, actualBytes, estimate.resultingMemory, Residual(actualBytes, estimate.resultingMemory)
		, AsString(estimate.confidence), estimate.expectedCalcTime
		, regimeStr + scheduleStr
	);
}

TIC_CALL void ReportReadPerformance(const TreeItem* focusItem, Float64 elapsedMSec)
{
	assert(IsPerformanceLogging());

	auto itemName = ItemLabel(focusItem);
	auto nrElements = ResolvedNrElements(focusItem);

	if (!IsDefined(nrElements))
	{
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "read {}: took {:.1f} ms; size not established", itemName, elapsedMSec);
		return;
	}

	auto nrBytes = EstimateDataBytes(AsDataItem(focusItem), nrElements);
	reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
		, "read {}: took {:.1f} ms; n {}; bytes {}; {:.1f} MB/s"
		, itemName, elapsedMSec, nrElements, nrBytes
		, elapsedMSec > 0.0 ? (Float64(nrBytes) / (1024.0 * 1024.0)) / (elapsedMSec / 1000.0) : 0.0
	);
}
