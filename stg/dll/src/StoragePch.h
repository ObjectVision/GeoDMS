// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __STORAGE_PCH
#define __STORAGE_PCH

#if defined(STGDLL_CALL)
#error "STGDLL_CALL must be defined as import or export. Include this before StgBase or not at all."
#endif //STGDLL_CALL

#define DMSTGDLL_EXPORTS
#include "StgImpl.h"

#include "dbg/Check.h"
#include "stg/AbstrStorageManager.h"
#include "stg/AsmUtil.h"

#endif // __STORAGE_PCH
