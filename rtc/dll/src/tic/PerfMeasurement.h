// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_PERFMEASUREMENT_H)
#define __TIC_PERFMEASUREMENT_H

/*
File: PerfMeasurement.h
Purpose:
- Measure what operator evaluations and storage reads actually cost, and report it against what
  was predicted for them, under MsgCategory::performance.

This is stage P0 of doc/development/schedule-with-lookahead.md: it only observes. No scheduling
decision reads these numbers, and nothing here runs unless the PerformanceLogging setting is on,
so an unmeasured run pays one relaxed atomic load per payload.

The residual line reports the estimate's confidence alongside the ratio, because an estimate that
is 10x off with 'assumed' confidence is the estimator working as designed, while the same ratio
with 'derived' confidence is a modelling defect.
*/

#include "TicBase.h"
#include "Operator.h"
#include "utl/Environment.h"

#include <chrono>

// Stopwatch that only reads the clock when its caller is going to report. Take the enabled flag
// once and pass it here, so the decision to measure and the decision to report cannot disagree.
struct PerfTimer
{
	explicit PerfTimer(bool enabled) : m_Start(enabled ? clock_t::now() : clock_t::time_point()) {}

	auto ElapsedMSec() const -> Float64
	{
		return std::chrono::duration<Float64, std::milli>(clock_t::now() - m_Start).count();
	}

private:
	using clock_t = std::chrono::steady_clock;
	clock_t::time_point m_Start;
};

// The item's domain element count once its data is there, or Undefined when establishing it would
// cost more than the measurement is worth: an unresolved count must never be forced to report on it.
TIC_CALL SizeT ResolvedNrElements(const TreeItem* item);

// Predict an operator evaluation's cost, for later comparison against the measurement. Returns a
// default (all-zero, 'assumed') record when the estimator itself fails: a prediction is never
// allowed to break the calculation it describes.
TIC_CALL auto EstimateOperPerformance(const Operator* oper, TreeItemDualRef& resultHolder
	, const ArgRefs& args) -> PerformanceEstimationData;

// Report a completed operator evaluation against its estimates. 'actualNrElements' is the result
// domain's now-resolved count, or Undefined when it could not be established cheaply.
//
// Two estimates, because the difference between them is the point: 'scheduleEstimate' is what was
// knowable when the operation was queued, 'runEstimate' what is knowable once its suppliers are
// done and it is about to run -- which is where the admission gate of §5.1 actually sits. For a
// calculated domain the first is usually blind (neither its count nor its tiling exists yet) and
// the second is exact, so reporting both measures what re-estimation (§6.2) is worth.
TIC_CALL void ReportOperPerformance(CharPtr operName, const TreeItem* result
	, const PerformanceEstimationData& scheduleEstimate, const PerformanceEstimationData& runEstimate
	, Float64 elapsedMSec, SizeT actualNrElements);

// Predict a pending storage read. Reads are not Operators, so they get their estimate here:
// StorageMetaInfo::PrepareReadDataOrSuspend has already resolved the domain count and the values
// range by the time the gated task runs, and tiling comes from the storage's native geometry --
// so unlike a calculation, a read's size is knowable before it starts. §2.6 and §4.3 of the plan.
TIC_CALL auto EstimateReadResources(const TreeItem* focusItem) -> PerformanceEstimationData;

// Report a completed storage read against that estimate, establishing the per-manager throughput
// a read cost model will be calibrated against.
TIC_CALL void ReportReadPerformance(const TreeItem* focusItem, const PerformanceEstimationData& estimate
	, Float64 elapsedMSec);

#endif // __TIC_PERFMEASUREMENT_H
