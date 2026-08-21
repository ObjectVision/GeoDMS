// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
/****************** Lisp interpreter              *******************/

#ifndef __MG_SYMBOL_LISPEVAL_H
#define __MG_SYMBOL_LISPEVAL_H

#include "Assoc.h"

/****************** Function headers              *******************/

LispRef MakeVarsOfUnderscores(LispPtr expr);

#if defined(MG_USE_LISPFUNCS)
SYM_CALL LispRef         Eval(LispPtr expr, AssocListPtr env);
SYM_CALL LispRef RepeatedEval(LispPtr expr, AssocListPtr env);
#endif

//SYM_CALL LispRef        Apply (LispPtr expr, AssocListPtr env);
//SYM_CALL LispRef RepeatedApply(LispPtr expr, AssocListPtr env);

//==============================

void SetEnv(AssocListPtr env);
LispRef ApplyTopEnv(LispPtr expr);


#endif // __MG_SYMBOL_LISPEVAL_H
