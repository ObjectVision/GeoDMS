// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_PCH
#define __TIC_PCH

#if !defined(DMTIC_EXPORTS)
#pragma message("TicPCH.h included without DMTIC_EXPORTS defined; is this included from the right code-unit?")
#define DMTIC_EXPORTS
#endif

#include "TicBase.h"
#include "TreeItem.h"

#endif // __TIC_PCH