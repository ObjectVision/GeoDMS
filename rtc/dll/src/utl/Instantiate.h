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

#if !defined(__RTC_UTL_INSTANTIATE_H)
#define __RTC_UTL_INSTANTIATE_H

#include "RtcTypeModel.h"

//----------------------------------------------------------------------
// Some macros to support instantiation of a set of value-types
//----------------------------------------------------------------------

#if defined(DMS_TM_HAS_UINT2)
#define INSTANTIATE_UINT2  INSTANTIATE(UInt2)
#else
#define INSTANTIATE_UINT2
#endif

#if defined(DMS_TM_HAS_UINT4)
#define INSTANTIATE_UINT4  INSTANTIATE(UInt4)
#else
#define INSTANTIATE_UINT4
#endif

#if defined (DMS_TM_HAS_INT64)
#define INSTANTIATE_UINT64  INSTANTIATE(UInt64)
#define INSTANTIATE_INT64   INSTANTIATE(Int64)
#else
#define INSTANTIATE_UINT64
#define INSTANTIATE_INT64
#endif

#define INSTANTIATE_SINTS_D32     INSTANTIATE( Int32) INSTANTIATE( Int16) INSTANTIATE( Int8) 
#define INSTANTIATE_UINTS_ORG_D32 INSTANTIATE(UInt32) INSTANTIATE(UInt16) INSTANTIATE(UInt8)

#if defined(DMS_64)
#define INSTANTIATE_SINTS_D     INSTANTIATE_INT64  INSTANTIATE_SINTS_D32 
#define INSTANTIATE_UINTS_ORG_D INSTANTIATE_UINT64 INSTANTIATE_UINTS_ORG_D32
#define INSTANTIATE_SINTS     INSTANTIATE_SINTS_D
#define INSTANTIATE_UINTS_ORG INSTANTIATE_UINTS_ORG_D
#else
#define INSTANTIATE_SINTS_D     INSTANTIATE_SINTS_D32 
#define INSTANTIATE_UINTS_ORG_D INSTANTIATE_UINTS_ORG_D32
#define INSTANTIATE_SINTS     INSTANTIATE_INT64  INSTANTIATE_SINTS_D
#define INSTANTIATE_UINTS_ORG INSTANTIATE_UINT64 INSTANTIATE_UINTS_ORG_D
#endif

#define INSTANTIATE_UINTS_NEW INSTANTIATE_UINT4 INSTANTIATE_UINT2

#define INSTANTIATE_UINTS INSTANTIATE_UINTS_ORG INSTANTIATE_UINTS_NEW
#define INSTANTIATE_INTS_ORG_D INSTANTIATE_UINTS_ORG_D INSTANTIATE_SINTS_D
#define INSTANTIATE_INTS_ORG   INSTANTIATE_UINTS_ORG   INSTANTIATE_SINTS
#define INSTANTIATE_INTS INSTANTIATE_UINTS INSTANTIATE_SINTS

#if defined(DMS_TM_HAS_FLOAT80)
#define INSTANTIATE_FLOATS INSTANTIATE(Float32) INSTANTIATE(Float64) INSTANTIATE(Float80)
#else
#define INSTANTIATE_FLOATS INSTANTIATE(Float32) INSTANTIATE(Float64)
#endif

#define INSTANTIATE_POINTS INSTANTIATE(SPoint) INSTANTIATE(WPoint) INSTANTIATE(IPoint) INSTANTIATE(UPoint) INSTANTIATE(FPoint) INSTANTIATE(DPoint)

#define INSTANTIATE_RECTS INSTANTIATE(SRect) INSTANTIATE(WRect) INSTANTIATE(IRect) INSTANTIATE(URect) INSTANTIATE(FRect) INSTANTIATE(DRect)

