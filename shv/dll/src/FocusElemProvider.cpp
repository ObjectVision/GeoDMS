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

#include "FocusElemProvider.h"

#include <map>

//----------------------------------------------------------------------
//	module SelThemeCreator
//----------------------------------------------------------------------

#include "act/MainThread.h"
#include "set/VectorFunc.h"

#include "DataItemClass.h"
#include "DataArray.h"
#include "ItemLocks.h"
#include "Unit.h"
#include "UnitClass.h"

#include "DataView.h"
#include "ShvUtils.h"
#include "ThemeSet.h"
#include "Theme.h"

using ThemeSetArrayMap = std::map<DataView*, ThemeSetArray> ;
static ThemeSetArrayMap s_ThemeSets;

void SelThemeCreator::UnregisterDataView(DataView* dv)
{
	assert(IsMainThread());
	assert(dv);
	ThemeSetArrayMap::iterator p = s_ThemeSets.find(dv);
	if (p != s_ThemeSets.end())
	{
		ThemeSetArray& thisArray = p->second;
		ThemeSetArray::iterator
			b = thisArray.begin(),
			e = thisArray.end();
		while(b != e)
			(*--e)->m_DataView.reset();
		s_ThemeSets.erase(p);
	}
}

void SelThemeCreator::RegisterThemeSet(DataView* dv, ThemeSet* ts)
{
	assert(IsMainThread());
	s_ThemeSets[dv].push_back(ts);
}

void SelThemeCreator::UnregisterThemeSet(DataView* dv, ThemeSet* ts)
{
	assert(IsMainThread());
	assert(dv);
	ThemeSetArrayMap::iterator p = s_ThemeSets.find(dv);
	if (p != s_ThemeSets.end()) // Insertion on dv must have been done in s_ThemeSets unless read from 
	{
		vector_erase_first(p->second, ts);
		if (p->second.empty())
			s_ThemeSets.erase(p);
	}
}

static TokenID valuesUnitID = GetTokenID_st("SelValues");

void SelThemeCreator::CreateSelectionsThemeInDesktop(DataView* dv, const AbstrUnit* entity)
{
	assert(IsMainThread());
	assert(dv);
	assert(entity);

	TreeItem* desktopItem = dv->GetDesktopContext();

	entity = GetUltimateSourceItem(entity);
	TreeItem* selectionParent = CreateDesktopContainer(desktopItem, entity);
//	dms_assert(selectionParent && !selectionParent->GetSubTreeItemByID(GetAspectNameID(AN_Selections)));

	AbstrUnit* userValuesUnit = Unit<SelectionID>::GetStaticClass()->CreateUnit(desktopItem, valuesUnitID);

	AbstrDataItem* newSelData = CreateDataItem(selectionParent, GetAspectNameID(AN_Selections), entity, userValuesUnit);
	SharedDataItemInterestPtr newSelDataManager = newSelData;

	DataWriteLock(newSelData, dms_rw_mode::write_only_mustzero).Commit();

	auto result = Theme::Create(AN_Selections, newSelData, 0, CreateSystemColorPalette(dv, userValuesUnit, AN_BrushColor, false, false, false, nullptr, nullptr));

	// ===================== Notify all relevant ThemeSets of the creation and remove them from the s_ThemeSets's ???

	auto selThemeNotifier = [entity, selTheme = result.get()](ThemeSet* candidate)
	{
		auto ae = candidate->GetActiveEntity();
		if (!ae)
			return false;
		ae->DetermineState();
		entity->DetermineState();
		if (ae->UnifyDomain(entity))
		{
			candidate->OnSelectionsThemeCreated(selTheme);
			return true;
		}
		return false;
	};
	for (auto& ts: s_ThemeSets)
	{
		if (ts.first->GetDesktopContext() == desktopItem)
		{
			ThemeSetArray& thisArray = ts.second;
			auto e = thisArray.end();
			thisArray.erase(std::remove_if(thisArray.begin(), e, selThemeNotifier), e);
		}
	}
}

std::shared_ptr<Theme> SelThemeCreator::GetSelectionsThemeInDesktop(DataView* dv, const AbstrUnit* entity)
{
	TreeItem* selectionParent = CreateDesktopContainer(dv->GetDesktopContext(), GetUltimateSourceItem(entity));
	if (selectionParent)
	{
		const AbstrDataItem* selData = AsDynamicDataItem(selectionParent->GetConstSubTreeItemByID(GetAspectNameID(AN_Selections)));
		if (selData)
			return Theme::Create(AN_Selections, selData, 0, GetSystemPalette(selData->GetAbstrValuesUnit(), AN_BrushColor));
	}
	return std::shared_ptr<Theme>();
}

