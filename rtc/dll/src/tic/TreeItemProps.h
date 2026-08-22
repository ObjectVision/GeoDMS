// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_TREEITEMPROPS_H
#define __TIC_TREEITEMPROPS_H

#include "vt/Pair.h"
#include "mci/PropDef.h"

using PropBool = Bool;

TIC_CALL extern PropDef<TreeItem, SharedStr>* calcRulePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* descrPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* integrityCheckPropDefPtr;
extern PropDef<TreeItem, SharedStr>* sizeExpectationPropDefPtr;
extern PropDef<TreeItem, SharedStr>* sizeUpperboundPropDefPtr;
extern PropDef<TreeItem, SharedStr>* explicitSupplPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* storageNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* storageTypePropDefPtr;
extern PropDef<TreeItem, SharedStr>* storageDriverPropDefPtr;
extern PropDef<TreeItem, SharedStr>* storageOptionsPropDefPtr;
TIC_CALL extern PropDef<TreeItem, PropBool >* storageReadOnlyPropDefPtr;
extern PropDef<TreeItem, TokenID  >* syncModePropDefPtr;

extern PropDef<TreeItem, TokenID  >* dialogTypePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* dialogDataPropDefPtr;
extern PropDef<TreeItem, TokenID  >* paramTypePropDefPtr;
extern PropDef<TreeItem, SharedStr>* paramDataPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* labelPropDefPtr;
extern PropDef<TreeItem, SharedStr>* viewActionPropDefPtr;
TIC_CALL extern PropDef<TreeItem, TokenID  >* configStorePropDefPtr;
extern PropDef<TreeItem, SharedStr>* caseDirPropDefPtr;
extern PropDef<TreeItem, SharedStr>* sourceDescrPropDefPtr;

TIC_CALL extern PropDef<TreeItem, SharedStr>* sqlStringPropDefPtr;
extern PropDef<TreeItem, TokenID  >* tableTypeNamePropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* cdfPropDefPtr;
TIC_CALL extern PropDef<TreeItem, SharedStr>* urlPropDefPtr;

extern PropDef<TreeItem, UInt32>* storageTileSizeXPropDefPtr;
extern PropDef<TreeItem, UInt32>* storageTileSizeYPropDefPtr;

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
void TreeItem_DumpSourceCalculator(const TreeItem* studyObject, SourceDescrMode sdm, bool bShowHidden, OutStreamBase* xmlOutStrPtr);

#endif // __TIC_TREEITEMPROPS_H
