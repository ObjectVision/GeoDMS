// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// File: Dijkstra.h
//
// Overview:
//   Provides two related template classes implementing the core data
//   structures used for (variants of) Dijkstra's shortest path algorithm.
//   - DijkstraHeap:   Non-owning container managing tentative/final distances
//                     (impedances) and a binary heap of active nodes.
//   - OwningDijkstraHeap: Owning variant that allocates result arrays and
//                         (optionally) traceback information.
// 
// Core Concepts:
//   1. Distance / Impedance (ImpType):
//        The cost metric being minimized.
//   2. Heap of frontier nodes (m_NodeHeap):
//        Maintains nodes discovered but not yet finalized.
//   3. Zone Stamp Mechanism (m_SrcZoneStamp / m_CurrSrcZoneTick):
//        Optional lazy reset system to avoid O(n) reinitialization per run.
//        - When enabled (useSrcZoneStamps == true), we keep a per-node stamp
//          that matches m_CurrSrcZoneTick if the node's distance is valid
//          for the current source iteration.
//        - ResetImpedances() then becomes O(1); stale nodes are detected
//          via IsStale() and treated as having implicit distance = infinity.
//        - When zone stamps are NOT used, ResetImpedances() explicitly fills
//          the distance buffer with m_MaxImp.
//   4. Traceback (m_TraceBackDataPtr):
//        Optional array storing predecessor link (LinkType) for each node,
//        enabling path reconstruction.
//
// Template Parameters:
//   NodeType : integral (or similar) index type for graph nodes.
//   LinkType : type used to store backward reference for path rebuild
//              (may be small bit-packed type).
//   ZoneType : integral type for stamping iteration cycles.
//   ImpType  : numeric type representing distance/impedance.
//
// Invariants / Expectations:
//   - Distances are non-negative.
//   - A node inserted into the heap must have its distance recorded
//     (MarkTentative) before being considered for relaxation.
//   - MarkFinal is invoked after extracting the min-distance node; it
//     validates that the tentative distance is still optimal.
//   - m_MaxImp acts as a dynamic upper bound (e.g. for early stopping).
//
// Complexity Notes:
//   - InsertNode: O(log k) where k = current heap size.
//   - PopNode:    O(log k).
//   - ResetImpedances:
//        * O(1) if zone stamps are active.
//        * O(n) if explicit fill is required (no zone stamps).
//
// Thread Safety:
//   - Not thread-safe. External synchronization is required if shared.
//
// Potential Improvements (TODO):
//   - Provide custom comparator injection if heapElemType does not encode it.
//   - Add noexcept specifiers where safe.
//   - Consider small-vector optimization for tiny graphs.
//   - Provide an interface to shrink / clear heap without reallocation.
//   - Validate / fix trailing extra parenthesis in include guard end line.
//
// Caution:
//   - The trailing parenthesis in the final #endif looks unintended:
//       #endif //!defined(__GEO_DIJKSTRA_H))
//     Kept unchanged to avoid semantic edits.
//
//-----------------------------------------------------------------------------


#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__GEO_DIJKSTRA_H)
#define __GEO_DIJKSTRA_H

#include "vt/HeapElem.h"
#include "ptr/OwningPtrSizedArray.h"

#include <utility>

