// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "PerfMeasurement.h"
#include "utl/Registry.h" // IsPerformanceLogging

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "ItemLocks.h"
#include "TiledRangeData.h"
#include "TreeItem.h"
#include "dbg/Check.h"
#include "dbg/SeverityType.h"
#include "utl/StrFormat.h"

namespace {

	// actual/estimate as a printable factor. Both zero (a void domain, say) is a match, not a 0x miss.
	Float64 Residual(SizeT actual, SizeT estimate)
	{
		if (!estimate)
			return actual ? 0.0 : 1.0;
		return Float64(actual) / Float64(estimate);
	}

	// Compact byte size. MsgDispatch truncates a message at 256 characters, so these lines have to
	// stay terse or they lose their tail -- which is where the schedule-vs-run comparison sits.
	SharedStr Bytes(SizeT n)
	{
		// Two decimals above 1 MB. Integer division flattened everything in [1 GB, 2 GB) to "1G",
		// which made five different operators report an identical peak and hid whether a sequence
		// result was carrying its element array or only its 16-byte-per-sequence index (§8.1.16).
		if (n >= (SizeT(1) << 30)) return mySSPrintF("{:.2f}G", Float64(n) / Float64(SizeT(1) << 30));
		if (n >= (SizeT(1) << 20)) return mySSPrintF("{:.2f}M", Float64(n) / Float64(SizeT(1) << 20));
		if (n >= (SizeT(1) << 10)) return mySSPrintF("{}K", n >> 10);
		return mySSPrintF("{}", n);
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
	if (!domain)
		return UNDEFINED_VALUE(SizeT);
	// Probed on the range item, under interest: IsDataReady's Debug preconditions require both --
	// a delegating unit is not its own range item, and an uninterested item's answer is volatile
	// (#1181). Either miss means the count is not resolvable, which the report renders as n=?.
	auto rangeItem = domain->GetCurrRangeItem();
	if (!rangeItem || !rangeItem->HasInterest() || !IsDataReady(rangeItem.get()))
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
	, Float64 elapsedMSec, SizeT actualNrElements, SizeT actualAllocBytes, SizeT actualPeakBytes)
{
	assert(IsPerformanceLogging()); // callers gate; keep the formatting off the hot path

	// The run-time estimate is the better one and the one a gate would use; the schedule-time
	// figure is reported alongside only where it disagreed, which is the interesting case.
	const auto& estimate = runEstimate;
	auto resultLabel = ItemLabel(result);
	auto scheduleStr = (scheduleEstimate.resultingNrElements == runEstimate.resultingNrElements
		&& scheduleEstimate.regime == runEstimate.regime)
		? SharedStr()
		: mySSPrintF(" | sched n={} {} {}"
			, scheduleEstimate.resultingNrElements, AsString(scheduleEstimate.regime)
			, AsString(scheduleEstimate.confidence));
	auto workStr = estimate.workingMemorySize
		? mySSPrintF(" ws={}", Bytes(estimate.workingMemorySize))
		: SharedStr();
	if (estimate.reclaimableInputMemory) // last-consumer inputs: what this op's completion releases
		workStr = workStr + mySSPrintF(" rel={}", Bytes(estimate.reclaimableInputMemory));

	// MEASURED allocation demand vs what was predicted to be needed to run: the residual that says
	// whether the estimate is fit to schedule on. Counts every >= 4 KB allocation charged to this
	// operation's CancelableFrame -- tile work on other workers included, and temporary memory any
	// operator took along the way, which is the part no estimate models today. A ratio far above 1
	// is an operator whose real demand the gate cannot see (the GEOS/GDAL case of §8.1.13c).
	if (estimate.argMaterializationMemory) // arguments this op materializes itself; see §8.1.16
		workStr = workStr + mySSPrintF(" argmat={}", Bytes(estimate.argMaterializationMemory));

	if (actualAllocBytes || actualPeakBytes)
	{
		auto predictedDemand = estimate.residentMemory + estimate.workingMemorySize
			+ estimate.argMaterializationMemory;
		// gross= is allocation TRAFFIC (churn included); peak= is the high-water of what this
		// operation actually held at once, which is what an admission budget must be compared
		// against and what residentMemory is trying to predict. The ratio is taken on peak for that
		// reason -- grading against gross would condemn every streaming operator for recycling tiles.
		workStr = workStr + mySSPrintF(" gross={} peak={}{}"
			, Bytes(actualAllocBytes), Bytes(actualPeakBytes)
			, predictedDemand
				? mySSPrintF(" ({:.2f}x pred)", Float64(actualPeakBytes) / Float64(predictedDemand))
				: SharedStr());
	}

	if (!result || !IsDataItem(result))
	{
		// A unit or container result: the operator only did meta work, so there is nothing to
		// compare. Still worth timing -- meta work on the meta thread delays everything behind it.
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "oper {} {}: {:.1f}ms meta result, no data cost", operName, resultLabel, elapsedMSec);
		return;
	}

