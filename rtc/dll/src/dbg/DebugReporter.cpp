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
#include "RtcPCH.h"
#pragma hdrstop

#include "mem/FixedAlloc.h"

#if defined(MG_DEBUGREPORTER)

#include "DbgInterface.h"

#include <stdarg.h>

#include "dbg/DebugReporter.h"
#include "dbg/SeverityType.h"
#include "ptr/StaticPtr.h"
#include "ptr/OwningPtr.h"
#include "utl/MySPrintF.h"

#include "ser/FormattedStream.h"

/********** DebugReporter interface  **********/

namespace { // local defs
	linked_list* s_ReporterList = nullptr;
} // end anonymous namespace

DebugReporter::DebugReporter() : linked_list( &s_ReporterList )
{
}

DebugReporter::~DebugReporter()
{
	clear();
}

void DebugReporter::clear()
{
	// clear can be called multiple times, thus this may already have been removed
	if (GetNext())
		remove_from_list( &s_ReporterList );
}

void DebugReporter::ReportAll()
{
	const DebugReporter* i = static_cast<DebugReporter*>(s_ReporterList);
	while (i)
	{
		i->Report();
		i = static_cast<const DebugReporter*>(i->GetNext());
	}
}
#endif

void DMS_CONV DBG_DebugReport()
{
#if defined(MG_DEBUGREPORTER)
	DebugReporter::ReportAll();
#endif
}


#if defined(MG_DEBUG)

void ReportCount() // TODO RECOMPILE G8: include in DbgInterface.h
{
	static int debugCounter = 0;
	static std::mutex debugOutputSection;
	{
		std::lock_guard serializedDebugOutput(debugOutputSection);
		debugCounter++;
		reportF(SeverityTypeID::ST_MajorTrace, "GetKeyExprImpl debugCounter=%d", debugCounter);
	}
}
#endif






/* UNIT TESTS */
#if defined(MG_DEBUGREPORTER)

auto test = MakeDebugCaller(
	[]() { dms_assert(1 + 1 == 2);  }
);

#endif defined(MG_DEBUGREPORTER)