// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "ThemeSet.h"

#include "act/ActorVisitor.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "FocusElemProvider.h"
#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include "AspectGroup.h"
#include "DataView.h"
#include "ShvUtils.h"
#include "Theme.h"

//----------------------------------------------------------------------
// class  : ThemeSet
//----------------------------------------------------------------------

ThemeSet::ThemeSet(AspectNrSet possibleAspects, AspectNr activeTheme)
	:	m_ActiveTheme(activeTheme)
	,	m_PossibleAspects(possibleAspects)
	,	m_PossibleAspectGroups(0)
	,	m_DisabledAspectGroups(0)
{
	// Initialize m_PossibleAspectGroups
	for (AspectGroup ag=AG_First; ag != AG_Count; ++reinterpret_cast<UInt32&>(ag))
	{
		if (AspectGroupArray[ag].aspects & possibleAspects)
			m_PossibleAspectGroups |= (1 << ag);
	}
}

ThemeSet::ThemeSet(const ThemeSet& src)
	:	m_ActiveTheme         (src.m_ActiveTheme)
	,	m_PossibleAspects     (src.m_PossibleAspects)
	,	m_PossibleAspectGroups(src.m_PossibleAspectGroups)
	,	m_DisabledAspectGroups(src.m_DisabledAspectGroups)
	,	m_FocusElemProvider   (src.m_FocusElemProvider)
{
	for (AspectGroup ag=AG_First; ag != AG_Count; ++reinterpret_cast<UInt32&>(ag))
		if (src.m_Themes[ag])
			m_Themes[ag] = Theme::Create( src.m_Themes[ag].get() );

	if (m_FocusElemProvider)
		m_FocusElemProvider->AddThemeSet(this);

	auto dv = src.m_DataView.lock();
	if (dv)
		ConnectSelectionsTheme(dv.get());
}

ThemeSet::~ThemeSet()
{
	if (m_FocusElemProvider)
		m_FocusElemProvider->DelThemeSet(this);

	auto dv = m_DataView.lock();
	if (dv)
		SelThemeCreator::UnregisterThemeSet(dv.get(), this);
}

static TokenID activeID = GetTokenID_st("ActiveTheme");

void ThemeSet::SyncThemes(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);

	UInt32 activeThemeNr = m_ActiveTheme;
	SyncValue<UInt32>(viewContext, activeID, activeThemeNr, AN_AspectCount, sm);
	if ((sm == SM_Load) && (activeThemeNr < AN_AspectCount) && activeThemeNr != AN_Feature)
		m_ActiveTheme = AspectNr(activeThemeNr);

	for (AspectNr aNr = AN_Feature; aNr < AN_AspectCount; ++(UInt32&)aNr)
	{
		SharedPtr<TreeItem> aspectContext = const_cast<TreeItem*>(
			viewContext->GetConstSubTreeItemByID(
				GetTokenID_mt(AspectArray[aNr].name)
			).get()
		);
		if (sm == SM_Load)
		{
			if (aspectContext)
			{
				if (!m_Themes[aNr])
					m_Themes[aNr] = Theme::Create(aNr, aspectContext);
				else
					m_Themes[aNr]->Sync(aspectContext, SM_Load);
			}
		}
		else 
		{
			assert(sm == SM_Save);
			if (m_Themes[aNr])
				m_Themes[aNr]->Sync( viewContext->CreateItem( GetAspectNameID(aNr) ), SM_Save );
		}
	}

	for (AspectGroup ag = AG_First; ag != AG_Other; ++reinterpret_cast<UInt32&>(ag))
	{
		TokenID agID = GetAspectGroupNameID(ag);
		TreeItem* refItem = viewContext->GetSubTreeItemByID(agID);
		if (refItem && sm == SM_Load)
			EnableAspectGroup(ag,  ! debug_cast<AbstrDataItem*>(refItem)->LockAndGetValue<Bool>(0) );
		else if (sm == SM_Save && (refItem || IsDisabledAspectGroup(ag)) )
			SaveValue<Bool>(viewContext, agID, IsDisabledAspectGroup(ag));
	}
}


std::shared_ptr<Theme> ThemeSet::GetTheme(AspectNr aNr) const
{
	assert(aNr >= 0);
	assert(aNr < AN_AspectCount);
	return m_Themes[aNr];
}

std::shared_ptr<Theme> ThemeSet::GetEnabledTheme(AspectNr nr) const
{
	if (IsDisabledAspectGroup(AspectArray[nr].aspectGroup) )
		return nullptr;

	std::shared_ptr<Theme> theme = GetTheme(nr);
	if (theme && theme->IsDisabled()) 
		theme = nullptr; 
	return theme; 
}

