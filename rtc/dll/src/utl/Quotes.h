// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_UTL_QUOTES_H
#define __RTC_UTL_QUOTES_H

#include "RtcBase.h"

RTC_CALL SharedStr DoubleQuote(CharPtr str);

RTC_CALL SharedStr DoubleUnQuoteMiddle(CharPtr str);
RTC_CALL void      DoubleUnQuoteMiddle(SharedStr& result, CharPtr first, CharPtr last);

RTC_CALL SharedStr SingleQuote(CharPtr str);
RTC_CALL SharedStr SingleQuote(CharPtr begin, CharPtr end);

RTC_CALL void      SingleUnQuoteMiddle(SharedStr& result, CharPtr first, CharPtr last);

RTC_CALL void DoubleQuote(struct FormattedOutStream& os,CharPtr str);
RTC_CALL void DoubleQuote(struct FormattedOutStream& os,CharPtr first, CharPtr last);
inline   void DoubleQuote(struct FormattedOutStream& os, WeakStr str) { DoubleQuote(os, str.cbegin(), str.csend()); }
void DoubleQuoteMiddle(OutStreamBuff& buf, CharPtr begin, CharPtr end);


RTC_CALL void SingleQuote(struct FormattedOutStream& os,CharPtr str);
RTC_CALL void SingleQuote(struct FormattedOutStream& os,CharPtr first, CharPtr last);
inline   void SingleQuote(struct FormattedOutStream& os, WeakStr str) { SingleQuote(os, str.cbegin(), str.csend()); }

RTC_CALL void DoubleQuote  (StringRef& ref, CharPtr b, CharPtr e);
RTC_CALL void DoubleUnQuote(StringRef& ref, CharPtr b, CharPtr e);

RTC_CALL void SingleQuote  (StringRef& ref, CharPtr b, CharPtr e);
RTC_CALL void SingleUnQuote(StringRef& ref, CharPtr b, CharPtr e);

RTC_CALL void SingleQuote  (SharedStr& ref, CharPtr b, CharPtr e);

// Points at the character following a backslash that is not a known escape code, or nullptr when
// the given quoted-string middle has none. Known are \0, \t, \r, \n, \xHH and the self-escaping
// \\, \" and \'; anything else has its backslash silently swallowed by the UnQuote functions
// above, which is why parsers warn about it (issue #292).
RTC_CALL CharPtr FindUnknownEscapeCode(CharPtr begin, CharPtr end);


#endif // __RTC_UTL_QUOTES_H