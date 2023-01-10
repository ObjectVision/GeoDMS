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

#include "GraphicLayer.h"

#include "act/ActorVisitor.h"
#include "dbg/DebugCast.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

#include "DataArray.h"
#include "DisplayValue.h"
#include "StateChangeNotification.h"
#include "Unit.h"
#include "UnitClass.h"

#include "AbstrCmd.h"
#include "AspectGroup.h"
#include "AbstrController.h"
#include "DataView.h"
#include "FocusElemProvider.h"
#include "IndexCollector.h"
#include "LayerClass.h"
#include "LayerControl.h"
#include "LayerInfo.h"
#include "LayerSet.h"
#include "LockedIndexCollectorPtr.h"
#include "MenuData.h"
#include "MouseEventDispatcher.h"
#include "Theme.h"
#include "ThemeReadLocks.h"

//----------------------------------------------------------------------
// class  : GraphicLayer
//----------------------------------------------------------------------

GraphicLayer::GraphicLayer(GraphicObject* owner, const LayerClass* cls)
	:	base_type(owner)
	,	ThemeSet(cls->GetPossibleAspects(), cls->GetMainAspect() )
{
	m_State.Set(GOF_IgnoreActivation|SOF_DetailsVisible);
	m_cmdFocusElemChanged.connect(
		[this](auto _1, auto _2) { this->OnFocusElemChanged(_1, _2); }
	);
}

GraphicLayer::~GraphicLayer() // hide destruction of data members
{}

std::weak_ptr<LayerSet> GraphicLayer::GetLayerSet() 
{
	return std::static_pointer_cast<LayerSet>(GetOwner().lock());
}

void GraphicLayer::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	dms_assert(viewContext);
	base_type::Sync(viewContext, sm);
	SyncThemes(viewContext, sm);
	if (sm==SM_Load)
		UpdateShowSelOnly(); // followup on base_type::Sync call to SyncShowSelOnly
}

//	override virtuals of Actor
ActorVisitState GraphicLayer::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (ThemeSet::VisitSuppliers(svf, visitor, IsActive()) == AVS_SuspendedOrFailed) 
		return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void GraphicLayer::DoInvalidate() const
{
	base_type::DoInvalidate();

	dms_assert(!m_State.HasInvalidationBlock());

	m_EntityIndexCollector = nullptr;
	m_State.Clear(GLF_EntityIndexReady|GLF_FeatureIndexReady);

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
}

void GraphicLayer::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	SubMenu subMenu(med.m_MenuData, GetThemeDisplayName(this)); // SUBMENU

	FillLcMenu(med.m_MenuData);
}

void GraphicLayer::SetActive(bool newState)
{
	if (IsActive() == newState)
		return;

	base_type::SetActive(newState);

	if (IsDrawn() || newState)
	{
		SizeT e = GetFocusElemIndex();
		
		if (IsDrawn() && IsDefined(e))
		{
			if (HasEntityAggr())
				InvalidateDraw();
			else
			{
				e = Entity2FeatureIndex(e);
				if (IsDefined(e))
					_InvalidateFeature(e);
			}
		}
		if (newState)
		{
			CreateViewValueAction(GetActiveAttr(), GetFocusElemIndex(), false);
			auto dv = GetDataView().lock();
			dv->OnCaptionChanged();
		}
	}
}

void GraphicLayer::ChangeTheme(Theme* theme)
{
	ThemeSet::SetTheme(theme, GetContext() );
}

void GraphicLayer::SetThemeAndActivate(Theme* theme, const AbstrDataItem* focus)
{	
	ChangeTheme(theme);
	dms_assert(focus);
	if (theme->GetThemeAttr() == focus || theme->GetClassification() == focus || theme->GetPaletteAttr() == focus)
		SetActiveTheme(theme->GetAspectNr());
}

