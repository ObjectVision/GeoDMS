// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__STG_BASE_H)
#define __STG_BASE_H

#include "TicBase.h"
#include "stg/AbstrStorageManager.h"
#include "stg/StorageClass.h"

#include <set>
#include "vt/color.h"
#include "geom/Transform.h"

#if !defined(_MSC_VER)
#	define STGDLL_CALL
#elif defined(DMSTGDLL_EXPORTS)
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
bool CreateTreeItemColumnInfo(TreeItem* tiTable, CharPtr colName, const AbstrUnit* domainUnit, const ValueClass* dbValuesClass, ValueComposition vc = ValueComposition::Single);

#endif __STG_BASE_H
