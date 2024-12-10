// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STX_BASE_H)
#define __STX_BASE_H

#include "TicBase.h"

#if defined(DMSTX_EXPORTS)
#	define SYNTAX_CALL __declspec(dllexport)
#else
#	define SYNTAX_CALL __declspec(dllimport)
#endif

// functions called from within the stx module
TreeItem* AppendTreeFromConfiguration(CharPtr p_pSourcefile, TreeItem* p_pRoot, bool rootIsFirstItem);

#endif // __STX_BASE_H