SharedStr GraphicLayer::GetCaption() const
{
	auto dv = GetDataView().lock();
	if (!dv)
		return {};

	const AbstrUnit* domain = GetActiveEntity();
	if (!domain)
		return mgFormat2SharedStr("MapView %s", GetThemeDisplayName(this));

	SizeT nrRecs = dv && const_cast<GraphicLayer*>(this)->PrepareDataOrUpdateViewLater(domain) ? domain->GetCount() : UNDEFINED_VALUE(SizeT);
	SharedStr domainName = SharedStr(domain->GetID());
	return mgFormat2SharedStr("%s recs in MapView %s %s", AsString(nrRecs, FormattingFlags::ThousandSeparator), GetThemeDisplayName(this), domainName);
}

struct ActivateClassificationCmd : AbstrCmd
{
	ActivateClassificationCmd(const AbstrDataItem* classification, Theme* theme)
		:	m_Classification(classification)
		,	m_Theme(theme)
	{}

	GraphVisitState DoLayer(GraphicLayer* gl) override 
	{ 
		AspectNr aNr = m_Theme->GetAspectNr();
		const AbstrUnit* domain = m_Classification->GetAbstrDomainUnit();

		SharedDataItemInterestPtr palette = FindAspectAttr(aNr, m_Classification, domain, gl->GetLayerClass());
		if (!palette)
			palette = FindAspectAttr(aNr, domain, domain, gl->GetLayerClass());
		if (!palette) {
			auto dv = gl->GetDataView().lock(); if (!dv) return GVS_Handled;
			palette = CreatePaletteData(dv.get(), domain, aNr, m_Classification != 0, false, nullptr, nullptr);
		}
		dms_assert(palette);
		gl->ChangeTheme(
			Theme::Create(
				aNr, 
				m_Theme->GetThemeAttr(),
				m_Classification,
				palette
			).get()
		);
		gl->Invalidate();
		return GVS_Handled;
	}

private:
	const AbstrDataItem* m_Classification;
	Theme*                m_Theme;
};

#include "DedicatedAttrs.h"

typedef bool (*TSupplCallbackFunc)(ClientHandle clientHandle, const TreeItem* supplier);

template <typename Func>
struct FuncWrapper
{
	FuncWrapper(Func&& f) : m_Func(std::move(f)) {}

	ClientHandle Handle() { return reinterpret_cast<ClientHandle>(&m_Func); }

	static bool Callback(ClientHandle clientHandle, const TreeItem* supplier)
	{
		return reinterpret_cast<Func*>(clientHandle)->operator()(supplier);
	}

	Func m_Func;
};


template <typename Func>
FuncWrapper<Func> MakeFuncWrapper(Func&& f) { return FuncWrapper<Func>(std::move(f)); }

struct ActivateUniqueValuesPaletteCmd : AbstrCmd
{
	ActivateUniqueValuesPaletteCmd(AspectNr a, const AbstrDataItem* themeAttr)
		: m_AspectNr(a)
		, m_ThemeAttr(themeAttr)
	{
		dms_assert(themeAttr);
	}

	GraphVisitState DoLayer(GraphicLayer* gl) override
	{
		dms_assert(m_ThemeAttr);
		auto paletteDomain = m_ThemeAttr->GetAbstrValuesUnit();

		SharedDataItemInterestPtr palette = FindAspectAttr(m_AspectNr, m_ThemeAttr, paletteDomain, gl->GetLayerClass());
		if (!palette) {
			auto dv = gl->GetDataView().lock(); if (!dv) return GVS_Handled;
			palette = CreatePaletteData(dv.get(), paletteDomain, m_AspectNr, false, false, nullptr, nullptr);
		}
		gl->ChangeTheme(
			Theme::Create(m_AspectNr, m_ThemeAttr, nullptr, palette).get()
		);
		gl->Invalidate();
		return GVS_Handled;
	}

private:
	AspectNr m_AspectNr;
	SharedPtr<const AbstrDataItem> m_ThemeAttr;
};

