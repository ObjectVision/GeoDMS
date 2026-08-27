// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  heapElemType<Dist,Value>: the (distance, value) pair with heap ordering,
 *  used by Dijkstra-style algorithms.
 */

#if !defined(__VT_HEAPELEM_H)
#define __VT_HEAPELEM_H

#include "RtcBase.h"

// *****************************************************************************
//									heapElemType
// *****************************************************************************

template <typename DistType, typename ValueType = SizeT>
struct heapElemType
{
	heapElemType(ValueType v, DistType d) : m_Value(v), m_Imp(d) {};

	const ValueType& Value() const { return m_Value; }
	const DistType & Imp () const { return m_Imp;   }

	bool operator <(const heapElemType& b) const { return m_Imp > b.m_Imp; } // bubble smallest values to the top of the heap first
private:
	ValueType m_Value;
	DistType  m_Imp;
};

#endif //!defined(__VT_HEAPELEM_H)
