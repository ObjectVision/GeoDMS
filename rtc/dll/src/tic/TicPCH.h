// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of the tic sub-tree of the Rtc DLL: the tic
 *  prelude (TicBase.h, TreeItem.h) plus the high-fan-in, low-churn
 *  headers that nearly all tic TUs include anyway (see
 *  doc/development/header-hygiene-2026-08.md §4b).
 */

#ifndef __TIC_PCH
#define __TIC_PCH

#if !defined(DMTIC_EXPORTS)
#pragma message("TicPCH.h included without DMTIC_EXPORTS defined; is this included from the right code-unit?")
#define DMTIC_EXPORTS
#endif

#include "TicBase.h"
#include "TreeItem.h"

#include "dbg/DmsCatch.h"
#include "xct/DmsException.h"
#include "LispTreeType.h"
#include "Unit.h"
#include "UnitClass.h"

#endif // __TIC_PCH