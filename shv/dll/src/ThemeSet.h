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
#pragma once

#ifndef __SHV_THEMESET_H
#define __SHV_THEMESET_H

#include "ptr/SharedPtr.h"

#include "DataLocks.h"

#include "Aspect.h"
#include "ShvSignal.h"
class Theme;

struct FocusElemProvider;

/* 
An aspect combines:
- a ThematicAttr: E->V
- optional Classification: Cls->V            (if NULL, Cls == V)
- optional AspectPalette:  Cls->AspectValues (if NULL, AspectValues == Cls)

Each Layer  gives an main-entry in a Legend
Each Aspect gives a LegendTable
a LegendTable has one entry per Cls
- a Cls- related description from the classification
- a representation for the palettevalue; if Cls = AspectDomain, then this representation is directly taken from Cls
- optionally, one or several (label) attributes of Cls.
*/


struct ThemeSet
{
	ThemeSet(AspectNrSet possibleAspects, AspectNr activeTheme);
	ThemeSet(const ThemeSet& src);
	~ThemeSet();

	//	called by embedder
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, bool isActive) const;
	void SyncThemes(TreeItem* viewContext, ShvSyncMode sm);

	//	interface

	std::shared_ptr<Theme> GetTheme(AspectNr nr) const;
	std::shared_ptr<Theme> GetEnabledTheme(AspectNr nr) const;
	void   SetTheme(Theme* theme, TreeItem* context);
	std::shared_ptr<Theme> GetActiveTheme() const;
	void   SetActiveTheme(AspectNr nr);
	bool   HasAttrTheme(AspectNrSet ase) const;

	const AbstrDataItem* GetThemeDisplayItem() const;

	SharedStr GetThemeDisplayNameWithinContext(const GraphicObject* owner, bool inclMetric) const;
	SharedStr GetUnitDisplayName() const;
	void SetThemeDisplayName(WeakStr str) { m_Name = str; }

	const AbstrDataItem* GetActiveAttr() const;
	const AbstrUnit* GetActiveEntity() const;
	const AbstrUnit* GetActiveValuesUnit() const;

	SizeT NrElements() const;
	SizeT GetFocusElemIndex() const;

	std::shared_ptr<Theme> CreateSelectionsTheme();
	std::shared_ptr<const Theme> CreateSelectionsTheme() const { return const_cast<ThemeSet*>(this)->CreateSelectionsTheme(); }
	void ConnectSelectionsTheme(DataView*);
	void OnSelectionsThemeCreated(Theme* selTheme);

	//	AspectGroup support
	bool IsPossibleAspectGroup(AspectGroup ag) const { return m_PossibleAspectGroups & (1 << ag); }
	bool IsDisabledAspectGroup(AspectGroup ag) const { return m_DisabledAspectGroups & (1 << ag); }

	void EnableAspectGroup(AspectGroup ag) { m_DisabledAspectGroups &= ~(1 << ag); }
	void DisableAspectGroup(AspectGroup ag) { m_DisabledAspectGroups |= (1 << ag); }
	void EnableAspectGroup(AspectGroup ag, bool enable);

	ActorVisitState PrepareThemeSetData(const Actor* act) const;
	AspectNrSet GetPossibleAspects() const { return m_PossibleAspects;  }
public:
	FocusElemChangedSignal m_cmdFocusElemChanged;

protected:
	void SetFocusElemIndex(SizeT focusElemIndex);
	void ConnectFocusElemIndexParam(const DataView* dv, const AbstrUnit* entity);

	std::shared_ptr<Theme> m_Themes[AN_AspectCount];
	AspectNr         m_ActiveTheme;
	AspectNrSet      m_PossibleAspects;

private: friend struct ThemeReadLocks; friend struct SelThemeCreator; friend class LayerControl;
	SharedStr                    m_Name;
	SharedPtr<FocusElemProvider> m_FocusElemProvider;
	UInt32                       m_PossibleAspectGroups;
	UInt32                       m_DisabledAspectGroups;
	std::weak_ptr<DataView>      m_DataView;
};

template <typename GO>
SharedStr GetThemeDisplayName(const GO* themedObj)
{
	auto owner = themedObj->GetOwner().lock(); if (!owner) return SharedStr();
	return themedObj->GetThemeDisplayNameWithinContext(owner.get(), false);
}

template <typename GO>
SharedStr GetThemeDisplayNameInclMetric(const GO* themedObj)
{
	auto owner = themedObj->GetOwner().lock(); if (!owner) return SharedStr();
	return themedObj->GetThemeDisplayNameWithinContext(owner.get(), true);
}

#endif // __SHV_THEMESET_H

