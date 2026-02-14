// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__STG_IMPL_H)
#define __STG_IMPL_H

#include "StgBase.h"
#include "FileResult.h"

// ------------------------------------------------------------------------
//
// Helper functions
//
// ------------------------------------------------------------------------

STGDLL_CALL const ValueClass* GetStreamType(const AbstrDataObject* adi);
STGDLL_CALL const ValueClass* GetStreamType(const AbstrDataItem* adi);

const AbstrUnit*  StorageHolder_GetTableDomain(const TreeItem* storageHolder);
bool              TableDomain_IsAttr(const AbstrUnit* domain, const AbstrDataItem* adi);

STGDLL_CALL SharedUnit FindProjectionRef (const TreeItem* storageHolder, const AbstrUnit* uDomain);
STGDLL_CALL SharedUnit FindProjectionBase(const TreeItem* storageHolder, const AbstrUnit* uDomain);

auto GetAffineTransformationFromGridDataItem(const AbstrDataItem* grid_adi, bool offset_to_top_left_cell=true)->Transformation<Float64>;
FileResult WriteGeoRefFile(const AbstrDataItem* diGrid, WeakStr geoRefFileName);
void GetImageToWorldTransformFromFile(TreeItem* storageHolder, WeakStr geoRefFileName);


#endif __STG_IMPL_H
