// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "StringProd.h"

#include "dbg/Check.h"
#include "utl/Quotes.h"
#include "Parallel.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Product Holder for multi purpose string grammar
//
///////////////////////////////////////////////////////////////////////////////

void StringProd::ProdStringLiteral1(CharPtr first, CharPtr last) 
{ 
	dms_assert(last);
	if (*last != '\'')
		throwErrorD("ParseString", "single quoted string terminator expected");
	SingleUnQuoteMiddle(m_StringLiteral, first, last); 
}

void StringProd::ProdStringLiteral2(CharPtr first, CharPtr last) 
{ 
	dms_assert(last);
	if (*last != '\"')
		throwErrorD("ParseString", "double quoted string terminator expected");
	DoubleUnQuoteMiddle(m_StringLiteral, first, last); 
}

