// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// String formatting implementation: myVSSPrintF and RepeatedDots (declared
// in utl/FixedBufferFormat.h) plus the path-splitting helpers of
// utl/splitPath.h. (Formerly mySPrintF.cpp.)

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

// The path-string helpers declared in utl/splitPath.h: splitPathBase,
// getFileName(+Base/Extension), splitFullPath, replaceFileExtension,
// IsAbsolutePath, DelimitedConcat, MakeAbsolutePath, MakeFileName and
// MakeDataFileName. Split out of StrFormat.cpp (formerly mySPrintF.cpp)
// 2026-08 to reunite them with their declaration header.

SharedStr splitPathBase(CharPtr full_path, CharPtr* new_path_ptr)
{
	// parsing the path recursively by calling this method on path parts
	dms_assert(full_path);
	dms_assert(new_path_ptr);

	CharPtr new_path = full_path;  // start search at the beginning of full_path

	// find delimiter
	while (*new_path && *new_path != DELIMITER_CHAR)
	{
		dms_assert(*new_path != '\\');
		++new_path;
	}

	*new_path_ptr = new_path;

	if (!*new_path) // delim not found in full_path?
		return SharedStr(full_path MG_DEBUG_ALLOCATOR_SRC("splitPathBase"));
	else
	{
		// delimiter found: sub item name is the left of the delimter and rest is 
		// new path (starting after delim) to be recursively parsed (it may contain more occurences of delim)
		++*new_path_ptr;
		return SharedStr(CharPtrRange(full_path, new_path));
	}
}

//////////////////////////////////////////////////////////////////////
// function to get the fileName or last folder name from a full path possibly including 
// delim as path separators
// Arguments:
//   (I) full_path : search in this file name that possibly has an extension
// ReturnValue: CharPtr to first fileName char in full_path, if any; else end of full_path

CharPtr getFileName(CharPtr fullPath)
{
	if (*fullPath)
	{
		CharPtr fullPathEnd = fullPath + StrLen(fullPath); // go to end of null-terminated fullPath
		assert(!*fullPathEnd);
		while (--fullPathEnd != fullPath)
		{
			assert(*fullPathEnd != '\\');
			if (*fullPathEnd == DELIMITER_CHAR)
				return fullPathEnd+1; // return extracted file-name and return path base without last delimiter
		}
		assert(fullPathEnd == fullPath);
	}
	return fullPath;
}

CharPtr getFileName(CharPtr fullPath, CharPtr fullPathEnd)
{
	assert(!*fullPathEnd);
	while (--fullPathEnd != fullPath)
	{
		assert(*fullPathEnd != '\\');
		if (*fullPathEnd == DELIMITER_CHAR)
			return fullPathEnd+1; // return extracted file-name and return path base without last delimiter
	}
	assert(fullPathEnd == fullPath);
	return fullPath;
}

SharedStr splitFullPath(CharPtr full_path)
{
	CharPtr new_path = full_path + StrLen(full_path); // go to end of null-terminated full_path
	assert(!*new_path);
	if (new_path != full_path)
		while (--new_path != full_path)
		{
			assert(*new_path != '\\');
			if (*new_path == DELIMITER_CHAR)
				return SharedStr(CharPtrRange(full_path, new_path)); // return extracted file-name and return path base without last delimiter
		}
	assert(new_path == full_path);
	return SharedStr();
}

//////////////////////////////////////////////////////////////////////
// inline function to get the fileNameExtension from a file name possibly including 
// delim as path separators
// Arguments:
//   (I) full_path : search in this file name that possibly has an extension
// ReturnValue: CharPtr to first extension char in full_path, if any; else end of full_path

CharPtr getFileNameExtension(CharPtr full_path)
{
	CharPtr full_path_end = full_path + StrLen(full_path); // go to end of null-terminated full_path

	CharPtr full_path_search = full_path_end;
	while (full_path_search != full_path &&	*full_path_search != '.' && *full_path_search != DELIMITER_CHAR)
	{
		dms_assert(*full_path_search != '\\');
		--full_path_search;
	}
	return *full_path_search == '.'
		? full_path_search+1
		: full_path_end;
}

