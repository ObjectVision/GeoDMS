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

// std::vsnprintf on a va_copy of argList, so that argList stays usable for another pass: a va_list that
// vsnprintf has consumed is indeterminate (with the x86-64 SysV ABI, va_list is an array type, so the
// callee advances the caller's cursor).
static int vsnprintfOnCopy(char* buf, SizeT size, CharPtr format, va_list argList)
{
	va_list argCopy;
	va_copy(argCopy, argList);
	int resultLength = std::vsnprintf(buf, size, format, argCopy);
	va_end(argCopy);
	return resultLength;
}

SharedStr myVSSPrintF(CharPtr format, va_list argList)
{
	const SizeT DEFAULT_BUFFER_SIZE = 300;

	// vsnprintf returns the length of the complete result, but writes at most size-1 characters and a terminating zero;
	// a negative result means an encoding error
	char stackBuffer[DEFAULT_BUFFER_SIZE];
	int resultLength = vsnprintfOnCopy(stackBuffer, DEFAULT_BUFFER_SIZE, format, argList);
	MG_CHECK2(resultLength >= 0, "myVSSPrintF: vsnprintf failed on an encoding error");
	if (SizeT(resultLength) < DEFAULT_BUFFER_SIZE)
		return SharedStr(CharPtrRange(stackBuffer, stackBuffer + resultLength));

	SizeT heapBufferSize = SizeT(resultLength) + 1;
	std::unique_ptr<char[]> heapBuffer(new char[heapBufferSize]);
	int heapResultLength = vsnprintfOnCopy(heapBuffer.get(), heapBufferSize, format, argList);
	MG_CHECK(heapResultLength == resultLength);
	return SharedStr(CharPtrRange(heapBuffer.get(), heapBuffer.get() + resultLength));
}


//////////////////////////////////////////////////////////////////////
// inline function to split name to new_path based on delim = DELIMITER_CHAR
// Arguments:
//   (I) full_path : search in this file name that possibly includes 
//   (O) new_path:   rest of full_path after first occurrence of delim, or ptr to string terminator (0) of full_path if not found
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

