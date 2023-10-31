// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_UTL_QUOTES_H
#define __RTC_UTL_QUOTES_H

#include "RtcBase.h"

RTC_CALL SharedStr DoubleQuote(CharPtr str);
RTC_CALL SharedStr DoubleUnQuoteMiddle(CharPtr str);
RTC_CALL void      DoubleUnQuoteMiddle(SharedStr& result, CharPtr first, CharPtr last);
RTC_CALL SharedStr DoubleUnQuote(CharPtr str);

RTC_CALL SharedStr SingleQuote(CharPtr str);
RTC_CALL SharedStr SingleQuote(CharPtr begin, CharPtr end);

RTC_CALL SharedStr SingleUnQuoteMiddle(CharPtr str);
RTC_CALL void      SingleUnQuoteMiddle(SharedStr& result, CharPtr first, CharPtr last);
RTC_CALL SharedStr SingleUnQuote(CharPtr str);

RTC_CALL void DoubleQuote(struct FormattedOutStream& os,CharPtr str);
RTC_CALL void DoubleQuote(struct FormattedOutStream& os,CharPtr first, CharPtr last);
inline   void DoubleQuote(struct FormattedOutStream& os, WeakStr str) { DoubleQuote(os, str.cbegin(), str.csend()); }


RTC_CALL void SingleQuote(struct FormattedOutStream& os,CharPtr str);
RTC_CALL void SingleQuote(struct FormattedOutStream& os,CharPtr first, CharPtr last);
inline   void SingleQuote(struct FormattedOutStream& os, WeakStr str) { SingleQuote(os, str.cbegin(), str.csend()); }

RTC_CALL void DoubleQuote  (StringRef& ref, CharPtr b, CharPtr e);
RTC_CALL void DoubleUnQuote(StringRef& ref, CharPtr b, CharPtr e);

RTC_CALL void SingleQuote  (StringRef& ref, CharPtr b, CharPtr e);
RTC_CALL void SingleUnQuote(StringRef& ref, CharPtr b, CharPtr e);

RTC_CALL void DoubleQuote  (SharedStr& ref, CharPtr b, CharPtr e);
RTC_CALL void DoubleUnQuote(SharedStr& ref, CharPtr b, CharPtr e);

RTC_CALL void SingleQuote  (SharedStr& ref, CharPtr b, CharPtr e);
RTC_CALL void SingleUnQuote(SharedStr& ref, CharPtr b, CharPtr e);



#endif // __RTC_UTL_QUOTES_H