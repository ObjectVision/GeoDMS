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