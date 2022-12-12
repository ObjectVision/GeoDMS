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
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#include "ShvDllPch.h"

#include "MenuData.h"
#include "Theme.h"

#include "TreeItem.h"
#include "utl/IncrementalLock.h"


#include "GraphicObject.h"
#include "AbstrCmd.h"

MenuItem::MenuItem()
{}

MenuItem::MenuItem(WeakStr caption)
	: m_Caption(caption)
{}

MenuItem::MenuItem(WeakStr caption, OwningPtr<AbstrCmd> cmd, GraphicObject* host, UInt32 flags)
	:	m_Caption(caption)
	,	m_Cmd(std::move(cmd))
	,	m_Host((m_Cmd && host) ? host->shared_from_this() : nullptr)
	,	m_Flags(flags) 
{
}

MenuItem::MenuItem(MenuItem&& rhs) noexcept
	:	m_Caption(std::move(rhs.m_Caption))
	,	m_Cmd(std::move(rhs.m_Cmd))
	,	m_Host(std::move(rhs.m_Host))
	,	m_Flags(rhs.m_Flags)
	,	m_Level(rhs.m_Level)
{}

bool MenuItem::Execute() const
{
	if (!m_Cmd)
		return false;

	auto host = m_Host.lock();
	if (!host)
		return false;
	SuspendTrigger::SilentBlocker blockDuringExecution;
	return m_Cmd->Visit(host.get());
}

UInt32& InsertAndReturnLevelRef(MenuData& menuData, WeakStr caption)
{
	menuData.push_back(MenuItem(caption));
	return menuData.m_CurrLevel;
}

SubMenu::SubMenu(MenuData& menuData, WeakStr caption)
	:	m_MenuDataRef(menuData)
{
	menuData.push_back(MenuItem(caption));
	m_StartSize = menuData.size();
	dms_assert(m_StartSize); // no overflow

	++menuData.m_CurrLevel;
}

SubMenu::~SubMenu()
{
	--m_MenuDataRef.m_CurrLevel;

	dms_assert(m_MenuDataRef.size() >= m_StartSize);
	switch (m_MenuDataRef.size() - m_StartSize)
	{
	case 1: {
		// merge singleton submenu with its submenu caption menu item
		auto& subMenu = m_MenuDataRef.end()[-2];
		auto& subMenuItem = m_MenuDataRef.back();

		dms_assert(subMenu.m_Level < subMenuItem.m_Level);
		dms_assert(subMenu.m_Cmd.is_null());

		subMenu.m_Caption = subMenu.m_Caption + ' ' + subMenuItem.m_Caption;
		subMenu.m_Cmd = std::move(subMenuItem.m_Cmd);
		subMenu.m_Flags |= subMenuItem.m_Flags;
		subMenu.m_Host = subMenuItem.m_Host;
		// don't inherit level
	}
	[[fallthrough]]; // now that singleton submenu has been cleared, consider it as empty.
	case 0:
		// remove empty (or cleared) sub-menu
		m_MenuDataRef.pop_back();
	}
}


//----------------------------------------------------------------------
// struct : RequestClientCmd
//----------------------------------------------------------------------

#include "StateChangeNotification.h"
#include "IdleTimer.h"

GraphVisitState RequestClientCmd::DoObject(GraphicObject* go)
{
	dms_assert(m_TI);
//	if (CC_ShowStatistics)
	IdleTimer provideUpdates;
	NotifyStateChange(m_TI, m_NC);
	return GVS_Handled;
}

//----------------------------------------------------------------------
// InsertSubMenu
//----------------------------------------------------------------------

void InsertItemRequests(MenuData& menuData, const TreeItem* item, GraphicObject* host)
{
	dms_assert(item);
	const TreeItem* parent;

	while (parent = item->GetTreeParent())
	{
		menuData.push_back(
			MenuItem(
				item->GetFullName(),
				new RequestClientCmd(item, CC_Activate), 
				host
			) 
		);
		item = parent;
	}
}

void InsertSubMenu(MenuData& menuData, CharPtr subMenuCaption, const TreeItem* item, GraphicObject* host)
{
	if (!item)
		return;

	if (subMenuCaption)
	{
		SubMenu subMenu(menuData, SharedStr(subMenuCaption));
		InsertItemRequests(menuData, item, host);
	}
	else
		InsertItemRequests(menuData, item, host);
}

CharPtr SelectName(UInt32 nrSubMenus, AspectNr anr, CharPtr featureName, CharPtr otherName)
{
	if (nrSubMenus == 1)
		return 0;
	if (anr == AN_Feature)
		return featureName;
	return otherName;
}

void InsertThemeActivationMenu(MenuData& menuData, const Theme* theme, GraphicObject* host)
{
	if (!theme)
		return;
	UInt32 nrSubMenus = UInt32(theme->GetThemeAttr()!=0) + UInt32(theme->GetClassification()!=0) + UInt32(theme->GetPaletteAttr()!=0);

	dms_assert(nrSubMenus); // invariant of theme
	if (!nrSubMenus) // REMOVE if previous assert can be poven
		return;

	SubMenu subMenu(menuData, SharedStr(theme->GetAspectName()));

	AspectNr anr = theme->GetAspectNr();
	InsertSubMenu(menuData, SelectName(nrSubMenus, anr, "KeysPerTheme",   "ThematicData"), theme->GetThemeAttr(),      host);
	InsertSubMenu(menuData, SelectName(nrSubMenus, anr, "KeysPerFeature", "ClassBreaks" ), theme->GetClassification(), host);
	InsertSubMenu(menuData, SelectName(nrSubMenus, anr, "Features",       "AspectValues"), theme->GetPaletteAttr(),    host);
}