// *****************************************************************************
// DijkstraHeap
//   Non-owning base: external code must set m_ResultDataPtr (and optionally
//   m_TraceBackDataPtr) before invoking algorithmic operations.
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct DijkstraHeap
{
	typedef heapElemType<ImpType, NodeType> HeapElemType;  // Must provide ordering for std::push_heap/pop_heap
	typedef std::vector<HeapElemType>       HeapType;

	DijkstraHeap()
		: m_NrV(0)
	{
		ResetZoneStamps();
	}

	// Construct with number of vertices and optional zone stamp usage.
	DijkstraHeap(NodeType nrV, bool useSrcZoneStamps)
	{
		Init(nrV, useSrcZoneStamps);
	}

	// Initialize heap context for nrV nodes.
	// If useSrcZoneStamps == true, per-node lazy invalidation stamps are allocated.
	void Init(NodeType nrV, bool useSrcZoneStamps)
	{
		m_NrV = nrV;
		if (useSrcZoneStamps)
			m_SrcZoneStamp = OwningPtrSizedArray<ZoneType>(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_SrcZoneStamp"));
		ResetZoneStamps();
	}

	// Resets stamping state:
	//   - Sets current tick to UNDEFINED
	//   - Marks all stamps as undefined (if stamping is used)
	void ResetZoneStamps()
	{
		m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
		if (m_SrcZoneStamp)
			fast_undefine(m_SrcZoneStamp.begin(), m_SrcZoneStamp.begin() + m_NrV);
	}

	// Prepares for a new source run:
	//   - Increments tick (lazy invalidation when stamping active)
	//   - Otherwise performs an O(n) fill with m_MaxImp (infinity sentinel)
	void ResetImpedances()
	{
		++m_CurrSrcZoneTick;
		if (!m_SrcZoneStamp)
			fast_fill(m_ResultDataPtr, m_ResultDataPtr + m_NrV, m_MaxImp); // OPTIMIZE: INIT operations per Src of Complexity Order(nrV)
	}

	// Returns true if node v has no valid distance for the current tick.
	bool IsStale(NodeType v) const
	{
		assert(v < m_NrV);
		return m_SrcZoneStamp && (m_SrcZoneStamp[v] != m_CurrSrcZoneTick);
	}

	// Stamps node v as up-to-date for current tick.
	void Stamp(NodeType v)
	{
		assert(v < m_NrV);
		if (m_SrcZoneStamp)
			m_SrcZoneStamp[v] = m_CurrSrcZoneTick;
	}

	// Writes tentative distance d for node v and stamps it.
	void MarkTentative(NodeType v, ImpType d)
	{
		assert(v < m_NrV);
		m_ResultDataPtr[v] = d;
		Stamp(v);
	}

	// Returns true if d improves (is less than) currently known distance (or stale).
	bool IsBetter(NodeType v, ImpType d) const
	{
		assert(v < m_NrV);
		assert(IsStale(v) || m_ResultDataPtr[v] >= 0);
		return IsStale(v) || (d < m_ResultDataPtr[v]);
	}

	// Marks node v as finalized with distance d if consistent.
	// Returns true if finalization accepted; false if existing distance is better.
	bool MarkFinal(NodeType v, ImpType d)
	{
		assert(!IsStale(v)); // Must have been inserted already.
		assert(v < m_NrV);
		assert(m_ResultDataPtr[v] >= 0);
		if (m_ResultDataPtr[v] < d)
			return false;

		if (d < m_MaxImp) // m_MaxImp could have decreased due to external early-stop logic.
		{
			assert(m_ResultDataPtr[v] == d);
			return true;
		}
		return false;
	}

	// Attempts to insert node v with tentative distance d and optional backTrace.
	// Skips insertion if d >= m_MaxImp or not an improvement.
	// startPoint is the origin-side provenance of the accepted route: the start point that
	// seeded it. It is written in lockstep with the distance, so it is valid for exactly the
	// nodes MarkTentative stamped for the current tick. A variant that keeps node state per
	// pareto option must carry the provenance per option too, on this same write.
	void InsertNode(NodeType v, ImpType d, LinkType backTrace, ZoneType startPoint = UNDEFINED_VALUE(ZoneType))
	{
		if (d < m_MaxImp)
		{
			assert(v < m_NrV);
			assert(d >= 0);
			if (!IsBetter(v, d))
				return;

			// Push into binary heap; heapElemType defines ordering (likely min-heap via > comparator adaptation).
			m_NodeHeap.push_back(HeapElemType(v, d));
			std::push_heap(m_NodeHeap.begin(), m_NodeHeap.end());

			MarkTentative(v, d);
			if (m_TraceBackDataPtr)
				m_TraceBackDataPtr[v] = backTrace;
			if (m_StartPointDataPtr)
				m_StartPointDataPtr[v] = startPoint;
		}
	}

	// The start point that produced node v's current distance; undefined when provenance is
	// not tracked or v was never reached.
	ZoneType StartPointOf(NodeType v) const
	{
		if (!m_StartPointDataPtr || !IsDefined(v))
			return UNDEFINED_VALUE(ZoneType);
		assert(v < m_NrV);
		return m_StartPointDataPtr[v];
	}

	// Removes top (best) node from heap.
	void PopNode()
	{
		std::pop_heap(m_NodeHeap.begin(), m_NodeHeap.end());
		m_NodeHeap.pop_back();
	}

	// Heap state queries.
	bool                Empty() const { return m_NodeHeap.empty(); }
	const HeapElemType& Front() const { return m_NodeHeap.front(); }

	// External pointers (non-owning by this base type):
	ImpType* m_ResultDataPtr = nullptr; // Distance array (size: m_NrV)
	typename sequence_traits<LinkType>::seq_t::iterator m_TraceBackDataPtr = {}; // Optional traceback array
	ZoneType* m_StartPointDataPtr = nullptr; // Optional per-node origin start point (size: m_NrV)
	ImpType m_MaxImp = MaxValue<ImpType>(); // Sentinel for "infinite" distance / current cutoff

protected:
	NodeType m_NrV = 0;                          // Number of nodes
	ZoneType m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType); // Current stamp tick
	HeapType m_NodeHeap;                         // Binary heap container
	OwningPtrSizedArray<ZoneType> m_SrcZoneStamp; // Optional per-node stamp buffer
};

