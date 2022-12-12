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

#include "LayerControl.h"

#include "act/ActorVisitor.h"
#include "mci/Class.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"

#include "AbstrUnit.h"
#include "DisplayValue.h"

#include "StgBase.h"

#include "AbstrCmd.h"
#include "AbstrController.h"
#include "DataView.h"
#include "FocusElemProvider.h"
#include "GraphicLayer.h"
#include "GraphVisitor.h"
#include "LayerSet.h"
#include "MenuData.h"
#include "MouseEventDispatcher.h"
#include "PaletteControl.h"
#include "ShvDllInterface.h"
#include "Theme.h"

//----------------------------------------------------------------------
// class  : LayerHeaderControl
//----------------------------------------------------------------------

LayerHeaderControl::LayerHeaderControl(MovableObject* owner)
	:	TextControl(owner, 2* DEF_TEXT_PIX_WIDTH)
{}

//----------------------------------------------------------------------
// class  : LayerInfoControl
//----------------------------------------------------------------------

LayerInfoControl::LayerInfoControl(MovableObject* owner)
	:	TextControl(owner, 2* DEF_TEXT_PIX_WIDTH)
{}


bool LayerInfoControl::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EID_LBUTTONDBLCLK)
	{
		ExplainValue();
		return GVS_Handled;
	}
	return TextControl::MouseEvent(med);
}

void LayerInfoControl::SetExplainableValue(WeakStr text, SharedPtr<const AbstrDataItem> themeAttr, SizeT focusID)
{
	TextControl::SetText(text);
	m_ThemeAttr = themeAttr;
	m_FocusID = focusID;
}

void LayerInfoControl::ExplainValue()
{
	if (m_ThemeAttr && IsDefined(m_FocusID))
		CreateViewValueAction(m_ThemeAttr, m_FocusID, true);
}

//----------------------------------------------------------------------
// class  : LayerControlBase
//----------------------------------------------------------------------

LayerControlBase::LayerControlBase(MovableObject* owner, ScalableObject* layerSetElem)
	:	base_type(owner)
	,	m_LayerElem   (layerSetElem)
	,	m_FID(FontSizeCategory::SMALL)
	,	m_connDetailsVisibilityChanged(layerSetElem->m_cmdDetailsVisibilityChanged.connect([this]() { this->OnDetailsVisibilityChanged();}))
	,	m_connVisibilityChanged(layerSetElem->m_cmdVisibilityChanged.connect([this]() { this->InvalidateDraw();}))
{
	SetRowSepHeight(0);
	SetBorder(true);
	dms_assert(m_LayerElem);
}

void LayerControlBase::Init()
{
	base_type::Init();
	m_HeaderControl = make_shared_gr<LayerHeaderControl>(this)();
	InsertEntry(m_HeaderControl.get());
}

void LayerControlBase::SetFontSizeCategory(FontSizeCategory fid)
{
	if (m_FID == fid)
		return;
	m_FID = fid;
	m_HeaderControl->SetHeight(GetDefaultFontHeightDIP(fid));
	InvalidateDraw();
}

ScalableObject* LayerControlBase::GetLayerSetElem() const
{
	return m_LayerElem;
}

struct SetFontCmd : public AbstrCmd
{
	SetFontCmd(FontSizeCategory fid) : m_FID(fid) {}

	GraphVisitState DoLayerControlBase(LayerControlBase* lc) override
	{
		dms_assert(lc);
		lc->SetFontSizeCategory(m_FID);
		return GVS_Handled;
	}
private:
	FontSizeCategory m_FID;
};


void FillFontMenu(MenuData& md, LayerControlBase* self)
{
	SubMenu subMenu(md, mySSPrintF("Set %s &Font", self->GetDynamicClass()->GetName().c_str()));
	for (UInt32 i = 0; i != static_cast<int>(FontSizeCategory::COUNT); ++i)
	{
		md.push_back(
			MenuItem(
				SharedStr(GetDefaultFontName(FontSizeCategory(i))),
				new SetFontCmd(FontSizeCategory(i)),
				self,
				(FontSizeCategory(i) == self->GetFontSizeCategory()) ? MFS_CHECKED : 0
			)
		);
	}

}
void LayerControlBase::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	med.m_MenuData.AddSeparator();

	med.m_MenuData.push_back(
		MenuItem(
			mySSPrintF("Hide %s for %s"
			,	GetDynamicClass()->GetName().c_str()
			,	GetCaption()
			)
		,	new MembFuncCmd<GraphicObject>(&GraphicObject::ToggleVisibility)
		,	this
		)
	);

	FillFontMenu(med.m_MenuData, this);

	m_LayerElem->FillLcMenu(med.m_MenuData);
}

