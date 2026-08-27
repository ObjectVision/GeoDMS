// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of the Geo DLL: the geo prelude (GeoBase.h) and
 *  the tic data/operator interface, plus the high-fan-in, low-churn
 *  headers that most operator TUs include anyway (see
 *  doc/development/header-hygiene-2026-08.md §4b).
 */

#if !defined(__GEO_PCH_H)
#define __GEO_PCH_H

#define GEO_EXPORTS

#include "GeoBase.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "DataItemClass.h"

#include "Operator.h"

#include "dbg/DmsCatch.h"
#include "xct/DmsException.h"
#include "LispTreeType.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"

#endif // __GEO_PCH_H