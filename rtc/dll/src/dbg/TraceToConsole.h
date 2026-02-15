// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __DBG_TRACETOCONSOLE_H
#define __DBG_TRACETOCONSOLE_H

#include "dbg/debug.h"
#include <iostream>

namespace {

std::ostream*  localDebugStream = &std::cerr;

CharPtr GetName(SeverityTypeID st)
{
	switch(st)
	{
		case ST_MinorTrace: return "\n";
		case ST_MajorTrace: return "\nTRACE:";
		case ST_Warning:    return "\nWARNING:";
		case ST_Error:      return "\nERROR:";
		case ST_FatalError: return "\nFATAL ERROR:";
	}
	return "Unknown SeverityType";
}

void DMS_CONV writeDebugStream(SeverityTypeID st, CharPtr msg, UInt32 hld)
{
	if (localDebugStream)
		(*localDebugStream) << GetName(st) << msg;
}

struct initTrace {
	initTrace() 
	{ 
		DMS_RegisterMsgCallback(writeDebugStream, 0);
		MG_TRACE(("begin trace.log"));
	}
	~initTrace()
	{
		MG_TRACE(("end trace.log"));
		DMS_ReleaseMsgCallback(writeDebugStream, 0);
	}
};

initTrace doInitTrace;
}

#endif // __DBG_TRACETOCONSOLE_H
