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

#if !defined(__RTC_SER_DEBUGOUTSTREAM_H)
#define __RTC_SER_DEBUGOUTSTREAM_H

#include "ser/FormattedStream.h"
#include "dbg/SeverityType.h"
#include <concrt.h>

struct DebugOutStream : FormattedOutStream, leveled_critical_section
{
	DebugOutStream();

	RTC_CALL void NewLine();
	RTC_CALL void PrintSpaces();

private:
	void SetSeverity(SeverityTypeID st);
	void SetMsgCategory(MsgCategory msgCat);

public:
	struct flush_after {
		~flush_after();
	};
	struct scoped_lock : flush_after, leveled_critical_section::scoped_lock
	{
		RTC_CALL scoped_lock(DebugOutStream* str, SeverityTypeID st = SeverityTypeID::ST_MinorTrace, MsgCategory = MsgCategory::nonspecific);
		RTC_CALL ~scoped_lock();
	private:
		WeakPtr<DebugOutStream> m_Str;
	};
};

RTC_CALL extern static_ptr<DebugOutStream> g_DebugStream;


#endif // __RTC_SER_DEBUGOUTSTREAM_H