bool   ThemeSet::HasAttrTheme(AspectNrSet ase) const
{
	for (AspectNr aNr = AN_First; aNr != AN_AspectCount; ++reinterpret_cast<UInt32&>(aNr) )
		if (ase & (1<< aNr))
		{
			auto result = GetTheme(aNr);
			if (result && !result->IsAspectParameter())
				return true;
		}
	return false;
}

std::shared_ptr<Theme> ThemeSet::GetActiveTheme() const
{
	// first look at the LayerClass related Color aspect
	static_assert(AN_Feature == 0);

	std::shared_ptr<Theme> result = GetTheme(m_ActiveTheme);
	if (result && !result->IsAspectParameter())
		return result;

	// else look for the first aspect that has a non parameter theme and a separate palette, save any non parameter theme as second best

	for (AspectNr aNr = AspectNr(1); aNr != AN_AspectCount; ++reinterpret_cast<UInt32&>(aNr) )
		if (aNr != AN_Selections)
		{
			std::shared_ptr<Theme> result2 = GetTheme(aNr);
			if (result2 && result2->IsAspectAttr())
			{
				result = result2;
				if (result->GetPaletteAttr())
					break;
			}
		}

	// apparently there are no non-feature non-parameter themes, go for parameter theme now.
	if (GetTheme(AN_Feature) && !GetTheme(AN_Feature)->IsAspectParameter())
		return GetTheme(AN_Feature);

	if (result)
		return result; // possible Aspect Parameter

	for (AspectNr aNr = AspectNr(1); aNr != AN_AspectCount; ++reinterpret_cast<UInt32&>(aNr) )
		if (aNr != AN_Selections)
		{
			result = GetTheme(aNr);
			if (result)
				return result;
		}
	return GetTheme(AN_Feature);
}

void ThemeSet::SetActiveTheme(AspectNr nr)
{
	m_ActiveTheme = nr;
}

void ThemeSet::SetTheme(Theme* theme, TreeItem* context)
{
	dms_assert(theme);
	AspectNr aNr = theme->GetAspectNr();
	dms_assert(aNr < AN_AspectCount);
	dms_assert((1 << aNr) & m_PossibleAspects); // PRECONDITION

	if (aNr < 0 || aNr > AN_AspectCount)
		return;

	m_Themes[aNr] = theme->shared_from_this();

	if (context)
		theme->Sync( context->CreateItem( GetAspectNameID(aNr) ), SM_Save);
}

