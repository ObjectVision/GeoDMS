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
// *****************************************************************************
// Project:     Storage
//
// Module/Unit: GeolibInterface.h
//
// Copyright (C)1998 YUSE GSO Object Vision B.V.
// Author:      M. Hilferink
// Created on:  15/09/98 15:13:17
// *****************************************************************************

#if !defined(__STG_GEOLIB_STORAGEMANAGER_H)
#define __STG_GEOLIB_STORAGEMANAGER_H

#include "dbg/check.h"

#define GL_TYPE_INTEGER		1
#define GL_TYPE_FLOAT		2
#define GL_TYPE_STRING		3
#define GL_TYPE_DATE		4
#define GL_TYPE_BOOL		5
//#define GL_TYPE_UINT32		6
#define GL_TYPE_UINT32		1
#define GL_TYPE_BYTE		7
#define GL_TYPE_CHAR		8
#define GL_TYPE_POINT		10
#define GL_TYPE_BOX			11
#define GL_TYPE_CIRCLE		12
#define GL_TYPE_LINE		13
#define GL_TYPE_POLY		14
#define GL_TYPE_REGION		15
#define GL_TYPE_GRID		16
#define GL_TYPE_UNKNOWN		-1

const Int32 GL_MAX_STRING = 256;
#ifdef GL_USE_OLDSTRUCT
const Int32 GL_NAME_LEN   = 44;
#else
const Int32 GL_NAME_LEN   = GL_MAX_STRING;
#endif

struct GlColumninfoStruct {

#ifdef GL_USE_OLDSTRUCT
	char Name[GL_NAME_LEN];
	char Description[255];
	Int32 ColumnType;
	Int32 Length;
	Int32 Decimals;
#else // Geolib Version 3.0.1.2: 14-7-1999
	char Name[GL_NAME_LEN];
	char Description[GL_MAX_STRING];
	Int32 ColumnType;
	Int32 Length;
	Int32 Decimals;
	Int32 Offset;
	Int32 AsString;
	Int32 UseNodata;
// MTA: 27-1-2001: op basis van overleg met Michel Bakkenes ivm GGl32 19-10-2000
#ifdef GL_3_2_1_0 
	Int32	Hidden;
	Int32 ReadOnly;
#endif
	double Nodata;

#endif

};

//typedef Int32 (__stdcall* PFUNC_TableExists)(const char*);
typedef Int32 (__stdcall* PFUNC_TableOpen)(const char*,Int32);
typedef Int32 (__stdcall* PFUNC_TableClose)(Int32);
typedef Int32 (__stdcall* PFUNC_TableCreateByStruct)(const char*,GlColumninfoStruct*,Int32,Int32,Int32,Int32);

typedef Int32 (__stdcall* PFUNC_TableRowCount)(Int32);
typedef Int32 (__stdcall* PFUNC_TableColumnCount)(Int32);
typedef Int32 (__stdcall* PFUNC_TableColumnInfoGet)(Int32,Int32,const char*,GlColumninfoStruct*);

typedef Int32 (__stdcall* PFUNC_TableRowAppend)(Int32, UInt32);

typedef Int32 (__stdcall* PFUNC_TableFloatSetByIndex)(Int32,Int32,Int32,Float64);
typedef Int32 (__stdcall* PFUNC_TableIntegerSetByIndex)(Int32,Int32,Int32,Int32);
typedef Int32 (__stdcall* PFUNC_TableStringGetByIndex)(Int32 TableHandle, Int32 rowIndex, Int32 colIndex, char* value);
typedef Int32 (__stdcall* PFUNC_TableStringSetByIndex)(Int32 TableHandle, Int32 rowIndex, Int32 colIndex, const char* value);

typedef Int32 (__stdcall* PFUNC_TableFloatColumnGet)(Int32,const char*,Int32,Int32,Float64*,Int32);
typedef Int32 (__stdcall* PFUNC_TableIntegerColumnGet)(Int32,const char*,Int32,Int32,Int32*,Int32);
typedef Int32 (__stdcall* PFUNC_TableFloatColumnSet)(Int32,const char*,Int32,Int32,Float64*,Int32);
typedef Int32 (__stdcall* PFUNC_TableIntegerColumnSet)(Int32,const char*,Int32,Int32,Int32*,Int32);




#endif // __STG_GEOLIB_STORAGEMANAGER_H