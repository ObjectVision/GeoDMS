// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "GraphicObject.h"

#include "act/MainThread.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "utl/mySPrintF.h"
#include "mci/Class.h"
#include "dbg/DmsCatch.h"
#include "LockLevels.h"

#include "DataStoreManagerCaller.h"
#include "PropFuncs.h"
#include "StateChangeNotification.h"

#include "DataView.h"
#include "DcHandle.h"
#include "GraphVisitor.h"
#include "LayerClass.h"
#include "MouseEventDispatcher.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : GraphicObject
//----------------------------------------------------------------------

GraphicObject::GraphicObject(GraphicObject* owner)
{
	m_State.Set(GOF_IsVisible|GOF_AllVisible);
	if (owner)
		SetOwner(owner);
}

void GraphicObject::SetDisconnected()
{
	ClearContext();
	SetIsVisible(false); 
	m_Owner.reset();
}

void GraphicObject::ClearContext()
{
	TreeItem* context = GetContext();
	if (context)
	{
		context->SetIsHidden(true);
		context->EnableAutoDelete();
	}
	m_ViewContext = nullptr;
}

void GraphicObject::ClearAllUpdated()
{ 
	std::shared_ptr<GraphicObject> parent = weak_from_this().lock();
	while (parent && parent->AllUpdated())
	{
		parent->m_State.Clear(GOF_AllUpdated);
		if (!parent->IsVisible())
			return;
		parent = parent->GetOwner().lock();
	}
}

void GraphicObject::SetAllUpdated()
{ 
#if defined(MG_DEBUG)
	dms_assert(AllVisible());
	dms_assert(IsUpdated() || WasFailed(FailType::Data));
	SizeT n = NrEntries();
	while (n)
	{
		GraphicObject* child = GetEntry(--n);
		dms_assert(child->AllUpdated() || !child->IsVisible() || child->WasFailed());
	}
#endif
	m_State.Set(GOF_AllUpdated);
}

#if defined(MG_DEBUG)

void GraphicObject::CheckState() const
{
	auto owner = GetOwner().lock();
	bool allVisible = 
		m_State.Get(GOF_IsVisible)        //    IsVisible
	&&	(!owner || owner->AllVisible());  // && m_Owner->AllVisible

	assert(m_State.Get(GOF_AllVisible) == allVisible);
	assert(allVisible || !IsDrawn());
	assert( m_State.Get(GOF_IsUpdated) || WasFailed(FailType::Data) || !m_State.Get(GOF_AllUpdated));

	if (!IsDrawn())
		return;
	if (!owner)
		return;
	assert(owner->IsDrawn());

	auto drawnNettOwnerAbsDeviceRect = owner->GetDrawnNettAbsDeviceRect(); //drawnNettOwnerAbsDeviceRect.Expand(1);
	auto drawnFullAbsDeviceRect = GetDrawnFullAbsDeviceRect();

	assert( IsIncluding(Inflate(drawnNettOwnerAbsDeviceRect, DPoint(0.5, 0.5)), drawnFullAbsDeviceRect) );
}

void GraphicObject::CheckSubStates() const
{
	SizeT n = NrEntries();
	while (n)
		GetConstEntry(--n)->CheckState();
	CheckState();
}

#endif

//	define new virtuals for accessing sub-entries (composition pattern)
gr_elem_index GraphicObject::NrEntries() const
{
	return 0;
}

GraphicObject* GraphicObject::GetEntry(SizeT i)       
{
	throwIllegalAbstract(MG_POS, this, "GetEntry");
}

SharedStr GraphicObject::GetCaption() const
{
	return SharedStr( GetDynamicClass()->GetName() );
}

//	define new virtuals for invitation of GraphicObjects (visitor pattern)

GraphVisitState GraphicObject::InviteGraphVistor(AbstrVisitor& v)
{
	return v.DoObject(this);
}

void GraphicObject::SetIsVisible(bool value)
{
	if (value == m_State.Get(GOF_IsVisible))
		return;
	
	if (value)
	{
		auto owner = GetOwner().lock();
		if (!AllUpdated() && owner)
			owner->ClearAllUpdated();
	}
	else
		InvalidateDraw();

	m_State.Set(GOF_IsVisible, value);
	UpdateAllVisible();
	if (GetContext())
		SaveValue<Bool>(GetContext(), GetTokenID_mt("Visible"), value);
}

