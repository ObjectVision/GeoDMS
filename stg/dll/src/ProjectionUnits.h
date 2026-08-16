// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Projection-unit helpers for consumers outside the storage module: resolve
 *  the base projection unit behind a data item's values unit, and the size of
 *  one such unit in meters (degree-aware). Declared here so that shv (ScaleBar,
 *  ViewPort, PaletteControl, ResourceIndexCache, FeatureLayer) does not need to
 *  include the full GDAL surface header gdal/gdal_base.h for these two
 *  functions; the implementations live in gdal/gdal_base.cpp (they use
 *  OGRSpatialReference internally).
 */

#if !defined(__STG_PROJECTIONUNITS_H)
#define __STG_PROJECTIONUNITS_H

#include "StgBase.h"

STGDLL_CALL auto GetBaseProjectionUnitFromValuesUnit(const AbstrDataItem* adi) -> const AbstrUnit*;
STGDLL_CALL auto GetUnitSizeInMeters(const AbstrUnit* projectionBaseUnit) -> Float64;

#endif // __STG_PROJECTIONUNITS_H
