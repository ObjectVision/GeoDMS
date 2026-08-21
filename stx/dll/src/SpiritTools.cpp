// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// (StringProd merged in, 2026-08)


#include "SpiritTools.h"

#include "vt/BaseBounds.h"
#include "vt/MinMax.h"
#include "ptr/IterCast.h"
#include "utl/StrFormat.h"

///////////////////////////////////////////////////////////////////////////////
//
//  AuthErrorDisplayLock
//
///////////////////////////////////////////////////////////////////////////////

std::atomic<UInt32>  s_AuthErrorDisplayLockRecursionCount = 0;
UInt32  s_AuthErrorDisplayLockCatchCount = 0;

///////////////////////////////////////////////////////////////////////////////
//
//  textblock helper functions
//
///////////////////////////////////////////////////////////////////////////////

UInt32 eolpos(CharPtr first, CharPtr last)
{
	CharPtr mid = first;
	while (mid != last) 
	{
		switch(*mid++) {
			case 0:
			case '\n':
			case '\r':
				return mid-first-1;
		}
	}
	return mid - first;
}

UInt32 bolpos(CharPtr first, CharPtr last)
{
	CharPtr mid = last;
	while (mid != first) 
	{
		switch(*--mid) {
			case 0:
			case '\n':
			case '\r':
				return mid-first+1;
		}
	}
	return 0;
}

UInt32 untabbed_size(CharPtr first, CharPtr last, UInt32 tabSize, UInt32 pos)
{
	while (first != last)
	{
		if (*first++ == '\t')
			pos += (tabSize -(pos % tabSize));
		else ++pos;
	}
	return pos;
}

UInt32 untab(CharPtr first, CharPtr last, char* outBuffer, UInt32 tabSize, UInt32 pos)
{
	while (first != last)
	{
		if (*first == '\t')
		{
			UInt32 newPos = pos + (tabSize -(pos % tabSize));
			dms_assert(newPos > pos);
			do {
				*outBuffer++ = ' ';
			} while (++pos != newPos);
		}
		else
		{	
			*outBuffer++ = *first;
			++pos;
		}
		++first;
	}
	return pos;
}

SharedStr problemlocAsString(CharPtr bufferBegin, CharPtr bufferEnd, CharPtr problemLoc)
{
	if (!problemLoc)
		return SharedStr();

	dms_assert(bufferBegin <= problemLoc);
	dms_assert(problemLoc  <= bufferEnd );

	CharPtr lineBegin =  bufferBegin + bolpos(bufferBegin, problemLoc);
	CharPtr lineEnd   =  problemLoc  + eolpos(problemLoc,  Min<CharPtr>(problemLoc+80, bufferEnd));

	std::vector<char> untabbedLine( untabbed_size(lineBegin, lineEnd, 4), ' ');

	if (untabbedLine.empty())
		return SharedStr();

	SizeT untabPos = untab(lineBegin, problemLoc, &*untabbedLine.begin(), 4, 0);
	SizeT untabEnd = untab(problemLoc, lineEnd,   &*untabbedLine.begin()+untabPos, 4, untabPos);
	dms_assert(untabEnd == untabbedLine.size());
	
	auto utb = begin_ptr(untabbedLine);
	return SharedStr(CharPtrRange(utb, utb + untabEnd)) + "\n" 
		+ SharedStr(CharPtrRange(utb, utb + untabPos)) + "^";
}

UInt32 nrLineBreaks(CharPtr first, CharPtr last)
{
	UInt32 lineBreakCount = 0;
	while (first != last)
	{
		switch (*first++)
		{
			case '\n': 
				{
					++lineBreakCount;
					if (first != last && *first == '\r')
						++first;
					break;
				}
			case '\r':
				{
					++lineBreakCount;
					if (first != last && *first == '\n')
						++first;
				}
		}
	}
	return lineBreakCount;
}

///////////////////////////////////////////////////////////////////////////////
//
//  parse helper functions
//
///////////////////////////////////////////////////////////////////////////////

const boost::spirit::uint_parser<UInt64>  uint64_p;
const boost::spirit::uint_parser<UInt64, 16> hex64_p;


void CheckInfo(const parse_info_t& info)
{
	if (! info.full)
		boost::spirit::throw_<error_descr_t>(info.stop, SharedStr("unexpected token(s)") );
}


// ==== StringProd ====

#include "StringProd.h"

#include "ConfigFileName.h"
#include "dbg/Diagnostics.h"
#include "utl/StrFormat.h"
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