void GraphicObject::ToggleVisibility()
{
	MG_DEBUGCODE( CheckState(); )

	// enforce AllVisible if Visibility goes on
	if (AllVisible())
		SetIsVisible(false);
	else
		MakeAllVisible();
		
	if (MustPushVisibility() && IsVisible())
	{
		dms_assert(AllVisible());
		// make all layers visible as well (recursively)
		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* entry = GetEntry(--n);
			if (!entry->IsVisible())
				entry->ToggleVisibility();				
		}
	}
	assert(AllVisible() == IsVisible());
}

void GraphicObject::MakeAllVisible()
{
	MG_DEBUGCODE( CheckState(); )
	if (AllVisible())
		return;
	auto owner = GetOwner().lock();
	if (owner)
	{
		owner->MakeAllVisible();
		MG_DEBUGCODE( CheckState(); )
		if (AllVisible()) 
			return;
	}
	assert(!IsVisible());
	
	if (MustPushVisibility())
	{
		// hide all layers to enable only the activated child (layer) to become visible
		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* entry = GetEntry(--n);
			dms_assert(entry);
			dms_assert(!entry->m_State.Get(GOF_AllVisible) ); // this wasn;t visible.
			entry->SetIsVisible(false);                   // and now it still isn't
		}
	}
	SetIsVisible(true);
}

void GraphicObject::OnVisibilityChanged()
{
//	OnSizeChanged();
}

void GraphicObject::OnSizeChanged()
{
	DBG_START("GraphObject", "OnSizeChanged", MG_DEBUG_VIEWINVALIDATE);
	DBG_TRACE(("%s %x: %x", GetDynamicClass()->GetName().c_str(), this, &*(GetOwner().lock())));

	assert(!m_State.HasInvalidationBlock());

	auto owner = GetOwner().lock();
	if (owner)
		owner->OnChildSizeChanged();
}

void GraphicObject::OnChildSizeChanged()
{
	OnSizeChanged();
}


#include "OperationContext.h"

typedef std::pair<GraphicObject*, SharedPtr<const TreeItem>> UpdateActionType;

static std::set<UpdateActionType> s_UpdateActionSet;
leveled_critical_section sm_UAS(item_level_type(0), ord_level_type::UpdateActionSet, "UpdateActionSet");

struct PairRemover
{
	PairRemover(std::set<UpdateActionType>::iterator pos): m_Pos(pos) {}
	PairRemover() = delete;
	PairRemover(PairRemover&&) = delete;
	PairRemover(const PairRemover&) = delete;
	void operator =(const PairRemover&) = delete;
	void operator =(PairRemover&&) = delete;

	~PairRemover()
	{ 
		leveled_critical_section::scoped_lock lock(sm_UAS);
		s_UpdateActionSet.erase(m_Pos);
	}
	std::set<UpdateActionType>::iterator m_Pos;
};

auto RegisterNew(GraphicObject* obj, const TreeItem* item) -> std::shared_ptr<PairRemover>
{
	UpdateActionType itemPair(obj, item);

	leveled_critical_section::scoped_lock lock(sm_UAS);
	auto pos = s_UpdateActionSet.lower_bound(itemPair);
	if (pos != s_UpdateActionSet.end() && *pos == itemPair)
		return {};

	pos = s_UpdateActionSet.insert(pos, itemPair);
	return std::make_shared<PairRemover>(pos);
}