SharedStr getFileNameBase(CharPtr full_path)
{
	CharPtr full_path_end = full_path + StrLen(full_path); // go to end of null-terminated full_path

	CharPtr full_path_search = full_path_end;
	while (full_path_search != full_path &&	*full_path_search != '.' && *full_path_search != DELIMITER_CHAR)
	{
		assert(*full_path_search != '\\');
		--full_path_search;
	}
	if (*full_path_search != '.')
		full_path_search = full_path_end;

	return SharedStr(CharPtrRange(full_path, full_path_search));
}

SharedStr replaceFileExtension(CharPtr full_path, CharPtrRange newExt)
{
	return getFileNameBase(full_path) + "." + newExt;
}

bool IsAbsolutePath(CharPtr full_path)
{
	if (*full_path == DELIMITER_CHAR)
		return true;
	while (*full_path && *full_path != DELIMITER_CHAR)
		if ((*full_path++) == ':')
			return true;
	return false;
}

bool IsAbsolutePath(CharPtrRange full_path)
{
	if (!full_path.empty())
	{
		if (*full_path.first == DELIMITER_CHAR)
			return true;
		while (full_path.first != full_path.second && *full_path.first != DELIMITER_CHAR)
			if ((*full_path.first++) == ':')
				return true;
	}
	return false;
}

SharedStr DelimitedConcat(CharPtr a, CharPtr b)
{
	dms_assert(a && b);
	dms_assert(!HasDosDelimiters(a));
	dms_assert(!HasDosDelimiters(b));

	if (!*a || IsAbsolutePath(b))
		return SharedStr(b MG_DEBUG_ALLOCATOR_SRC("DelimitedConcat"));

	SizeT aLen = StrLen(a), bLen = StrLen(b);
	if (!bLen)
		return SharedStr(CharPtrRange(a, a + aLen));

	dms_assert(*b != DELIMITER_CHAR);

	SharedCharArray* aPtr = SharedCharArray::CreateUninitialized(aLen + bLen + 2 MG_DEBUG_ALLOCATOR_SRC("DelimitedConcat"));
	SharedStr aStr(aPtr);

	char* ptr = fast_copy(a, a + aLen, aPtr->begin());
	*ptr++ = DELIMITER_CHAR;
	ptr = fast_copy(b, b + bLen, ptr);
	*ptr = char(0);
	dms_assert(ptr == aStr.csend());

	return aStr;
}

SharedStr DelimitedConcat(CharPtrRange a, CharPtrRange b)
{
	dms_assert(a.IsDefined() && b.IsDefined());
	dms_assert(!HasDosDelimiters(a));
	dms_assert(!HasDosDelimiters(b));

	if (a.empty() || IsAbsolutePath(b))
		return SharedStr(b);

	if (b.empty())
		return SharedStr(a);

	dms_assert(*b.first != DELIMITER_CHAR);

	SharedCharArray* aPtr = SharedCharArray::CreateUninitialized(a.size() + b.size() + 2 MG_DEBUG_ALLOCATOR_SRC("DelimitedConcat"));
	SharedStr aStr(aPtr);

	char* ptr = fast_copy(a.first, a.second, aPtr->begin());
	*ptr++ = DELIMITER_CHAR;
	ptr = fast_copy(b.first, b.second, ptr);
	*ptr = char(0);
	dms_assert(ptr == aStr.csend());

	return aStr;
}

RTC_CALL SharedStr MakeAbsolutePath(CharPtr relPath)
{
	if ( IsAbsolutePath(relPath))
		return SharedStr(relPath MG_DEBUG_ALLOCATOR_SRC("MakeAbsolutePath"));
	return DelimitedConcat(GetCurrentDir().c_str(), relPath);
}

RTC_CALL SharedStr MakeFileName(CharPtr path)
{
	while (*path == DELIMITER_CHAR) 
		++path; // skip leading delimiters

	if (!*path)
		return SharedStr();

	SharedCharArray* streamName = SharedCharArray_Create(path MG_DEBUG_ALLOCATOR_SRC("MakeFileName"));

	dms_assert(*(streamName->begin()) != ' ');

	return SharedStr( streamName );
}

RTC_CALL SharedStr MakeDataFileName(CharPtr path)
{
	return MakeFileName(path) + ".dmsdata";
}