ActorVisitState ThemeSet::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, bool isActive) const
{
	for (AspectNr a = AspectNr(0); a < AN_AspectCount; a = AspectNr(a+1))
		if (m_Themes[a] && m_Themes[a]->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	if (isActive && m_FocusElemProvider)
		if (visitor.Visit(m_FocusElemProvider->GetIndexParam()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	return AVS_Ready;
}

const AbstrDataItem* ThemeSet::GetThemeDisplayItem() const
{
	std::shared_ptr<Theme> activeTheme = GetActiveTheme();
	if (!activeTheme)
		return nullptr;

	const AbstrDataItem* ta = activeTheme->GetActiveAttr();
	return ta;
}

SharedStr UnitDisplayStr(const AbstrUnit* valuesUnit)
{
	if (!valuesUnit)
		return SharedStr();
	return mySSPrintF("[%s]", valuesUnit->GetDisplayName().c_str() );
}

#include "TreeItemUtils.h"

SharedStr ThemeSet::GetThemeDisplayNameWithinContext(const GraphicObject* owner, bool inclMetric) const
{
	dms_assert(owner);
	const AbstrDataItem* themeDisplayItem = GetThemeDisplayItem();
	if (!themeDisplayItem)
	{
		if (!m_Name.empty())
			return m_Name;
		return SharedStr("{EMPTY-THEMESET}");
	}
	gr_elem_index si = owner->NrEntries();
	return GetDisplayNameWithinContext(themeDisplayItem, inclMetric, [si, owner]() mutable ->const AbstrDataItem* {
		while (si)
		{
			const GraphicObject* sibbling = owner->GetConstEntry(--si);
			const ThemeSet* sibblingTs = dynamic_cast<const ThemeSet*>(sibbling);
			if (sibblingTs)
				return sibblingTs->GetThemeDisplayItem();
		}
		return nullptr;
	}
	);
}

SharedStr ThemeSet::GetUnitDisplayName() const
{
	return UnitDisplayStr( GetActiveValuesUnit() );
}

const AbstrDataItem* ThemeSet::GetActiveAttr() const
{
	std::shared_ptr<Theme> activeTheme = GetActiveTheme();
	if (activeTheme && ! activeTheme->IsAspectParameter() )
		return activeTheme->GetActiveAttr();
	else
	{
		const Theme* featureTheme = m_Themes[AN_Feature].get();
		if (!featureTheme)
			return activeTheme ? activeTheme->GetThemeAttr() : nullptr;

		dms_assert(featureTheme && featureTheme->GetPaletteAttr());
		const AbstrDataItem* fa = featureTheme->GetPaletteAttr();

		dms_assert(fa);
		dms_assert(!featureTheme->GetClassification()); // else there would have been an active non-parameter aspect theme
		dms_assert(!featureTheme->GetThemeAttr());      // else there would have been an active non-parameter aspect theme

		return fa;
	}
}

const AbstrUnit* ThemeSet::GetActiveEntity() const
{
	const AbstrDataItem* activeAttr = GetActiveAttr();
	return activeAttr ? activeAttr->GetAbstrDomainUnit() : nullptr;
}

const AbstrUnit* ThemeSet::GetActiveValuesUnit() const
{
	const auto* adi = GetActiveAttr();
	return adi ? GetRealAbstrValuesUnit( adi ) : nullptr;
}

SizeT ThemeSet::NrElements() const
{
	return GetActiveEntity()->GetCount();
}

void ThemeSet::SetFocusElemIndex(SizeT focusElemIndex)
{
	if (m_FocusElemProvider)
		m_FocusElemProvider->SetIndex(focusElemIndex);
}

SizeT ThemeSet::GetFocusElemIndex() const
{
	if (!m_FocusElemProvider)
		return UNDEFINED_VALUE(SizeT);
	return m_FocusElemProvider->GetIndex();
}

void ThemeSet::ConnectFocusElemIndexParam(const DataView* dv, const AbstrUnit* entity) 
{
	dms_assert(!m_FocusElemProvider);
	m_FocusElemProvider = GetFocusElemProvider(dv, entity);
	if (m_FocusElemProvider)
		m_FocusElemProvider->AddThemeSet(this);
}

std::shared_ptr<Theme> ThemeSet::CreateSelectionsTheme()
{
	if (!m_Themes[AN_Selections])
	{
		auto dv = m_DataView.lock(); if (!dv) return nullptr;
		SelThemeCreator::CreateSelectionsThemeInDesktop(dv.get(), GetActiveEntity()); // assignes m_Themes[AN_Selections]
	}
	dms_assert(m_Themes[AN_Selections]);
	return m_Themes[AN_Selections];
}

void ThemeSet::ConnectSelectionsTheme(DataView* dv)
{
	auto mdv = m_DataView.lock();
	if (mdv)
	{
		assert(mdv.get() == dv);
		return;
	}

	const AbstrUnit* entity = GetActiveEntity();
	if (entity)
	{
		if (!m_FocusElemProvider)
			ConnectFocusElemIndexParam(dv, entity);

		if (m_Themes[AN_Selections])
			return;

		m_Themes[AN_Selections] = SelThemeCreator::GetSelectionsThemeInDesktop(dv, entity);
		if (m_Themes[AN_Selections])
			return;

		// subscribe to OnSelectionThemeCreated notification
 	    SelThemeCreator::RegisterThemeSet(dv, this);
	}
	m_DataView = dv->shared_from_this();
}

void ThemeSet::OnSelectionsThemeCreated(Theme* selTheme)
{
	dms_assert(!m_Themes[AN_Selections]);

	GetActiveEntity()->UnifyDomain(selTheme->GetThemeEntityUnit(), "Domain of thematic attribute", "Domain of selection attribute", UM_Throw);

	m_Themes[AN_Selections] = selTheme->shared_from_this();
	m_DataView.reset();
}

//	AspectGroup support
void ThemeSet::EnableAspectGroup (AspectGroup ag, bool enable) 
{ 
	if (enable) 
		EnableAspectGroup(ag); 
	else 
		DisableAspectGroup(ag); 
}

ActorVisitState ThemeSet::PrepareThemeSetData(const Actor* act) const
{
	for (AspectNr a = AN_First; a!= AN_AspectCount; ++reinterpret_cast<UInt32&>(a))
		if (m_Themes[a])
			if (m_Themes[a]->PrepareThemeData(act) == AVS_SuspendedOrFailed)
			{
				if (!SuspendTrigger::DidSuspend())
				{
					dms_assert(act->WasFailed(FR_Data));
				}
				return AVS_SuspendedOrFailed;
			}
	return AVS_Ready;
}