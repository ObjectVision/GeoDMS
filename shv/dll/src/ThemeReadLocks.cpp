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

#include "ThemeReadLocks.h"

#include "act/TriggerOperator.h"
#include "AbstrDataItem.h"

#include "GraphicObject.h"
#include "Theme.h"
#include "ThemeSet.h"

//----------------------------------------------------------------------
// class  : ThemeReadLocks
//----------------------------------------------------------------------

ThemeReadLocks::~ThemeReadLocks()
{
	for (auto i=m_Themes.begin(), e=m_Themes.end(); i!=e; ++i)
	{
		dms_assert(*i);
		(*i)->ResetDataLock();
	}
}


bool ThemeReadLocks::push_back(const Theme* theme, const AbstrDataItem* adi, DrlType drlType)
{
	dms_assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	dms_assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	dms_assert(theme);

	m_Themes.insert(theme);
	try {
		if (DataReadLockContainer::Add(adi, drlType))
		{
			dms_assert(!SuspendTrigger::DidSuspend());
			return true;
		}
	}
	catch (...)
	{
		if (!adi->IsFailed())
			throw; // rethrow if source of exception is unknown
	}

	dms_assert((drlType == DrlType::Suspendible && SuspendTrigger::DidSuspend()) || adi->WasFailed());
	if ( adi->WasFailed() )
	{
		theme->Fail( adi );
		dms_assert(!m_FailedTheme);
		m_FailedTheme = theme;
	}
	return false;
}

bool ThemeReadLocks::push_back(const Theme* theme, DrlType drlType)
{
	if (!theme)
		return true;

	dms_assert(drlType != DrlType::Certain || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility
	dms_assert((drlType != DrlType::Suspendible) || !SuspendTrigger::DidSuspend()); // should have been acted upon

	return	push_back(theme, theme->GetThemeAttr(),      drlType)
		&&	push_back(theme, theme->GetClassification(), drlType)
		&&	push_back(theme, theme->GetPaletteAttr(),    drlType);
}


bool ThemeReadLocks::push_back(const ThemeSet* ts, DrlType drlType)
{
	dms_assert(ts);
	const std::shared_ptr<Theme>* themePtrPtr = ts->m_Themes;
	const std::shared_ptr<Theme>* themePtrEnd = ts->m_Themes + AN_AspectCount;

	for (; themePtrPtr != themePtrEnd; ++themePtrPtr)
	{
		Theme* themePtr = themePtrPtr->get();
		dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon
		if (themePtr && !push_back(themePtr, drlType))
		{
			dms_assert(SuspendTrigger::DidSuspend() || m_FailedTheme);
			return false;
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend());
	return true;
}

bool ThemeReadLocks::ProcessFailOrSuspend(const GraphicObject* go) const
{
	dms_assert(go);
	if (m_FailedTheme)
	{
		go->Fail(m_FailedTheme);
		return false; // calculation complete
	}
	dms_assert(SuspendTrigger::DidSuspend());
	return true; // suspended?
}
