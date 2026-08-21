// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __RTC_UTL_SPLITPATH_H
#define __RTC_UTL_SPLITPATH_H

#include "ptr/SharedStr.h"

RTC_CALL CharPtr getFileName(CharPtr full_path);
RTC_CALL CharPtr getFileName(CharPtr fullPath, CharPtr fullPathEnd);
SharedStr splitPathBase(CharPtr full_path, CharPtr* new_path); // inline function to split name to new_path
RTC_CALL SharedStr splitFullPath(CharPtr full_path);

//////////////////////////////////////////////////////////////////////
// inline function to get the fileNameExtension from a file name possibly including 
// delim as path separators
// Arguments:
//   (I) full_path : search in this file name that possibly has an extension
//   (I) delim:      delimiter char
// ReturnValue: CharPtr to first extension char in full_path, if any; else end of full_path

RTC_CALL CharPtr getFileNameExtension(CharPtr full_path);
RTC_CALL SharedStr getFileNameBase(CharPtr full_path);
RTC_CALL SharedStr replaceFileExtension(CharPtr full_path, CharPtrRange newExt);

bool   IsAbsolutePath (CharPtr full_path);
bool   IsAbsolutePath(CharPtrRange full_path);
RTC_CALL SharedStr DelimitedConcat(CharPtr a, CharPtr b);
RTC_CALL SharedStr DelimitedConcat(CharPtrRange a, CharPtrRange b);
RTC_CALL SharedStr MakeAbsolutePath(CharPtr rel_path);
RTC_CALL SharedStr MakeFileName    (CharPtr path);
RTC_CALL SharedStr MakeDataFileName(CharPtr path);

#endif // __RTC_UTL_SPLITPATH_H