//----------------------------------------------------------------------
// module FocusElemProvider
//----------------------------------------------------------------------
static TokenID s_focusElemIndexID = GetTokenID_st("FocusElemIndex");

AbstrDataItem* CreateFocusElemIndexParamInDesktop(const FocusElemKey& key)
{
	TreeItem* desktopItem   = key.first;
	const AbstrUnit* entity = key.second;

	TreeItem* parent = CreateDesktopContainer(desktopItem, entity);
	AbstrDataItem* result = AsDynamicDataItem(parent->GetSubTreeItemByID(s_focusElemIndexID) );
	if(!result)
	{
		result = CreateDataItem(parent, s_focusElemIndexID
		,	Unit<Void >::GetStaticClass()->CreateDefault() 
		,	Unit<SizeT>::GetStaticClass()->CreateDefault()
		);
		result->DisableStorage(true);
		result->UpdateMetaInfo();
		DataWriteHandle handle(result);
		handle->SetValue<SizeT>(0, UNDEFINED_VALUE(SizeT));
		handle.Commit();
	}
	return result;
}

typedef std::map<FocusElemKey, FocusElemProvider*>   FocusElemProviderMapType;
static FocusElemProviderMapType s_FocusElemProviderMap;

//----------------------------------------------------------------------
// FocusElemProvider mf implementation
//----------------------------------------------------------------------

#include "TableControl.h"

FocusElemProvider::FocusElemProvider(const FocusElemKey& key, AbstrDataItem* param)
	:	m_Key(key)
	,	m_IndexParam(param) 
{}

FocusElemProvider::~FocusElemProvider()
{
	s_FocusElemProviderMap.erase(m_Key);
}

SizeT FocusElemProvider::GetIndex() const 
{
	if (IsMainThread()) m_IndexParam->UpdateMetaInfo();
	if (!IsCalculatingOrReady(m_IndexParam->GetCurrUltimateItem()))
		return UNDEFINED_VALUE(SizeT);
	DataReadLock lock(m_IndexParam); return m_IndexParam->GetValue<SizeT>(0); 
}

void FocusElemProvider::SetIndex(SizeT newIndex)
{
	SizeT oldIndex = GetIndex();
	DataWriteHandle hnd(const_cast<AbstrDataItem*>(m_IndexParam.get_ptr()), dms_rw_mode::write_only_all);
	hnd->SetValue<SizeT>(0, newIndex);
	hnd.Commit();

	for (std::vector<TableControl*>::iterator ci = m_TableControls.begin(), ce = m_TableControls.end(); ci!=ce; ++ci)
		(*ci)->OnFocusElemChanged(newIndex, oldIndex);
	for (std::vector<ThemeSet*>::iterator si = m_ThemeSets.begin(), se = m_ThemeSets.end(); si!=se; ++si)
		(*si)->m_cmdFocusElemChanged(newIndex, oldIndex);
}

void FocusElemProvider::Release() const
{
	if (!DecRef()) delete this;	
}

void FocusElemProvider::AddThemeSet(ThemeSet* ts)
{
	dms_assert(vector_find(m_ThemeSets, ts) >= m_ThemeSets.size()); 
	m_ThemeSets.push_back(ts);
}

void FocusElemProvider::DelThemeSet(ThemeSet* ts)
{
	dms_assert(vector_find(m_ThemeSets, ts) <  m_ThemeSets.size());
	vector_erase_first(m_ThemeSets, ts);
}

void FocusElemProvider::AddTableControl(TableControl* tc)
{
	dms_assert(vector_find(m_TableControls, tc) >= m_TableControls.size()); 
	m_TableControls.push_back(tc);
}

void FocusElemProvider::DelTableControl(TableControl* tc)
{
	dms_assert(vector_find(m_TableControls, tc) <  m_TableControls.size());
	vector_erase_first(m_TableControls, tc);
}


//----------------------------------------------------------------------
// global helper funcs
//----------------------------------------------------------------------

FocusElemProvider* GetFocusElemProvider(const DataView* dv, const AbstrUnit* entity)
{
	if (entity->IsDefaultUnit())
		return nullptr;

	entity = debug_cast<const AbstrUnit*>(GetUltimateSourceItem(entity));
	FocusElemKey elemKey(dv->GetDesktopContext(), entity);

	FocusElemProvider*& focusElemProviderPtrRef = s_FocusElemProviderMap[elemKey];
	if (!focusElemProviderPtrRef)
		focusElemProviderPtrRef = new FocusElemProvider(
			elemKey,
			CreateFocusElemIndexParamInDesktop(elemKey)
		);
	return focusElemProviderPtrRef;
}

