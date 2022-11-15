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
	std::shared_ptr<GraphicObject> parent = shared_from_this();
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
	dms_assert(IsUpdated() || WasFailed(FR_Data));
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

	dms_assert(m_State.Get(GOF_AllVisible) == allVisible);
	dms_assert(allVisible || !IsDrawn());
	dms_assert( m_State.Get(GOF_IsUpdated) || WasFailed(FR_Data) || !m_State.Get(GOF_AllUpdated));
	dms_assert(!IsDrawn() || !owner || owner->IsDrawn());

	dms_assert(!IsDrawn() || !owner || IsIncluding( owner->GetDrawnNettAbsRect(), GetDrawnFullAbsRect() ) );
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
	dms_assert(AllVisible() == IsVisible());
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
	dms_assert(!IsVisible());
	
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

	dms_assert(!m_State.HasInvalidationBlock());

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
	dms_assert(IsMainThread());
	dms_assert(item);
//	dms_assert(IsDataItem(item));

	item->UpdateMetaInfo();
	dms_assert(item->GetInterestCount());

	if (IsDataReady(item->GetCurrRangeItem()))
		return true;

	SuspendTrigger::FencedBlocker lockSuspend;
	SharedTreeItemInterestPtr itemHolder(item);
	dms_assert(item->HasInterest());

	if (!itemHolder->PrepareDataUsage(DrlType::Certain))
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
				dv_sptr->AddGuiOper([objWPtr, itemHolder, iReadLock, dReadLock, pairRemover, vpZoom]() {
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

	if (IsUpdated() || WasFailed(FR_Data))
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
		DoFail(err, FR_Data);
	}
}

void GraphicObject::InvalidateView()
{
	dbg_assert(!m_State.Get(GOFD_BlockInvalidateView));
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

GRect GraphicObject::GetDrawnFullAbsRect() const
{
	dms_assert(IsDrawn());
	return m_DrawnFullAbsRect;
}

GRect GraphicObject::GetClippedCurrFullAbsRect(const GraphVisitor& v) const
{
	return TRect2GRect(GetCurrFullAbsRect(v)) & v.GetAbsClipRect();
}


void GraphicObject::InvalidateDraw()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	dbg_assert(!dv->md_InvalidateDrawLock );

	if (!IsDrawn())
		return;

	ClearDrawFlag();

	dv->InvalidateRect( m_DrawnFullAbsRect );
}

void GraphicObject::TranslateDrawnRect(const GRect& clipRect, const GPoint& delta)
{
	dms_assert(IsDrawn());    // callers responsibility
	dms_assert(AllVisible()); // invariant consequence of IsDrawn
	dms_assert(!m_DrawnFullAbsRect.empty()); 
	dms_assert(IsIncluding(clipRect, m_DrawnFullAbsRect));

	m_DrawnFullAbsRect += delta;
	m_DrawnFullAbsRect &= clipRect;
	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		auto owner = GetOwner().lock();
		dms_assert(!owner || IsIncluding(owner->GetDrawnFullAbsRect(), GetDrawnFullAbsRect())); // invariant IsIncluding relation restored ater this TranslateDrawnRect of children
		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* subObj = GetEntry(--n);
			if (subObj->IsDrawn())
				subObj->TranslateDrawnRect(clipRect, delta);
		}
	}
}

