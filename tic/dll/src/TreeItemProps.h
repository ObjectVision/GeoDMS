// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_TREEITEMPROPS_H
#define __TIC_TREEITEMPROPS_H

#include "geo/Pair.h"
#include "mci/PropDef.h"

using PropBool = Bool;

TIC_CALL extern PropDef<TreeItem, SharedStr>* calcRulePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* descrPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* integrityCheckPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* explicitSupplPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* storageNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* storageTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* storageDriverPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* storageOptionsPropDefPtr;
TIC_CALL extern PropDef<TreeItem, PropBool >* storageReadOnlyPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* syncModePropDefPtr;

TIC_CALL extern PropDef<TreeItem, TokenID  >* dialogTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* dialogDataPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* paramTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* paramDataPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* labelPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* viewActionPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* configStorePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* caseDirPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* sourceDescrPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* sqlStringPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* tableTypeNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* cdfPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* urlPropDefPtr;

// Generic Properties

TIC_CALL SharedStr TreeItemPropertyValue       (const TreeItem* ti, const AbstrPropDef* pd);
TIC_CALL bool      TreeItemHasPropertyValue    (const TreeItem* ti, const AbstrPropDef* pd);

enum class SourceDescrMode {
	Configured = 0,
	ReadOnly = 1,
	WriteOnly = 2,
	All = 3,
	DatasetInfo
};

TIC_CALL SharedStr TreeItem_GetSourceDescr(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden); // defined in SourceDescr.cpp
TIC_CALL void TreeItem_DumpSourceCalculator(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden, OutStreamBase* xmlOutStrPtr);

#endif // __TIC_TREEITEMPROPS_H