bool GraphicObject::PrepareDataOrUpdateViewLater(const TreeItem* item)
{
	assert(IsMainThread());
	assert(item);
//	dms_assert(IsDataItem(item));

	item->UpdateMetaInfo();
	assert(item->GetInterestCount());

	if (IsDataReady(item->GetCurrRangeItem()))
		return true;

	SuspendTrigger::FencedBlocker lockSuspend("@GraphicObject::PrepareDataOrUpdateViewLater");
	SharedTreeItemInterestPtr itemHolder(item);
	assert(item->HasInterest());

	if (!item->SuspendibleUpdate(ProgressState::Committed))
		return false;

	if (!itemHolder->PrepareDataUsageImpl(DrlType::Suspendible))
		return false;

	if (!IsMultiThreaded2())
		return true;

	auto pairRemover = RegisterNew(this, itemHolder);
	if (!pairRemover && !ViewPort::g_CurrZoom)
		return false;

	std::weak_ptr<GraphicObject> objWPtr = this->shared_from_this();
	TimeStamp tsActive = UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("Obtaining active frame for UpdateViewLater"));
	std::weak_ptr<ViewPort> vpZoom;
	if (ViewPort::g_CurrZoom)
		vpZoom = ViewPort::g_CurrZoom->weak_from_base<ViewPort>();

	auto prepareDataTask = [itemHolder, objWPtr, pairRemover, tsActive, vpZoom]() {
		try {
			auto objSPtr = objWPtr.lock(); if (!objSPtr) return;

			UpdateMarker::ChangeSourceLock tsLock(tsActive, "UpdateViewLater.impl");

			auto iReadLock = std::make_shared<ItemReadLock>(itemHolder.get_ptr()->GetCurrRangeItem()); // TODO, avoid heap alloc by making ItemReadLock const copyable
			auto dReadLock = std::make_shared<DataReadLock>(AsDynamicDataItem(itemHolder.get_ptr()));
			auto dv_sptr = objSPtr->GetDataView().lock();
			if (dv_sptr)
				dv_sptr->PostGuiOper([objWPtr, itemHolder, iReadLock, dReadLock, pairRemover, vpZoom]() {
					auto objSPtr = objWPtr.lock();
					if (objSPtr) {
						objSPtr->InvalidateView();
						objSPtr->UpdateView();
						objSPtr->InvalidateDraw();
					}

					auto vpZoomSPtr = vpZoom.lock();
					if (vpZoomSPtr)
						vpZoomSPtr->AL_ZoomAll();
				});
		}
		catch (...) {} // let it go, it's just GUI.
	};

	dms_task updater = dms_task(prepareDataTask); // XXX, TODO: WaitForReadyOrSuspend on itemHolder. XXX how to deal with suspend or cancel here ?
	return false;
}

void GraphicObject::UpdateView() const
{
	dms_assert(IsMainThread());
	dbg_assert(!SuspendTrigger::DidSuspend());
	GetLastChangeTS();

	if (IsUpdated()) // || WasFailed(FailType::Data))
		return;

	auto dv = GetDataView().lock();
	try {
		dbg_assert( !dv || !dv->md_InvalidateDrawLock );

		const_cast<GraphicObject*>(this)->DoUpdateView();
		m_State.Set(GOF_IsUpdated);
	}
	catch(...)
	{
		auto err = catchException(true);
		DoFail(err, FailType::Data);
	}
}

void GraphicObject::InvalidateView()
{
	ClearAllUpdated();
	m_State.Clear(GOF_IsUpdated);
}

void GraphicObject::InvalidateViews()
{
	InvalidateView();
	for (SizeT i = NrEntries(); i; )
		GetEntry(--i)->InvalidateViews();
}

void GraphicObject::DoUpdateView()
{
}

void GraphicObject::ClearDrawFlag()
{
	if (!IsDrawn())
		return;

//	dms_assert(AllVisible());

	SizeT n = NrEntries();
	while (n)
	{
		GraphicObject* entry = GetEntry(--n);
		dms_assert(entry);
		entry->ClearDrawFlag();
	}

	m_State.Clear(GOF_IsDrawn); 
}

void GraphicObject::SetIsDrawn()
{
//	dms_assert(IsUpdated()); 
	dms_assert(AllVisible());
	m_State.Set(GOF_IsDrawn); 
#if defined(MG_DEBUG)
	auto owner = GetOwner().lock();
	dms_assert(!owner || owner->IsDrawn());
#endif
}

CrdRect GraphicObject::GetDrawnFullAbsDeviceRect() const
{
	assert(IsDrawn());
	return m_DrawnFullAbsRect;
}