// *****************************************************************************
// OwningDijkstraHeap
//   Extends DijkstraHeap by allocating (owning) result buffers.
//   Optional allocation of traceback data controlled by useTraceBack.
// *****************************************************************************
template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct OwningDijkstraHeap : DijkstraHeap<NodeType, LinkType, ZoneType,ImpType>
{
	OwningDijkstraHeap()
	{}

	// Construct with ownership of distance and optional traceback / start-point arrays.
	OwningDijkstraHeap(NodeType nrV, bool useSrcZoneStamps, bool useTraceBack, bool useStartPoints = false)
	{
		Init(nrV, useSrcZoneStamps, useTraceBack, useStartPoints);
	}

	// Initializes base and allocates buffers if not already allocated.
	void Init(NodeType nrV, bool useSrcZoneStamps, bool useTraceBack, bool useStartPoints = false)
	{
		DijkstraHeap<NodeType, LinkType, ZoneType, ImpType>::Init(nrV, useSrcZoneStamps);
		if (nrV && !m_ResultData)
		{
			m_ResultData = OwningPtrSizedArray<ImpType>(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_ResultData"));
			this->m_ResultDataPtr = m_ResultData.begin();
			if (useTraceBack && !m_TraceBackData)
			{
				m_TraceBackData = OwningPtrSizedArray<LinkType>(nrV, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_TraceBackData"));
				this->m_TraceBackDataPtr = m_TraceBackData.begin();
			}
			// Undefined() rather than dont_initialize: a node that this origin never reached must
			// read back as undefined, since the provenance is not re-initialised per origin.
			if (useStartPoints && !m_StartPointData)
			{
				m_StartPointData = OwningPtrSizedArray<ZoneType>(nrV, Undefined() MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_StartPointData"));
				this->m_StartPointDataPtr = m_StartPointData.begin();
			}
		}
	}

	// Copy constructor delegates to unified Init logic (note: shallow semantics for stamps/traceback pointer usage).
	OwningDijkstraHeap(const OwningDijkstraHeap& rhs)
		: OwningDijkstraHeap(rhs.m_NrV, rhs.m_SrcZoneStamp, rhs.m_TraceBackDataPtr, rhs.m_StartPointDataPtr)
	{}

	// Owned arrays:
	OwningPtrSizedArray<ImpType> m_ResultData;      // Distance buffer
	OwningPtrSizedArray<LinkType> m_TraceBackData;  // Optional predecessor links
	OwningPtrSizedArray<ZoneType> m_StartPointData; // Optional per-node origin start point
	OwningPtrSizedArray<ImpType> m_AltLinkWeight,   // per-node scratch: UpdateALW accumulator for alternative impedance, reused as the link-flow accumulator
                             m_LinkAttr;         // per-node scratch: UpdateALW accumulator for link_attr
};

// *****************************************************************************
// BiCriteriaDijkstraHeap
//   Label-setting heap for the bi-criteria (pareto) variant -- issue #856. The
//   heap holds LABELS (imp, imp2, node), several of which may refer to the same
//   node, in lexicographic (imp, imp2) order: heapElemType over std::pair,
//   whose operator> is exactly that order.
//
//   Dominance needs only ONE scalar per node (Hansen 1980): labels pop in
//   lexicographically nondecreasing order, so a popped label (t, c) at node v
//   is Pareto-optimal among all v-paths iff c < m_MinImp2[v], the minimum imp2
//   over labels ACCEPTED at v so far. Accepted labels per node are then exactly
//   the distinct Pareto front, in strictly increasing imp and strictly
//   decreasing imp2; in particular the FIRST accepted label per node carries
//   the scalar engine's minimum imp. The same test pre-prunes at push time,
//   since m_MinImp2 is nonincreasing during a run; pending labels are not
//   compared against each other, which only makes the queue larger, not the
//   front wrong. NOTE: this reduction is intrinsically 2-dimensional -- three
//   or more criteria need real per-node front storage, not a scalar.
//
//   Correctness preconditions: BOTH per-link weight arrays nonnegative
//   (negative imp2 cycles yield unbounded label sets), and the heap ordered on
//   the FULL pair -- an imp-only order with imp2 as payload can pop equal-imp
//   labels of one node in the wrong order and leak a dominated pair into the
//   front.
//
//   m_MinImp2 reuses the zone-stamp lazy reset of DijkstraHeap: a stale stamp
//   reads as +infinity, keeping ResetImpedances O(1) per origin. Both cutoffs
//   are fixed per origin (no limit() in this mode), so a label admitted at push
//   time stays admissible.
// *****************************************************************************
template <typename NodeType, typename ZoneType, typename ImpType>
struct BiCriteriaDijkstraHeap
{
	using ImpPairType = std::pair<ImpType, ImpType>; // (imp, imp2), compared lexicographically

	// The heap's payload: the labeled node plus the start point that seeded the label's route.
	// StartPoint_rel is an attribute of the ROUTE, and a node's accepted labels may descend
	// from different start points, so the scalar heap's per-node provenance array cannot serve
	// here -- provenance travels in the label. For 8-byte-aligned ImpTypes the extra field
	// rides the element's existing padding for free.
	struct LabelRef { NodeType node; ZoneType startPoint; };
	using HeapElemType = heapElemType<ImpPairType, LabelRef>;
	using HeapType = std::vector<HeapElemType>;

	void Init(NodeType nrV, bool useSrcZoneStamps)
	{
		m_NrV = nrV;
		if (nrV && !m_MinImp2)
			m_MinImp2 = OwningPtrSizedArray<ImpType>(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_MinImp2"));
		if (useSrcZoneStamps && !m_SrcZoneStamp)
			m_SrcZoneStamp = OwningPtrSizedArray<ZoneType>(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: bi m_SrcZoneStamp"));
		ResetZoneStamps();
	}

	void ResetZoneStamps()
	{
		m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
		if (m_SrcZoneStamp)
			fast_undefine(m_SrcZoneStamp.begin(), m_SrcZoneStamp.begin() + m_NrV);
	}

	// Prepares for a new origin: O(1) with stamps, O(n) fill otherwise.
	void ResetImpedances()
	{
		++m_CurrSrcZoneTick;
		if (!m_SrcZoneStamp)
			fast_fill(m_MinImp2.begin(), m_MinImp2.begin() + m_NrV, MaxValue<ImpType>());
	}

	bool IsStale(NodeType v) const
	{
		assert(v < m_NrV);
		return m_SrcZoneStamp && (m_SrcZoneStamp[v] != m_CurrSrcZoneTick);
	}

	// True iff a label with second criterion d2 at node v is not (weakly) dominated by any
	// label accepted at v so far: every accepted label has imp <= any future label's imp,
	// so dominance reduces to this imp2 comparison. Weak (>= rejects) so that duplicates
	// and zero-weight cycles terminate, mirroring the strict < of IsBetter.
	bool IsUndominated(NodeType v, ImpType d2) const
	{
		assert(v < m_NrV);
		return IsStale(v) || d2 < m_MinImp2[v];
	}

	// Attempt to push label (d, d2) for node v; prunes on both cutoffs and on dominance.
	// startPoint is the origin-side provenance of the route the label extends.
	void InsertLabel(NodeType v, ImpType d, ImpType d2, ZoneType startPoint)
	{
		if (d >= m_MaxImp || d2 >= m_MaxImp2)
			return;
		assert(v < m_NrV);
		assert(d >= 0);
		assert(d2 >= 0);
		if (!IsUndominated(v, d2))
			return;
		m_LabelHeap.push_back(HeapElemType(LabelRef{ v, startPoint }, ImpPairType(d, d2)));
		std::push_heap(m_LabelHeap.begin(), m_LabelHeap.end());
	}

	// Pop-time acceptance: re-test dominance (a cheaper-imp2 label for v may have been
	// accepted after this label was pushed) and record the new per-node minimum.
	bool AcceptLabel(NodeType v, ImpType d2)
	{
		assert(v < m_NrV);
		if (!IsUndominated(v, d2))
			return false;
		m_MinImp2[v] = d2;
		if (m_SrcZoneStamp)
			m_SrcZoneStamp[v] = m_CurrSrcZoneTick;
		return true;
	}

	void PopLabel()
	{
		std::pop_heap(m_LabelHeap.begin(), m_LabelHeap.end());
		m_LabelHeap.pop_back();
	}

	bool                Empty() const { return m_LabelHeap.empty(); }
	const HeapElemType& Front() const { return m_LabelHeap.front(); }

	ImpType m_MaxImp  = MaxValue<ImpType>(); // cutoff on the first criterion: cut(OrgZone_max_imp), required in pareto mode
	ImpType m_MaxImp2 = MaxValue<ImpType>(); // optional cutoff on the second criterion: pareto(OrgZone_max_imp2)

	OwningPtrSizedArray<ImpType>  m_MinImp2;      // min imp2 over ACCEPTED labels, per node

protected:
	NodeType m_NrV = 0;
	ZoneType m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
	HeapType m_LabelHeap;
	OwningPtrSizedArray<ZoneType> m_SrcZoneStamp; // optional per-node stamp buffer (lazy reset)
};

#endif //!defined(__GEO_DIJKSTRA_H))