#include "Carets.h"
#include "Controllers.h"
#include "CaretOperators.h"
#include "geo/PointOrder.h"

class LayerControlBaseDragger : public DualPointCaretController
{
	typedef DualPointCaretController base_type;
public:
	LayerControlBaseDragger(DataView* owner, LayerControlBase* target, GPoint origin)
		:	DualPointCaretController(owner, new RectCaret, target, origin, EID_MOUSEDRAG|EID_LBUTTONUP, EID_LBUTTONUP, EID_CLOSE_EVENTS)
		,	m_HooverRect( owner->ViewRect() )
	{}
protected:
	bool Move(EventInfo& eventInfo) override
	{
		dms_assert(m_Caret);
		auto dv = GetOwner().lock(); if (!dv) return true;
		auto to = GetTargetObject().lock(); if (!to) return true;
		std::shared_ptr<MovableObject> hooverObj = GraphObjLocator::Locate(dv.get(), eventInfo.m_Point, GetDesktopDIP2pixFactor())->shared_from_this();
		while	(	hooverObj 
				&&	(	!dynamic_cast<LayerControlBase*>(hooverObj.get())
					||	to->IsOwnerOf(hooverObj->GetOwner().lock().get())
					)
				)
			hooverObj = hooverObj->GetOwner().lock();

		if	(hooverObj != m_HooverObj)
		{
			m_Activated = true;

			m_HooverObj = std::static_pointer_cast<LayerControlBase>(hooverObj);

			if (m_HooverObj && m_HooverObj != GetTargetObject().lock())
			{
				m_HooverRect = TRect2GRect( m_HooverObj->GetCurrFullAbsRect() );
				m_Above = (m_HooverRect.top < m_Origin.y);
				if (m_Above)
					MakeMin(m_HooverRect.bottom, TType(m_HooverRect.top    + 6));
				else
					MakeMax(m_HooverRect.top,    TType(m_HooverRect.bottom - 6));
			}
			else 
				m_HooverRect = GRect(0,0,0,0);

			dv->MoveCaret(m_Caret, DualPointCaretOperator(m_HooverRect.TopLeft(), m_HooverRect.BottomRight(), m_HooverObj.get()));
		}
		return true;
	}

	bool Exec(EventInfo& eventInfo) override
	{
		auto dv = GetOwner().lock(); if (!dv) return true;
		auto to = GetTargetObject().lock(); if (!to) return true;
		if (!m_Activated || m_HooverObj == to)
			return false;

		//	insert m_TargetObj as last of the main LayerControlSet
		std::shared_ptr<ScalableObject> srcLayer = debug_cast<LayerControlBase*>(to.get())->GetLayerSetElem()->shared_from_base<ScalableObject>();
		std::shared_ptr<LayerSet>       srcOwner = std::static_pointer_cast<LayerSet>      (srcLayer->GetOwner().lock());

		if (m_HooverObj)
		{
			ScalableObject* dstLayer = m_HooverObj->GetLayerSetElem();
			if (dstLayer == srcLayer.get())
				return false;
			auto dstOwner = std::static_pointer_cast<LayerSet>( dstLayer->GetOwner().lock());
			dms_assert(!srcLayer->IsOwnerOf(dstOwner.get()));
			srcOwner->MoveEntry(srcLayer.get(), dstOwner.get(), dstOwner->GetEntryPos(dstLayer) + (m_Above ? 1 : 0));
		}
		else
		{
			//	insert m_TargetObj as last of the main LayerControlSet
			std::shared_ptr<LayerSet> dstOwner = srcOwner;

			while (true)
			{
				auto owner = std::dynamic_pointer_cast<LayerSet>(dstOwner->GetOwner().lock());
				if (!owner)
					break;
				dstOwner = owner;
			}
			srcOwner->MoveEntry(srcLayer.get(), dstOwner.get(), dstOwner->NrEntries());
		}	
		return true;
	}

private:
	std::shared_ptr<LayerControlBase> m_HooverObj;
	GRect                             m_HooverRect;
	bool                              m_Activated = false;
	bool                              m_Above = false;
};