CrdRect GraphicObject::GetClippedCurrFullAbsDeviceRect(const GraphVisitor& v) const
{
	return GetCurrFullAbsDeviceRect(v) & GRect2CrdRect(v.GetAbsClipDeviceRect());
}

void GraphicObject::InvalidateDraw()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	dbg_assert(!dv->md_InvalidateDrawLock );

	if (!IsDrawn())
		return;

	ClearDrawFlag();

	dv->InvalidateDeviceRect( CrdRect2GRect( m_DrawnFullAbsRect) );
}

void GraphicObject::TranslateDrawnRect(CrdRect clipRect, GPoint delta)
{
	assert(IsDrawn());    // callers responsibility
	assert(AllVisible()); // invariant consequence of IsDrawn
	assert(!m_DrawnFullAbsRect.empty()); 
	assert(IsIncluding(clipRect, m_DrawnFullAbsRect));

	m_DrawnFullAbsRect += GPoint2CrdPoint(delta);
	m_DrawnFullAbsRect &= clipRect;

	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		auto owner = GetOwner().lock();
		dms_assert(!owner || IsIncluding(Inflate(owner->GetDrawnFullAbsDeviceRect(), DPoint(0.5,0.5)), GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation restored ater this TranslateDrawnRect of children
		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* subObj = GetEntry(--n);
			if (subObj->IsDrawn())
				subObj->TranslateDrawnRect(clipRect, delta);
		}
	}
}

void GraphicObject::ClipDrawnRect(CrdRect clipRect)
{
	assert(IsDrawn());    // callers responsibility
	assert(!m_DrawnFullAbsRect.empty() || !HasDefinedExtent()); 

	if (IsIncluding(clipRect, m_DrawnFullAbsRect))
		return;

	m_DrawnFullAbsRect &= clipRect;

	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		auto owner = GetOwner().lock();
		dms_assert(!owner || IsIncluding(owner->GetDrawnFullAbsDeviceRect(), GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation restored ater this clipping
		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* subObj = GetEntry(--n);
			if (subObj->IsDrawn())
				subObj->ClipDrawnRect(m_DrawnFullAbsRect);
		}
	}
	dms_assert(AllVisible()); // invariant consequence of IsDrawn, also checks Draw clipping inclusion relation
}

void Resize_impl(CrdType& drawRectPos, long delta, long invariantLimit)
{
	if (drawRectPos >= invariantLimit) 
		drawRectPos += delta; 
	else if (delta < 0) 
		MakeMin(drawRectPos, invariantLimit + delta);
}

void Resize(CrdRect& drawRect, GPoint delta, GPoint invariantLimit)
{
	Resize_impl(drawRect.first.X(), delta.x, invariantLimit.x); // was >
	Resize_impl(drawRect.first.Y(), delta.y, invariantLimit.y); // was >
	Resize_impl(drawRect.second.X(), delta.x, invariantLimit.x); // was >=
	Resize_impl(drawRect.second.Y(), delta.y, invariantLimit.y); // was >=
}

void GraphicObject::ResizeDrawnRect(CrdRect clipRect, GPoint delta, GPoint invariantLimit)
{
	assert(IsDrawn()); // callers responsibility
	auto owner = GetOwner().lock();
	assert(!owner || owner->IsDrawn());

	assert(AllVisible());                 // invariant consequence of IsDrawn
	assert(!m_DrawnFullAbsRect.empty());  // invariant consequence of IsDrawn
	assert(IsIncluding(clipRect, m_DrawnFullAbsRect)); // callers responsibility to provide GetOwner()->GetDrawnFullRect(), invariant IsIncluding relation

	if (delta == GPoint(0, 0))
		return;

	Resize(m_DrawnFullAbsRect, delta, invariantLimit);

	m_DrawnFullAbsRect &= clipRect;

	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		assert(!owner || IsIncluding(owner->GetDrawnNettAbsDeviceRect(), GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation restored ater this ResizeDrawnRect

		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* subObj = GetEntry(--n);
			if (subObj->IsDrawn())
			{
				if (!IsLowerBound(CrdPoint2GPoint(subObj->m_DrawnFullAbsRect.second), invariantLimit))
				{
//					assert(!IsStrictlyLower(subObj->m_DrawnFullAbsRect.first, GPoint2CrdPoint(invariantLimit)));
					subObj->TranslateDrawnRect(clipRect, delta);
				}
				else
					subObj->ClipDrawnRect( GetDrawnNettAbsDeviceRect() );
				assert(!subObj->IsDrawn() || IsIncluding(GetDrawnNettAbsDeviceRect(), subObj->GetDrawnFullAbsDeviceRect())); // invariant IsIncluding relation restored ater this ResizeDrawnRect
			}
		}
	}
}

