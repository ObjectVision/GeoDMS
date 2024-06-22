// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "IppBase.h"

#if !defined(GEO_NO_IPP)

#include "UseIpp.h"

#include "ippcore.h"

void DmsIppCheckResult(IppStatus status, CharPtr func, CharPtr file, int line)
{
	if (status == ippStsNoErr)
		return;

	const IppLibraryVersion* lib = ippGetLibVersion();

	throwErrorF("IPP Library returned error status", 
		"%s(%s,%d)\n"
		"Lib: %s IPP Version: %s \n" // CpuTypeID: %d\n"
		"function: %s\n",

		ippGetStatusString(status), file, line, 
		lib->Name, lib->Version, // ippGetCpuType(),
		func ? func : "<unknown>"
	);
}

#endif //!defined(GEO_NO_IPP)