void AddClassificationMenu(MenuData& menuData, AspectNr a, Theme* classifiedTheme, const AbstrDataItem* themeAttr, GraphicLayer* layer)
{
	const AbstrDataItem* classification = classifiedTheme ? classifiedTheme->GetClassification() : nullptr;
	if (classifiedTheme && classifiedTheme->GetThemeAttr())
		themeAttr = classifiedTheme->GetThemeAttr();

	if (!themeAttr)
		return;

	menuData.push_back(
		MenuItem(mySSPrintF("Unique Values of %s", themeAttr->GetFullName())
			, MakeOwned<AbstrCmd, ActivateUniqueValuesPaletteCmd>(a, themeAttr)
		,	layer
		,	0
		)
	);

		// get all available classifications
	auto funcWrapper = MakeFuncWrapper( [&menuData, classifiedTheme, classification, layer] (const TreeItem* supplier)->bool
		{
			const AbstrDataItem* adi = AsDataItem(supplier);
			dms_assert(adi);
			menuData.push_back(
				MenuItem(adi->GetFullName()
				,	MakeOwned<AbstrCmd, ActivateClassificationCmd>(adi, classifiedTheme)
				,	layer
				,	(adi == classification) ? MF_CHECKED : 0
				)
			);
			return true;
		}
	);

	DMS_DataItem_VisitClassBreakCandidates(themeAttr, funcWrapper.Callback, funcWrapper.Handle());
}

void GraphicLayer::FillLcMenu(MenuData& menuData)
{
	ScalableObject::FillLcMenu(menuData);

//	Get Statistics
	if (GetThemeDisplayItem()) // false if ThemeSet has just been initialized
		menuData.push_back(
			MenuItem(
				"Show Statistics on " + GetThemeDisplayName(this), 
				new RequestClientCmd(GetThemeDisplayItem(), CC_ShowStatistics),
				this
			) 
		);

	std::vector<AspectNr> classifialbeAspects;

	const AbstrDataItem* themeAttr = nullptr;
	if (GetActiveTheme())
		themeAttr = GetActiveTheme()->GetThemeAttr();

	if (themeAttr)
		classifialbeAspects.push_back( GetActiveTheme().get()->GetAspectNr() );

	for (AspectNr a = AN_OrderBy; a != AN_AspectCount; ++reinterpret_cast<int&>(a))
		if (m_Themes[a] && m_Themes[a] != GetActiveTheme())
			classifialbeAspects.push_back(a);

	auto aspectSet = GetLayerClass()->GetPossibleAspects();
	for (AspectNr a = AN_OrderBy; a != AN_AspectCount; ++reinterpret_cast<int&>(a))
		if (!m_Themes[a] && ((1 <<a) & aspectSet))
			classifialbeAspects.push_back(a);

	if (classifialbeAspects.size())
	{
		SubMenu subMenu(menuData, SharedStr("Classify ..."));  // SUBMENU
		for (AspectNr a: classifialbeAspects)
		{
			SubMenu aspectSubMenu(menuData, SharedStr(AspectArray[a].name));
			AddClassificationMenu(menuData, a, m_Themes[a].get(), themeAttr, this);
		}
	}

	menuData.AddSeparator();

//	subMenu Activate
	auto activeTheme = GetActiveTheme();

	SubMenu subMenu(menuData, SharedStr("Activate TreeItem of Layer Aspect")); // SUBMENU

	InsertThemeActivationMenu(menuData, activeTheme.get(), this);
	for (AspectNr aNr = AspectNr(0); aNr != AN_AspectCount; ++reinterpret_cast<UInt32&>(aNr) )
	{
		auto theme = GetTheme(aNr);
		if (theme != activeTheme)
			InsertThemeActivationMenu(menuData, theme.get(), this);
	}
}

