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

#ifndef __RTC_UTL_ENCODES_H
#define __RTC_UTL_ENCODES_H

#include "RtcBase.h"

RTC_CALL SharedStr AsFilename(WeakStr filenameStr);
RTC_CALL SharedStr UrlDecode(WeakStr urlStr);
RTC_CALL SharedStr to_utf   (CharPtr first, CharPtr last);
RTC_CALL SharedStr from_utf (CharPtr first, CharPtr last);
RTC_CALL SharedStr as_item_name(CharPtr first, CharPtr last);

inline bool itemNameFirstChar_test(unsigned char ch)
{
	return isalpha(ch) || ch == '_' || ch == '@' || ch >= 128;
}

inline bool itemNameNextChar_test(unsigned char ch)
{
	return isalnum(ch) || ch == '_' || ch == '@' || ch >= 128; // TODO: behavior under different code tables?
}

RTC_CALL bool itemName_test(CharPtr p);
RTC_CALL CharPtr ParseTreeItemName(CharPtr name);
RTC_CALL CharPtr ParseTreeItemPath(CharPtr name);
RTC_CALL void CheckTreeItemName(CharPtr name);
RTC_CALL void CheckTreeItemPath(CharPtr name);

#endif // __RTC_UTL_ENCODES_H