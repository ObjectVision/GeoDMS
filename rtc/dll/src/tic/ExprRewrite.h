// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#pragma once

// *****************************************************************************
// ParseSpecs Implementation of public interface
// *****************************************************************************

#if !defined(__TIC_EXPR_REWRITE_H)
#define __TIC_EXPR_REWRITE_H

#include "TicInterface.h"
#include "LispRef.h"

LispRef RewriteExpr(LispPtr org);
TIC_CALL LispRef RewriteExprTop(LispPtr org);

// true iff RewriteExpr.lsp contains a rule whose pattern head is headID AND whose
// argument pattern captures GENERIC calls (every argument a plain variable, incl. a
// variable tail); such names are reserved: rewriting runs before head dispatch and
// would capture a user-defined function. Structural-argument rules (destructurers,
// idempotence collapses) do NOT reserve the name: they fire only on specific
// spellings and compose with a same-named function.
TIC_CALL bool HasRewriteRuleForHead(TokenID headID);

inline LispPtr RewriteExprTop_InParse(LispPtr org) { return org; }

#endif // !defined(__TIC_EXPR_REWRITE_H)
