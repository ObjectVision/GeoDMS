// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "IdleTimer.h"
#include "DataView.h"

//----------------------------------------------------------------------
// struct IdleTimer implementation
//----------------------------------------------------------------------

static UInt32   s_IdleTimerCount    = 0;
static std::vector<std::weak_ptr<DataView>> s_DataViewArray;


IdleTimer::IdleTimer()
{
	assert(IsMainThread());
	if (!s_IdleTimerCount++)
	{
		assert(s_DataViewArray.empty());
	}
}

IdleTimer::~IdleTimer()
{
	assert(IsMainThread());
	if (--s_IdleTimerCount)
		return;

	for (auto wpdv : s_DataViewArray)
	{
		if (auto spdv = wpdv.lock())
			spdv->SetUpdateTimer();
	}
	s_DataViewArray.clear();
}

bool IdleTimer::IsInIdleMode() // is any IdleTimer active?
{
	assert(IsMainThread());
	return s_IdleTimerCount;
}

void IdleTimer::Subscribe  (std::weak_ptr<DataView> dv)
{
	assert(IsMainThread());
	s_DataViewArray.emplace_back(dv);
}
