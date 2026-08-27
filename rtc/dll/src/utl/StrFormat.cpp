// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// String formatting implementation: myVSSPrintF and RepeatedDots, both declared
// in utl/FixedBufferFormat.h. The path-splitting helpers that used to sit here
// now live in utl/splitPath.cpp, beside their declaration header.

#include "utl/FixedBufferFormat.h"
#include "utl/StrFormat.h"

#include "dbg/DebugContext.h"
#include "vt/iterrange.h"
#include "vt/StringBounds.h"
#include "utl/splitPath.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "act/MainThread.h"

#include <stdio.h>
#include <stdarg.h>

//----------------------------------------------------------------------


#include "ptr/SharedStr.h"

// The formatting helpers of utl/StrFormat.h + utl/FixedBufferFormat.h that
// need out-of-line definitions: myVSSPrintF and RepeatedDots. The path
// helpers moved to splitPath.cpp (2026-08).

SharedStr myVSSPrintF(CharPtr format, va_list argList)
{
	const SizeT DEFAULT_BUFFER_SIZE = 300;

	std::unique_ptr<char[]> heapBuffer;

	char stackBuffer[DEFAULT_BUFFER_SIZE];
	char* buf  = stackBuffer;
	SizeT size = DEFAULT_BUFFER_SIZE;

	for (;;) 
	{
		SizeT nrCharsWritten = std::vsnprintf(buf, size, format, argList); // returns UInt32(-1) if size of buf is too small
		if (nrCharsWritten <= size)
			return SharedStr(CharPtrRange(buf, buf+nrCharsWritten));
		size *= 2;
		heapBuffer.reset(new char[size]);
		buf = heapBuffer.get();
	}
}


//////////////////////////////////////////////////////////////////////
// inline function to split name to new_path based on delim = DELIMITER_CHAR
// Arguments:
//   (I) full_path : search in this file name that possibly includes 
//   (O) new_path:   rest of full_path after first occurence of delim, or ptr to string terminator (0) of full_path if not found
//   (I) delim:      delimiter char
// ReturnValue: SharedStr
//////////////////////////////////////////////////////////////////////
static char sixteenDots[] = "................";

CharPtr RepeatedDots(SizeT n)
{
	if (n <= 16)
		return sixteenDots + (16 - n);

	dms_assert(IsMetaThread());
	static std::vector<char> moreDots;
	if (moreDots.size() <= n)
	{
		if (moreDots.size())
			moreDots.back() = '.';
		moreDots.resize(n + 1, '.');
		moreDots.back() = char(0);
	}
	dms_assert(moreDots.size() > n);
	return &*(moreDots.end() - (n + 1));
}

