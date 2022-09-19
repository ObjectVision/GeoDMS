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

#if !defined(__SHV_MENUDATA_H)
#define __SHV_MENUDATA_H

#include "ShvBase.h"

#include "ptr/SharedPtr.h"
#include "ptr/PersistentSharedObj.h"

#include "AbstrCmd.h"

class GraphicObject;

// =======================================

#include "dbg/SeverityType.h"

template <typename T>
struct ObjCountMonitor
{
	ObjCountMonitor()                           { ++s_defCtor; Report("DefConstr"); }
	ObjCountMonitor(const ObjCountMonitor& src) { ++s_cpyCtor; Report("CpyConstr"); }
	~ObjCountMonitor()                          { ++s_Destroy; Report("Destruct "); }
	static void Report(CharPtr action)
	{
		reportF(ST_MinorTrace, "%s %s Def %d Cpy %d Del %d Rem %d", action, typeid(T).name(), s_defCtor, s_cpyCtor, s_Destroy, s_defCtor+s_cpyCtor-s_Destroy);
	}
	static UInt32 s_defCtor;
	static UInt32 s_cpyCtor;
	static UInt32 s_Destroy;
};

template <typename T> UInt32 ObjCountMonitor<T>::s_defCtor = 0;
template <typename T> UInt32 ObjCountMonitor<T>::s_cpyCtor = 0;
template <typename T> UInt32 ObjCountMonitor<T>::s_Destroy = 0;

// =======================================

struct MenuItem : movable
{
	MenuItem();
	MenuItem(WeakStr caption);
	MenuItem(WeakStr caption, OwningPtr<AbstrCmd> cmd, GraphicObject* host, UInt32 flags = 0);
	MenuItem(MenuItem&& rhs) noexcept;

	bool Execute() const;

	SharedStr                 m_Caption;
	OwningPtr<AbstrCmd>       m_Cmd;
	std::weak_ptr<GraphicObject>  m_Host;
	UInt32                    m_Flags = 0;
	UInt32                    m_Level = 0;
};

struct SubMenu
{
	SubMenu(MenuData& menuData, WeakStr caption);
	~SubMenu();

private:
	MenuData& m_MenuDataRef;
	SizeT m_StartSize;
};

// =======================================

struct MenuData : std::vector<MenuItem>
{
	MenuData()
		:	m_CurrLevel(0)
	{}

	void push_back(MenuItem&& item)
	{
		std::vector<MenuItem>::emplace_back(std::move(item));
		back().m_Level = m_CurrLevel;
	}

	void AddSeparator()
	{
		if (!empty())
			push_back( MenuItem() );
	}

	UInt32                              m_CurrLevel;
	std::weak_ptr<const DataItemColumn> m_DIC;
};

//----------------------------------------------------------------------
// struct : RequestClientCmd
//----------------------------------------------------------------------

struct RequestClientCmd : public AbstrCmd
{
	RequestClientCmd(SharedPtr<const TreeItem> ti, NotificationCode nc) : m_TI(std::move(ti)), m_NC(nc) {}

	GraphVisitState DoObject(GraphicObject* go) override;

private:
	SharedPtr<const TreeItem>  m_TI;
	NotificationCode m_NC;
};

//----------------------------------------------------------------------
// InsertThemeActivationMenu
//----------------------------------------------------------------------

void InsertThemeActivationMenu(MenuData& menuData, const Theme* theme, GraphicObject* host);

#endif // !defined(__SHV_MENUDATA_H)
