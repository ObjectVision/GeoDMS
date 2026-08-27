// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STX_BASE_H)
#define __STX_BASE_H

#include "TicBase.h"

#if !defined(_MSC_VER)
#	define SYNTAX_CALL
#elif defined(DMSTX_EXPORTS)
#	define SYNTAX_CALL __declspec(dllexport)
#else
#	define SYNTAX_CALL __declspec(dllimport)
#endif

// functions called from within the stx module
// Returns an OWNING ref: the parsed (root) item is owned by the returned SharedPtr from creation until
// the caller takes ownership (e.g. SessionData::Open), since there is no longer an auto-delete pin to
// keep a parent-less config root alive across the parser's transient SharedPtrs.
SharedMutableTreeItem AppendTreeFromConfiguration(CharPtr p_pSourcefile, TreeItem* p_pRoot, bool rootIsFirstItem);

#endif // __STX_BASE_H