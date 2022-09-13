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
