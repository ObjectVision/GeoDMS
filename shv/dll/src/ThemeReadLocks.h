// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_THEMEREADLOCKS_H
#define __SHV_THEMEREADLOCKS_H

#include "DataLockContainers.h"
#include <set>

class Theme;
struct ThemeSet;
class GraphicObject;

//----------------------------------------------------------------------
// class  : ThemeReadLocks
//----------------------------------------------------------------------

struct ThemeReadLocks : DataReadLockContainer
{
	ThemeReadLocks()	: m_FailedTheme(0) {}
	~ThemeReadLocks();

	using DataReadLockContainer::push_back;

	bool push_back(const ThemeSet* ts, DrlType drlType);
 	bool push_back(const Theme* theme, DrlType type);

	bool ProcessFailOrSuspend(const GraphicObject* go) const;

private:
	bool push_back(const Theme* theme, const AbstrDataItem* adi, DrlType type);
	const Theme* m_FailedTheme;

	std::set<const Theme*> m_Themes;
};


#endif // __SHV_THEMEREADLOCKS_H

