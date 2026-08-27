// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of the Stg DLL: StorageUtils.h (the storage prelude,
 *  which reaches the TreeItem graph via stg/AbstrStorageManager.h) plus
 *  the diagnostics and storage-manager helpers that all storage TUs use.
 */

#ifndef __STG_STORAGEPCH_H
#define __STG_STORAGEPCH_H

#if defined(STGDLL_CALL)
#error "STGDLL_CALL must be defined as import or export. Include this before StgBase or not at all."
#endif //STGDLL_CALL

#define DMSTGDLL_EXPORTS
#include "StorageUtils.h"

#include "dbg/Diagnostics.h"
#include "stg/AbstrStorageManager.h"
#include "stg/AsmUtil.h"

#endif // __STG_STORAGEPCH_H
