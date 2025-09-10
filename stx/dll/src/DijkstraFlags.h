// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// DijkstraFlags.h
// Purpose:
//   Defines DijkstraFlag, a 64-bit bitmask enumeration controlling variants of
//   (multi-source) Dijkstra / OD matrix / interaction / trip distribution
//   computations, output products, impedance modeling, and sparse filtering.
//
//   The flags fall roughly into these categories:
//     - Core algorithm behavior (VerboseLogging, Bidirectional, OD, etc.).
//     - Origin (Org*) and destination (Dst*) data specification and attributes.
//     - Impedance modeling variants (UseAltLinkImp, UseLinkAttr, DistDecay, DistLogit).
//     - Production / output selection for OD matrices and aggregated origin metrics.
//     - Interaction / trip distribution modeling (TripDistr, Interaction, etc.).
//     - Spatial / Euclidean pre-filters (OrgZoneLoc, DstZoneLoc, UseEuclidicFilter).
//     - Composite convenience bundles (TripDistr, Interaction, etc.).
//
//   Notes:
//     * OD implies multiple sources (multi-source Dijkstra).
//     * OrgZone requires OD (zones partition origins).
//     * Some flags require additional per-edge mass/attributes (UseAltLinkImp, UseLinkAttr).
//     * Composite flags are combinations; keep them synchronized with individual base flags.
//     * Bit values must remain stable (persisted settings / parsing logic).
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STX_DIJKSTRAFLAGS_H)
#define __STX_DIJKSTRAFLAGS_H

#include "StxBase.h"
#include "set/enum_flags.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_DIJKSTRA true
#endif // defined(MG_DEBUG)

// 64-bit mask; do NOT reorder or renumber existing flags.
enum class DijkstraFlag : UInt64
{
	None = 0,                     // No options.

	// Core algorithm control
	VerboseLogging = 0x01,        // Emit detailed tracing/logging (debug / diagnostics).
	Bidirectional  = 0x02,        // Use bidirectional Dijkstra search (faster for point-to-point).
	BidirFlag      = 0x04,        // Internal helper flag/state marker for bidirectional processing.

	// OD (Origin-Destination) mode
	OD             = 0x08,        // Multi-source OD style run: treat each origin as separate source set.

	// Origin specification / attributes
	OrgNode        = 0x10,        // Explicit subset of nodes act as origins (else: all nodes). Required when not OD.
	OrgZone        = 0x20,        // Partition selected origin nodes into zones (implies OD semantics).
	OrgImp         = 0x40,        // Per-origin impedance attribute present.
	OrgData        = OrgNode | OrgZone | OrgImp, // Bundle of all origin specification data.

	// Destination specification / attributes
	DstNode        = 0x0080,      // Subset of nodes act as destinations; else all candidate nodes.
	DstZone        = 0x0100,      // Destination zones specified.
	DstImp         = 0x0200,      // Per-destination impedance attribute present.
	DstData        = DstNode | DstZone | DstImp, // Bundle of all destination specification data.

	// Sparsity / filtering
	ImpCut         = 0x0400,      // Cutoff: ignore paths above an impedance threshold.
	DstLimit       = 0x0800,      // Limit number of destinations stored per origin (top-K).
	SparseResult   = ImpCut | DstLimit, // Any sparse limiting behavior.

	// Alternative impedance / link attributes
	UseAltLinkImp  = 0x1000,      // Requires additional per-edge mass; compute alternative impedance.
	UseLinkAttr    = 0x2000,      // Requires per-edge attribute; produce alt distance / attribute outputs.

	// Output / storage mode toggles
	UInt64_Od      = 0x4000,      // Store OD indices / metrics in 64-bit (extended range).

	// Minimum impedance constraints
	OrgMinImp      = 0x8000,      // Enforce per-origin minimum impedance threshold.
	DstMinImp      = 0x1'0000,    // Enforce per-destination minimum impedance threshold.

	// Interaction modeling flags
	InteractionVi  = 0x4'0000'0000'0000, // Include Vi term (production-side interaction variable).
	InteractionWj  = 0x8'0000'0000'0000, // Include Wj term (attraction-side interaction variable).

	// Distance / deterrence forms
	InteractionAlpha = 0x2'0000,  // Include alpha parameter in interaction potential.
	DistDecay        = 0x4'0000,  // Apply exponential or general decay function.
	DistLogit        = 0x8'0000,  // Use logit-based impedance / choice formulation.