void GraphicLayer::EnableAspectGroup(AspectGroup ag, bool enable)
{
	if (enable == IsDisabledAspectGroup(ag))
	{
		ThemeSet::EnableAspectGroup(ag, enable);

		SaveValue<Bool>(GetContext(), GetAspectGroupNameID(ag), IsDisabledAspectGroup(ag));

		InvalidateView();
		InvalidateDraw();
		if (enable && !AllVisible())
			ToggleVisibility();
	}
}

std::shared_ptr<LayerControlBase> GraphicLayer::CreateControl(MovableObject* owner)
{
	return make_shared_gr<LayerControl>(owner, this)();
}

const LayerClass* GraphicLayer::GetLayerClass() const
{
	return debug_cast<const LayerClass*>(GetDynamicClass());
}

TokenID GraphicLayer::GetID() const
{
	SharedStr themeDisplayName = GetThemeDisplayName(this);
	return GetTokenID_mt(
		themeDisplayName.begin(), 
		themeDisplayName.send()
	);
}

bool GraphicLayer::VisibleLevel(GraphDrawer& d) const
{
	Float64 currNrPixelsPerUnit = d.GetTransformation().ZoomLevel() / d.GetSubPixelFactor();
	return VisibleLevel(currNrPixelsPerUnit);
}

bool GraphicLayer::VisibleLevel(Float64 currNrPixelsPerUnit) const
{
	ThemeReadLocks trl;
	auto minPixSizeTheme = GetTheme(AN_MinPixSize);                      // f.e. 0.5 meter / pixel
	if	(minPixSizeTheme)
		if (	!trl.push_back(minPixSizeTheme.get(), DrlType::Suspendible)
			||	minPixSizeTheme->GetNumericAspectValue() * currNrPixelsPerUnit > 1)  // f.e. more than 2 pixels / meter is zoomed-in too much
		return false;

	auto maxPixSizeTheme = GetTheme(AN_MaxPixSize);                      // f.e. 5.0 meter / pixel
	if	(maxPixSizeTheme )
		if	(	!trl.push_back(maxPixSizeTheme.get(), DrlType::Suspendible)
			||	maxPixSizeTheme->GetNumericAspectValue() * currNrPixelsPerUnit < 1)  // f.e. less than 0.2 pixels / meter is zoomed-out too much
		return false;

	return true;
}

const IndexCollector* GraphicLayer::GetIndexCollector() const
{
	GetLastChangeTS(); // checks suppliers for changes and calls DoInvalidate if any supplier changed
	dms_assert(m_State.Get(GLF_EntityIndexReady) || !m_EntityIndexCollector);
	if (!m_State.Get(GLF_EntityIndexReady))
	{
		dms_assert(!m_EntityIndexCollector);
		if (GetActiveTheme())
			m_EntityIndexCollector = IndexCollector::Create( m_Themes[AN_Feature].get() );
		m_State.Set(GLF_EntityIndexReady);
	}
	return m_EntityIndexCollector;
}

bool GraphicLayer::HasEntityIndex() const
{
	const Theme* featureTheme = m_Themes[AN_Feature].get();
	return featureTheme && (featureTheme->GetThemeAttr() || featureTheme->GetClassification());
}

bool GraphicLayer::HasEntityAggr() const
{
	const Theme* featureTheme = m_Themes[AN_Feature].get();
	return featureTheme && featureTheme->GetClassification();
}

SizeT GraphicLayer::Feature2EntityIndex(SizeT featureIndex) const
{
	if (HasEntityIndex() && IsDefined(featureIndex))
	{
		const IndexCollector* ic = GetIndexCollector();
		dms_assert(ic);
		dms_assert(ic->HasExtKey() || ic->HasGeoRel());

		auto featureLoc = ic->GetTiledLocation(featureIndex);
		LockedIndexCollectorPtr lockedPtr(ic, featureLoc.first); // can change featureIndex
		return Convert<SizeT>(lockedPtr->GetEntityIndex(featureLoc.second));
	}
	return featureIndex;
}

