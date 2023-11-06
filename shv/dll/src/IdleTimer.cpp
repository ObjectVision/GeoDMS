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

#include "ShvDllPch.h"

#include "IdleTimer.h"

#include "act/Actor.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "set/VectorFunc.h"
#include "utl/IncrementalLock.h"


#include "DataView.h"

//----------------------------------------------------------------------
// struct IdleTimer implementation
//----------------------------------------------------------------------

UInt32   s_IdleTimerCount    = 0;
std::atomic<UInt32> s_TimerSuspendCount = 0;
bool     s_InEventLoop       = false;
bool     s_IdleTimerState    = false;
UINT_PTR s_TimerID           = 0;


VOID CALLBACK ProcessTimerMsg(HWND hwnd,
    UINT     uMsg,
    UINT_PTR idEvent,
    DWORD    dwTime
)
{
	dms_assert(hwnd == 0);
	dms_assert(uMsg == WM_TIMER);
	dms_assert(s_IdleTimerState);
	dms_assert(s_TimerID == idEvent);

	if (dms_is_debuglocked)
		return;

	IdleTimer::ProcessIdle();
}

void UpdateIdleTimerState()
{
	bool newState = (s_IdleTimerCount || s_InEventLoop) && !s_TimerSuspendCount;

	if (newState == s_IdleTimerState)
		return;

	if (newState)
	{
		s_IdleTimerState = true;
		s_TimerID = SetTimer(0, 0, 300, ProcessTimerMsg);
	}
	else
	{
		KillTimer(0, s_TimerID);
		s_IdleTimerState = false;
	}

}


IdleTimer::IdleTimer()
{
	if (!s_IdleTimerCount++)
		UpdateIdleTimerState();
}

IdleTimer::~IdleTimer()
{
	if (!--s_IdleTimerCount)
		UpdateIdleTimerState();
}

static std::vector<DataView*> s_DataViewArray;

void IdleTimer::Subscribe  (DataView* dv)
{
	s_DataViewArray.push_back(dv);
}

void IdleTimer::Unsubscribe(DataView* dv)
{
	vector_erase(s_DataViewArray, dv);
}

void IdleTimer::OnStartLoop()
{
	s_InEventLoop = true;
	UpdateIdleTimerState();
}

void IdleTimer::OnFinishLoop()
{
	s_InEventLoop = false;
	UpdateIdleTimerState();
}

void IdleTimer::ProcessIdle()
{
	DBG_START("IdleTimer", "ProcessIdle", MG_DEBUG_IDLE);

	dms_assert(s_IdleTimerState);
	SuspendTrigger::Resume();
	{
		StaticMtIncrementalLock<s_TimerSuspendCount> suspendTimer;
		UpdateIdleTimerState();
		dms_assert(!s_IdleTimerState);


		std::vector<DataView*>::const_iterator 
			e = s_DataViewArray.end(),
			b = s_DataViewArray.begin();
		while (e != b && !SuspendTrigger::MustSuspend())
		{
//			MessageBeep(-1); // DEBUG
			(*--e)->UpdateView();
		}
	}
	UpdateIdleTimerState();
	dms_assert(s_IdleTimerState);
}

