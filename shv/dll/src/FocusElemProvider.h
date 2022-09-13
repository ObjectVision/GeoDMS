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

