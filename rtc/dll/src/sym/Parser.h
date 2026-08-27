// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
/****************** Parser                        *******************/
#ifndef __MG_SYMBOL_PARSER_H
#define __MG_SYMBOL_PARSER_H

#include "LispRef.h"
#include "ser/FormattedStream.h"

LispRef GetExpr(FormattedInpStream& istr);
SYM_CALL FormattedInpStream& operator >>(FormattedInpStream& is, LispRef& expr);

#endif // __MG_SYMBOL_PARSER_H
