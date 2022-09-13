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

#if defined (DMS_TM_HAS_INT_SEQ)

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

#endif

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

#endif // !defined(__GEO_SEQUENCE_H)


