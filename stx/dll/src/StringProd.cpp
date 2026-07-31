// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "StringProd.h"

#include "ConfigFileName.h"
#include "dbg/Check.h"
#include "utl/mySPrintF.h"
#include "utl/Quotes.h"
#include "Parallel.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Product Holder for multi purpose string grammar
//
///////////////////////////////////////////////////////////////////////////////

// A backslash escapes the next character; an unknown escape code such as the \U of
// "C:\Users\me\file.pdf" is silently dropped, which surprises config authors who meant a file path
// (issue #292). Warn about it, naming the offending code and where it was read.
static void WarnOnUnknownEscapeCode(CharPtr first, CharPtr last, char quoteChar, const text_position* pos)
{
	CharPtr unknownEscapeCode = FindUnknownEscapeCode(first, last);
	if (!unknownEscapeCode)
		return;

	// during config load the position refers to the .dms file; a calculation rule is parsed later,
	// with its own text as the parse buffer, so then only the offending literal identifies the spot.
	auto fileDescr = ConfigurationFilenameLock::GetCurrentFileDescrFromConfigLoadDir();
	auto location = (fileDescr && pos)
		?	mySSPrintF("{}({}, {}): ", fileDescr->GetFileName().c_str(), pos->line, pos->column)
		:	SharedStr();

	reportF(MsgCategory::other, SeverityTypeID::ST_Warning
		,	"{}unknown escape code '\\{}' in the string {}{}{}.\n"
			"A backslash escapes the next character and is then dropped, so a file path needs '\\\\' or '/' as separator."
		,	location.c_str()
		,	*unknownEscapeCode
		,	quoteChar, CharPtrRange(first, last), quoteChar
	);
}

void StringProd::ProdStringLiteral1(CharPtr first, CharPtr last, const text_position* pos)
{
	dms_assert(last);
	if (*last != '\'')
		throwErrorD("ParseString", "single quoted string terminator expected");
	WarnOnUnknownEscapeCode(first, last, '\'', pos);
	SingleUnQuoteMiddle(m_StringLiteral, first, last);
}

void StringProd::ProdStringLiteral2(CharPtr first, CharPtr last, const text_position* pos)
{
	dms_assert(last);
	if (*last != '\"')
		throwErrorD("ParseString", "double quoted string terminator expected");
	WarnOnUnknownEscapeCode(first, last, '\"', pos);
	DoubleUnQuoteMiddle(m_StringLiteral, first, last);
}

