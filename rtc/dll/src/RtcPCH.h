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

#if !defined(__RTC_PCH)
#define __RTC_PCH

#if !defined(DMRTC_EXPORTS) && !defined(DMRTC_STATIC)
#pragma message("RtcPCH.h included without DMRTC_EXPORTS or DMRTC_STATIC defined; is this included from the right code-unit?")
#define DMRTC_EXPORTS
#endif

#include "RtcBase.h"
#include "dbg/Check.h"
#include "dbg/debug.h"

//----------------------------------------------------------------------
// RtcLock
//----------------------------------------------------------------------

struct ElemAllocComponent
{
	ElemAllocComponent();
	~ElemAllocComponent();
};

struct RtcStreamLock : ElemAllocComponent
{
	RtcStreamLock();
	~RtcStreamLock();
};

struct IndexedStringsComponent: ElemAllocComponent
{
	IndexedStringsComponent();
   ~IndexedStringsComponent();
};

struct TokenComponent : IndexedStringsComponent
{
	TokenComponent();
   ~TokenComponent();
};

struct RtcReportLock : RtcStreamLock, TokenComponent
{
	RtcReportLock();
   ~RtcReportLock();
};

//----------------------------------------------------------------------
// Section      : IString, used for returning string-handles to ClientAppl
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

struct IStringComponentLock : RtcStreamLock
{
	RTC_CALL IStringComponentLock();
	RTC_CALL ~IStringComponentLock();
};


#endif

//----------------------------------------------------------------------


#endif // __RTC_PCH