#if defined(DMS_TM_HAS_INT_SEQ)
#define INSTANTIATE_SINT_POLYS_OBSOLETE INSTANTIATE(Int64Seq) INSTANTIATE(Int32Seq) INSTANTIATE(Int16Seq) INSTANTIATE(Int8Seq)
#define INSTANTIATE_UINT_POLYS_OBSOLETE INSTANTIATE(UInt64Seq) INSTANTIATE(UInt32Seq) INSTANTIATE(UInt16Seq) INSTANTIATE(UInt8Seq)
#define INSTANTIATE_FLOAT_POLYS_OBSOLETE INSTANTIATE(Float32Seq) INSTANTIATE(Float64Seq)
#define INSTANTIATE_NUM_POLYS_OBSOLETE INSTANTIATE_SINT_POLYS_OBSOLETE INSTANTIATE_UINT_POLYS_OBSOLETE INSTANTIATE_FLOAT_POLYS_OBSOLETE
#define INSTANTIATE_POINT_POLYS_OBSOLETE INSTANTIATE(SPolygon) INSTANTIATE(IPolygon) INSTANTIATE(WPolygon) INSTANTIATE(UPolygon) INSTANTIATE(FPolygon) INSTANTIATE(DPolygon)
#define INSTANTIATE_POLYS INSTANTIATE_NUM_POLYS_OBSOLETE INSTANTIATE_POINT_POLYS_OBSOLETE 
#else
#define INSTANTIATE_POLYS INSTANTIATE(FPolygon) INSTANTIATE(DPolygon)
#endif

#define INSTANTIATE_VOID  INSTANTIATE(Void)
#define INSTANTIATE_BOOL  INSTANTIATE(Bool)
#define INSTANTIATE_OTHER INSTANTIATE_BOOL INSTANTIATE(String)

#define INSTANTIATE_ORD_ELEM    INSTANTIATE_UINTS_ORG_D                   // typelists::uints


#define X_DOMAIN_INTS_ORG INSTANTIATE_INTS_ORG_D


#define INSTANTIATE_DOMAIN_INTS INSTANTIATE_UINTS_NEW INSTANTIATE_BOOL X_DOMAIN_INTS_ORG // typelists::domain_ints
// typelists::domain_elements
#define X_DOMAIN_POINTS         INSTANTIATE(SPoint) INSTANTIATE(IPoint) INSTANTIATE(WPoint) INSTANTIATE(UPoint)

#define INSTANTIATE_CNT_ELEM    INSTANTIATE_DOMAIN_INTS X_DOMAIN_POINTS
#define INSTANTIATE_CNT_VAL     X_DOMAIN_INTS_ORG       X_DOMAIN_POINTS

#define INSTANTIATE_NUM_ORG     INSTANTIATE_INTS_ORG INSTANTIATE_FLOATS // typelists::num_objects (also includes Bool)
#define INSTANTIATE_NUM_ELEM    INSTANTIATE_INTS INSTANTIATE_FLOATS     // typelists::numerics (also includes Bool)
#define INSTANTIATE_CNV_ELEM    INSTANTIATE_NUM_ELEM INSTANTIATE_OTHER  // typelists::scalars

#define INSTANTIATE_FLD_ELEM    INSTANTIATE_CNV_ELEM INSTANTIATE_POINTS // typelists::fields
#define INSTANTIATE_ALL_ELEM    INSTANTIATE_FLD_ELEM INSTANTIATE_POLYS  // typelists::value_elements 

//REMOVE #define INSTANTIATE_ALL            // typelists::value_types

#define INSTANTIATE_ALL_VC      INSTANTIATE_ALL_ELEM INSTANTIATE_VOID INSTANTIATE_RECTS INSTANTIATE(TokenID)

//REMOVE #define INSTANTIATE_ALL_UNITS INSTANTIATE_NUM_ORG INSTANTIATE_POINTS INSTANTIATE_UINTS_NEW INSTANTIATE_OTHER INSTANTIATE_VOID // typelists::all_unit_types


#endif // __RTC_UTL_INSTANTIATE_H