std::weak_ptr<DataView> GraphicObject::GetDataView() const
{
	auto owner = GetOwner().lock();
	if (!owner)
		return std::weak_ptr<DataView>();
	return owner->GetDataView();
}

CrdPoint GraphicObject::GetScaleFactors() const
{
	auto dv = GetDataView().lock();
	if (!dv)
		return { 1.0, 1.0 };
	return dv->GetScaleFactors();
}

ToolButtonID GraphicObject::GetControllerID() const
{ 
	auto dv = GetDataView().lock(); 
	return dv ? dv->m_ControllerID : TB_Neutral; 
}

void GraphicObject::DoInvalidate() const
{
	const_cast<GraphicObject*>(this)->InvalidateView();
	Actor::DoInvalidate();
	assert(DoesHaveSupplInterest() || !GetInterestCount());
}

TokenID GraphicObject::GetXmlClassID() const
{
	return GetClsID();
}

void GraphicObject::UpdateAllVisible()
{
	auto owner = GetOwner().lock();
	bool newState = m_State.Get(GOF_IsVisible) && (!owner || owner->AllVisible());
	if (newState == m_State.Get(GOF_AllVisible))
		return;
	m_State.Set(GOF_AllVisible, newState);
	MG_DEBUGCODE( CheckState(); )

	OnVisibilityChanged();

	if (newState && owner)
	{
		if (!AllUpdated() )
			owner->ClearAllUpdated();
	}

	SizeT n = NrEntries();
	while (n)
		GetEntry(--n)->UpdateAllVisible();

	MG_DEBUGCODE( CheckState(); )
}

void GraphicObject::SetActive(bool newState)
{
#if defined(MG_DEBUG)
	if (newState == IsActive())
		reportF(SeverityTypeID::ST_MinorTrace, "SetActive(%d) called on %sactive object", newState, IsActive() ? "" : "non-");
#endif

	m_State.Set(GOF_Active, newState);
}

// ShowSelectedOny stuff

bool GraphicObject::ShowSelectedOnlyEnabled() const
{
	return false;
}

void GraphicObject::UpdateShowSelOnly()
{
}

void GraphicObject::SetShowSelectedOnly(bool on)
{
	if (m_State.Get(GOF_ShowSelectedOnly) == on)
		return;

	m_State.Toggle(GOF_ShowSelectedOnly);

	InvalidateViews();
	InvalidateDraw();
//	OnVisibilityChanged();

	SyncShowSelOnly(SM_Save);
	UpdateShowSelOnly();
}

void GraphicObject::Sync(TreeItem* context, ShvSyncMode sm)
{
	dms_assert(context);
	m_ViewContext = context;

	if (sm == SM_Save && TreeItem_GetDialogType(context) == TokenID::GetEmptyID() && HasShvCreator())
		TreeItem_SetDialogType(context, GetDynamicClass()->GetID());

	SyncState(this, context, GetTokenID_mt("Visible"), GOF_IsVisible, true, sm);
	SyncState(this, context, GetTokenID_mt("Active"), GOF_Active, false, sm);
	if (sm == SM_Load)
	{
		UpdateAllVisible();
		SyncShowSelOnly(sm); // derived sync must call update
	}
}

void GraphicObject::SyncShowSelOnly(ShvSyncMode sm)
{
	SyncState(this, GetContext(), GetTokenID_mt("ShowSelectedOnly"), GOF_ShowSelectedOnly, false, sm);
}


bool GraphicObject::HasShvCreator()
{
	const ShvClass* cls = dynamic_cast<const ShvClass*>(GetDynamicClass());
	return cls && cls->HasShvCreator();
}