bool LayerControlBase::MouseEvent(MouseEventDispatcher& med)
{
	if (med.GetEventInfo().m_EventID & EID_LBUTTONDBLCLK)
	{
		m_LayerElem->ToggleVisibility();

		return true; // cancel further processing of this mouse event.
	}
	else if (med.GetEventInfo().m_EventID & EID_LBUTTONDOWN)
	{
		auto medOwner = med.GetOwner().lock();
		medOwner->InsertController(
			new DualPointCaretController(
				medOwner.get(), 
				new BoundaryCaret(this),
				this,
				med.GetEventInfo().m_Point,
				EID_MOUSEDRAG, 0, EID_CLOSE_EVENTS
			)
		);
		medOwner->InsertController(
			new LayerControlBaseDragger(
				medOwner.get(), 
				this,
				med.GetEventInfo().m_Point
			)
		);
		return true;
	}

	return base_type::MouseEvent(med);
}

//	override virtuals of Actor
ActorVisitState LayerControlBase::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (m_LayerElem->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

void LayerControlBase::SetHeaderCaption(CharPtr caption)
{
	m_HeaderControl->SetText(SharedStr(caption));
}

void LayerControlBase::SetActive(bool newState)
{
	if (IsActive() == newState)
		return;

	base_type::SetActive(newState);

	dms_assert(m_LayerElem);
	m_LayerElem->SetActive(newState);

	m_HeaderControl->SetBkColor(
		(newState)
		?	GetSysColor( COLOR_HIGHLIGHT )
		:	DmsColor2COLORREF( UNDEFINED_VALUE(DmsColor) ) // COLOR_BTNFACE
	);

	m_HeaderControl->SetTextColor(
		(newState)
			?	GetSysColor(COLOR_HIGHLIGHTTEXT)
			:	DmsColor2COLORREF( UNDEFINED_VALUE(DmsColor) )
	);
	if (newState)
	{
		auto dv = GetDataView().lock();

	}
}

//----------------------------------------------------------------------
// class  : LayerControl
//----------------------------------------------------------------------

LayerControl::LayerControl(MovableObject* owner, GraphicLayer* layer)
	:	LayerControlBase(owner, layer)
	,	m_Layer(layer->shared_from_base<GraphicLayer>())
	,	m_connFocusElemChanged(layer->m_cmdFocusElemChanged.connect([this](auto _1, auto _2) { this->OnFocusElemChanged(_1, _2);}))
{

	dms_assert(layer);
}

void LayerControl::Init()
{
	base_type::Init();
	m_InfoControl = std::make_shared<LayerInfoControl>(this);
	InsertEntry(m_InfoControl.get());
}

LayerControl::~LayerControl()
{}

COLORREF LayerControl::GetBkColor() const
{
	if (m_Layer->IsFailed())
		return DmsColor2COLORREF( DmsRed );

	if (m_Layer->ShowSelectedOnly())
		return DmsColor2COLORREF( STG_Bmp_GetDefaultColor( true ) );
	return 
		base_type::GetBkColor();
}

void LayerControl::OnDetailsVisibilityChanged()
{
	if (NrEntries() < 3)
		return;

	GraphicObject* palCtrl = GetEntry(2);
	if(	m_Layer->DetailsVisible() != palCtrl->IsVisible()	)
		TogglePaletteIsVisible();
}

void LayerControl::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	if (NrEntries() < 3)
		return;

	med.m_MenuData.push_back(
		MenuItem(SharedStr("Show &Palette"),
			new MembFuncCmd<GraphicLayer>(&GraphicLayer::ToggleDetailsVisibility), 
			m_Layer.get(), 
			GetEntry(2)->IsVisible() ? MFS_CHECKED : 0 
		)
	);

	auto layer = GetLayer();  if (!layer) return;
	auto theme = layer->GetActiveTheme(); if (!theme) return;
	auto attr = theme->GetActiveAttr(); if (!attr) return;

	med.m_MenuData.push_back(
		MenuItem(SharedStr("&Edit Palette"),
			new MembFuncCmd<LayerControl>(&LayerControl::EditPalette), 
			this, 
			0 // GetEntry(2)->IsVisible() ? 0 : MFS_DISABLED
		)
	);
}