	// Naming the regime is what keeps these lines comparable. Under deferred/streaming the elapsed
	// time is tile-functor construction, not compute -- the element work is charged to whoever
	// pulls the tiles -- and residentMemory is what a ledger would book, which for streaming is
	// concurrency x one tile rather than the whole array (§4.4 of the plan; /C3 for eager numbers).
	// The regime the object actually got, beside the prediction: several channels build a
	// recalculating object whatever LazyCalculated says (doc/tile-data-retainment.md §4.3), so a
	// disagreement here is an estimator defect worth seeing rather than a detail.
	// GetCurrDataObj, NOT GetRefObj: this runs on the OperationContext worker that just produced
	// the result, and GetRefObj asserts IsMetaThread -- its GetUltimateItem walk can trigger meta
	// processing -- which made any Debug run with a data-producing operator die here (#1181). The
	// caller passes resultHolder.GetUlt(), already the ultimate item, whose m_DataObject is the
	// object the operation just stored (asserted at the call site) and is kept alive by the
	// running context's interest; reading it directly needs no walk and no meta processing.
	auto actualRegime = estimate.regime;
	if (auto ado = AsDataItem(result)->GetCurrDataObj())
		actualRegime = ado->GetMaterialization();
	auto regimeMismatch = (actualRegime != estimate.regime)
		? mySSPrintF(" !regime={} predicted={}", AsString(actualRegime), AsString(estimate.regime))
		: SharedStr();

	auto regimeStr = (estimate.regime == materialization::eager)
		? mySSPrintF("eager res={}", Bytes(estimate.residentMemory))
		: mySSPrintF("{} {}x{} res={}", AsString(estimate.regime), estimate.nrChores
			, Bytes(estimate.choreMemory), Bytes(estimate.residentMemory));

	if (!IsDefined(actualNrElements))
	{
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "oper {} {}: {:.1f}ms n=? est n={} B={} {} {}{}{}"
			, operName, resultLabel, elapsedMSec
			, estimate.resultingNrElements, Bytes(estimate.resultingMemory)
			, AsString(estimate.confidence), regimeStr, workStr, scheduleStr + regimeMismatch
		);
		return;
	}

	auto actualBytes = EstimateDataBytes(AsDataItem(result), actualNrElements);
	reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
		, "oper {} {}: {:.1f}ms n={} ({:.2f}x {}) B={} ({:.2f}x) ops={:.0f} {}{}{}"
		, operName, resultLabel, elapsedMSec
		, actualNrElements, Residual(actualNrElements, estimate.resultingNrElements), AsString(estimate.confidence)
		, Bytes(actualBytes), Residual(actualBytes, estimate.resultingMemory)
		, estimate.expectedCalcTime
		, regimeStr, workStr, scheduleStr + regimeMismatch
	);
}

TIC_CALL auto EstimateReadResources(const TreeItem* focusItem) -> PerformanceEstimationData
{
	auto result = PerformanceEstimationData();
	if (!focusItem || !IsDataItem(focusItem))
	{
		result.regime = materialization::meta; // a unit range read: the range itself, no bulk data
		result.confidence = estimate_confidence::derived;
		return result;
	}

	auto adi = AsDataItem(focusItem);
	try {
		auto domain = adi->GetAbstrDomainUnit();
		auto count = domain->EstimateCount();
		result.resultingNrElements = count.expected;
		result.resultingMemory = EstimateDataBytes(adi, count.expected);
		result.resultingMemoryUpperBound = EstimateDataBytes(adi, count.upperBound);
		result.confidence = count.confidence;
		// guarded like Operator::EstimatePerformance: GetNrTiles MG_CHECKs a not-yet-calculated
		// range, which the surrounding catch recovered from in Release but killed Debug (#1181)
		if (auto rangeItem = domain->GetCurrRangeItem())
			if (auto trd = AsUnit(rangeItem.get())->GetTiledRangeData())
			{
				result.nrChores = trd->GetNrTiles();
				result.choreMemory = EstimateDataBytes(adi, trd->GetMaxTileSize());
			}
		// A read's bytes cross the storage boundary; that traffic IS the cost, and the result holds
		// the same volume. Tiles are written into the result array, so nothing is streamed away.
		result.ioBytes = result.resultingMemory;
		result.residentMemory = result.resultingMemory;
		result.regime = materialization::eager;
	}
	catch (...) {
		result.confidence = estimate_confidence::assumed;
	}
	return result;
}

TIC_CALL void ReportReadPerformance(const TreeItem* focusItem, const PerformanceEstimationData& estimate
	, Float64 elapsedMSec)
{
	assert(IsPerformanceLogging());

	auto itemName = ItemLabel(focusItem);
	auto nrElements = ResolvedNrElements(focusItem);

	if (!IsDefined(nrElements))
	{
		reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
			, "read {}: {:.1f}ms n=? est B={} {}"
			, itemName, elapsedMSec, Bytes(estimate.ioBytes), AsString(estimate.confidence));
		return;
	}

	auto nrBytes = EstimateDataBytes(AsDataItem(focusItem), nrElements);
	reportF(MsgCategory::performance, SeverityTypeID::ST_MinorTrace
		, "read {}: {:.1f}ms n={} ({:.2f}x {}) B={} ({:.2f}x) {:.1f}MB/s {} chores"
		, itemName, elapsedMSec
		, nrElements, Residual(nrElements, estimate.resultingNrElements), AsString(estimate.confidence)
		, Bytes(nrBytes), Residual(nrBytes, estimate.ioBytes)
		, elapsedMSec > 0.0 ? (Float64(nrBytes) / (1024.0 * 1024.0)) / (elapsedMSec / 1000.0) : 0.0
		, estimate.nrChores
	);
}
