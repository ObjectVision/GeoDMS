// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
/****************** Lisp interpreter              *******************/

#ifndef __MG_SYMBOL_LISPEVAL_H
#define __MG_SYMBOL_LISPEVAL_H

#include "Assoc.h"

/****************** Function headers              *******************/

void SetEnv(AssocListPtr env);
LispRef ApplyTopEnv(LispPtr expr);


#endif // __MG_SYMBOL_LISPEVAL_H
