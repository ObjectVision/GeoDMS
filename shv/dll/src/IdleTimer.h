// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_IDLETIMER_H
#define __SHV_IDLETIMER_H

//----------------------------------------------------------------------
// struct IdleTimer
//----------------------------------------------------------------------

struct IdleTimer
{
	IdleTimer();
	~IdleTimer();

	static bool IsInIdleMode(); // is any IdleTimer active?
	static void Subscribe  (std::weak_ptr<DataView> dv);
};


#endif // __SHV_IDLETIMER_H