	// Production flags (non OD / or aggregated outputs)
	ProdTraceBack    = 0x10'0000, // Store predecessor info for path reconstruction (non-OD single-source).
	ProdOdLinkSet    = 0x20'0000, // Produce per-OD link set (set of traversed edges).

	// OD relative outputs
	ProdOdOrgZone_rel    = 0x100'0000, // Output: origin zone (per OD pair).
	ProdOdDstZone_rel    = 0x200'0000, // Output: destination zone (per OD pair).
	ProdOdImpedance      = 0x400'0000, // Output: impedance (primary).
	ProdOdAltImpedance   = 0x800'0000, // Output: alternative impedance (if UseAltLinkImp).
	ProdOdLinkAttr       = 0x1000'0000, // Output: aggregated link attribute per OD pair.

	// Aggregated origin / destination factors
	ProdOrgFactor     = 0x1'0000'0000, // Output: production factor for origin zone/node.
	ProdOrgDemand     = 0x2'0000'0000, // Output: demand measure at origin.
	ProdOrgMaxImp     = 0x4'0000'0000, // Output: maximum impedance reached / considered per origin.
	ProdDstFactor     = 0x8'0000'0000, // Output: attraction factor for destination.
	ProdDstSupply     = 0x10'0000'0000, // Output: supply measure at destination.
	ProdLinkFlow      = 0x20'0000'0000, // Output: per-link flow accumulation.

	// Aggregated origin statistics
	ProdOrgNrDstZones  = 0x40'0000'0000, // Output: number of distinct destination zones reached.
	ProdOrgSumImp      = 0x80'0000'0000, // Output: sum of impedances over reachable destinations.
	ProdOrgSumLinkAttr = 0x100'0000'0000, // Output: sum of link attributes over reachable destinations.
	ProdOrgSqrtDist    = 0x200'0000'0000, // Planned: sqrt(distance) statistic (reserved).
	ProdOrgDist        = 0x400'0000'0000, // Planned: distance statistic (reserved).

	Counting           = 0x800'0000'0000, // Enable counting-only mode (for performance / diagnostics).

	// OD geometric relative outputs
	ProdOdStartPoint_rel = 0x1000'0000'0000, // Output: geometry ref for OD start (relative).
	ProdOdEndPoint_rel   = 0x2000'0000'0000, // Output: geometry ref for OD end (relative).

	// Spatial location / filtering
	OrgZoneLoc        = 0x4000'0000'0000, // Origin zone location geometry provided.
	DstZoneLoc        = 0x8000'0000'0000, // Destination zone location geometry provided.
	UseEuclidicFilter = 0x1'0000'0000'0000, // Use Euclidean distance pre-filter (prune by straight-line test).

	PrecalculatedNrDstZones = 0x2'0000'0000'0000, // Use externally precomputed number of destination zones.

	// Composite Euclidean group
	EuclidFlags = OrgZoneLoc | DstZoneLoc | UseEuclidicFilter, // Any Euclidean spatial capability.

	// Composite bundles (high-level modeling presets)
	TripDistr        = ProdOrgFactor | ProdOrgDemand | ProdDstFactor | ProdDstSupply | ProdOrgSumImp | ProdOrgSumLinkAttr, // Trip distribution essentials.
	Interaction      = TripDistr | ProdLinkFlow, // Adds link flow to trip distribution.
	InteractionOrMaxImp = Interaction | ProdOrgMaxImp, // Interaction plus maximum impedance per origin.
	Calc_pot_ij      = ProdDstFactor | ProdDstSupply | ProdLinkFlow | ProdOrgSumImp | ProdOrgSumLinkAttr, // Potential calculation (i-j pair metrics).

	OD_Data = ProdOdOrgZone_rel | ProdOdDstZone_rel | ProdOdStartPoint_rel | ProdOdEndPoint_rel | ProdOdImpedance | ProdOdAltImpedance | ProdOdLinkAttr | ProdOdLinkSet, // All per-OD outputs.

	// TmbTB = UseAltLinkImp | ProdLinkFlow | ProdOdLinkSet, // (Commented legacy bundle)
};

ENUM_FLAGS_EX(DijkstraFlag, UInt64)

// Parsing utility:
// ParseDijkstraString:
//   Input: textual (case-insensitive) list of flag tokens separated by delimiters (e.g., ',', '|', whitespace).
//   Output: Combined DijkstraFlag mask. Unknown tokens should be handled (implementation elsewhere).
SYNTAX_CALL DijkstraFlag ParseDijkstraString(CharPtr str);

#endif // __STX_DIJKSTRAFLAGS_H