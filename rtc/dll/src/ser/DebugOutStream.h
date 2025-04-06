// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_SER_DEBUGOUTSTREAM_H)
#define __RTC_SER_DEBUGOUTSTREAM_H

#include "ser/FormattedStream.h"
#include "dbg/SeverityType.h"

struct DebugOutStream : FormattedOutStream, leveled_critical_section
{
	DebugOutStream();

	RTC_CALL void NewLine();
	RTC_CALL void PrintSpaces();
	RTC_CALL static void SetSeverity(DebugOutStream* self, SeverityTypeID st);
private:
	void SetSeverity(SeverityTypeID st);
	void SetMsgCategory(MsgCategory msgCat);

public:
	struct flush_after : RequestMainThreadOperProcessingBlocker
	{
		~flush_after();
	};
	struct scoped_lock : flush_after, leveled_critical_section::scoped_lock
	{
		RTC_CALL scoped_lock(DebugOutStream* str, SeverityTypeID st = SeverityTypeID::ST_MinorTrace, MsgCategory = MsgCategory::progress);
		RTC_CALL ~scoped_lock();
	private:
		WeakPtr<DebugOutStream> m_Str;
	};
};

RTC_CALL extern static_ptr<DebugOutStream> g_DebugStream;


#endif // __RTC_SER_DEBUGOUTSTREAM_H
