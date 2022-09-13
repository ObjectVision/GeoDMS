//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#ifndef __RTC_UTL_SPLITPATH_H
#define __RTC_UTL_SPLITPATH_H

#include "ptr/SharedStr.h"

RTC_CALL CharPtr getFileName(CharPtr full_path);
RTC_CALL CharPtr getFileName(CharPtr fullPath, CharPtr fullPathEnd);
RTC_CALL SharedStr splitPathBase(CharPtr full_path, CharPtr* new_path); // inline function to split name to new_path
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

RTC_CALL bool   IsAbsolutePath (CharPtr full_path);
RTC_CALL bool   IsAbsolutePath(CharPtrRange full_path);
RTC_CALL SharedStr DelimitedConcat(CharPtr a, CharPtr b);
RTC_CALL SharedStr DelimitedConcat(CharPtrRange a, CharPtrRange b);
RTC_CALL SharedStr MakeAbsolutePath(CharPtr rel_path);
RTC_CALL SharedStr MakeFileName    (CharPtr path);
RTC_CALL SharedStr MakeDataFileName(CharPtr path);

#endif // __RTC_UTL_SPLITPATH_H

