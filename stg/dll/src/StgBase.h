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

#if !defined(__STG_BASE_H)
#define __STG_BASE_H

#include "TicBase.h"
#include "stg/AbstrStorageManager.h"

#include <set>
#include "geo/color.h"

#ifdef DMSTGDLL_EXPORTS
#	define STGDLL_CALL __declspec(dllexport)
#else
#	ifdef DMSTGDLL_STATIC
#		define STGDLL_CALL
#	else
#		define STGDLL_CALL __declspec(dllimport)
#	endif
#endif

#define STGIMPL_CALL

const PALETTE_SIZE CI_NODATA     = 255;
const PALETTE_SIZE CI_BACKGROUND = 256;
const PALETTE_SIZE CI_NRCOLORS   = 256;
const PALETTE_SIZE CI_RAMPSTART  = 257;
const PALETTE_SIZE CI_RAMPEND    = 258;
const PALETTE_SIZE CI_LAST       = 258;

extern "C" STGDLL_CALL void     DMS_CONV STG_Bmp_SetDefaultColor(PALETTE_SIZE i, DmsColor color);
extern "C" STGDLL_CALL DmsColor DMS_CONV STG_Bmp_GetDefaultColor(PALETTE_SIZE i);
extern "C" STGDLL_CALL void DMS_Stg_Load();

//----------------------------------------------------------------------
// section : generic Storage related TreeItem helper funcs
//----------------------------------------------------------------------

bool CompatibleTypes(const ValueClass* dbCls, const ValueClass* configCls);
bool TreeItemIsColumn(TreeItem *ti);
STGDLL_CALL bool CreateTreeItemColumnInfo(TreeItem* tiTable, CharPtr colName, const AbstrUnit* domainUnit, const ValueClass* dbValuesClass);

#endif __STG_BASE_H
