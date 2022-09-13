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

#include "TicPCH.h"
#pragma hdrstop

#include "LispContextHandle.h"

#include "dbg/SeverityType.h"
#include "ser/AsString.h"
#include "utl/mySPrintF.h"

#if defined(MG_DEBUG)
#define MG_DEBUG_LISPCONTEXT
#endif

/********** LispContextHandle **********/

#if defined(MG_DEBUG_LISPCONTEXT)
THREAD_LOCAL UInt32 s_NrLispContexts = 0;
#endif

LispContextHandle::LispContextHandle(CharPtr expr, LispPtr ref)
	:	m_Expr(expr)
	,	m_Ref(ref)
{
#if defined(MG_DEBUG_LISPCONTEXT)
	++s_NrLispContexts;
	GetDescription();
//	reportF(ST_MinorTrace, "LCH::CTOR %d(%s)", s_NrLispContexts, GetDescription());
#endif
}

LispContextHandle::~LispContextHandle()
{
#if defined(MG_DEBUG_LISPCONTEXT)
//	reportF(ST_MinorTrace, "LCH::DTOR %d(%s)", s_NrLispContexts, GetDescription());
	s_NrLispContexts--;
#endif
}


void LispContextHandle::GenerateDescription()
{
	SetText(
		mySSPrintF("Expression=%s;\nLocal Lisp Tree=%s.", 
			m_Expr, 
			AsString(m_Ref).c_str()
		)
	);
}