//	define new virtuals for size & display of GraphicObjects

bool GraphicObject::Draw(GraphDrawer& d) const 
{
	return false;
}

void GraphicObject::DrawBackground(const GraphDrawer& d) const
{
	if (! MustFill())
		return;

	COLORREF bkColor = GetBkColor();
	if (bkColor == DmsColor2COLORREF(UNDEFINED_VALUE(DmsColor)))
		return;

	CheckColor(bkColor);

	GdiHandle<HBRUSH> br( CreateSolidBrush( bkColor ) );
	FillRgn(d.GetDC(), d.GetAbsClipRegion().GetHandle(), br);
}

COLORREF GraphicObject::GetBkColor() const
{
	return GetSysColor(COLOR_BTNFACE);
}

FontSizeCategory GraphicObject::GetFontSizeCategory() const
{
	auto owner = GetOwner().lock();
	if (owner)
		return owner->GetFontSizeCategory();
	return FontSizeCategory::MEDIUM;
}

bool GraphicObject::MouseEvent(MouseEventDispatcher& med)
{
	auto dv = GetDataView().lock();
	if (!dv)
		return false;


	if (MouseEventFlags(med.GetEventInfo().GetID()) == EventID::MOUSEMOVE)
	{
		auto hoveredObject = dv->m_hoveredObject.lock();
		if (!hoveredObject || hoveredObject.get() != this || dv->m_hoverStartLocation != med.r_EventInfo.m_Point)
		{
			dv->HideActiveTooltip();
			dv->m_hoverStartLocation = med.r_EventInfo.m_Point;
			dv->m_hoveredObject = std::weak_ptr<GraphicObject>(shared_from_this());
			SetTimer(dv->GetHWnd(), HOVER_TIMER_ID, 700, nullptr);
		}
		return true;
	}
	return false;
}

bool GraphicObject::OnKeyDown(UInt32 nVirtKey)
{
	return false;
}

bool GraphicObject::OnCommand(ToolButtonID id)
{
	return false;
}

CommandStatus GraphicObject::OnCommandEnable(ToolButtonID id) const
{
	return CommandStatus::ENABLED;
}

void GraphicObject::FillMenu(MouseEventDispatcher& med)
{
	if (WasFailed())
	{
		auto ft = GetFailType();
		auto fr = GetFailReason(); if (!fr) return;

		SubMenu subMenu(med.m_MenuData, mySSPrintF("%s of %s", FailStateName(ft), GetDynamicClass()->GetName().c_str()));
		SharedStr failReason = fr->GetAsText();
		CharPtr
			bol = failReason.begin(),
			eos = failReason.send();
		SharedTreeItem item;
		if (auto curr = DSM::Curr())
			if (auto cr = curr->GetConfigRoot())
				item = cr->FindBestItem(fr->FullName()).first;
		while (true) {
			CharPtr eol = std::find(bol, eos, '\n');
			auto txt = SharedStr(CharPtrRange(bol, eol));
			if (item)
				med.m_MenuData.push_back( MenuItem(txt, std::make_unique<RequestClientCmd>(item, CC_Activate), this) );
			else
				med.m_MenuData.push_back(MenuItem(txt));
			if (eol == eos)
				break;
			bol = eol+1;
		}
	}
}

void GraphicObject::SetOwner(GraphicObject* owner)
{
	assert(owner);
	auto sharedOwner = owner->shared_from_this();
	if (!sharedOwner)
		return;
	if (m_Owner.lock() == sharedOwner)
		return;

	dms_assert(!m_Owner.lock());
	dms_assert(!IsDrawn());
	MG_DEBUGCODE( CheckState(); )

	if (!AllUpdated())
		sharedOwner->ClearAllUpdated();
	m_Owner = sharedOwner;

	UpdateAllVisible();
	MG_DEBUGCODE( CheckState(); )
}

TreeItem* GraphicObject::GetContext()
{
	return m_ViewContext; 
}

const TreeItem* GraphicObject::GetContext() const
{
	return m_ViewContext;
}

void GraphicObject::ClearOwner()
{
	InvalidateDraw();
	m_Owner.reset();
	UpdateAllVisible();
}

