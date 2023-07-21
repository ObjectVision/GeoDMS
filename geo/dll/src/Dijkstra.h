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

#pragma once

#if !defined(__GEO_DIJKSTRA_H)
#define __GEO_DIJKSTRA_H

#include "geo/HeapElem.h"
#include "ptr/OwningPtrSizedArray.h"

// *****************************************************************************
//									DijkstraHeap
// *****************************************************************************

template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct DijkstraHeap
{
	typedef heapElemType<ImpType, NodeType> HeapElemType;
	typedef std::vector<HeapElemType> HeapType;

	DijkstraHeap()
		: m_NrV(0)
	{
		ResetZoneStamps();
	}

	DijkstraHeap(NodeType nrV, bool useSrcZoneStamps)
	{
		Init(nrV, useSrcZoneStamps);
	}

	void Init(NodeType nrV, bool useSrcZoneStamps)
	{
		m_NrV = nrV;
		if (useSrcZoneStamps)
			m_SrcZoneStamp = OwningPtrSizedArray<ZoneType>(nrV, dont_initialize MG_DEBUG_ALLOCATOR_SRC("dijkstra: m_SrcZoneStamp"));
		ResetZoneStamps();
	}

	void ResetZoneStamps()
	{
		m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
		if (m_SrcZoneStamp)
			fast_undefine(m_SrcZoneStamp.begin(), m_SrcZoneStamp.begin() + m_NrV);
	}

	void ResetImpedances()
	{
		++m_CurrSrcZoneTick;
		if (!m_SrcZoneStamp)
			fast_fill(m_ResultDataPtr, m_ResultDataPtr + m_NrV, m_MaxImp); // OPTIMIZE: INIT operations per Src of Complexity Order(nrV)	
	}
	bool IsStale(NodeType v)
	{
		dms_assert(v < m_NrV);
		return m_SrcZoneStamp && (m_SrcZoneStamp[v] != m_CurrSrcZoneTick);
	}
	void Stamp(NodeType v)
	{
		dms_assert(v < m_NrV);
		if (m_SrcZoneStamp)
			m_SrcZoneStamp[v] = m_CurrSrcZoneTick;
	}

	void MarkTentative(NodeType v, ImpType d)
	{
		dms_assert(v < m_NrV);
		m_ResultDataPtr[v] = d;
		Stamp(v);
	}
	bool IsBetter(NodeType v, ImpType d)
	{
		dms_assert(v < m_NrV);

		dms_assert(IsStale(v) || m_ResultDataPtr[v] >= 0);
		return IsStale(v) || (d < m_ResultDataPtr[v]);
	}
	bool MarkFinal(NodeType v, ImpType d)
	{
		dms_assert(!IsStale(v)); // was already inserted in heap.
		dms_assert(v < m_NrV);
		dms_assert(m_ResultDataPtr[v] >= 0);
		if (m_ResultDataPtr[v] < d)
			return false;
		if (d >= m_MaxImp) // maximp could have been reduced when dstlimit was reached.
			return false;
		dms_assert(m_ResultDataPtr[v] == d); // was already marked tentatively, which is 
		return true;
	}

	void InsertNode(NodeType v, ImpType d, LinkType backTrace)
	{
		if (d >= m_MaxImp)
			return;
		dms_assert(v < m_NrV);

		dms_assert(d >= 0);
		if (!IsBetter(v, d))
			return;

		m_NodeHeap.push_back(HeapElemType(v, d));
		std::push_heap(m_NodeHeap.begin(), m_NodeHeap.end());

		MarkTentative(v, d);
		if (m_TraceBackDataPtr)
			m_TraceBackDataPtr[v] = backTrace;
	}
	void PopNode()
	{
		std::pop_heap(m_NodeHeap.begin(), m_NodeHeap.end());
		m_NodeHeap.pop_back();
	}

	bool                Empty() const { return m_NodeHeap.empty(); }
	const HeapElemType& Front() const { return m_NodeHeap.front(); }

	ImpType* m_ResultDataPtr = nullptr;
	typename sequence_traits<LinkType>::seq_t::iterator m_TraceBackDataPtr = typename sequence_traits<LinkType>::seq_t::iterator(); // LinkType can be BitValue<4>
	ImpType m_MaxImp = MaxValue<ImpType>();

protected:
	NodeType m_NrV = 0;
	ZoneType m_CurrSrcZoneTick = UNDEFINED_VALUE(ZoneType);
	HeapType m_NodeHeap;
	OwningPtrSizedArray<ZoneType> m_SrcZoneStamp;
};

template <typename NodeType, typename LinkType, typename ZoneType, typename ImpType>
struct OwningDijkstraHeap : DijkstraHeap<NodeType, LinkType, ZoneType,ImpType>
{
	OwningDijkstraHeap()
	{}

	OwningDijkstraHeap(NodeType nrV, bool useSrcZoneStamps, bool useTraceBack)
	{
		Init(nrV, useSrcZoneStamps, useTraceBack);
	}
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

	OwningDijkstraHeap(const OwningDijkstraHeap& rhs)
		: OwningDijkstraHeap(rhs.m_NrV, rhs.m_SrcZoneStamp, rhs.m_TraceBackDataPtr)
	{}

	OwningPtrSizedArray<ImpType> m_ResultData;
	OwningPtrSizedArray<LinkType> m_TraceBackData;
	OwningPtrSizedArray<ImpType> m_AltLinkWeight;
};

#endif //!defined(__GEO_DIJKSTRA_H))
