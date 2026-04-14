// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_MENUDATA_H)
#define __SHV_MENUDATA_H

#include "ShvBase.h"

#include "ptr/SharedPtr.h"

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
		reportF(SeverityTypeID::ST_MinorTrace, "%s %s Def %d Cpy %d Del %d Rem %d", action, typeid(T).name(), s_defCtor, s_cpyCtor, s_Destroy, s_defCtor+s_cpyCtor-s_Destroy);
	}
	static UInt32 s_defCtor;
	static UInt32 s_cpyCtor;
	static UInt32 s_Destroy;
};

template <typename T> UInt32 ObjCountMonitor<T>::s_defCtor = 0;
template <typename T> UInt32 ObjCountMonitor<T>::s_cpyCtor = 0;
template <typename T> UInt32 ObjCountMonitor<T>::s_Destroy = 0;

// =======================================

struct SHV_CALL MenuItem : movable
{
	MenuItem();
	MenuItem(WeakStr caption);
	MenuItem(WeakStr caption, std::unique_ptr<AbstrCmd> cmd, GraphicObject* host, UInt32 flags = 0);
	MenuItem(MenuItem&& rhs) noexcept;

	bool Execute() const;

	SharedStr                   m_Caption;
	std::unique_ptr<AbstrCmd>   m_Cmd;
	std::weak_ptr<GraphicObject>m_Host;
	UInt32                      m_Flags = 0;
	UInt32                      m_Level = 0;
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

struct MenuData : private std::vector<MenuItem>
{
	using std::vector<MenuItem>::const_iterator;

	using std::vector<MenuItem>::size;
	using std::vector<MenuItem>::empty;
	using std::vector<MenuItem>::begin;
	using std::vector<MenuItem>::end;
	using std::vector<MenuItem>::back;
	using std::vector<MenuItem>::pop_back;
	using std::vector<MenuItem>::operator [];

	MenuData() {}


	template <typename ...Args>
	constexpr decltype(auto) emplace_back(Args&&... args) {
		auto& reference = std::vector<MenuItem>::emplace_back<Args...>(std::forward<Args>(args)...);
		reference.m_Level = m_CurrLevel;
		return reference;
	}

	void push_back(MenuItem&& item)
	{
		emplace_back(std::move(item));
	}

	void AddSeparator()
	{
		if (!empty())
			push_back( MenuItem() );
	}

	UInt32                              m_CurrLevel = 0;
	std::weak_ptr<const DataItemColumn> m_DIC;
};

//----------------------------------------------------------------------
// struct : RequestClientCmd
//----------------------------------------------------------------------

struct RequestClientCmd : public AbstrCmd
{
	RequestClientCmd(SharedPtr<const TreeItem> ti, NotificationCode nc) : m_TI(std::move(ti)), m_NC(nc) {}

	GraphVisitState Visit(GraphicObject* go) override;

private:
	SharedPtr<const TreeItem>  m_TI;
	NotificationCode m_NC;
};

//----------------------------------------------------------------------
// InsertThemeActivationMenu
//----------------------------------------------------------------------

void InsertSubMenu(MenuData& menuData, CharPtr subMenuCaption, const TreeItem* item, GraphicObject* host);
void InsertThemeActivationMenu(MenuData& menuData, const Theme* theme, GraphicObject* host);

#endif // !defined(__SHV_MENUDATA_H)
