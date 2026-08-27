// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__RTC_VERSIONCOMPONENT_H)
#define __RTC_VERSIONCOMPONENT_H

#include "RtcInterface.h"

//----------------------------------------------------------------------

struct AbstrVersionComponent 
{
	RTC_CALL AbstrVersionComponent();
	RTC_CALL ~AbstrVersionComponent();
	virtual void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const = 0;
};

struct VersionComponent :AbstrVersionComponent
{
	RTC_CALL VersionComponent(CharPtr name);
	RTC_CALL ~VersionComponent();

	void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const override;

private:
	CharPtr m_Name;
};


#endif // __RTC_VERSIONCOMPONENT_H
