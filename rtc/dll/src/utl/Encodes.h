// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __RTC_UTL_ENCODES_H
#define __RTC_UTL_ENCODES_H

#include "RtcBase.h"

RTC_CALL SharedStr AsFilename(WeakStr filenameStr);
RTC_CALL SharedStr UrlDecode(WeakStr urlStr);
RTC_CALL SharedStr UrlEncode(WeakStr urlStr);   // exact inverse of UrlDecode
RTC_CALL SharedStr HtmlEncode(WeakStr htmlStr); // escapes < > & ' " as the XML predefined entities
RTC_CALL SharedStr HtmlDecode(WeakStr htmlStr); // inverse, plus &nbsp; and numeric character references
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

bool itemName_test(CharPtr p);
CharPtr ParseTreeItemName(CharPtr name);
CharPtr ParseTreeItemPath(CharPtr name);
void CheckTreeItemName(CharPtr name);
RTC_CALL void CheckTreeItemPath(CharPtr name);

#endif // __RTC_UTL_ENCODES_H