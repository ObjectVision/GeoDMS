// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
// File: Dijkstra.cpp
//
// High-level purpose:
//   Implements a versatile Dijkstra-based engine supporting:
//     * Standard single-source multi-destination shortest path
//     * OD impedance matrix generation (sparse or dense)
//     * Optional per-link alternative impedances and attributes
//     * Interaction / gravity / logit style potential & flow calculations
//     * Cumulative mass limit cutoffs (DstLimit)
//     * Org / Dst specific min / max impedance constraints
//     * Euclidean pruning (optional)
//     * Path reconstruction via TraceBack or LinkSet output
//     * Link-flow accumulation (assignment style)
//     * Multi-threaded per-origin parallelization with thread-local heaps
//
// Core components overview:
//   - TreeRelations: Lightweight parent/child traversal support for accepted nodes.
//   - NetworkInfo:    Immutable network + zone relational metadata (nodes/links/zones/start/end).
//   - GraphInfo:      Adjacency inversion (node -> incident edge lists).
//   - NodeZoneConnector: Tracks which destination zones have been reached per origin
//                        (dense vs sparse result modes) and maps result indices.
//   - ResultInfo:     Bundled raw pointers into output buffers (filled inside ProcessDijkstra).
//   - ProcessDijkstra: The main iterative multi-origin driver; executes parallel for each origin.
//   - DijkstraMatrOperator<T>: Operator wrapper exposing functionality to scripting/runtime.
//
// Concurrency design:
//   - Each origin zone is processed independently in parallel (parallel_for).
//   - Thread-local combinables hold: heap (OwningDijkstraHeap), TreeRelations, NodeZoneConnector,
//     temporary vectors (potentials, link-flow buffers), minimizing contention.
//   - Output writes requiring aggregation (e.g., dstZone_Factor, dstZone_Supply, LinkSet)
//     use fine-grained leveled_critical_section locks.
//
// Heap / traversal notes:
//   - The custom heap supports "stale" nodes & finalization semantics to avoid redundant processing.
//   - Endpoints may have their own impedance offsets (entered post-finalization via a secondary heap).
//   - Euclidean pruning short-circuits CommitY if geometric distance exceeds a threshold.
//
// Interaction model (when enabled via flags):
//   - Computes potentials t_ij with either power/log decay or logit formulation
//   - Aggregates D_i (OrgZone_Factor), flows M_ix (OrgZone_Demand), supplies M_xj (DstZone_Supply)
//   - Optionally accumulates impedance and attribute weighted sums (OrgZone_SumImp / SumLinkAttr)
//   - Supports alt exponents via alpha, decay beta, logit parameters.
//   - Optional link-flow accumulation reconstructs subtree demands bottom-up.
//
// Path reconstruction modes:
//   - TraceBack array (per node) (node_TB) for single tree extraction
//   - Per OD LinkSet sequences (od_LS) built by reverse walking TB links
//
// Sparse vs Dense mode decision:
//   - Sparse result when flags(df & SparseResult) and OD mode — uses stamping arrays per origin
//   - Dense mode: directly positions OD results at (org * nrDst + dst)
//
// Safety & assertions:
//   - Defensive checks on undefined values, bounds, monotonic commits, and flag consistency.
//   - Many MG_CHECK/MG_USERCHECK macros enforce invariants, fail fast on inconsistent inputs.
//
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "Dijkstra.h"

#include "dbg/SeverityType.h"
#include "dbg/Timer.h"
#include "geo/Point.h"
#include "mci/ValueClassID.h"
#include "utl/scoped_exit.h"

#include "LockLevels.h"

#include "CheckedDomain.h"
#include "DataCheckMode.h"
#include "DataItemClass.h"
#include "OperationContext.h"
#include "ParallelTiles.h"
#include "UnitClass.h"

#include "makeCululative.h"

#include "TreeBuilder.h"
#include "DijkstraFlags.h"
#include "InvertedRel.h"

#include <numeric>
#include <semaphore>

// Utility absolute template (kept inline for performance)
template <class T>
inline T absolute(const T& x)
{
	return (x<0) ? -x : x;
}

// Input validation helpers
void CheckNoneMode(const AbstrDataItem* adi, CharPtr role)
{
	if (adi && adi->DetermineActualCheckMode() != DCM_None)
		adi->throwItemError(SharedStr(role) + ": out of range or undefined values not allowed");
}

void CheckDefineMode(const AbstrDataItem* adi, CharPtr role)
{
	if (adi && adi->DetermineActualCheckMode() & DCM_CheckDefined)
		adi->throwItemError(SharedStr(role) + ": undefined values not allowed");
}
bool HasVoidDomainGuarantee(const AbstrDataItem* adi)
{
	return adi && adi->HasVoidDomainGuarantee();
}

// Validates mutually exclusive and dependent DijkstraFlag combinations.
// Centralizes user-facing correctness constraints.
void CheckFlags(DijkstraFlag df)
{
	MG_USERCHECK2(!flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::ProdTraceBack), "Cannot produce a TraceBack attribute for an impedance matrix");
	MG_USERCHECK2(flags(df & DijkstraFlag::OD) || flags(df & DijkstraFlag::OrgNode), "OrgNode required for impedance_table");
	MG_USERCHECK2(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::OrgZone), "OrgZone is not allowed for impedance_table");
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::UInt64_Od));
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::OD_Data));
	MG_CHECK(flags(df & DijkstraFlag::OD) || !flags(df & DijkstraFlag::PrecalculatedNrDstZones));
	MG_USERCHECK2(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::InteractionAlpha), "pointless specification of InteractionAlpha without request for producing trip distribution output.");
	MG_USERCHECK2(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::InteractionVi), "pointless specification of V_i without request for producing trip distribution output.");
	MG_USERCHECK2(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::InteractionWj), "pointless specification of W_j without request for producing trip distribution output.");
	MG_USERCHECK2(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::OrgMinImp), "pointless specification of OrgMinImp without request for producing trip distribution output.");
	MG_USERCHECK2(flags(df & DijkstraFlag::Interaction) || !flags(df & DijkstraFlag::DstMinImp), "pointless specification of DstMinImp without request for producing trip distribution output.");
	MG_CHECK(!flags(df & DijkstraFlag::Bidirectional) || !flags(df & DijkstraFlag::BidirFlag));
	MG_USERCHECK2(flags(df & DijkstraFlag::UseAltLinkImp) || !flags(df & DijkstraFlag::ProdOdAltImpedance), "alternative(link_imp) required for alt_imp");
	MG_USERCHECK2(flags(df & DijkstraFlag::UseLinkAttr) || !flags(df & DijkstraFlag::ProdOdLinkAttr), "alternative(link_Attr) required for link_attr");
	MG_USERCHECK2(flags(df & DijkstraFlag::UseLinkAttr) || !flags(df & DijkstraFlag::ProdOrgSumLinkAttr), "alternative(link_Attr) required for interaction:OrgZone_SumLinkAttr");
	MG_CHECK(((df & DijkstraFlag::EuclidFlags) == DijkstraFlag::None) || ((df & DijkstraFlag::EuclidFlags) == DijkstraFlag::EuclidFlags));
}

using sqr_dist_t = UInt32;
using euclid_location_t = SPoint;

// *****************************************************************************
// TreeRelations
// Purpose: Lightweight in-memory parent/child representation of the
//          accepted shortest path forest (one tree per start component).
//          Allows both top-down and bottom-up traversal needed for
//          alternative attribute accumulation and link-flow backprop.
// *****************************************************************************

TreeRelations::TreeRelations(NodeType nrNodes)
	: m_TreeNodes(nrNodes)
{
}

TreeRelations::~TreeRelations()
{
}

TreeRelations::TreeRelations(const LinkType* firstTB, const NodeType* f1, const NodeType* f2, NodeType nrNodes, LinkType nrEdges)
	: m_TreeNodes(nrNodes)
{
	treenode_pointer tnPtr = begin_ptr(m_TreeNodes);

	for (NodeType v = 0; v != nrNodes; ++firstTB, ++tnPtr, ++v)
	{
		LinkType tbEdge = *firstTB;
		if (tbEdge >= nrEdges) continue;
		if (f2[tbEdge] == v && f1[tbEdge] < nrNodes)
			m_TreeNodes[f1[tbEdge]].AddChild(tnPtr);
		if (f1[tbEdge] == v && f2[tbEdge] < nrNodes)
			m_TreeNodes[f2[tbEdge]].AddChild(tnPtr);
	}
	dms_assert(tnPtr == end_ptr(m_TreeNodes));
}

void TreeRelations::InitNodes(NodeType nrNodes)
{
	vector_resize(m_TreeNodes, nrNodes);
}

// *****************************************************************************
// Helper structs: ZoneInfo / NetworkInfo
//  - ZoneInfo: Raw pointers into per-start or per-end arrays for nodes,
//              (optional) impedance offsets, zone_rel remapping, and geographic locations.
//  - NetworkInfo: Logical container describing the full sized network instance
//                 plus structures needed for zone iteration/inversion.
// *****************************************************************************

template <typename NodeType, typename ZoneType, typename ImpType>
struct ZoneInfo {
	const NodeType* Node_rel;
	const ImpType * Impedances;
	const ZoneType* Zone_rel;
	const euclid_location_t* Zone_location;
};

template <typename NodeType, typename ZoneType, typename ImpType>
struct NetworkInfo {
	using zone_info = ZoneInfo<NodeType, ZoneType, ImpType>;
	
	NetworkInfo(NodeType nrV_, LinkType nrE_,
		ZoneType nrX_, ZoneType nrY_, ZoneType nrSrcZones_, ZoneType nrDstZones_,
		zone_info&& startPoints_, zone_info&& endPoints_)
	: nrV(nrV_)
	, nrE(nrE_)
	, nrX(nrX_)
	, nrY(nrY_)
	, nrOrgZones(nrSrcZones_)
	, nrDstZones(nrDstZones_)
	, startPoints(std::move(startPoints_))
	, endPoints(std::move(endPoints_))
	{
		if (startPoints.Zone_rel)
			orgZone_startPoint_inv.Init(startPoints.Zone_rel, nrX, nrOrgZones);
		else
		{
			if (nrOrgZones == 1)
			{
				dms_assert(nrOrgZones == 1);
				orgZone_startPoint_inv.InitAllToOne(nrX);
			}
			else
			{
				dms_assert(nrOrgZones == nrX); // Each X acts as separate org zone when OD
			}
		}
		if (!nrV)
			reportD(SeverityTypeID::ST_Warning, "no nodes in network");
		if (!nrE)
			reportD(SeverityTypeID::ST_Warning, "no edges in network");

		if (!nrOrgZones)
			reportD(SeverityTypeID::ST_Warning, "no OrgZones related to network");
		if (!nrDstZones)
			reportD(SeverityTypeID::ST_Warning, "no DstZones related to network");

		if (!nrX)
			reportD(SeverityTypeID::ST_Warning, "no startpoints to relate OrgZones to network nodes");
		if (!nrY)
			reportD(SeverityTypeID::ST_Warning, "no endpoints to relate network nodes to DstZones");
	}

	NodeType  nrV;
	LinkType  nrE;
	ZoneType nrX, nrY, nrOrgZones, nrDstZones;

	ZoneInfo<NodeType, ZoneType, ImpType> startPoints, endPoints;
	Inverted_rel<ZoneType> orgZone_startPoint_inv;
};

// *****************************************************************************
// GraphInfo:
//   Stores raw arrays for F1/F2 endpoints and original link impedance.
//   Also stores inverted adjacency (node->edge lists) for forward/backward traversal.
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ImpType>
struct GraphInfo {
	const NodeType* linkF1Data;
	const NodeType* linkF2Data;
	const ImpType * linkImpDataPtr;

	Inverted_rel<LinkType> node_link1_inv, node_link2_inv;
};

