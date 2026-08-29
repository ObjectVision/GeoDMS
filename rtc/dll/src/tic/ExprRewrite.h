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

// the whole-tree rewrite: RewriteExprTop applied bottom-up. Every substitution of a PARSED
// expression must go through this first -- a raw parse still spells the head of every
// rewrite rule, and such a head need not be a registered operator ('value', which rule 1
// turns into 'convert', is not one), so substituting one unrewritten fails (#1224).
// Exported: Clc's collecting meta-operations inspect candidate expressions with it
// (Subset.cpp).
TIC_CALL LispRef RewriteExpr(LispPtr org);
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