bool GraphicObject::IsOwnerOf(GraphicObject* obj) const
{
	auto ownedObj = obj->shared_from_this();
	while (ownedObj)
	{
		if (ownedObj.get() == this)
			return true;
		ownedObj = ownedObj->GetOwner().lock();
	}
	return false;
}

#include "ViewPort.h"

IMPL_ABSTR_CLASS(GraphicObject)

// =============================================== ToolTip

#include <commctrl.h>

auto GetMovableObject(GraphicObject* obj) -> MovableObject*
{
	if (auto vp = dynamic_cast<MovableObject*>(obj))
		return vp;

	auto owner = obj->GetOwner().lock();
	if (!owner)
		return nullptr;
	return GetMovableObject(owner.get());
}

bool GraphicObject::HitTest(POINT ptClient) const noexcept
{
	if (!IsDrawn())
		return false;
	auto owner = GetOwner().lock();
	if (!owner)
		return false;
	return owner->HitTest(ptClient); // bounding box thing; find a MovableObject for this; TODO: clip also on world coords and bounding box of enclosing ViewPort
}

void GraphicObject::OnHoverTimer()
{
	auto dv = GetDataView().lock();
	if (!dv)
		return;

	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(dv->GetHWnd(), &pt);

	if (HitTest(pt))
		ShowTooltipAt(pt);
}

void GraphicObject::ShowTooltipAt(POINT ptClient)
{
	auto dv = GetDataView().lock();
	if (!dv) 
		return;

	auto vp = GetMovableObject(this);
	if (!vp)
		return;

	HWND hwndTT = dv->EnsureTooltipWindow();
	auto devRect = vp->GetCurrClientAbsDeviceRect();

	dv->m_ToolTipText = Utf8_2_wchar(GetTooltipText(ptClient));
	// Ensure tool exists / update rect + text
	TOOLINFOW ti{};
	ti.cbSize = TTTOOLINFOW_V2_SIZE; // sizeof(ti);
	ti.uFlags = TTF_TRACK | TTF_ABSOLUTE;
	ti.hwnd = dv->GetHWnd();
	ti.uId = ToolId();

	ti.rect = CrdRect2GRect(devRect);

	ti.lpszText = dv->m_ToolTipText.get();

	// Add if missing, else update
	if (!SendMessageW(hwndTT, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti))
	{
		BOOL addOk = SendMessageW(hwndTT, TTM_ADDTOOLW, 0, (LPARAM)&ti);
		assert(addOk);
	}
	else
		SendMessageW(hwndTT, TTM_NEWTOOLRECTW, 0, (LPARAM)&ti);

	// Position near cursor (screen coords)
	POINT ptScreen = ptClient;
	ClientToScreen(dv->GetHWnd(), &ptScreen);
	ptScreen.x += 14;
	ptScreen.y += 20;

	SendMessageW(hwndTT, TTM_TRACKPOSITION, 0, MAKELONG(ptScreen.x, ptScreen.y));
	SendMessageW(hwndTT, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti);

	m_State.Set(GOF_ToolTipVisible);
	dv->SetActiveTooltipObject(this);
}

void GraphicObject::HideTooltipNoWatchdog() noexcept
{
	if (!IsTooltipVisible())
		return;

	auto dv = GetDataView().lock();
	if (!dv)
		return;

	if (!dv->m_hwndTooltip)
		return;

	HWND hwndTT = dv->EnsureTooltipWindow();

	TOOLINFOW ti{};
	ti.cbSize = sizeof(ti);
	ti.hwnd = dv->GetHWnd();
	ti.uId = ToolId();

	SendMessageW(hwndTT, TTM_TRACKACTIVATE, FALSE, (LPARAM)&ti);
	m_State.Clear(GOF_ToolTipVisible);
}

void GraphicObject::ForceHideTooltip() noexcept
{
	HideTooltipNoWatchdog();

	auto dv = GetDataView().lock();
	if (!dv)
		return;
	dv->ClearActiveTooltipObject(this);
}

CharPtr GraphicObject::GetTooltipText(POINT ptClient) const
{
	return GetDynamicClass()->GetName().c_str();
}