SizeT GraphicLayer::Entity2FeatureIndex(SizeT entityIndex) const
{
	dms_assert(!HasEntityAggr());
	if (IsDefined(entityIndex) && HasEntityIndex())
		return LockedIndexCollectorPtr(GetIndexCollector(), no_tile)->GetFeatureIndex(entityIndex);
	return entityIndex;
}

void GraphicLayer::SelectAll(bool select)
{
	if (!m_Themes[AN_Selections] && !select)
		return;

	auto selTheme = CreateSelectionsTheme();
	dms_assert(selTheme);
	AbstrDataItem* selAttr = const_cast<AbstrDataItem*>(selTheme->GetThemeAttr());

	DataWriteLock lock(selAttr);

	auto selData = mutable_array_cast<SelectionID>(lock)->GetDataWrite();

	DataArray<SelectionID>::iterator
		i = selData.begin(),
		e = selData.end();

	if (select)
		fast_fill(i, e, true);
	else
		fast_zero(i, e);
	lock.Commit();
}

void GraphicLayer::SelectDistrict(const CrdPoint& pnt, EventID eventID)
{
	throwErrorD("SelectDistrict", "Active Layer is not a GridLayer");
}

bool GraphicLayer::SelectFeatureIndex(AbstrDataObject* selAttrObj, SizeT featureIndex, EventID eventID)
{
	SizeT i = Feature2EntityIndex(featureIndex);
	if (!(eventID & EID_REQUEST_SEL))
		return SetFocusEntityIndex(i, eventID & EID_LBUTTONDBLCLK);

	return SelectEntityIndex(selAttrObj, i, eventID);
}

bool GraphicLayer::IsFeatureSelected(SizeT featureIndex) const
{
	return IsEntitySelected(Feature2EntityIndex(featureIndex));
}


bool GraphicLayer::HasClassIdAttr() const
{
	auto theme = GetActiveTheme();
	if (!theme)
		return false;
	const AbstrDataItem* themeAttr = theme->GetThemeAttr();
	if (!themeAttr)
		return false;
	return themeAttr->GetAbstrValuesUnit()->GetUnitClass() == Unit<ClassID>::GetStaticClass();
}

bool GraphicLayer::HasEditAttr() const
{
	return HasClassIdAttr()
		&&	GetActiveTheme()->GetThemeAttrSource()->HasConfigData();
}

std::shared_ptr<const Theme> GraphicLayer::GetEditTheme() const
{
	if (HasEditAttr() && IsDefined(GetCurrClassID()))
		return GetActiveTheme();
	return CreateSelectionsTheme();
}

AbstrDataItem* GraphicLayer::GetEditAttr() const
{
	return const_cast<AbstrDataItem*>(GetEditTheme()->GetThemeAttrSource());
}

ClassID GraphicLayer::GetCurrClassID() const
{
	auto dv = GetDataView().lock();
	if (dv) {
		FocusElemProvider* fep = GetFocusElemProvider(dv.get(), GetActiveTheme()->GetThemeValuesUnit());
		if (fep)
			return fep->GetIndex();
	}
	return UNDEFINED_VALUE(ClassID);
}

SharedStr GraphicLayer::GetCurrClassLabel() const
{
	//dms_assert(HasEditAttr()); // PRECONDITION, BUT MUTATING
	GuiReadLock tileLock;
	return DisplayValue(GetActiveTheme()->GetThemeValuesUnit(), GetCurrClassID(), true,
		m_ActiveThemeValuesUnitLabelLock, MAX_TEXTOUT_SIZE, tileLock
	);
}

