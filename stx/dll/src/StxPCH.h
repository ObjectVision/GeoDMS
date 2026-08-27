// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of the Stx DLL: the syntax-parser prelude
 *  (StxBase.h). TextPosition.h checks this header's guard to enforce that
 *  spirit-using code units come in through a PCH.
 */

#if !defined(__STX_STXPCH_H)
#define __STX_STXPCH_H

#if !defined(DMSTX_EXPORTS)
#pragma message("StxPCH.h included without DMSTX_EXPORTS defined; is this included from the right code-unit?")
#define DMSTX_EXPORTS
#endif

#include "StxBase.h"

#endif // __STX_STXPCH_H
