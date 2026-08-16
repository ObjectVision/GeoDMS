// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Range utilities over CharPtrRange (Search and kin).
 */

#if !defined(__VT_ITERRANGEFUNCS_H)
#define __VT_ITERRANGEFUNCS_H

#include "vt/iterrange.h"

//----------------------------------------------------------------------
// IterRangeFuncs
//----------------------------------------------------------------------

RTC_CALL CharPtr Search(CharPtrRange str, CharPtrRange pattern);

#endif // !defined(__VT_ITERRANGEFUNCS_H)
