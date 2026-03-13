/// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__GEO_SEQUENCE_H)
#define __GEO_SEQUENCE_H

#include "geo/Point.h"
#include "geo/Geometry.h"
#include "geo/SequenceTraits.h"

//=======================================
// specific sequence_arrays for Points
//=======================================

#define DEFINE_POINTTYPES(PolyType, PointType) \
	typedef sequence_traits<PointType>::container_type PolyType; \
	typedef sequence_traits<PolyType >::container_type PolyType##Array; \

DEFINE_POINTTYPES(UInt64Seq, UInt64)
DEFINE_POINTTYPES(UInt32Seq, UInt32)
DEFINE_POINTTYPES(UInt16Seq, UInt16)
DEFINE_POINTTYPES(UInt8Seq, UInt8)
DEFINE_POINTTYPES(Int64Seq, Int64)
DEFINE_POINTTYPES(Int32Seq, Int32)
DEFINE_POINTTYPES(Int16Seq, Int16)
DEFINE_POINTTYPES(Int8Seq, Int8)
DEFINE_POINTTYPES(Float32Seq, Float32)
DEFINE_POINTTYPES(Float64Seq, Float64)

DEFINE_POINTTYPES(SPolygon, SPoint)
DEFINE_POINTTYPES(WPolygon, WPoint)
DEFINE_POINTTYPES(IPolygon, IPoint)
DEFINE_POINTTYPES(UPolygon, UPoint)

DEFINE_POINTTYPES(FPolygon, FPoint)
DEFINE_POINTTYPES(DPolygon, DPoint)

#undef DEFINE_POINTTYPES


//----------------------------------------------------------------------
// Section: vector_cut support
//----------------------------------------------------------------------

template <typename T> inline 
void vector_cut(sequence_array<T>& vec, typename sequence_array<T>::size_type  n) 
{
	vec.cut(n);
}

template <typename T> inline 
void vector_cut(SA_Reference<T>& sec, typename sequence_array<T>::size_type  n)
{
	sec.erase(sec.begin()+n, sec.end());
}

//----------------------------------------------------------------------
// Section: IsIncluding & MakeIncluding
//----------------------------------------------------------------------

template <typename T> inline
bool IsIncluding(const Range<T>& a, SA_ConstReference<T> p)
{ 
	return IsIncluding(a, p.begin(), p.end());
}

template <typename T> inline
bool IsIncluding(const Range<T>& a, const std::vector<T>& p)
{ 
	return IsIncluding(a, p.begin(), p.end());
}

template <class T> inline
void operator |= (Range<T>& a, SA_ConstReference<T> p)
{
	MakeIncluding(a, p.begin(), p.end());
}


template <typename T> inline
void operator |= (Range<T>& a, const std::vector<T>& p)
{
	MakeIncluding(a, p.begin(), p.end());
}


template <typename T>
inline Float64 AsFloat64(const std::vector<T>& x )
{
	throwIllegalAbstract(MG_POS, "Vector<T>::AsFloat64");
}

template <typename T>
inline Float64 AsFloat64(const locked_sequence<T>& x)
{
	throwIllegalAbstract(MG_POS, "locked_sequence<T>::AsFloat64");
}

#endif // !defined(__GEO_SEQUENCE_H)


