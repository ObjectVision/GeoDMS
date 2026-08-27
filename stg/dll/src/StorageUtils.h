// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Free-function helpers shared by the storage managers: stream-type
 *  determination for data items, table-domain lookup and attribute matching,
 *  projection reference/base resolution, and grid georeferencing (the affine
 *  transformation of a grid data item, reading/writing GeoRef files).
 */

#if !defined(__STG_STORAGEUTILS_H)
#define __STG_STORAGEUTILS_H

#include "StgBase.h"
#include "FileResult.h"

// ------------------------------------------------------------------------
//
// Helper functions
//
// ------------------------------------------------------------------------

const ValueClass* GetStreamType(const AbstrDataObject* adi);
const ValueClass* GetStreamType(const AbstrDataItem* adi);

const AbstrUnit*  StorageHolder_GetTableDomain(const TreeItem* storageHolder);
bool              TableDomain_IsAttr(const AbstrUnit* domain, const AbstrDataItem* adi);

SharedUnit FindProjectionRef (const TreeItem* storageHolder, const AbstrUnit* uDomain);
SharedUnit FindProjectionBase(const TreeItem* storageHolder, const AbstrUnit* uDomain);

auto GetAffineTransformationFromGridDataItem(const AbstrDataItem* grid_adi, bool offset_to_top_left_cell=true)->Transformation<Float64>;
FileResult WriteGeoRefFile(const AbstrDataItem* diGrid, WeakStr geoRefFileName);
void GetImageToWorldTransformFromFile(TreeItem* storageHolder, WeakStr geoRefFileName);


#endif // __STG_STORAGEUTILS_H
