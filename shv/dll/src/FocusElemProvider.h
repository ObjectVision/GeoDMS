// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#ifndef __SHV_FOCUSELEMPROVIDER_H
#define __SHV_FOCUSELEMPROVIDER_H


#include "ptr/SharedBase.h"

#include "DataLocks.h"

//----------------------------------------------------------------------
//	module SelectionsTheme
//----------------------------------------------------------------------

class AbstrUnit;

class DataView;
class Theme;
struct ThemeSet;
typedef std::vector<ThemeSet*> ThemeSetArray;

struct SelThemeCreator
{
	static void CreateSelectionsThemeInDesktop(DataView* dv, const AbstrUnit* entity);
	static std::shared_ptr<Theme> GetSelectionsThemeInDesktop(DataView* dv, const AbstrUnit* entity);

	static void UnregisterDataView(DataView* dv);
	static void RegisterThemeSet  (DataView* dv, ThemeSet* ts);
	static void UnregisterThemeSet(DataView* dv, ThemeSet* ts);
};

//----------------------------------------------------------------------
typedef std::pair<TreeItem*, const AbstrUnit*> FocusElemKey;

struct FocusElemProvider : SharedBase
{
	FocusElemProvider(const FocusElemKey& key, AbstrDataItem* param);
	~FocusElemProvider();

	SizeT GetIndex() const;
	void SetIndex(SizeT newIndex);

	void Release() const;
	void AddThemeSet(ThemeSet* ts);
	void DelThemeSet(ThemeSet* ts);
	void AddTableControl(TableControl* tc);
	void DelTableControl(TableControl* tc);

	const AbstrDataItem* GetIndexParam() const { return m_IndexParam; }
private:
	FocusElemKey               m_Key;
	SharedDataItemInterestPtr  m_IndexParam;
	std::vector<TableControl*> m_TableControls;
	ThemeSetArray              m_ThemeSets;
};

//----------------------------------------------------------------------
// global helper funcs
//----------------------------------------------------------------------

FocusElemProvider* GetFocusElemProvider(const DataView* dv, const AbstrUnit* entity);

#endif // __SHV_FOCUSELEMPROVIDER_H

