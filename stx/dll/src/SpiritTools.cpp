// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "SpiritTools.h"

#include "geo/BaseBounds.h"
#include "geo/MinMax.h"
#include "ptr/IterCast.h"
#include "utl/mySPrintF.h"

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

SYNTAX_CALL SharedStr problemlocAsString(CharPtr bufferBegin, CharPtr bufferEnd, CharPtr problemLoc)
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
	return SharedStr(utb, utb + untabEnd) + "\n" + SharedStr(utb, utb + untabPos) + "^";
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
