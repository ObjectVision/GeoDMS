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

#include "DbgInterface.h"

#include "debug.h"

#include <stdarg.h>

#include "dbg/DmsCatch.h"
#include "ser/DebugOutStream.h"
#include "utl/Environment.h"
#include "utl/MySPrintF.h"
#include "utl/swapper.h"
#include "Parallel.h"

#include <windows.h>

//----------------------------------------------------------------------
// class  : TreeItemContextNotification
//----------------------------------------------------------------------

namespace { // local defs

	TContextNotification    s_clientFunc          = nullptr;
	ClientHandle            s_cientHandle         = 0;
} // end anonymous namespace

extern "C" RTC_CALL void DMS_CONV DMS_SetContextNotification(TContextNotification clientFunc, ClientHandle clientHandle)
{
	dms_assert(IsMainThread());
	s_clientFunc  = clientFunc;
	s_cientHandle = clientHandle;
}

void ProgressMsg(CharPtr msg)
{
	dms_assert(IsMainThread());
	if (s_clientFunc)
		(*s_clientFunc)(s_cientHandle, msg);
}

/********** MsgCategory **********/

CharPtr AsString(MsgCategory msgCat)
{
	switch (msgCat) {
	case MsgCategory::system: return "[system]";
	case MsgCategory::wms: return "[wms]";
	case MsgCategory::disposable: return "[disposable]";
	case MsgCategory::progress: return "[progress]";
	}
	return "";
}
/********** AbstrContextHandle **********/

THREAD_LOCAL static AbstrContextHandle* s_LastAbstrContextHandle = nullptr;

#if defined(MG_DEBUG)
THREAD_LOCAL static AbstrContextHandle* sd_PrevAbstrContextHandle = nullptr;
THREAD_LOCAL static UInt32 sd_ContextHandleCount = 0;
#endif

AbstrContextHandle::AbstrContextHandle() noexcept
	:	m_Prev(s_LastAbstrContextHandle)
{ 
	s_LastAbstrContextHandle = this; 
#if defined(MG_DEBUG)
	++sd_ContextHandleCount;
	sd_PrevAbstrContextHandle = m_Prev;
#endif
	dms_assert(m_Prev != this);
}

AbstrContextHandle::~AbstrContextHandle() noexcept
{ 
	dms_assert(s_LastAbstrContextHandle == this);
	dms_assert(m_Prev != this);
	s_LastAbstrContextHandle = m_Prev;
#if defined(MG_DEBUG)
	--sd_ContextHandleCount;
	dms_assert(m_Prev == sd_PrevAbstrContextHandle); // no stack frame corruption due to buffer overflow ?
	if (m_Prev)
		sd_PrevAbstrContextHandle = m_Prev->m_Prev;
#endif
	dms_assert(s_LastAbstrContextHandle != this);
}

bool AbstrContextHandle::Describe(FormattedOutStream& fos) // default: calls GetDescription
{
	CharPtr extraInfo = GetDescription();
	if (!extraInfo || !*extraInfo)
		return false;

	fos << extraInfo;
	return true;
}

CharPtr AbstrContextHandle::GetDescription()
{
	return "ABSTRACT AbstrContextHandle::GetDescription()";
}

AbstrContextHandle* AbstrContextHandle::GetPrev() const
{
	return m_Prev;
}


UInt32 AbstrContextHandle::GetContextLevel() const
{
	dms_assert(this);
	UInt32 r=1;
	AbstrContextHandle* prev = m_Prev;
	while (prev)
	{
		prev = prev->m_Prev;
		++r;
	}
	return r;
}


AbstrContextHandle* AbstrContextHandle::GetLast()
{
	return s_LastAbstrContextHandle;
}

/********** FixedContextHandle **********/


CharPtr FixedContextHandle::GetDescription()
{ 
	return m_Msg; 
}

/********** SharedStrContextHandle **********/


CharPtr SharedStrContextHandle::GetDescription()
{ 
	return m_MsgStr.c_str(); 
}

/********** ContextHandle **********/

void ContextHandle::SetText(WeakStr context) 
{
	m_Context = context;
}

CharPtr ContextHandle::GetDescription()
{
	try {

		GenerateDescription();
		return m_Context.c_str();

	}
	catch (...) {}
	return "<ContextHandle::GenerateDescription() threw an exception>";
}

/********** ObjectContextHandle **********/

void ObjectContextHandleBase::GenerateDescription()
{
	if (!m_Role) 
		m_Role = "Object";
	if (m_Obj)
		SetText(
			mySSPrintF("while in %s for %s"
			,	m_Role
			,	m_Obj->GetSourceName().c_str()
			)
		);
	else
		SetText(mySSPrintF("while in %s('null')", m_Role));
}

bool ObjectContextHandleBase::IsFinalContext() const
{
	return m_Obj && m_Obj->GetLocation() != nullptr;
}

void ObjectIdContextHandle::GenerateDescription()
{
	SharedStr role = mySSPrintF("%s('%s')", m_Role, m_ID.GetStr().c_str());

	tmp_swapper<CharPtr> swapper(m_Role, role.c_str());
	ObjectContextHandle::GenerateDescription();
}

/********** CDebugContextHandle  **********/

#include <concrt.h>

namespace { // local defs

	THREAD_LOCAL UInt32 sCallCount = 0;

} // end anonymous namespace


UInt32 GetCallCount()
{
	return sCallCount;
}
#include <time.h>

CDebugContextHandle::CDebugContextHandle(CharPtr className, CharPtr funcName, bool active)
	:	m_Active(active)
	,	m_ClassName(className)
	,	m_FuncName(funcName)
	,	m_StartTime(0)
{
	if (m_Active)
	{
		DebugOutStream::scoped_lock lock(g_DebugStream);

		m_StartTime = clock();

		*g_DebugStream << "{ " << className << "::" << funcName;
		++sCallCount;
		dms_assert(sCallCount);
	}
}

CDebugContextHandle::~CDebugContextHandle() 
{
	if (m_Active)
	{
		dms_assert(sCallCount);
		sCallCount--; 

		DebugOutStream::scoped_lock lock(g_DebugStream);
		*g_DebugStream << "} " << m_ClassName  << "::" << m_FuncName << " (" << RunningTime() << " secs)";
	}

//	PopNotification(); AVOID DIFFERENCES BETWEEN DEBUG AND RELEASE
}

void CDebugContextHandle::GenerateDescription()
{
	SetText(mySSPrintF("while in %s::%s", m_ClassName, m_FuncName));
}

double CDebugContextHandle::RunningTime() const
{
	ClockTicks currTime = clock();
	assert(currTime >= m_StartTime);
	return double(currTime - m_StartTime ) / CLOCKS_PER_SEC;
}

void CDebugContextHandle::LogTime(CharPtr action)
{
	if (m_Active)
	{
		--sCallCount;

		DebugOutStream::scoped_lock lock(g_DebugStream);
		*g_DebugStream << "! " << m_ClassName << "::" << action << "(" << RunningTime() << " secs)";

		++sCallCount;
	}
}
void DMS_CONV DBG_TraceStr(CharPtr msg)
{
	DebugOutStream::scoped_lock lock(g_DebugStream);

	*g_DebugStream << msg;
}

bool DMS_Test(CharPtr name, CharPtr condStr, bool cond)
{
	MG_TRACE("%32s: %s", name, (cond) ? "OK" : condStr); 
	return cond;
}