// *****************************************************************************
// NodeZoneConnector:
//   Manages destination zone reachability state for the current origin.
//   Supports dense mode (direct indexing by dst zone) or sparse mode
//   (compress only reached zones + stamping for O(1) duplicate prevention).
//   Also applies optional Euclidean cutoff (if coordinates provided).
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct NodeZoneConnector 
{
	using network_info = NetworkInfo<NodeType, ZoneType, ImpType>;

	NodeZoneConnector() {}

	void Init(const network_info& networkInfo, DijkstraFlag df, sqr_dist_t euclidSqrDist)
	{
		m_NetworkInfoPtr = &networkInfo;
		m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
		m_EuclidSqrDist = euclidSqrDist;
		if (!m_ResImpPerDstZone) // first-time lazy allocation
		{
			m_ResImpPerDstZone = OwningPtrSizedArray<ImpType>( m_NetworkInfoPtr->nrDstZones, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_ResImpPerDstZone"));

			if (flags(df & DijkstraFlag::OD) && flags(df & DijkstraFlag::SparseResult))
			{
				dms_assert(m_NetworkInfoPtr->nrDstZones);
				m_LastCommittedSrcZone = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrDstZones, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_LastCommittedSrcZone"));
				if (flags(df & DijkstraFlag::ProdLinkFlow))
					m_FoundResPerY = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrY, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_FoundResPerY"));
			}
			if (m_NetworkInfoPtr->endPoints.Zone_rel)
				m_FoundYPerDstZone = OwningPtrSizedArray<ZoneType>(m_NetworkInfoPtr->nrDstZones, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_FoundYPerDstZone"));
			m_OrgZoneLocations = networkInfo.startPoints.Zone_location;
		}
	}

	bool IsDense() const
	{
		return !m_LastCommittedSrcZone;
	}

	void ResetSrc(ZoneType orgZone)
	{
		++m_CurrSrcZoneTick;
		if (IsDense())
		{
			fast_undefine(m_ResImpPerDstZone.begin(), m_ResImpPerDstZone.begin() + m_NetworkInfoPtr->nrDstZones);
			if (m_FoundYPerDstZone)
				fast_undefine(m_FoundYPerDstZone.begin(), m_FoundYPerDstZone.begin() + m_NetworkInfoPtr->nrDstZones);
		}
		else
			m_FoundYPerRes.clear();

		if (m_OrgZoneLocations)
			m_OrgZoneLocation = m_OrgZoneLocations[orgZone];
	}

	// Attempt to commit an endpoint (y) producing an impedance into its zone bucket.
	// Returns false if already committed (monotonic order ensures first is minimal).
	bool CommitY(ZoneType y, ImpType dstImp)
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));
		dms_assert(y < m_NetworkInfoPtr->nrY);
		ZoneType dstZone = m_NetworkInfoPtr->endPoints.Zone_rel ? m_NetworkInfoPtr->endPoints.Zone_rel[y] : y;
		dms_assert(dstZone < m_NetworkInfoPtr->nrDstZones);

		// Euclidean filter (if configured)
		if (m_OrgZoneLocations)
		{
			auto dstLocation = m_NetworkInfoPtr->endPoints.Zone_location[dstZone];
			auto sqrDist = SqrDist<sqr_dist_t>(dstLocation, m_OrgZoneLocation);
			if (sqrDist > m_EuclidSqrDist)
				return false;
		}

		if (IsDense())
		{
			if (IsDefined(m_ResImpPerDstZone[dstZone]))
			{
				dms_assert(m_ResImpPerDstZone[dstZone] <= dstImp);
				return false;
			}
		}
		else
		{
			if (m_LastCommittedSrcZone[dstZone] == m_CurrSrcZoneTick)
			{
				dms_assert(IsDefined(m_ResImpPerDstZone[dstZone]));
				dms_assert(m_ResImpPerDstZone[dstZone] <= dstImp);
				return false;
			}
			m_LastCommittedSrcZone[dstZone] = m_CurrSrcZoneTick;

			ZoneType resIndex = m_FoundYPerRes.size();
			if (m_FoundResPerY)
				m_FoundResPerY[y] = resIndex;
			m_FoundYPerRes.push_back(y);
		}
		m_ResImpPerDstZone[dstZone] = dstImp;
		if (m_FoundYPerDstZone)
			m_FoundYPerDstZone[dstZone] = y;
		return true;
	}

	ZoneType Y2Res(ZoneType y) const
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));
		dms_assert(y < m_NetworkInfoPtr->nrY);
		ZoneType dstZone = LookupOrSame(m_NetworkInfoPtr->endPoints.Zone_rel, y);
		dms_assert(dstZone < m_NetworkInfoPtr->nrDstZones);
		if (IsDense())
		{
			if (!IsDefined(m_ResImpPerDstZone[dstZone]))
				return UNDEFINED_VALUE(ZoneType);
			return dstZone;
		}

		if (m_LastCommittedSrcZone[dstZone] != m_CurrSrcZoneTick)
			return UNDEFINED_VALUE(ZoneType);

		dms_assert(m_FoundResPerY);
		dms_assert(IsDefined(m_FoundResPerY[y]));
		return m_FoundResPerY[y];
	}

	ZoneType ZonalResCount() const
	{
		dms_assert(IsDefined(m_CurrSrcZoneTick));
		if (IsDense())
			return m_NetworkInfoPtr->nrDstZones;
		return (ZoneType)m_FoundYPerRes.size();
	}

	bool IsConnected(ZoneType zoneID) const
	{
		assert(zoneID < m_NetworkInfoPtr->nrDstZones);
		if (!IsDense() && m_LastCommittedSrcZone[zoneID] != m_CurrSrcZoneTick)
			return false;
		return IsDefined(m_ResImpPerDstZone[zoneID]);
	}

	ZoneType Res2EndPoint(ZoneType resIndex) const
	{
		assert(resIndex < ZonalResCount());
		return LookupOrSame(begin_ptr(m_FoundYPerRes), resIndex);
	}

	ZoneType Res2DstZone(ZoneType resIndex) const
	{
		ZoneType y = Res2EndPoint(resIndex);
		return LookupOrSame(m_NetworkInfoPtr->endPoints.Zone_rel, y);
	}
	ZoneType DstZone2EndPoint(ZoneType dstZone) const
	{
		assert(dstZone < m_NetworkInfoPtr->nrDstZones);
		if (IsDense() && !IsConnected(dstZone))
			return UNDEFINED_VALUE(NodeType);
		ZoneType y = LookupOrSame(m_FoundYPerDstZone.begin(), dstZone);
		return y;
	}

	NodeType DstZone2EndNode(ZoneType dstZone) const
	{
		auto y = DstZone2EndPoint(dstZone);
		if (!IsDefined(y))
			return UNDEFINED_VALUE(NodeType);
		NodeType node = LookupOrSame(m_NetworkInfoPtr->endPoints.Node_rel, y);
		assert(node < m_NetworkInfoPtr->nrV);
		return node;
	}

	NodeType Res2EndNode(ZoneType resIndex) const
	{
		ZoneType dstZone = Res2DstZone(resIndex);
		NodeType node = DstZone2EndNode(dstZone);
		return node;
	}

	ImpType Res2Imp(ZoneType resIndex) const
	{
		ZoneType zoneId = Res2DstZone(resIndex);
		dbg_assert(IsConnected(zoneId));
		ImpType d = m_ResImpPerDstZone[zoneId];
		return d;
	}

	const network_info* m_NetworkInfoPtr = nullptr;

	ZoneType m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);

	OwningPtrSizedArray<ImpType>  m_ResImpPerDstZone;
	OwningPtrSizedArray<ZoneType> m_FoundYPerDstZone;
	std::vector<ZoneType>         m_FoundYPerRes;
	OwningPtrSizedArray<ZoneType> m_FoundResPerY;

	OwningPtrSizedArray<ZoneType> m_LastCommittedSrcZone;
	const euclid_location_t*      m_OrgZoneLocations = nullptr;
	euclid_location_t             m_OrgZoneLocation = UNDEFINED_VALUE(euclid_location_t);
	sqr_dist_t                    m_EuclidSqrDist = 0;
};

// *****************************************************************************
// ResultInfo:
//   Bundles raw target pointers for output arrays to avoid repetitive
//   argument lists in ProcessDijkstra. Populated at call site.
// *****************************************************************************
template <typename ZoneType, typename ImpType, typename MassType>
struct ResultInfo {
	using ImpTypeArray = ImpType*;
	using ZoneTypeArray = ZoneType*;
	using MassTypeArray = MassType*;
	using LinkSeqArray = typename sequence_traits<std::vector<LinkType>>::seq_t::iterator;

	ImpTypeArray  od_ImpData = nullptr, od_AltLinkImp = nullptr, od_LinkAttr = nullptr;
	ZoneTypeArray od_SrcZoneIds = nullptr, od_DstZoneIds = nullptr;
	ZoneTypeArray od_StartPointIds = nullptr, od_EndPointIds = nullptr;
	LinkSeqArray od_LS = {};
	LinkType* node_TB = nullptr;

	ZoneTypeArray orgZone_NrDstZones = nullptr;
	MassTypeArray orgZone_Factor     = nullptr;
	MassTypeArray orgZone_Demand     = nullptr;
	MassTypeArray orgZone_SumImp     = nullptr;
	MassTypeArray orgZone_SumLinkAttr= nullptr;
	ImpTypeArray  orgZone_MaxImp     = nullptr;
	MassTypeArray dstZone_Factor     = nullptr;
	MassTypeArray dstZone_Supply     = nullptr;
	MassTypeArray LinkFlow           = nullptr;
};