void LayerControl::TogglePaletteIsVisible()
{
	dms_assert(NrEntries() == 3);
	GetEntry(2)->SetIsVisible(!GetEntry(2)->IsVisible());
}

GraphVisitState LayerControl::InviteGraphVistor(AbstrVisitor& av)
{
	return av.DoLayerControl(this);
}

void LayerControl::OnFocusElemChanged(SizeT selectedID, SizeT oldSelectedID)
{
//	dms_assert(selectedID == m_Layer->GetFocusElemIndex());, MUTATING
	Invalidate();
}

ActorVisitState LayerControl::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	dms_assert(!SuspendTrigger::DidSuspend()); // precondition
	if (m_Layer && m_Layer->m_FocusElemProvider)
		if (visitor.Visit(m_Layer->m_FocusElemProvider->GetIndexParam()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	auto activeTheme = m_Layer->GetActiveTheme();
	if (activeTheme)
	{
		const AbstrDataItem* themeAttr = activeTheme->GetThemeAttr();
		if (themeAttr)
		{
			if (themeAttr->VisitLabelAttr(visitor, m_LabelLocks.m_DomainLabel)  == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
			if (themeAttr->GetAbstrValuesUnit()->VisitLabelAttr(visitor, m_LabelLocks.m_valuesLabel) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
		else
		{
			SuspendTrigger::SilentBlockerGate allowLabelInterests;

			if (activeTheme->GetThemeEntityUnit()->VisitLabelAttr(visitor, m_LabelLocks.m_DomainLabel) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
	}
	return base_type::VisitSuppliers(svf, visitor);
}


ActorVisitState LayerControl::DoUpdate(ProgressState ps)
{
	SizeT selectedID = m_Layer->IsTopographic()
		?	UNDEFINED_VALUE(SizeT)
		:	m_Layer->GetFocusElemIndex();

	SharedStr text;
	SharedPtr<const AbstrDataItem> themeAttr;
	if (IsDefined(selectedID))
	{
		auto activeTheme = m_Layer->GetActiveTheme();
		if (activeTheme && !activeTheme->IsAspectParameter())
		{
			themeAttr = activeTheme->GetThemeAttr();
			if (themeAttr)
			{
				themeAttr->PrepareDataUsage(DrlType::Suspendible);
				if (SuspendTrigger::DidSuspend())
					return AVS_SuspendedOrFailed;
				GuiReadLockPair locks;
				text = 
					DisplayValue(themeAttr, selectedID, true,
						m_LabelLocks, MAX_TEXTOUT_SIZE, locks
					);
			}
			else
			{
				GuiReadLock lock;
				text =
					DisplayValue(activeTheme->GetThemeEntityUnit(), selectedID, true,
						m_LabelLocks.m_DomainLabel, MAX_TEXTOUT_SIZE, lock
					);
			}
		}
		else
		{
			GuiReadLock lock;
			dms_assert(m_Layer->GetActiveAttr() == m_Layer->GetTheme(AN_Feature)->GetPaletteAttr());
			text =
				DisplayValue(m_Layer->GetTheme(AN_Feature)->GetThemeEntityUnit(), selectedID, true,
					m_LabelLocks.m_DomainLabel, MAX_TEXTOUT_SIZE, lock
				);
		}
	}
	else
		text = m_Layer->GetUnitDisplayName ();

	m_InfoControl->SetExplainableValue(text, themeAttr, selectedID);

/*	NYI, REMOVE COMMENT
	GetHeaderControl()->SetHint(
		m_Layer->IsFailed()
		?	m_Layer->GetFailReason()
		:	Verticalize(ta->GetFullName().c_str())
	);
	m_InfoControl   ->SetHint(
		Verticalize(ta->GetValuesUnit()->GetFullName().c_str())
	);
*/
	return AVS_Ready;
}

void LayerControl::SetPaletteControl(std::shared_ptr<PaletteControl> pc)
{
	if (NrEntries() == 3)
	{
		if (GetEntry(2) == pc.get())
			return;
		RemoveEntry(2);
		m_PaletteControl = nullptr;
	}

	dms_assert(NrEntries() == 2);

	if (!pc)
		return;

	InsertEntry(pc.get() );
	m_PaletteControl = pc;
}

void LayerControl::SetFontSizeCategory(FontSizeCategory fid)
{
	if (GetFontSizeCategory() == fid)
		return;

	base_type::SetFontSizeCategory(fid);

	m_InfoControl->SetHeight(GetDefaultFontHeightDIP(GetFontSizeCategory()));
	m_InfoControl->InvalidateDraw();
	if (m_PaletteControl)
		m_PaletteControl->InvalidateView();
}

void LayerControl::EditPalette()
{
	const AbstrDataItem* themeAttr = GetLayer()->GetActiveTheme()->GetActiveAttr();
	if (themeAttr)
		CreateEditPaletteMdiChild(GetLayer(), themeAttr);
}

const AbstrUnit* LayerControl::GetPaletteDomain() const
{
	auto activeTheme = m_Layer->GetActiveTheme();
	dms_assert(activeTheme);

	const AbstrUnit* paletteDomain = activeTheme->GetPaletteDomain();
	dms_assert(paletteDomain);
	return paletteDomain;
}

void LayerControl::DoUpdateView()
{
	dms_assert(m_Layer);

	SetHeaderCaption(GetThemeDisplayNameInclMetric(m_Layer.get()).c_str());

	SizeT focusElem = m_Layer->GetFocusElemIndex();

	auto activeTheme = m_Layer->GetActiveTheme();
	if (!m_PaletteControl || (activeTheme && (m_PaletteControl->GetEntity() != activeTheme->GetPaletteDomain())))
	{
		if (!activeTheme || PrepareDataOrUpdateViewLater(activeTheme->GetPaletteDomain()))
		{
			SetPaletteControl(make_shared_gr<PaletteControl>(this, m_Layer.get(), true)());
			//	REMOVE	if (m_Layer->DetailsVisible())
			OnDetailsVisibilityChanged(); // process when m_Layer->DetailsVisible() == false
			m_PaletteControl->CalcClientSize();
			MG_DEBUGCODE(m_PaletteControl->CheckState(); )
		}
	}

	base_type::DoUpdateView();
}

IMPL_RTTI_CLASS(LayerControl)


//----------------------------------------------------------------------
// class  : LayerControlSet
//----------------------------------------------------------------------

LayerControlSet::LayerControlSet(MovableObject* owner, LayerSet* layerSet)
	:	base_type(owner)
	,	m_LayerSet(layerSet->shared_from_base<LayerSet>())
	, m_connElemSetChanged(layerSet->m_cmdElemSetChanged.connect([this]() { this->OnElemSetChanged(); }))
{
	dms_assert(layerSet);
	SetRowSepHeight(0);
	dms_assert(MustFill());
//	InvalidateDraw();
}

bool LayerControlSet::HasHiddenControls() const
{
	SizeT i = NrEntries();
	while (i)
		if (!GetConstEntry(--i)->IsVisible())
			return true;
	return false;
}

void LayerControlSet::ShowHiddenControls()
{
	SizeT i = NrEntries();
	while (i)
		GetEntry(--i)->SetIsVisible(true);
}

void LayerControlSet::OnElemSetChanged()
{
	InvalidateView();
	InvalidateDraw();
	if (m_LayerSet->NrEntries() != NrEntries())
		OnSizeChanged();
}

GraphVisitState LayerControlSet::InviteGraphVistor(AbstrVisitor& av)
{
	return av.DoLayerControlSet(this);
}

void LayerControlSet::DoUpdateView()
{
	MG_DEBUGCODE(CheckSubStates(); )

	auto dv = GetDataView().lock(); if (!dv) return;
	gr_elem_index n = m_LayerSet->NrEntries();

	for (gr_elem_index i=0; i!=n; ++i)
	{
		ScalableObject* elem = m_LayerSet->GetEntry(n-i-1); // build up controls for top layers first
		dms_assert(elem);

		std::shared_ptr<LayerControlBase> lc;
		// find layer in m_Array
		array_type::iterator
			lcArrayCurr = m_Array.begin() + i,
			lcArrayPtr  = lcArrayCurr,
			lcArrayEnd  = m_Array.end();
		while (lcArrayPtr != lcArrayEnd)
		{	
			lc = lcArrayPtr->get()->shared_from_base<LayerControlBase>();
			if (lc->GetLayerSetElem() == elem)
			{
				if (lcArrayCurr != lcArrayPtr)
				{
					omni::swap(*lcArrayCurr, *lcArrayPtr);
					ProcessCollectionChange();
				}
				break;
			}
			++lcArrayPtr;
		}
		if (lcArrayPtr == lcArrayEnd)
		{
			// make new LayerControl for elem
			lc = elem->CreateControl(this);
			dms_assert(lc->GetOwner().lock().get() == this);
			lc->SetFontSizeCategory( GetFontSizeCategory() );

			InsertEntryAt(lc.get(), i);
		}
		if (elem->IsActive())
			dv->Activate( lc.get() );
	}

	while (m_Array.size() > n) 
		RemoveEntry(m_Array.size()-1);

	MG_DEBUGCODE( CheckSubStates(); )

	base_type::DoUpdateView(); // order and calculate size
}

void LayerControlSet::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	if (IsVisible() && HasHiddenControls())
		med.m_MenuData.push_back(
			MenuItem(SharedStr("&Show Hidden LayerControls")
			,	new MembFuncCmd<LayerControlSet>(&LayerControlSet::ShowHiddenControls)
			,	this
			)
		);
}

IMPL_RTTI_CLASS(LayerControlSet)

//----------------------------------------------------------------------
// class  : LayerControlGroup
//----------------------------------------------------------------------

LayerControlGroup::LayerControlGroup(MovableObject* owner, LayerSet* layerSet)
	: LayerControlBase(owner, layerSet)
	, m_LayerSet(layerSet->shared_from_base<LayerSet>())
{}

LayerControlGroup::~LayerControlGroup()
{}

void LayerControlGroup::Init(LayerSet* layerSet, CharPtr caption)
{
	dms_assert(layerSet);
	base_type::Init();
	m_Contents = std::make_shared<LayerControlSet>(this, layerSet);
	m_connVisibilityChanged = layerSet->m_cmdVisibilityChanged.connect([this]() { this->GetHeaderControl()->InvalidateDraw(); });

	InsertEntry(m_Contents.get());

	dms_assert(layerSet);
	dms_assert(MustFill());
	SetHeaderCaption(caption);

	OnDetailsVisibilityChanged();
}

GraphVisitState LayerControlGroup::InviteGraphVistor(AbstrVisitor& av)
{
	return av.DoLayerControlGroup(this);
}

void LayerControlGroup::FillMenu(MouseEventDispatcher& med)
{
	base_type::FillMenu(med);

	const LayerControlSet* lcs = GetConstControlSet();
	bool layerControlSetVisible = lcs->IsVisible();
	med.m_MenuData.push_back(
		MenuItem(SharedStr("Show &LayerControls"),
			new MembFuncCmd<LayerSet>(&LayerSet::ToggleDetailsVisibility), 
			m_LayerSet.get(), 
			layerControlSetVisible ? MFS_CHECKED : 0 
		)
	);
}

const LayerControlSet* LayerControlGroup::GetConstControlSet() const
{
	dms_assert(NrEntries() == 2); // Header + LayerControlSet
	return debug_cast<const LayerControlSet*>(GetConstEntry(1));
}

LayerControlSet* LayerControlGroup::GetControlSet()
{
	dms_assert(NrEntries() == 2); // Header + LayerControlSet
	return debug_cast<LayerControlSet*>(GetEntry(1));
}

void LayerControlGroup::SetFontSizeCategory(FontSizeCategory fid)
{
	base_type::SetFontSizeCategory(fid);

	LayerControlSet* contents = m_Contents.get();
	SizeT n = contents->NrEntries();
	for (UInt32 i = 0; i!=n; ++i)
		debug_valcast<LayerControlBase*>(contents->GetEntry(i))->SetFontSizeCategory(fid);
}

void LayerControlGroup::OnDetailsVisibilityChanged()
{
	GraphicObject* layerCtrlSet = GetEntry(1);
	bool willBeVisible = m_LayerSet->DetailsVisible();
	bool wasVisible    = layerCtrlSet->IsVisible();
	if (willBeVisible == wasVisible)
		return;
	layerCtrlSet->SetIsVisible( willBeVisible );
	layerCtrlSet->Invalidate();
}

IMPL_RTTI_CLASS(LayerControlGroup)


