// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of the shv DLL: ShvBase.h (the shv prelude incl.
 *  windows.h) plus dataview.h, whose 199-header closure is included by 53
 *  of the 72 shv TUs anyway — precompiling it once saves the biggest
 *  per-TU parse in the solution (see
 *  doc/development/header-hygiene-2026-08.md §4b).
 */

#if !defined(__SHV_SHVDLLPCH_H)
#define __SHV_SHVDLLPCH_H

#define DM_SHV_EXPORTS
#include "ShvBase.h"

#include "DataView.h"

#endif // __SHV_SHVDLLPCH_H
