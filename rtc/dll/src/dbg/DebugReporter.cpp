// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mem/FixedAlloc.h"

#if defined(MG_DEBUGREPORTER)

#include "DbgInterface.h"

#include <stdarg.h>

#include "dbg/DebugReporter.h"
#include "dbg/SeverityType.h"
#include "ptr/StaticPtr.h"
#include "ptr/OwningPtr.h"
#include "utl/mySPrintF.h"

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