bool GraphicLayer::SelectEntityIndex(AbstrDataObject* selAttrObj, SizeT selectedIndex, EventID eventID)
{
	dms_assert((eventID & EID_REQUEST_SEL) && !(eventID & EID_REQUEST_INFO));

	if (!IsDefined(selectedIndex))
		return false;

	bool doToggle = (eventID & EID_CTRLKEY );

	AbstrDataItem* selAttr = GetEditAttr();
//	InvalidationBlock changeLock(GetEditTheme()->GetThemeAttr()); // REMOVE, MOVE TO DataWriteLock as a Generic facility

	ClassID currClassID;
	bool doSetClassID = HasEditAttr() && IsDefined(currClassID = GetCurrClassID());

//	bool resetExistingValues = (!doSetClassID) && IsCreateNewEvent(eventID);
	bool keepExistingValues = doSetClassID || !IsCreateNewEvent(eventID);
//	DataWriteLock writeLock(selAttr, DmsRwChangeType(!keepExistingValues));

	if (doSetClassID)
	{ 
		auto selData = mutable_array_cast<ClassID>(selAttrObj)->GetDataWrite();
		if (ClassID(selData[selectedIndex]) == currClassID)
			goto cancel;

		selData[selectedIndex] = currClassID;
	}
	else
	{
		auto selData = mutable_array_cast<SelectionID>(selAttrObj)->GetDataWrite();

		// oldValue  FALSE   TRUE
		// ========  ======  =====
		// doToggle
		// FALSE     ->TRUE  cancel
		// TRUE      ->TRUE  ->FALSE

		if (doToggle && !selData[selectedIndex] )
			goto cancel;

		selData[selectedIndex] = (not doToggle) || ( selData[selectedIndex]==0);
	}
	if (keepExistingValues && !HasEntityAggr())
	{
		selectedIndex = Entity2FeatureIndex(selectedIndex);
		if (IsDefined(selectedIndex))
			_InvalidateFeature( selectedIndex ); // only invalidate changed feature of Layer
	}
	else
		InvalidateDraw(); // change 

//	writeLock.Commit(); // increases TimeStamp
//	changeLock.ProcessChange(); // absorbes GetLastTS without DoInvalidate
	return true;

cancel:
	return false;
}

bool GraphicLayer::SetFocusEntityIndex(SizeT focussedIndex, bool showDetail)
{
	bool changed = (focussedIndex != GetFocusElemIndex());

	if (changed)
		SetFocusElemIndex(focussedIndex);

	if (IsActive() && showDetail)
		CreateViewValueAction(GetActiveAttr(), focussedIndex, true);

	return changed;
}

bool GraphicLayer::IsEntitySelected(SizeT entityID) const
{
	if (!IsDefined(entityID) || !m_Themes[AN_Selections])
		return false;

	auto selTheme =  CreateSelectionsTheme();
	dms_assert(selTheme);
	const AbstrDataItem* selAttr = const_cast<AbstrDataItem*>(selTheme->GetThemeAttr());

	return selAttr->LockAndGetValue<SelectionID>(entityID);
}


void GraphicLayer::OnFocusElemChanged(SizeT newSelectedID, SizeT oldSelectedID)
{
	if (!IsActive())
		return;
	if (HasEntityAggr())
	{
		InvalidateDraw();
		return; // cannot go back from External Key to featureID's 
	}

	ThemeReadLocks readLocks;
	SuspendTrigger::FencedBlocker blockSuspend;
	if (!readLocks.push_back( GetTheme(AN_Feature).get(), DrlType::Certain))
	{
		readLocks.ProcessFailOrSuspend(this);
		return;
	}

	if (IsDefined(oldSelectedID)) _InvalidateFeature(Entity2FeatureIndex(oldSelectedID));
	if (IsDefined(newSelectedID)) _InvalidateFeature(Entity2FeatureIndex(newSelectedID));
}

bool GraphicLayer::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_CutSel:
		case TB_CopySel:
		case TB_PasteSel:
		case TB_PasteSelDirect:
		case TB_DeleteSel:
			throwErrorD("EditLayer", "This tool is only available for GridLayers");
	}
	return base_type::OnCommand(id);
}

const AbstrUnit* GraphicLayer::GetWorldCrdUnit() const
{
	return GetWorldCrdUnitFromGeoUnit( GetGeoCrdUnit() );
}


IMPL_ABSTR_CLASS(GraphicLayer)


