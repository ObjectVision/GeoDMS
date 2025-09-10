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

#include "geo/HeapElem.h"
#include "ptr/OwningPtrSizedArray.h"

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
	void InsertNode(NodeType v, ImpType d, LinkType backTrace)
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
		}
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

	// Construct with ownership of distance and optional traceback arrays.
	OwningDijkstraHeap(NodeType nrV, bool useSrcZoneStamps, bool useTraceBack)
	{
		Init(nrV, useSrcZoneStamps, useTraceBack);
	}

	// Initializes base and allocates buffers if not already allocated.
	void Init(NodeType nrV, bool useSrcZoneStamps, bool useTraceBack)
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
		}
	}

	// Copy constructor delegates to unified Init logic (note: shallow semantics for stamps/traceback pointer usage).
	OwningDijkstraHeap(const OwningDijkstraHeap& rhs)
		: OwningDijkstraHeap(rhs.m_NrV, rhs.m_SrcZoneStamp, rhs.m_TraceBackDataPtr)
	{}

	// Owned arrays:
	OwningPtrSizedArray<ImpType> m_ResultData;      // Distance buffer
	OwningPtrSizedArray<LinkType> m_TraceBackData;  // Optional predecessor links
	OwningPtrSizedArray<ImpType> m_AltLinkWeight,   // (Potential future use: alternative edge weights)
                             m_LinkAttr;         // (Potential future use: edge attribute storage)
};

#endif //!defined(__GEO_DIJKSTRA_H))