// *****************************************************************************
// WriteBlock: Fine-grained lock group
//   Locks only when writing shared aggregated arrays that can be updated
//   concurrently across origins (e.g., destination supply).
// *****************************************************************************
struct WriteBlock {
	WriteBlock()
		: od_LS(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.LS")
		, dstFactor(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.dstZone_Factor")
		, dstSupply(item_level_type(0), ord_level_type::SpecificOperator, "Dijkstra.dstZone_Supply")
	{}
	leveled_critical_section od_LS, dstFactor, dstSupply;
};

// *****************************************************************************
// UpdateALW:
//   Traverses accepted tree(s) for an origin to accumulate alternative
//   link-based metrics (e.g., alternative impedance or link attribute).
//   Works in-place over node arrays then maps node results back to OD
//   result layout via NodeZoneConnector.
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
void UpdateALW(const NetworkInfo<NodeType, ZoneType, ImpType>& ni, const OwningDijkstraHeap<NodeType, LinkType, ZoneType, ImpType>& dh, 
	const TreeRelations& tr, const NodeZoneConnector<NodeType, LinkType, ZoneType, ImpType>& nzc,
	const ImpType* altLinkWeights, bool altLinkWeightsHasVoidDomain, ZoneType orgZone, ZoneType zonalResultCount, SizeT resultCountBase, ImpType* nodeALW, ImpType* altLinkImp)
{
	assert(nodeALW);
	assert(dh.m_TraceBackDataPtr);

	// Walk each root(s) induced by origin start points
	for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
	{
		NodeType node = LookupOrSame(ni.startPoints.Node_rel, startPointIndex);
		assert(node < ni.nrV);

		auto currNodePtr = &tr.m_TreeNodes[node];
		if (currNodePtr->GetParent())
			continue;

		nodeALW[node] = 0;

		while (currNodePtr = tr.WalkDepthFirst_TopDown(currNodePtr))
		{
			assert(currNodePtr && currNodePtr->GetParent());
			node = tr.NrOfNode(currNodePtr);
			assert(node < ni.nrV);

			LinkType currLink = dh.m_TraceBackDataPtr[node];
			NodeType prevNode = tr.NrOfNode(currNodePtr->GetParent());
			assert(currLink < ni.nrE);
			nodeALW[node] = altLinkWeights[altLinkWeightsHasVoidDomain ? 0 : currLink] + nodeALW[prevNode];
		}
	}

	// Map node-based accumulations to OD indexed result arrays
	if (altLinkImp) {
		auto currPtr = altLinkImp + resultCountBase;
		for (ZoneType j = 0; j != zonalResultCount; ++j)
		{
			NodeType node = nzc.Res2EndNode(j);
			*currPtr++ = IsDefined(node) ? nodeALW[node] : UNDEFINED_VALUE(ImpType);
		}
	}
}

// *****************************************************************************
// ProcessDijkstra:
//   The central algorithm performing (potentially parallel) per-origin
//   shortest path expansions, recording required metrics, computing
//   interaction potentials, and writing OD or aggregated zone outputs.
//
//   Key steps per origin:
//     1. Initialize heap, TreeRelations (if needed), NodeZoneConnector.
//     2. Insert all start nodes with optional start offsets.
//     3. Extract-min loop over heap finalizing nodes (standard Dijkstra).
//     4. Queue/commit endpoints (with optional endpoint impedance).
//     5. Apply mass or max-impedance cuts.
//     6. After termination: write OD results (dense or sparse).
//     7. Optional: alt impedance / link attr accumulation.
//     8. Optional: interaction potential and link-flow derivation.
//     9. Optional: reconstruct per-OD link sets.
//
//   Concurrency: Each origin is processed independently in parallel_for.
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType, typename MassType, typename ParamType>
SizeT ProcessDijkstra(TreeItemDualRef& resultHolder
,	const NetworkInfo<NodeType, ZoneType, ImpType>& ni
,	const ImpType * orgMaxImpedances, bool orgMaxImpedancesHasVoidDomain
,	const MassType* srcMassLimitPtr, bool srcMassLimitHasVoidDomain
,	const MassType* dstMassPtr, bool dstMassHasVoidDomain
,   sqr_dist_t euclidicSqrDist

,	const GraphInfo<NodeType, LinkType, ImpType>& graph
,	const Inverted_rel<ZoneType>& node_endPoint_inv

,	DijkstraFlag df

,	const ImpType * altLinkWeights, bool altLinkWeightsHasVoidDomain
,	const ImpType*  linkAttr,       bool linkAttrHasVoidDomain
,	const ImpType * orgMinImp, bool orgMinImpHasVoidDomain
,	const ImpType * dstMinImp, bool dstMinImpHasVoidDomain
,	const MassType* tgOrgMass, bool tgOrgMassHasVoidDomain
,	const MassType* tgDstMass, bool tdDstMassHasVoidDomain
,   const ParamType tgBetaDecay
,	const ParamType tgAlphaLogit
,	const ParamType tgBetaLogit
,	const ParamType tgGammaLogit
,   const ParamType* tgOrgAlpha, bool tgOrgAlphaHasVoidDomain
,	const SizeT*   resCumulCount, SizeT nrRes

,	SizeT* resCount
,	ResultInfo<ZoneType, ImpType, MassType>&& res
,   tile_id numResultTiles
,	CharPtr   actionMsg
)
{
	DBG_START("ProcessDijkstra", actionMsg, MG_DEBUG_DIJKSTRA);

	Timer processTimer;
	std::atomic<SizeT> resultCount = 0, zoneCount = 0;
	auto itemRefStr = resultHolder.HasBackRef() ? resultHolder.GetBackRefStr() + " " : SharedStr();
	CharPtr itemRef = itemRefStr.c_str();

	assert(altLinkWeights || !res.od_AltLinkImp);

	bool tgBetaDecayIsZero = (tgBetaDecay == 0.0);
	bool tgBetaDecayIsOne  = (tgBetaDecay == 1.0);
	bool tgBetaDecayIsZeroOrOne = tgBetaDecayIsZero || tgBetaDecayIsOne;
	bool useSrcZoneStamps = flags(df & DijkstraFlag::SparseResult) && flags(df & DijkstraFlag::OD);
	bool useTraceBack = (altLinkWeights || linkAttr || res.od_LS || res.LinkFlow || flags(df & DijkstraFlag::VerboseLogging) && !res.node_TB);

	WriteBlock writeBlocks;

	// Thread-local combinables (one copy per worker thread)
	dms_combinable<NodeZoneConnector<NodeType, LinkType, ZoneType, ImpType>> nzcC;
	dms_combinable<OwningDijkstraHeap<NodeType, LinkType, ZoneType, ImpType>> dhC;
	dms_combinable<TreeRelations> trC;
	dms_combinable< std::vector<ImpType>> pot_ijC;

	dms_combinable<std::vector<MassType> >  resLinkFlowC;

	// Default masses if void-domain singletons
	MassType orgMass = 1.0; if (tgOrgMass && tgOrgMassHasVoidDomain)
	{
		orgMass = tgOrgMass[0]; tgOrgMass = nullptr; tgOrgMassHasVoidDomain = false;
	}

	ParamType orgAlpha = 0.0;
	if (tgOrgAlpha && tgOrgAlphaHasVoidDomain)
	{
		orgAlpha = tgOrgAlpha[0]; tgOrgAlpha = nullptr; tgOrgAlphaHasVoidDomain = false;
		MG_USERCHECK(orgAlpha >= 0.0);
	}
	auto orgZoneTask = 
		[=,
		&resultHolder,
		&writeBlocks,
		&ni, &graph, &node_endPoint_inv, &zoneCount, &resultCount, &processTimer,
		&nzcC, &dhC, &trC, &pot_ijC, &resLinkFlowC,
		&res
		](ZoneType orgZone)
		{
			DSM::CancelIfOutOfInterest(resultHolder.GetNew());
			if (CancelableFrame::CurrActiveCanceled())
				return;

			auto& nzc = nzcC.local();
			auto& dh = dhC.local();
			TreeRelations& tr = trC.local();
			auto& nodeALW = dh.m_AltLinkWeight;
			auto& nodeLA = dh.m_LinkAttr;
			auto& resLinkFlow = resLinkFlowC.local();

			assert(orgZone < ni.nrOrgZones);
			assert(dh.Empty());

			// One-time initialization of thread-local structures
			if (!nzc.m_ResImpPerDstZone)
			{
				nzc.Init(ni, df, euclidicSqrDist);
				dh.Init(ni.nrV, useSrcZoneStamps, useTraceBack);
			}

			bool trIsUsed = false;
			if (altLinkWeights || res.LinkFlow || linkAttr)
			{
				tr.InitNodes(ni.nrV);
				trIsUsed = true;
				if (altLinkWeights || res.LinkFlow)
				{
					if (!nodeALW)
						nodeALW = OwningPtrSizedArray<ImpType>(ni.nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: nodeALW"));
					if (res.LinkFlow)
						resLinkFlow.resize(ni.nrE, 0);
				}
				if (linkAttr)
				{
					if (!nodeLA)
						nodeLA = OwningPtrSizedArray<ImpType>(ni.nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: nodeLA"));
				}
			}

			if (res.node_TB)
			{
				assert(!useTraceBack);
				assert(!flags(df & DijkstraFlag::OD));
				assert(!dh.m_TraceBackData);
				dh.m_TraceBackDataPtr = res.node_TB;
				fast_undefine(dh.m_TraceBackDataPtr, dh.m_TraceBackDataPtr + ni.nrV);
			}

			// Initialize per-origin limits
			dh.m_MaxImp = (orgMaxImpedances) ? orgMaxImpedances[orgMaxImpedancesHasVoidDomain ? 0 : orgZone] : MAX_VALUE(ImpType);

			dh.ResetImpedances();
			nzc.ResetSrc(orgZone);
			if (res.orgZone_SumImp) res.orgZone_SumImp[orgZone] = 0.0;
			if (res.orgZone_SumLinkAttr) res.orgZone_SumLinkAttr[orgZone] = 0.0;

			// Seed all start nodes
			for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
			{
				dms_assert(startPointIndex < ni.nrX);
				NodeType startNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
				dh.InsertNode(startNode, ni.startPoints.Impedances ? ni.startPoints.Impedances[startPointIndex] : ImpType(0), UNDEFINED_VALUE(LinkType));
				if (trIsUsed)
					tr.InitRootNode(startNode);
			};
			MassType
				cumulativeMass = 0,
				maxSrcMass = (srcMassLimitPtr) ? srcMassLimitPtr[srcMassLimitHasVoidDomain ? 0 : orgZone] : MAX_VALUE(MassType);

			using EndPointHeapElemType = heapElemType<ImpType, ZoneType>;
			std::vector<EndPointHeapElemType> endPointHeap;

			// Main Dijkstra loop
			while (!dh.Empty())
			{
				NodeType currNode = dh.Front().Value(); dms_assert(currNode < ni.nrV);
				ImpType currImp = dh.Front().Imp();
				dh.PopNode();
				if (!dh.MarkFinal(currNode, currImp))
					continue;

				if (trIsUsed)
				{
					LinkType backLink = dh.m_TraceBackDataPtr[currNode];
					if (IsDefined(backLink))
					{
						dms_assert(backLink < ni.nrE);
						NodeType parentNode;
						if (graph.linkF2Data[backLink] == currNode)
							parentNode = graph.linkF1Data[backLink];
						else
						{
							dms_assert(graph.linkF1Data[backLink] == currNode);
							parentNode = graph.linkF2Data[backLink];
						}
						dms_assert(parentNode < ni.nrV);
						tr.InitChildNode(currNode, parentNode);
					}
				}
				ZoneType y = node_endPoint_inv.FirstOrSame(currNode);
				if (ni.endPoints.Impedances)
				{
					// Each endpoint offset considered as separate candidate
					while (IsDefined(y))
					{
						ImpType endpointImpData = currImp + ni.endPoints.Impedances[y];
						endPointHeap.push_back(EndPointHeapElemType(y, endpointImpData));
						std::push_heap(endPointHeap.begin(), endPointHeap.end());
						y = node_endPoint_inv.NextOrNone(y);
					}
					// Accept endpoints that are now <= current finalized node impedance
					while (!endPointHeap.empty() && endPointHeap.front().Imp() <= currImp)
					{
						ZoneType yy = endPointHeap.front().Value();
						ImpType dstImp = endPointHeap.front().Imp();

						if (nzc.CommitY(yy, dstImp))
						{
							ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
							if (dstMassPtr)
							{
								cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
								if (cumulativeMass >= maxSrcMass)
									MakeMin(dh.m_MaxImp, dstImp);
							}
						}

						std::pop_heap(endPointHeap.begin(), endPointHeap.end());
						endPointHeap.pop_back();
					}
				}
				else
				{
					while (IsDefined(y))
					{
						ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[y] : y;
						if (nzc.CommitY(y, currImp) && dstMassPtr)
						{
							cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
							if (cumulativeMass >= maxSrcMass)
								MakeMin(dh.m_MaxImp, currImp);
						}
						y = node_endPoint_inv.NextOrNone(y);
					}
				}

				// Relax outgoing edges (forward direction)
				LinkType currLink = graph.node_link1_inv.First(currNode);
				while (currLink != UNDEFINED_VALUE(LinkType))
				{
					dms_assert(currLink < ni.nrE);
					NodeType otherNode = graph.linkF2Data[currLink];
					ImpType deltaCost = graph.linkImpDataPtr[currLink];
					if (deltaCost < dh.m_MaxImp)
						dh.InsertNode(otherNode, currImp + deltaCost, currLink);
					currLink = graph.node_link1_inv.Next(currLink);
				}
				if (!flags(df & (DijkstraFlag::Bidirectional | DijkstraFlag::BidirFlag)))
					continue;

				// Relax reverse direction edges if enabled (bidirectional)
				currLink = graph.node_link2_inv.First(currNode);
				while (currLink != UNDEFINED_VALUE(LinkType))
				{
					dms_assert(currLink < ni.nrE);
					NodeType otherNode = graph.linkF1Data[currLink];
					ImpType deltaCost = graph.linkImpDataPtr[currLink];
					if (deltaCost < dh.m_MaxImp)
						dh.InsertNode(otherNode, currImp + deltaCost, currLink);
					currLink = graph.node_link2_inv.Next(currLink);
				}
			}
			if (ni.endPoints.Impedances)
			{
				// Final flush of remaining endpoint candidates (still under cutoff)
				while (!endPointHeap.empty() && endPointHeap.front().Imp() < dh.m_MaxImp)
				{
					ZoneType yy = endPointHeap.front().Value();
					ImpType dstImp = endPointHeap.front().Imp();

					if (nzc.CommitY(yy, dstImp))
					{
						if (dstMassPtr)
						{
							ZoneType dstZone = ni.endPoints.Zone_rel ? ni.endPoints.Zone_rel[yy] : yy;
							cumulativeMass += dstMassPtr[dstMassHasVoidDomain ? 0 : dstZone];
							if (cumulativeMass >= maxSrcMass)
								MakeMin(dh.m_MaxImp, dstImp);
						}
					}

					std::pop_heap(endPointHeap.begin(), endPointHeap.end());
					endPointHeap.pop_back();
				}
			}

			// Determine per-origin result base
			ZoneType zonalResultCount = nzc.ZonalResCount();
			SizeT resultCountBase = 0;
			if (nzc.IsDense())
				resultCountBase = SizeT(ni.nrDstZones) * SizeT(orgZone);
			else if (resCumulCount)
			{
				resultCountBase = resCumulCount[orgZone];
				if (orgZone + 1 < ni.nrOrgZones)
				{
					SizeT givenZonalResultCount = resCumulCount[orgZone + 1] - resultCountBase;
					if (zonalResultCount != givenZonalResultCount)
					{
						MG_CHECK(flags(df & DijkstraFlag::PrecalculatedNrDstZones));
						if (zonalResultCount > givenZonalResultCount)
							throwDmsErrF("orgZone %1%: zonalResultCount %2% != givenZonalResultCount %3%", orgZone, zonalResultCount, givenZonalResultCount);
					}
				}
			}

			resultCount += zonalResultCount;

			// Write OD-based arrays
			if (nzc.IsDense())
			{
				if (res.od_ImpData)
					fast_copy(nzc.m_ResImpPerDstZone.begin(), nzc.m_ResImpPerDstZone.begin() + zonalResultCount, res.od_ImpData + resultCountBase);
				if (res.od_DstZoneIds)
				{
					auto dstZonePtr = res.od_DstZoneIds + resultCountBase + zonalResultCount;
					ZoneType c = zonalResultCount;
					while (c)
						*--dstZonePtr = --c;
				}
				if (res.od_EndPointIds)
				{
					auto endPointPtr = res.od_EndPointIds + resultCountBase + zonalResultCount;
					ZoneType c = zonalResultCount;
					while (c)
						*--endPointPtr = nzc.DstZone2EndPoint(--c);
				}
			}
			else
			{
				if (flags(df & DijkstraFlag::Counting))
				{
					if (resCount)
						resCount[orgZone] = zonalResultCount;
				}
				else
				{
					if (res.od_ImpData)
					{
						auto currPtr = res.od_ImpData + resultCountBase;
						for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
							*currPtr++ = nzc.Res2Imp(resIndex);
					}
					if (res.od_DstZoneIds) {
						auto currPtr = res.od_DstZoneIds + resultCountBase;
						for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
							*currPtr++ = nzc.Res2DstZone(resIndex);
					}
					if (res.od_EndPointIds) {
						auto currPtr = res.od_EndPointIds + resultCountBase;
						for (ZoneType resIndex = 0; resIndex != zonalResultCount; ++resIndex)
							*currPtr++ = nzc.Res2EndPoint(resIndex);
					}
				}
			}
			if (res.od_SrcZoneIds)
			{
				auto currPtr = res.od_SrcZoneIds + resultCountBase;
				fast_fill(currPtr, currPtr + zonalResultCount, orgZone);
			}

			if (res.orgZone_NrDstZones)
				res.orgZone_NrDstZones[orgZone] = zonalResultCount;

			const ImpType* d_vj = dh.m_ResultDataPtr;
			const ImpType* la_vj = nullptr;

			// Compute alternative path-based metrics if requested
			if (altLinkWeights)
			{
				d_vj = dh.m_AltLinkWeight.begin();
				UpdateALW(ni, dh, tr, nzc, altLinkWeights, altLinkWeightsHasVoidDomain, orgZone, zonalResultCount, resultCountBase, dh.m_AltLinkWeight.begin(), res.od_AltLinkImp);
			}
			if (linkAttr)
			{
				la_vj = dh.m_LinkAttr.begin();
				UpdateALW(ni, dh, tr, nzc, linkAttr, linkAttrHasVoidDomain, orgZone, zonalResultCount, resultCountBase, dh.m_LinkAttr.begin(), res.od_LinkAttr);
			}

			// Interaction & aggregate metrics
			if (flags(df & DijkstraFlag::InteractionOrMaxImp))
			{
				std::vector<ImpType>& pot_ij = pot_ijC.local();
				Float64 totalPotential = 0;
				ImpType maxImp = 0;

				if (flags(df & DijkstraFlag::Calc_pot_ij))
					vector_zero_n_reuse(pot_ij, zonalResultCount);
				for (SizeT j = 0; j != zonalResultCount; ++j)
				{
					ZoneType dstZone = nzc.Res2DstZone(j);
					NodeType node = nzc.DstZone2EndNode(dstZone);
					if (!IsDefined(node))
						continue;
					Float64 impedance = d_vj[node];
					if (ni.endPoints.Impedances && !altLinkWeights)
						impedance += ni.endPoints.Impedances[dstZone];

					if (orgMinImp) MakeMax(impedance, orgMinImp[orgMinImpHasVoidDomain ? 0 : orgZone]);
					if (dstMinImp) MakeMax(impedance, dstMinImp[dstMinImpHasVoidDomain ? 0 : dstZone]);

					if (impedance <= 0 && !tgBetaDecayIsZero)
						continue;

					MakeMax(maxImp, (ImpType)impedance);
					if (!flags(df & DijkstraFlag::Interaction))
						continue;

					Float64 potential = 1.0;
					if (!tgBetaDecayIsZero)
					{
						if (tgBetaDecayIsOne)
							potential = 1.0 / impedance;
						else
							potential = exp(log(impedance) * -tgBetaDecay);
					}
					if (flags(df & DijkstraFlag::DistLogit))
					{
						if (impedance > 0)
							potential = 1.0 / (1.0 + exp(tgAlphaLogit + tgBetaLogit * log(impedance) + tgGammaLogit * impedance));
						else if (tgBetaLogit == 0)
							potential = 1.0 / (1.0 + exp(tgAlphaLogit + tgGammaLogit * impedance));
						else if (tgBetaLogit < 0)
							potential = 0;
					}
					if (flags(df & DijkstraFlag::Calc_pot_ij))
						pot_ij[j] = potential;

					if (tgDstMass)
						potential *= tgDstMass[tdDstMassHasVoidDomain ? 0 : dstZone];
					totalPotential += potential;
				}
				if (res.orgZone_MaxImp)
					res.orgZone_MaxImp[orgZone] = maxImp;

				if (!flags(df & DijkstraFlag::Interaction))
					goto afterInteraction;

				if (res.orgZone_Factor)
					res.orgZone_Factor[orgZone] = totalPotential;

				if (totalPotential)
				{
					Float64 balancingFactor = orgMass;
					if (tgOrgMass)
						balancingFactor = tgOrgMass[orgZone];
					auto orgAlphaCopy = orgAlpha;
					if (tgOrgAlpha)
					{
						orgAlphaCopy = tgOrgAlpha[orgZone];
						MG_USERCHECK(orgAlphaCopy >= 0.0);
					}
					if (orgAlphaCopy != 0.0)
					{
						if (orgAlphaCopy == 1.0)
							balancingFactor *= totalPotential;
						else
							balancingFactor *= exp(log(totalPotential) * orgAlphaCopy);
					}

					if (res.orgZone_Demand)
						res.orgZone_Demand[orgZone] = balancingFactor;
					balancingFactor /= totalPotential;

					if (flags(df & DijkstraFlag::Calc_pot_ij))
					{
						Float64 sumImp = 0.0, sumLinkAttr = 0.0;
						for (ZoneType j = 0; j != zonalResultCount; ++j)
						{
							pot_ij[j] *= balancingFactor;

							ZoneType dstZone = nzc.Res2DstZone(j);
							if (res.dstZone_Factor)
							{
								leveled_critical_section::scoped_lock lock(writeBlocks.dstFactor);
								res.dstZone_Factor[dstZone] += pot_ij[j];
							}
							if (res.dstZone_Supply || (flags(df & (DijkstraFlag::ProdLinkFlow | DijkstraFlag::ProdOrgSumImp | DijkstraFlag::ProdOrgSumLinkAttr))))
							{
								if (tgDstMass)
									pot_ij[j] *= tgDstMass[tdDstMassHasVoidDomain ? 0 : dstZone];
								if (res.dstZone_Supply)
								{
									leveled_critical_section::scoped_lock lock(writeBlocks.dstSupply);
									res.dstZone_Supply[dstZone] += pot_ij[j];
								}
							}

							NodeType node = nzc.DstZone2EndNode(dstZone);
							if (!IsDefined(node))
								continue;

							if (res.orgZone_SumImp)
							{
								Float64 impedance = d_vj[node];
								sumImp += impedance * pot_ij[j];
							}
							if (res.orgZone_SumLinkAttr)
							{
								Float64 lAttr = la_vj ? la_vj[node] : 0.0;
								sumLinkAttr += lAttr * pot_ij[j];
							}
						}
						if (res.orgZone_SumImp)
							res.orgZone_SumImp[orgZone] = sumImp;
						if (res.orgZone_SumLinkAttr)
							res.orgZone_SumLinkAttr[orgZone] = sumLinkAttr;
					}
				}
				else
				{
					if (res.orgZone_Demand)
						res.orgZone_Demand[orgZone] = 0.0;
					if (res.orgZone_SumImp)
						res.orgZone_SumImp[orgZone] = 0.0;
					if (res.orgZone_SumLinkAttr)
						res.orgZone_SumLinkAttr[orgZone] = 0.0;
				}

				// Link flow assignment
				if (res.LinkFlow && totalPotential)
				{
					assert(dh.m_TraceBackDataPtr);
					assert(trIsUsed);
					// Zero flows on used nodes
					for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
					{
						NodeType currNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
						treenode_pointer currNodePtr = &tr.m_TreeNodes[currNode];

						for (currNodePtr = tr.MostDown(currNodePtr); currNodePtr->GetParent(); currNodePtr = tr.WalkDepthFirst_BottomUp(currNodePtr))
						{
							NodeType n = tr.NrOfNode(currNodePtr);
							nodeALW[n] = 0;
						}
					}

					// Bottom-up accumulation
					for (ZoneType startPointIndex = ni.orgZone_startPoint_inv.FirstOrSame(orgZone); IsDefined(startPointIndex); startPointIndex = ni.orgZone_startPoint_inv.NextOrNone(startPointIndex))
					{
						NodeType currNode = ni.startPoints.Node_rel ? ni.startPoints.Node_rel[startPointIndex] : startPointIndex;
						treenode_pointer currNodePtr = &tr.m_TreeNodes[currNode];

						for (currNodePtr = tr.MostDown(currNodePtr); currNodePtr->GetParent(); currNodePtr = tr.WalkDepthFirst_BottomUp(currNodePtr))
						{
							currNode = tr.NrOfNode(currNodePtr);
							MassType* flowPtr = &nodeALW[currNode];
							ZoneType y = node_endPoint_inv.FirstOrSame(currNode);
							while (IsDefined(y))
							{
								ZoneType j = nzc.Y2Res(y);
								if (IsDefined(j))
									*flowPtr += pot_ij[j];
								y = node_endPoint_inv.NextOrNone(y);
							}
							NodeType prevNode = tr.NrOfNode(currNodePtr->GetParent());
							LinkType currLink = dh.m_TraceBackDataPtr[currNode];
							MassType flow = *flowPtr;
							nodeALW[prevNode] += flow;
							resLinkFlow[currLink] += flow;
						}
					}
				}
			}

		afterInteraction:

			// Reconstruct per-OD link sets if requested
			if (res.od_LS)
			{
				assert(dh.m_TraceBackDataPtr);
				for (ZoneType j = 0; j != zonalResultCount; ++j)
				{
					NodeType node = nzc.Res2EndNode(j);
					if (!IsDefined(node))
						continue;
					SizeT linkCount = 0;
					NodeType walk = node;
					while (true)
					{
						LinkType currLink = dh.m_TraceBackDataPtr[walk];
						if (!IsDefined(currLink))
							break;
						if (graph.linkF2Data[currLink] == walk)
							walk = graph.linkF1Data[currLink];
						else
							walk = graph.linkF2Data[currLink];
						++linkCount;
					}
					walk = node;
					leveled_critical_section::scoped_lock lock(writeBlocks.od_LS);
					auto resLinkSetRef = res.od_LS[resultCountBase + j];
					resLinkSetRef.resize_uninitialized(linkCount MG_DEBUG_ALLOCATOR_SRC("Dijkstra.LinkSet"));
					auto outIt = resLinkSetRef.begin();
					while (true)
					{
						LinkType currLink = dh.m_TraceBackDataPtr[walk];
						if (!IsDefined(currLink))
							break;
						if (graph.linkF2Data[currLink] == walk)
							walk = graph.linkF1Data[currLink];
						else
							walk = graph.linkF2Data[currLink];
						*outIt++ = currLink;
					}
				}
			}

			zoneCount++;
			if (processTimer.PassedSecs())
				reportF(SeverityTypeID::ST_MajorTrace, "%simpedance_matrix %s %s of %s sources: resulted in %s od-pairs"
					, itemRef
					, actionMsg
					, AsString(zoneCount), AsString(ni.nrOrgZones), AsString(resultCount));
		};

	// Launch parallel per-origin processing
	parallel_for<ZoneType>(ni.nrOrgZones, orgZoneTask);

	// Combine link-flow contributions
	if (res.LinkFlow)
		resLinkFlowC.combine_each(
			[&res](std::vector<MassType>& localLinkFlow){
				auto linkFlowPtr = res.LinkFlow;
				for (auto flowPtr = localLinkFlow.begin(), flowEnd = localLinkFlow.end(); flowPtr != flowEnd; ++flowPtr, ++linkFlowPtr)
					*linkFlowPtr += *flowPtr;
		});
	
	if (CancelableFrame::CurrActiveCanceled())
		return UNDEFINED_VALUE(SizeT);

	reportF(SeverityTypeID::ST_MajorTrace, "%simpedance_matrix %s all %d sources: resulted in %s od-pairs"
		, itemRef
		, actionMsg
		, AsString(ni.nrOrgZones), AsString(resultCount));

	return resultCount;
}

// *****************************************************************************
// DijkstraMatrOperator<T>:
//   Runtime operator wrapper integrating with the host framework's
//   data/argument resolution system. Responsible for:
//     * Parsing flags from spec string
//     * Validating argument count and types/unified units
//     * Allocating result units/items
//     * Executing two-pass run (count + fill) for sparse OD mode
//     * Delegating heavy lifting to ProcessDijkstra
//
// Template argument T = Impedance & mass numeric type.
// *****************************************************************************
template <class T>
class DijkstraMatrOperator : public VariadicOperator
{
	using ImpType = T;
	using MassType = T;
	using ParamType = div_type_t<T>;
	using LinkType = UInt32;
	using ZoneType = UInt32;
	using EdgeSeq = sequence_traits<LinkType>::container_type;

	using ArgImpType  = DataArray<ImpType>;
	using ArgNodeType = DataArray<NodeType>;
	using ArgZoneType = DataArray<ZoneType>;
	using ArgFlagType = DataArray<Bool>;

	using SrcNodeType = ArgNodeType;
	using SrcDistType = ArgImpType;
	using SrcZoneType = ArgZoneType;
	using ZoneLocType = DataArray<euclid_location_t>;

	using DstNodeType = ArgNodeType;
	using DstDistType = ArgImpType;
	using DstZoneType = ArgZoneType;

	using ArgMassType = DataArray<MassType>;

	using ArgSMType = DataArray<MassType>;
	using ArgDMType = DataArray<MassType>;
	using ArgParamType = DataArray<ParamType>;

	using ResultUnitType = Unit<ZoneType>;
	using ResultImpType  = DataArray<ImpType>;
	using ResultSubType  = DataArray<ZoneType>;
	using ResultLinkSetType = DataArray<EdgeSeq>;

	static arg_index CalcNrArgs(DijkstraFlag df)
	{
		arg_index nrArgs = 4;
		if (flags(df & DijkstraFlag::BidirFlag)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgNode)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgZone)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgZoneLoc)) ++nrArgs;
		if (flags(df & DijkstraFlag::OrgMinImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstNode)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstZone)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstZoneLoc)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstMinImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::ImpCut)) ++nrArgs;
		if (flags(df & DijkstraFlag::DstLimit)) nrArgs += 2;
		if (flags(df & DijkstraFlag::UseEuclidicFilter)) ++nrArgs;
		if (flags(df & DijkstraFlag::UseAltLinkImp)) ++nrArgs;
		if (flags(df & DijkstraFlag::UseLinkAttr)) ++nrArgs;
		if (flags(df & DijkstraFlag::InteractionVi)) ++nrArgs;
		if (flags(df & DijkstraFlag::InteractionWj)) ++nrArgs;
		if (flags(df & DijkstraFlag::DistDecay)) ++nrArgs;
		if (flags(df & DijkstraFlag::DistLogit)) nrArgs += 3;
		if (flags(df & DijkstraFlag::InteractionAlpha)) ++nrArgs;
		if (flags(df & DijkstraFlag::PrecalculatedNrDstZones)) ++nrArgs;

		assert(nrArgs >= 3 && nrArgs <= 25);
		return nrArgs;
	}

	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		dms_assert(firstArgValue == nullptr || *firstArgValue == char(0));
		return (argNr) 
			? oper_arg_policy::calc_as_result
			: oper_arg_policy::calc_always;
	}
	static const UnitClass* GetResultUnitClass(DijkstraFlag df)
	{
		dms_assert(flags(df & DijkstraFlag::OD));
		return flags(df & DijkstraFlag::UInt64_Od)
			? Unit<UInt64>::GetStaticClass()
			: Unit<UInt32>::GetStaticClass();
	}

