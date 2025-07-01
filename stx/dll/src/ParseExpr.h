// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// *****************************************************************************
// ParseSpecs Implementation of public interface
// *****************************************************************************

#if !defined(__STX_PARSEEXPR_H)
#define __STX_PARSEEXPR_H

#include "StxInterface.h"
#include "Lispref.h"

// *****************************************************************************
//							New Funcs
// *****************************************************************************

SYNTAX_CALL LispRef ParseExpr(WeakStr exprStr);
SYNTAX_CALL void annotateExpr(OutStreamBase& outStream, const TreeItem* searchContext, SharedStr expr);

#endif // !defined(__STX_PARSEEXPR_H)