void GraphicObject::ClipDrawnRect(const GRect& clipRect)
{
	dms_assert(IsDrawn());    // callers responsibility
	dms_assert(!m_DrawnFullAbsRect.empty() || !HasDefinedExtent()); 

	if (IsIncluding(clipRect, m_DrawnFullAbsRect))
		return;

	m_DrawnFullAbsRect &= clipRect;
	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		auto owner = GetOwner().lock();
		dms_assert(!owner || IsIncluding(owner->GetDrawnFullAbsRect(), GetDrawnFullAbsRect())); // invariant IsIncluding relation restored ater this clipping
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

void Resize(GRect& drawRect, GPoint delta, GPoint invariantLimit)
{
	if (drawRect.left  >  invariantLimit.x ) drawRect.left  += delta.x; else if (delta.x < 0) MakeMin(drawRect.left,   invariantLimit.x + delta.x);
	if (drawRect.top   >  invariantLimit.y ) drawRect.top   += delta.y; else if (delta.y < 0) MakeMin(drawRect.top,    invariantLimit.y + delta.y);
	if (drawRect.right >= invariantLimit.x ) drawRect.right += delta.x; else if (delta.x < 0) MakeMin(drawRect.right,  invariantLimit.x + delta.x);
	if (drawRect.bottom>= invariantLimit.y ) drawRect.bottom+= delta.y; else if (delta.y < 0) MakeMin(drawRect.bottom, invariantLimit.y + delta.y);
}

void GraphicObject::ResizeDrawnRect(const GRect& clipRect, GPoint delta, GPoint invariantLimit)
{
	dms_assert(IsDrawn()); // callers responsibility
	auto owner = GetOwner().lock();
	dms_assert(!owner || owner->IsDrawn());

	dms_assert(AllVisible());                 // invariant consequence of IsDrawn
	dms_assert(!m_DrawnFullAbsRect.empty());  // invariant consequence of IsDrawn
	dms_assert(IsIncluding(clipRect, m_DrawnFullAbsRect)); // callers responsibility to provide GetOwner()->GetDrawnFullRect(), invariant IsIncluding relation

	if (delta == GPoint(0, 0))
		return;

	Resize(m_DrawnFullAbsRect, delta, invariantLimit);

	m_DrawnFullAbsRect &= clipRect;
	if (m_DrawnFullAbsRect.empty())
		ClearDrawFlag();
	else
	{
		dms_assert(!owner || IsIncluding(owner->GetDrawnNettAbsRect(), GetDrawnFullAbsRect())); // invariant IsIncluding relation restored ater this ResizeDrawnRect

		SizeT n = NrEntries();
		while (n)
		{
			GraphicObject* subObj = GetEntry(--n);
			if (subObj->IsDrawn())
			{
				if (!IsLowerBound(subObj->m_DrawnFullAbsRect.BottomRight(), invariantLimit))
				{
					dms_assert(!IsStrictlyLower(subObj->m_DrawnFullAbsRect.TopLeft(), invariantLimit));
					subObj->TranslateDrawnRect(clipRect, delta);
				}
				else
					subObj->ClipDrawnRect( GetDrawnNettAbsRect() );
				dms_assert(!subObj->IsDrawn() || IsIncluding(GetDrawnNettAbsRect(), subObj->GetDrawnFullAbsRect())); // invariant IsIncluding relation restored ater this ResizeDrawnRect
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

ToolButtonID GraphicObject::GetControllerID() const
{ 
	auto dv = GetDataView().lock(); 
	return dv ? dv->m_ControllerID : TB_Neutral; 
}

void GraphicObject::DoInvalidate() const
{
	const_cast<GraphicObject*>(this)->InvalidateView();
	base_type::DoInvalidate();
	dms_assert(DoesHaveSupplInterest() || !GetInterestCount());
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
		reportF(SeverityTypeID::ST_Warning, "SetActive(%d) called on %sactive object", newState, IsActive() ? "" : "non-");
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
	OnVisibilityChanged();

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

DefaultFontID GraphicObject::GetDefaultFontID() const
{
	auto owner = GetOwner().lock();
	if (owner)
		return owner->GetDefaultFontID();
	return DF_SMALL;
}

bool GraphicObject::MouseEvent(MouseEventDispatcher& med)
{
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
	switch (id)
	{
		case TB_CutSel:
		case TB_CopySel:
		case TB_PasteSelDirect:
		case TB_PasteSel:
		case TB_DeleteSel:
			if (GetUserMode() < UM_Edit) return CommandStatus::HIDDEN;
			break;

		case TB_ZoomSelectedObj:
		case TB_SelectObject:
		case TB_SelectRect:
		case TB_SelectCircle:
		case TB_SelectPolygon:
		case TB_SelectDistrict:
		case TB_SelectRows:
		case TB_SelectAll:
		case TB_SelectNone:
			if (GetUserMode() < UM_Select) return CommandStatus::HIDDEN;
			break;

		case TB_ShowSelOnlyOn:
		case TB_ShowSelOnlyOff:
		{
			if (GetUserMode() < UM_Select) return CommandStatus::HIDDEN;
			bool showSelectedOnly = ShowSelectedOnly();
			return (ShowSelectedOnlyEnabled() || showSelectedOnly)
					?	showSelectedOnly
						? CommandStatus::DOWN
						: CommandStatus::UP
					: CommandStatus::DISABLED;
		}
	}
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
		if (DSM::Curr() && DSM::Curr()->m_ConfigRoot)
			item = DSM::Curr()->m_ConfigRoot->FindBestItem(fr->FullName()).first;
		while (true) {
			CharPtr eol = std::find(bol, eos, '\n');
			auto txt = SharedStr(bol, eol);
			if (item)
				med.m_MenuData.push_back( MenuItem(txt, new RequestClientCmd(item, CC_Activate), this) );
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
	dms_assert(owner);
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