public:
	DijkstraMatrOperator(AbstrOperGroup* og, DijkstraFlag df)
		: VariadicOperator(og, flags(df & DijkstraFlag::OD) ? typesafe_cast<const Class*>(GetResultUnitClass(df)) : typesafe_cast<const Class*>(ResultImpType::GetStaticClass())
		,	CalcNrArgs(df))
		,	m_OperFlags(df)
	{
		ClassCPtr* argClsIter = m_ArgClasses.get();

		dms_assert(og->AllowExtraArgs());
		*argClsIter++ = DataArray<SharedStr>::GetStaticClass();
		*argClsIter++ = ArgImpType::GetStaticClass();
		*argClsIter++ = ArgNodeType::GetStaticClass();
		*argClsIter++ = ArgNodeType::GetStaticClass();

		dms_assert(m_ArgClassesEnd == argClsIter);
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		DijkstraFlag df = m_OperFlags;

		arg_index argCounter = 0;
		const AbstrDataItem* paramStrA = AsDataItem(args[argCounter++]);
		MG_CHECK(paramStrA->HasVoidDomainGuarantee());

		SharedStr paramSpec = const_array_cast<SharedStr>(paramStrA)->GetIndexedValue(0);
		df = DijkstraFlag(df | ParseDijkstraString(paramSpec.c_str()));
		CheckFlags(DijkstraFlag(df));

		if (args.size() != CalcNrArgs(df))
			throwDmsErrF(
				"number of given arguments doesn't match the specification '%s': %d arguments given (including the specification), but %d expected"
			,	paramSpec.c_str()
			,	args.size()
			,	CalcNrArgs(df)
			); 

		// Argument extraction logic
		const AbstrDataItem* adiLinkImp             = AsDataItem(args[argCounter++]); 
		const AbstrDataItem* adiLinkF1              = AsDataItem(args[argCounter++]);
		const AbstrDataItem* adiLinkF2              = AsDataItem(args[argCounter++]);
		const AbstrDataItem* adiLinkBidirFlag       = flags(df & DijkstraFlag::BidirFlag) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPointNode      = flags(df & DijkstraFlag::OrgNode) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPointImpedance = flags(df & DijkstraFlag::OrgImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiStartPoinOrgZone    = flags(df & DijkstraFlag::OrgZone) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgZoneLocation     = flags(df & DijkstraFlag::OrgZoneLoc) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointNode        = flags(df & DijkstraFlag::DstNode) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointImpedance   = flags(df & DijkstraFlag::DstImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiEndPointDstZone     = flags(df & DijkstraFlag::DstZone) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstZoneLocation     = flags(df & DijkstraFlag::DstZoneLoc) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiOrgMaxImp           = flags(df & DijkstraFlag::ImpCut) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgMassLimit        = flags(df & DijkstraFlag::DstLimit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstMassLimit        = flags(df & DijkstraFlag::DstLimit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiEuclidicSqrDist     = flags(df & DijkstraFlag::UseEuclidicFilter) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiLinkAltImp          = flags(df & DijkstraFlag::UseAltLinkImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiLinkAttr            = flags(df & DijkstraFlag::UseLinkAttr  ) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		const AbstrDataItem* adiOrgMinImp  = flags(df & DijkstraFlag::OrgMinImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstMinImp  = flags(df & DijkstraFlag::DstMinImp) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgMass    = flags(df & DijkstraFlag::InteractionVi) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDstMass    = flags(df & DijkstraFlag::InteractionWj) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDistDecayBetaParam  = flags(df & DijkstraFlag::DistDecay) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDistLogitAlphaParam = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDistLogitBetaParam  = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiDistLogitGammaParam = flags(df & DijkstraFlag::DistLogit) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiOrgAlpha            = flags(df & DijkstraFlag::InteractionAlpha) ? AsCheckedDataItem(args[argCounter++]) : nullptr;
		const AbstrDataItem* adiPrecalculatedNrDstZones = flags(df & DijkstraFlag::PrecalculatedNrDstZones) ? AsCheckedDataItem(args[argCounter++]) : nullptr;

		MG_CHECK(argCounter == args.size());

		const AbstrUnit* resultUnit;
		AbstrUnit* mutableResultUnit;
		AbstrDataItem* resDist;
		TreeItem* resultContext;

		dms_assert(adiLinkImp && adiLinkF1 && adiLinkF2);
		dms_assert((adiOrgMassLimit != nullptr) == (adiDstMassLimit != nullptr));
		const Unit<LinkType>* e        = checked_domain<LinkType>(adiLinkImp, "Link Impedance");
		const Unit<NodeType>* v        = const_unit_cast<NodeType>(adiLinkF1->GetAbstrValuesUnit());
		const Unit<ImpType>* impUnit = const_unit_cast<ImpType>(adiLinkImp->GetAbstrValuesUnit());
		const Unit<NodeType>* x = adiStartPointNode ? checked_domain<NodeType>(adiStartPointNode, "startPoint Node_rel") : v;
		const Unit<NodeType>* y = adiEndPointNode   ? checked_domain<NodeType>(adiEndPointNode, "endPoint Node_rel") : v;
		const Unit<NodeType>* orgZones = flags(df & DijkstraFlag::OD) ? (adiStartPoinOrgZone ? const_unit_cast<NodeType>(adiStartPoinOrgZone->GetAbstrValuesUnit()) : x) : nullptr;
		const Unit<NodeType>* dstZones = adiEndPointDstZone ? const_unit_cast<NodeType>(adiEndPointDstZone->GetAbstrValuesUnit()) : y;
		const AbstrUnit* orgZonesOrVoid = orgZones ? orgZones : Unit<Void>::GetStaticClass()->CreateDefault();

		assert(e && v && impUnit && x && y && (orgZones || !flags(df & DijkstraFlag::OD))&& dstZones);
		e->UnifyDomain(adiLinkF1->GetAbstrDomainUnit(), "Links", "Domain of FromNode_rel attribute", UM_Throw);
		e->UnifyDomain(adiLinkF2->GetAbstrDomainUnit(), "Links", "Domain of ToNode_rel attribute", UM_Throw);
		v->UnifyDomain(adiLinkF1->GetAbstrValuesUnit(), "Nodes", "Values of FromNode_rel attribute", UM_Throw);
		v->UnifyDomain(adiLinkF2->GetAbstrValuesUnit(), "Nodes", "Values of ToNode_rel attribute", UM_Throw);
		if (adiLinkBidirFlag) e->UnifyDomain(adiLinkBidirFlag->GetAbstrDomainUnit(), "Links", "Domain of Bidirectional flag attribute", UM_Throw);
		if (adiLinkBidirFlag) MG_USERCHECK(adiLinkBidirFlag->GetAbstrValuesUnit()->IsKindOf(Unit<Bool>::GetStaticClass()));
		if (adiStartPointNode) v->UnifyDomain(adiStartPointNode->GetAbstrValuesUnit(), "NodeSet", "Domain of StartLink Node_rel", UM_Throw);
		if (adiStartPointImpedance) x->UnifyDomain(adiStartPointImpedance->GetAbstrDomainUnit(), "StartLinks", "Domain of StartLink Impedance", UM_Throw);
		if (adiStartPoinOrgZone) x->UnifyDomain(adiStartPoinOrgZone->GetAbstrDomainUnit(), "StartLinks", "Domain of StartLink OrgZone_rel", UM_Throw);
		if (adiStartPointImpedance) impUnit->UnifyValues(adiStartPointImpedance->GetAbstrValuesUnit(), "LinkImpedances", "StartLink Impedances", UM_Throw);
		if (adiOrgZoneLocation)
		{
			assert(adiDstZoneLocation);
			assert(adiEuclidicSqrDist);
			orgZones->UnifyDomain(adiOrgZoneLocation->GetAbstrDomainUnit(), "OrgZones", "OrgZoneLoc", UM_Throw);
			MG_USERCHECK(adiOrgZoneLocation->GetValueComposition() == ValueComposition::Single);
			MG_USERCHECK(adiOrgZoneLocation->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueWrap<euclid_location_t>::GetStaticClass()->GetValueClassID());
		}
		if (adiEndPointNode) v->UnifyDomain(adiEndPointNode->GetAbstrValuesUnit(), "NodeSet", "Domain of EndLink Node_rel ", UM_Throw);
		if (adiEndPointImpedance) y->UnifyDomain(adiEndPointImpedance->GetAbstrDomainUnit(), "EndLinks", "Domain of EndLink Impedance", UM_Throw);
		if (adiEndPointDstZone) y->UnifyDomain(adiEndPointDstZone->GetAbstrDomainUnit(), "EndLinks", "Domain of EndLink OrgZone_rel", UM_Throw);
		if (adiEndPointImpedance) impUnit->UnifyValues(adiEndPointImpedance->GetAbstrValuesUnit(), "LinkImpedances", "EndLink Impedances", UM_Throw);
		if (adiDstZoneLocation)
		{
			assert(adiOrgZoneLocation);
			assert(adiEuclidicSqrDist);
			dstZones->UnifyDomain(adiDstZoneLocation->GetAbstrDomainUnit(), "DstZones", "DstZoneLoc", UM_Throw);
			MG_USERCHECK(adiDstZoneLocation->GetValueComposition() == ValueComposition::Single);
			MG_USERCHECK(adiDstZoneLocation->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueWrap<euclid_location_t>::GetStaticClass()->GetValueClassID());
		}
		if (adiOrgMaxImp)
		{
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiOrgMaxImp->GetAbstrValuesUnit())
				, "value type of orgZone_MaxImp doesn't match with the values type of Impedances"
			);

			orgZonesOrVoid->UnifyDomain(adiOrgMaxImp->GetAbstrDomainUnit(), "OrgZones", "Domain of orgZone_MaxImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			impUnit->UnifyValues(adiOrgMaxImp->GetAbstrValuesUnit(), "ImpedanceUnit", "Values of orgZone_MaxImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiOrgMassLimit)
		{
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiOrgMassLimit->GetAbstrValuesUnit())
				, "value type of OrgZone_max_mass doesn't match with the values type of Impedances"
			);
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiDstMassLimit->GetAbstrValuesUnit())
				, "value type of DstZone_mass doesn't match with the values type of Impedances"
			);


			assert(adiDstMassLimit);
			adiOrgMassLimit->GetAbstrValuesUnit()->UnifyValues(adiDstMassLimit->GetAbstrValuesUnit(), "Values of OrgZone_max_mass", "Values of DstmassLimit", UnifyMode(UM_Throw | UM_AllowDefault));
			orgZonesOrVoid->UnifyDomain(adiOrgMassLimit->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgZone_max_mass", UnifyMode(UM_Throw | UM_AllowVoidRight));
			dstZones->UnifyDomain(adiDstMassLimit->GetAbstrDomainUnit(), "DstZones", "Domain of DstZone_mass", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}
		if (adiEuclidicSqrDist)
		{
			assert(adiOrgZoneLocation);
			assert(adiDstZoneLocation);
			MG_USERCHECK(!adiEuclidicSqrDist || adiEuclidicSqrDist->HasVoidDomainGuarantee());
			MG_USERCHECK(adiEuclidicSqrDist->GetValueComposition() == ValueComposition::Single);
			MG_USERCHECK(adiEuclidicSqrDist->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueWrap<sqr_dist_t>::GetStaticClass()->GetValueClassID());
		}
		const Unit<ImpType>* imp2Unit= impUnit;
		CharPtr impUnitRef = "ImpUnit";
		if (adiLinkAltImp)
		{
			MG_USERCHECK2(dynamic_cast<const Unit<ImpType>*>(adiLinkAltImp->GetAbstrValuesUnit())
				, "value type of Alternative Impedances doesn't match with the values type of Impedances"
			);

			imp2Unit= const_unit_cast<ImpType>(adiLinkAltImp->GetAbstrValuesUnit());
			impUnitRef = "AltImpUnit";
			e->UnifyDomain(adiLinkAltImp->GetAbstrDomainUnit(), "Edges", "Domain of Alternative Impedances", UnifyMode(UM_Throw | UM_AllowVoidRight));
		}
		const Unit<ImpType>* linkAttrUnit = nullptr;
		if (adiLinkAttr)
		{
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiLinkAttr->GetAbstrValuesUnit())
				, "value type of Link Attribute doesn't match with the values type of Impedances"
			);


			linkAttrUnit = const_unit_cast<ImpType>(adiLinkAttr->GetAbstrValuesUnit());
		}
		if (adiOrgMinImp)
		{
			orgZonesOrVoid->UnifyDomain(adiOrgMinImp->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMinImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			imp2Unit->UnifyValues(adiOrgMinImp->GetAbstrValuesUnit(), impUnitRef, "Values of OrgMinImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiDstMinImp)
		{
			dstZones->UnifyDomain(adiDstMinImp->GetAbstrDomainUnit(), "DstZones", "Domain of DstMinImp", UnifyMode(UM_Throw | UM_AllowVoidRight));
			imp2Unit->UnifyValues(adiDstMinImp->GetAbstrValuesUnit(), impUnitRef, "Values of DstMinImp", UnifyMode(UM_Throw | UM_AllowDefault));
		}
		if (adiOrgMass)
		{
			assert(adiDistDecayBetaParam);
			orgZonesOrVoid->UnifyDomain(adiOrgMass->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgMass attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiOrgMass->GetAbstrValuesUnit())
				, "value type of the OrgMass attribute doesn't match with the value type of Impedance");
		}
		if (adiDistDecayBetaParam)
		{
			MG_USERCHECK(adiDistDecayBetaParam->HasVoidDomainGuarantee());
			MG_USERCHECK2(dynamic_cast<const Unit<ParamType>*>(adiDistDecayBetaParam->GetAbstrValuesUnit())
				,	"value type of DistDecayBetaParam should be Float64"
			);
		}
		if (adiDistLogitAlphaParam)
		{
			MG_USERCHECK(adiDistLogitAlphaParam->HasVoidDomainGuarantee());
			MG_USERCHECK2(dynamic_cast<const Unit<ParamType>*>(adiDistLogitAlphaParam->GetAbstrValuesUnit())
				, "value type of DistLogitAlphaParam should be Float64"
			);
		}
		if (adiDistLogitBetaParam)
		{
			MG_USERCHECK(!adiDistLogitBetaParam || adiDistLogitBetaParam->HasVoidDomainGuarantee());
			MG_USERCHECK2(dynamic_cast<const Unit<ParamType>*>(adiDistLogitBetaParam->GetAbstrValuesUnit())
				, "value type of DistLogitBetaParam should be Float64"
			);
		}
		if (adiDistLogitGammaParam)
		{
			MG_USERCHECK(!adiDistLogitGammaParam || adiDistLogitGammaParam->HasVoidDomainGuarantee());
			MG_USERCHECK2(dynamic_cast<const Unit<ParamType>*>(adiDistLogitGammaParam->GetAbstrValuesUnit())
				, "value type of DistLogitGammaParam should be Float64"
			);
		}
		assert(adiDistDecayBetaParam || (!adiOrgMass && !adiDstMass));
		if (adiDstMass)
		{
			dstZones->UnifyDomain(adiDstMass->GetAbstrDomainUnit(), "DstZones", "Domain of DstMass attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiDstMass->GetAbstrValuesUnit())
				, "value type of the DstMass doesn't match with the value type of Impedance"
			);
		}
		if (adiOrgAlpha)
		{
			orgZonesOrVoid->UnifyDomain(adiOrgAlpha->GetAbstrDomainUnit(), "OrgZones", "Domain of OrgAlpha attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
			MG_USERCHECK2(dynamic_cast<const Unit<MassType>*>(adiOrgAlpha->GetAbstrValuesUnit())
				, "value type of OrgAlpha doesn't match with the value type of Impedance"
			);
		}
		if (adiPrecalculatedNrDstZones)
		{
			MG_CHECK(flags(df & DijkstraFlag::OD));
			orgZonesOrVoid->UnifyDomain(adiPrecalculatedNrDstZones->GetAbstrDomainUnit(), "OrgZones", "Domain of precalculated number of destination zones", UM_Throw);
			MG_USERCHECK2(dynamic_cast<const Unit<ZoneType>*>(adiPrecalculatedNrDstZones->GetAbstrValuesUnit())
				, "Precalculated number of destination zones (aka PrecalculatedNrDstZones) expected as UInt32 values"
			)
		}

		if (flags(df & DijkstraFlag::OD))
		{ 
			mutableResultUnit = GetResultUnitClass(df)->CreateResultUnit(resultHolder).release()	;
			mutableResultUnit->SetTSF(TSF_Categorical);
			resultHolder = mutableResultUnit;
			resultUnit = mutableResultUnit;
			resultContext = resultHolder;
			resDist = flags(df & DijkstraFlag::ProdOdImpedance)
				? CreateDataItem(resultContext, GetTokenID_mt("impedance"), resultUnit, impUnit)
				: nullptr;
		}
		else
		{
			mutableResultUnit = nullptr;
			resultUnit = dstZones;
			if (!resultHolder)
				resultHolder = CreateCacheDataItem(resultUnit, impUnit);
			resultContext = resultHolder.GetNew();
			resDist = AsDataItem(resultContext);
		}
			
		AbstrDataItem* resTB= nullptr;
		if (flags(df & DijkstraFlag::ProdTraceBack))
		{
			dms_assert(!flags(df & DijkstraFlag::OD));
			resTB = CreateDataItem(resultContext, GetTokenID_mt("TraceBack"), v, e);
			resTB->SetTSF(TSF_Categorical);
		}

		AbstrDataItem* resLS = flags(df & DijkstraFlag::ProdOdLinkSet)
			?	CreateDataItem(resultContext, GetTokenID_mt("LinkSet"), resultUnit, e, ValueComposition::Sequence)
			:	nullptr;
		if (resLS) resLS->SetTSF(TSF_Categorical);

		AbstrDataItem* resAltLinkImp = flags(df & DijkstraFlag::ProdOdAltImpedance)
			? CreateDataItem(resultContext, GetTokenID_mt("alt_imp"), resultUnit, imp2Unit)
			:	nullptr;

		AbstrDataItem* resLinkAttr = flags(df & DijkstraFlag::ProdOdLinkAttr)
			? CreateDataItem(resultContext, GetTokenID_mt("LinkAttr"), resultUnit, linkAttrUnit)
			: nullptr;

		auto mijMassUnit = Unit<MassType>::GetStaticClass()->CreateDefault();
		auto orgMassUnit = adiOrgMass ? adiOrgMass->GetAbstrValuesUnit() : mijMassUnit;
		auto dstMassUnit = adiDstMass ? adiDstMass->GetAbstrValuesUnit() : mijMassUnit;

		AbstrDataItem* resOrgFactor = flags(df & DijkstraFlag::ProdOrgFactor)
			? CreateDataItem(resultContext, GetTokenID_mt("D_i"), orgZonesOrVoid, dstMassUnit)
			: nullptr;
		AbstrDataItem* resOrgDemand = flags(df & DijkstraFlag::ProdOrgDemand)
			? CreateDataItem(resultContext, GetTokenID_mt("M_ix"), orgZonesOrVoid, mijMassUnit)
			: nullptr;

		AbstrDataItem* resDstFactor = flags(df & DijkstraFlag::ProdDstFactor)
			? CreateDataItem(resultContext, GetTokenID_mt("C_j"), dstZones, orgMassUnit)
			: nullptr;
		AbstrDataItem* resDstSupply = flags(df & DijkstraFlag::ProdDstSupply)
			? CreateDataItem(resultContext, GetTokenID_mt("M_xj"), dstZones, mijMassUnit)
			: nullptr;

		AbstrDataItem* resOrgNrDstZones = flags(df & DijkstraFlag::ProdOrgNrDstZones)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_NrDstZones"), orgZonesOrVoid, dstZones)
			: nullptr;

		AbstrDataItem* resOrgSumImp = flags(df & DijkstraFlag::ProdOrgSumImp)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_SumImp"), orgZonesOrVoid, imp2Unit)
			: nullptr;

		AbstrDataItem* resOrgSumLinkAttr = flags(df & DijkstraFlag::ProdOrgSumLinkAttr)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_SumLinkAttr"), orgZonesOrVoid, linkAttrUnit)
			: nullptr;

		AbstrDataItem* resOrgMaxImp = flags(df & DijkstraFlag::ProdOrgMaxImp)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_MaxImp"), orgZonesOrVoid, impUnit)
			: nullptr;

		AbstrDataItem* resLinkFlow = flags(df & DijkstraFlag::ProdLinkFlow)
			? CreateDataItem(resultContext, GetTokenID_mt("Link_flow"), e, adiOrgMass->GetAbstrValuesUnit())
			: nullptr;
		AbstrDataItem* resSrcZone = flags(df & DijkstraFlag::ProdOdOrgZone_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("OrgZone_rel"), resultUnit, orgZonesOrVoid)
			: nullptr;
		if (resSrcZone) resSrcZone->SetTSF(TSF_Categorical);

		AbstrDataItem* resDstZone = flags(df & DijkstraFlag::ProdOdDstZone_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("DstZone_rel"), resultUnit, dstZones)
			: nullptr;
		if (resDstZone) resDstZone->SetTSF(TSF_Categorical);

		AbstrDataItem* resStartPoint = flags(df & DijkstraFlag::ProdOdStartPoint_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("StartPoint_rel"), resultUnit, x)
			: nullptr;
		if (resStartPoint) resStartPoint->SetTSF(TSF_Categorical);
		AbstrDataItem* resEndPoint = flags(df & DijkstraFlag::ProdOdEndPoint_rel)
			? CreateDataItem(resultContext, GetTokenID_mt("EndPoint_rel"), resultUnit, y)
			: nullptr;
		if (resEndPoint) resEndPoint->SetTSF(TSF_Categorical);

		if (mustCalc)
		{
			DataReadLock arg1Lock(adiLinkImp);
			DataReadLock argLinkF1Lock(adiLinkF1);
			DataReadLock argLinkF2Lock(adiLinkF2);
			DataReadLock argLinkFlagLock(adiLinkBidirFlag);
			DataReadLock arg4Lock(adiStartPointNode);
			DataReadLock arg5Lock(adiStartPointImpedance);
			DataReadLock arg6Lock(adiStartPoinOrgZone);
			DataReadLock arg7Lock(adiEndPointNode);
			DataReadLock arg8Lock(adiEndPointImpedance);
			DataReadLock arg9Lock(adiEndPointDstZone);

			DataReadLock argALock(adiOrgMaxImp);
			DataReadLock argBLock(adiOrgMassLimit);
			DataReadLock argCLock(adiDstMassLimit);
			DataReadLock argWLock(adiLinkAltImp);
			DataReadLock argOrgMassLock(adiOrgMass);
			DataReadLock argDstMassLock(adiDstMass);
			DataReadLock argDistDecayB(adiDistDecayBetaParam);
			DataReadLock argDistLogitA(adiDistLogitAlphaParam);
			DataReadLock argDistLogitB(adiDistLogitBetaParam);
			DataReadLock argDistLogitC(adiDistLogitGammaParam);
			DataReadLock argOrgAlphaLock(adiOrgAlpha);
			DataReadLock argPrecalculatedNrDstZonesLock(adiPrecalculatedNrDstZones);

			DataReadLock argOrgZoneLocationLock(adiOrgZoneLocation);
			DataReadLock argDstZoneLocationLock(adiDstZoneLocation);
			DataReadLock argEuclidicSqrDistLock(adiEuclidicSqrDist);

			const ArgImpType* argLinkImp = const_array_cast<ImpType>(adiLinkImp);
			const ArgNodeType* argLinkF1 = const_array_cast<NodeType>(adiLinkF1);
			const ArgNodeType* argLinkF2 = const_array_cast<NodeType>(adiLinkF2);
			const ArgFlagType* argLinkBidirFlag = const_opt_array_checkedcast<Bool     >(adiLinkBidirFlag);
			const SrcNodeType* argStartPointNode = const_opt_array_checkedcast<NodeType >(adiStartPointNode);
			const SrcDistType* argStartPointImpedance = const_opt_array_checkedcast<ImpType  >(adiStartPointImpedance);
			const SrcZoneType* argStartPoinOrgZone = const_opt_array_checkedcast<ZoneType >(adiStartPoinOrgZone);
			const ZoneLocType* argOrgZoneLocation  = const_opt_array_checkedcast<euclid_location_t >(adiOrgZoneLocation);
			const DstNodeType* argEndPointNode = const_opt_array_checkedcast<NodeType >(adiEndPointNode);
			const DstDistType* argEndPointImpedance = const_opt_array_checkedcast<ImpType  >(adiEndPointImpedance);
			const DstZoneType* argEndPointDstZone = const_opt_array_checkedcast<ZoneType >(adiEndPointDstZone);
			const ZoneLocType* argDstZoneLocation = const_opt_array_checkedcast<euclid_location_t >(adiDstZoneLocation);
			const DstZoneType* argPrecalculatedNrDstZones = const_opt_array_checkedcast<ZoneType >(adiPrecalculatedNrDstZones);
			const ArgImpType* argOrgMinImp = const_opt_array_checkedcast<ImpType  >(adiOrgMinImp);
			const ArgImpType* argDstMinImp = const_opt_array_checkedcast<ImpType  >(adiDstMinImp);
			const ArgImpType* argOrgMaxImp = const_opt_array_checkedcast<ImpType  >(adiOrgMaxImp);
			const ArgMassType* argOrgMassLimit = const_opt_array_checkedcast<MassType >(adiOrgMassLimit);
			const ArgMassType* argDstMassLimit = const_opt_array_checkedcast<MassType >(adiDstMassLimit);
			const ArgImpType* argLinkAltImp = const_opt_array_checkedcast<MassType >(adiLinkAltImp);
			const ArgImpType* argLinkAttr = const_opt_array_checkedcast<MassType >(adiLinkAttr);
			const ArgSMType* argOrgMass = const_opt_array_checkedcast<MassType >(adiOrgMass);
			const ArgDMType* argDstMass = const_opt_array_checkedcast<MassType >(adiDstMass);
			const ArgParamType* argDistDecayBetaParam = const_opt_array_checkedcast<ParamType>(adiDistDecayBetaParam);
			const ArgParamType* argDistLogitAlphaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitAlphaParam);
			const ArgParamType* argDistLogitBetaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitBetaParam);
			const ArgParamType* argDistLogitGammaParam = const_opt_array_checkedcast<ParamType>(adiDistLogitGammaParam);
			const ArgParamType* argOrgAlpha = const_opt_array_checkedcast<ParamType>(adiOrgAlpha);

			if (IsDefined(vector_find_if(argLinkImp->GetLockedDataRead(), [](ImpType v) { return v < 0;  })))
				throwDmsErrD("Illegal negative value in Impedance data");

			bool isBidirectional = flags(df & (DijkstraFlag::Bidirectional|DijkstraFlag::BidirFlag));
			
			sqr_dist_t euclidicSqrDist = MAX_VALUE(sqr_dist_t); 
			if (adiEuclidicSqrDist)
				euclidicSqrDist = const_array_cast<sqr_dist_t>(adiEuclidicSqrDist)->GetTile(0)[0];

			CheckDefineMode(adiLinkImp, "Link_impedance");
			CheckNoneMode  (adiLinkF1, "Link_Node1_rel");
			CheckNoneMode  (adiLinkF2, "Link_Node2_rel");
			CheckNoneMode  (adiStartPointNode, "startPoint_Node_rel");
			CheckDefineMode(adiStartPointImpedance, "startPoint_impedance");
			CheckNoneMode  (adiStartPoinOrgZone, "startPoint_OrgZone_rel");
			CheckNoneMode  (adiEndPointNode, "endPoint_Node_rel");
			CheckDefineMode(adiEndPointImpedance, "endPoint_impedance");
			CheckNoneMode(adiEndPointDstZone, "endPoint_DstZone_rel");

			CheckDefineMode(adiOrgMaxImp, "OrgZone_MaxImpedance");
			CheckDefineMode(adiOrgMassLimit, "OrgZone_MaxMass");
			CheckDefineMode(adiDstMassLimit, "DstZone_MassLimit");
			CheckDefineMode(adiLinkAltImp, "Link_AltImpedance");
			CheckDefineMode(adiOrgMass, "OrgZone_Mass");
			CheckDefineMode(adiDstMass, "DstZone_Mass");
			CheckDefineMode(adiDistDecayBetaParam, "DistDecayBeta");
			CheckDefineMode(adiDistLogitAlphaParam, "DistLogitAlpha");
			CheckDefineMode(adiDistLogitBetaParam, "DistLogitBeta");
			CheckDefineMode(adiDistLogitGammaParam, "DistLogitGamma");
			CheckDefineMode(adiOrgAlpha, "OrgZone_Alpha");

			auto linkImpData           = argLinkImp->GetLockedDataRead();
			auto linkF1Data            = argLinkF1->GetLockedDataRead();
			auto linkF2Data            = argLinkF2->GetLockedDataRead();
			auto linkBidirFlagData     = argLinkBidirFlag       ? argLinkBidirFlag      ->GetLockedDataRead() : ArgFlagType ::locked_cseq_t();
			auto startpointNodeData    = argStartPointNode      ? argStartPointNode     ->GetLockedDataRead() : SrcNodeType ::locked_cseq_t();
			auto startpointImpData     = argStartPointImpedance ? argStartPointImpedance->GetLockedDataRead() : SrcDistType ::locked_cseq_t();
			auto startpointOrgZoneData = argStartPoinOrgZone    ? argStartPoinOrgZone   ->GetLockedDataRead() : SrcZoneType ::locked_cseq_t();
			auto orgZoneLocationData   = argOrgZoneLocation     ? argOrgZoneLocation    ->GetLockedDataRead() : ZoneLocType ::locked_cseq_t();
			auto endPoint_Node_rel_Data= argEndPointNode        ? argEndPointNode       ->GetLockedDataRead() : DstNodeType ::locked_cseq_t();
			auto endpointImpData       = argEndPointImpedance   ? argEndPointImpedance  ->GetLockedDataRead() : DstDistType ::locked_cseq_t();
			auto endpointDstZoneData   = argEndPointDstZone     ? argEndPointDstZone    ->GetLockedDataRead() : DstZoneType ::locked_cseq_t();
			auto dstZoneLocationData   = argDstZoneLocation     ? argDstZoneLocation    ->GetLockedDataRead() : ZoneLocType ::locked_cseq_t();
			auto orgMinImpData         = argOrgMinImp           ? argOrgMinImp          ->GetLockedDataRead() : ArgImpType  ::locked_cseq_t();
			auto dstMinImpData         = argDstMinImp           ? argDstMinImp          ->GetLockedDataRead() : ArgImpType  ::locked_cseq_t();
			auto orgMaxImpedances      = argOrgMaxImp           ? argOrgMaxImp          ->GetLockedDataRead() : ArgImpType  ::locked_cseq_t();
			auto orgMassLimit          = argOrgMassLimit        ? argOrgMassLimit       ->GetLockedDataRead() : ArgMassType ::locked_cseq_t();
			auto dstMassLimit          = argDstMassLimit        ? argDstMassLimit       ->GetLockedDataRead() : ArgMassType ::locked_cseq_t();
			auto altWeight             = argLinkAltImp          ? argLinkAltImp         ->GetLockedDataRead() : ArgImpType  ::locked_cseq_t();
			auto linkAttr              = argLinkAttr            ? argLinkAttr           ->GetLockedDataRead() : ArgImpType  ::locked_cseq_t();
			auto tgOrgMass             = argOrgMass             ? argOrgMass            ->GetLockedDataRead() : ArgSMType   ::locked_cseq_t();
			auto tgDstMass             = argDstMass             ? argDstMass            ->GetLockedDataRead() : ArgDMType   ::locked_cseq_t();
			auto tgDistDecayBetaParam  = argDistDecayBetaParam  ? argDistDecayBetaParam ->GetLockedDataRead() : ArgParamType::locked_cseq_t();
			auto tgDistLogitA          = argDistLogitAlphaParam ? argDistLogitAlphaParam->GetLockedDataRead() : ArgParamType::locked_cseq_t();
			auto tgDistLogitB          = argDistLogitBetaParam  ? argDistLogitBetaParam ->GetLockedDataRead() : ArgParamType::locked_cseq_t();
			auto tgDistLogitC          = argDistLogitGammaParam ? argDistLogitGammaParam->GetLockedDataRead() : ArgParamType::locked_cseq_t();
			auto tgOrgAlpha            = argOrgAlpha            ? argOrgAlpha           ->GetLockedDataRead() : ArgParamType::locked_cseq_t();

			NetworkInfo<NodeType, ZoneType, ImpType> networkInfo(
				v->GetCount(), e->GetCount()
			,	x->GetCount(), y->GetCount()
			,	orgZonesOrVoid->GetCount(), dstZones->GetCount()
			,	ZoneInfo<NodeType, ZoneType, ImpType>{startpointNodeData.begin(), startpointImpData.begin(), startpointOrgZoneData.begin(), orgZoneLocationData.begin()}
			,	ZoneInfo<NodeType, ZoneType, ImpType>{endPoint_Node_rel_Data.begin(), endpointImpData.begin(), endpointDstZoneData.begin(), dstZoneLocationData.begin()}
			);

			auto linkImpDataPtr = linkImpData.begin();
			GraphInfo<NodeType, LinkType, ImpType> graph{ linkF1Data.begin(), linkF2Data.begin(), linkImpDataPtr };
			graph.node_link1_inv.Init<NodeType>(linkF1Data.begin(), networkInfo.nrE, networkInfo.nrV);
			if (isBidirectional)
			{
				if (flags(df & DijkstraFlag::Bidirectional))
					graph.node_link2_inv.Init(linkF2Data.begin(), networkInfo.nrE, networkInfo.nrV);
				else
					graph.node_link2_inv.Init(linkF2Data.begin(), networkInfo.nrE, networkInfo.nrV, begin_ptr(linkBidirFlagData));
			}
			Inverted_rel<ZoneType> node_endPoint_inv;
			if (argEndPointNode)
				node_endPoint_inv.Init(endPoint_Node_rel_Data.begin(), networkInfo.nrY, networkInfo.nrV);

			SizeT nrRes = -1;
			OwningPtrSizedArray<SizeT> resCount;
			if (mutableResultUnit && flags(df & DijkstraFlag::OD_Data))
			{
				if (flags(df & DijkstraFlag::SparseResult))
				{
					resCount = OwningPtrSizedArray<SizeT>(networkInfo.nrOrgZones, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: resCount"));
					if (!flags(df & DijkstraFlag::PrecalculatedNrDstZones))
					{
						nrRes = ProcessDijkstra<NodeType, LinkType, ZoneType, ImpType, MassType, ParamType>(resultHolder, networkInfo
							, orgMaxImpedances.begin(), HasVoidDomainGuarantee(adiOrgMaxImp)
							, orgMassLimit.begin(), HasVoidDomainGuarantee(adiOrgMassLimit)
							, dstMassLimit.begin(), HasVoidDomainGuarantee(adiDstMassLimit)
							, euclidicSqrDist
							, graph
							, node_endPoint_inv
							, (df & ~DijkstraFlag::InteractionOrMaxImp & ~DijkstraFlag::UseLinkAttr) | DijkstraFlag::Counting
							, nullptr, HasVoidDomainGuarantee(adiLinkAltImp)
							, nullptr, HasVoidDomainGuarantee(adiLinkAttr)
							, nullptr, HasVoidDomainGuarantee(adiOrgMinImp)
							, nullptr, HasVoidDomainGuarantee(adiDstMinImp)
							, nullptr, HasVoidDomainGuarantee(adiOrgMass)
							, nullptr, HasVoidDomainGuarantee(adiDstMass)
							, ParamType()
							, ParamType(), ParamType(), ParamType()
							, nullptr, HasVoidDomainGuarantee(adiOrgAlpha)
							, nullptr, 0
							, resCount.begin()
							, ResultInfo<ZoneType, ImpType, MassType>()
						, no_tile
							, "Counting"
							);
						if (!IsDefined(nrRes))
							return false;
					}
					else
					{
						MG_CHECK(argPrecalculatedNrDstZones);
						auto precalculatedNrDstZoneData = argPrecalculatedNrDstZones->GetLockedDataRead();
						std::copy(precalculatedNrDstZoneData.begin(), precalculatedNrDstZoneData.end(), resCount.begin());
						nrRes = std::accumulate(precalculatedNrDstZoneData.begin(), precalculatedNrDstZoneData.end(), SizeT(0));
					}
					make_cumulative_base(resCount.begin(), resCount.begin() + networkInfo.nrOrgZones);
				}
				else
				{
					nrRes =  networkInfo.nrOrgZones;
					nrRes *= networkInfo.nrDstZones;
				}
				mutableResultUnit->SetCount(nrRes);
			}

			// Output data locking & final pass
			DataWriteLock resDistLock   (resDist);
			DataWriteLock resSrcZoneLock(resSrcZone);
			DataWriteLock resDstZoneLock(resDstZone);
			DataWriteLock resStartPointLock(resStartPoint);
			DataWriteLock resEndPointLock(resEndPoint);
			DataWriteLock resTraceBackLock(resTB);
			DataWriteLock resLinkSetLock(resLS,      dms_rw_mode::write_only_mustzero);

			DataWriteLock resALWLock(resAltLinkImp);
			DataWriteLock resLinkAttrLock (resLinkAttr);
			DataWriteLock resOrgNrDstZonesLock(resOrgNrDstZones);
			DataWriteLock resOrgSumImpLock(resOrgSumImp);
			DataWriteLock resOrgSumLinkAttrLock(resOrgSumLinkAttr);
			DataWriteLock resOrgFactorLock(resOrgFactor);
			DataWriteLock resODLock(resOrgDemand);
			DataWriteLock resOMILock(resOrgMaxImp);
			DataWriteLock resDFLock(resDstFactor,      dms_rw_mode::write_only_mustzero);
			DataWriteLock resDSLock(resDstSupply,      dms_rw_mode::write_only_mustzero);
			DataWriteLock resLinkFlowLock(resLinkFlow, dms_rw_mode::write_only_mustzero);

			SizeT nrRes2 = ProcessDijkstra<NodeType, LinkType, ZoneType, ImpType, MassType, ParamType>(resultHolder, networkInfo
			,	orgMaxImpedances.begin(), HasVoidDomainGuarantee(adiOrgMaxImp)
			,	orgMassLimit.begin(), HasVoidDomainGuarantee(adiOrgMassLimit)
			,	dstMassLimit.begin(), HasVoidDomainGuarantee(adiDstMassLimit)
			,	euclidicSqrDist
			,	graph, node_endPoint_inv
			,	df
			,	altWeight.begin(), HasVoidDomainGuarantee(adiLinkAltImp)
			,	linkAttr.begin(), HasVoidDomainGuarantee(adiLinkAttr)
			,	orgMinImpData.begin(), HasVoidDomainGuarantee(adiOrgMinImp)
			,	dstMinImpData.begin(), HasVoidDomainGuarantee(adiDstMinImp)
			,	tgOrgMass.begin(), HasVoidDomainGuarantee(adiOrgMass)
			,	tgDstMass.begin(), HasVoidDomainGuarantee(adiDstMass)
			,	argDistDecayBetaParam ? tgDistDecayBetaParam[0] : ParamType()
			,	argDistLogitAlphaParam? tgDistLogitA[0] : ParamType()
			,	argDistLogitBetaParam? tgDistLogitB[0] : ParamType()
			,	argDistLogitGammaParam? tgDistLogitC[0] : ParamType()
			,   tgOrgAlpha.begin(), HasVoidDomainGuarantee(adiOrgAlpha)
			,	resCount.begin(), nrRes
			,	nullptr
			,	ResultInfo<ZoneType, ImpType, MassType>{
      .od_ImpData          = resDist          ? mutable_array_cast<ImpType >(resDistLock          )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_AltLinkImp       = resAltLinkImp    ? mutable_array_cast<ImpType >(resALWLock           )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_LinkAttr         = resLinkAttr      ? mutable_array_cast<ImpType >(resLinkAttrLock      )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_SrcZoneIds       = resSrcZone       ? mutable_array_cast<ZoneType>(resSrcZoneLock       )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_DstZoneIds       = resDstZone       ? mutable_array_cast<ZoneType>(resDstZoneLock       )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_StartPointIds    = resStartPoint    ? mutable_array_cast<ZoneType>(resStartPointLock    )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_EndPointIds      = resEndPoint      ? mutable_array_cast<ZoneType>(resEndPointLock      )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .od_LS               = resLS            ? mutable_array_cast<EdgeSeq >(resLinkSetLock       )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : DataArray<EdgeSeq>::iterator()
				, .node_TB             = resTB            ? mutable_array_cast<LinkType>(resTraceBackLock     )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .orgZone_NrDstZones  = resOrgNrDstZones ? mutable_array_cast<ZoneType>(resOrgNrDstZonesLock )->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin() : nullptr
				, .orgZone_Factor      = resOrgFactor     ? mutable_array_cast<MassType>(resOrgFactorLock     )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .orgZone_Demand      = resOrgDemand     ? mutable_array_cast<MassType>(resODLock            )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .orgZone_SumImp      = resOrgSumImp     ? mutable_array_cast<MassType>(resOrgSumImpLock     )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .orgZone_SumLinkAttr = resOrgSumLinkAttr? mutable_array_cast<MassType>(resOrgSumLinkAttrLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .orgZone_MaxImp      = resOrgMaxImp     ? mutable_array_cast<ImpType >(resOMILock           )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .dstZone_Factor      = resDstFactor     ? mutable_array_cast<MassType>(resDFLock            )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .dstZone_Supply      = resDstSupply     ? mutable_array_cast<MassType>(resDSLock            )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				, .LinkFlow            = resLinkFlow      ? mutable_array_cast<MassType>(resLinkFlowLock      )->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero).begin() : nullptr
				}
			,	(flags(df & DijkstraFlag::OD_Data) || !mutableResultUnit) ? resultUnit->GetNrTiles() : no_tile
			,	"Filling"
			);
			if (!IsDefined(nrRes2))
				return false;

			if (mutableResultUnit)
			{
				if (!flags(df & DijkstraFlag::OD_Data))
				{ 
					mutableResultUnit->SetCount(nrRes2);
				}
			}
			if (resDist) resDistLock.Commit();
			if (resSrcZone) resSrcZoneLock.Commit();
			if (resDstZone) resDstZoneLock.Commit();
			if (resStartPoint) resStartPointLock.Commit();
			if (resEndPoint) resEndPointLock.Commit();
			if (resTB) resTraceBackLock.Commit();
			if (resLS) resLinkSetLock.Commit();
			if (resAltLinkImp) resALWLock.Commit();
			if (resLinkAttr) resLinkAttrLock.Commit();
			if (resOrgNrDstZones) resOrgNrDstZonesLock.Commit();
			if (resOrgFactor) resOrgFactorLock.Commit();
			if (resOrgDemand) resODLock.Commit();
			if (resOrgSumImp) resOrgSumImpLock.Commit();
			if (resOrgSumLinkAttr) resOrgSumLinkAttrLock.Commit();
			if (resOrgMaxImp) resOMILock.Commit();
			if (resDstFactor) resDFLock.Commit();
			if (resDstSupply) resDSLock.Commit();
			if (resLinkFlow) resLinkFlowLock.Commit();
		}
		return true;
	}

private:
	DijkstraFlag m_OperFlags;
};

// *****************************************************************************
// Instantiation & Registration
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"
#include "RtcInterface.h"

namespace
{
	oper_policy OldDijstraOperatorFlags()
	{
		// Dijkstra operator is renamed to impedance_table/matrix, but we want to keep the old name working for a while with a warning, 
		// so we mark it as depreciated in v19, obsolete in v20 and this code should be removed in v21.
		if (DMS_GetMajorVersionNumber() < 20)
			return oper_policy::depreciated;
		if (DMS_GetMajorVersionNumber() == 20)
			return oper_policy::obsolete;

		throwDmsErrD("This code should be removed in v21");
	}

	using DistTypeList = tl::type_list<Float64, Float32, UInt32, UInt64>;
	using DijkstraOperListType = tl_oper::inst_tuple_templ< DistTypeList, DijkstraMatrOperator>;

	Obsolete < CommonOperGroup> dsGroup("use impedance_table", "dijkstra_s", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting| OldDijstraOperatorFlags());
	Obsolete < CommonOperGroup> dm32Group("use impedance_matrix", "dijkstra_m", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting| OldDijstraOperatorFlags());
	Obsolete < CommonOperGroup> dm64Group("use impedance_matrix_od64", "dijkstra_m64", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting | OldDijstraOperatorFlags());

	CommonOperGroup itGroup("impedance_table", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting);
	CommonOperGroup im32Group("impedance_matrix", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting);
	CommonOperGroup im64Group("impedance_matrix_od64", oper_policy::allow_extra_args | oper_policy::better_not_in_meta_scripting);

	DijkstraOperListType dsOpers  (&dsGroup  , DijkstraFlag());
	DijkstraOperListType dm32Opers(&dm32Group, DijkstraFlag(DijkstraFlag::OD));
	DijkstraOperListType dm64Opers(&dm64Group, DijkstraFlag(DijkstraFlag::OD | DijkstraFlag::UInt64_Od));

	DijkstraOperListType itOpers(&itGroup, DijkstraFlag());
	DijkstraOperListType im32Opers(&im32Group, DijkstraFlag(DijkstraFlag::OD));
	DijkstraOperListType im64Opers(&im64Group, DijkstraFlag(DijkstraFlag::OD | DijkstraFlag::UInt64_Od));
}
