// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

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
