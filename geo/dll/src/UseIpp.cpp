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

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
