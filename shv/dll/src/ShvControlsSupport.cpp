// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Small view/control satellites of shv, merged (2026-08): ViewControl,
// TableDataView, WmsLegendControl, ActivationInfo, IdleTimer, Cmds, Wrapper.

// ==== ViewControl ====

#include "ViewControl.h"
#include "DataView.h"

#include "mci/Class.h"

#include "KeyFlags.h"

//----------------------------------------------------------------------
// class  : ViewControl
//----------------------------------------------------------------------

ViewControl::ViewControl(DataView* dv)
	:	base_type(0)
	,	m_DataView( dv->weak_from_this() )
{
};

void ViewControl::Init(DataView* dv)
{}

std::weak_ptr<DataView> ViewControl::GetDataView() const
{
	return m_DataView;
}

bool ViewControl::OnCommand(ToolButtonID id)
{
	switch (id)
	{
	case TB_Copy: {
		auto dv = GetDataView().lock();
		if (dv)
			CopyToClipboard(dv.get());
		return true;
	}
	}
	return base_type::OnCommand(id);
}

bool ViewControl::OnKeyDown(UInt32 virtKey)
{
	if ( KeyInfo::IsCtrl(virtKey) )
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'C': return OnCommand(TB_Copy);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

void ViewControl::SetClientSize(CrdPoint newSize)
{
	ProcessSize(LowerBound( newSize, GetCurrClientSize()));
	base_type::SetClientSize(newSize);
	ProcessSize(newSize);
}

IMPL_ABSTR_CLASS(ViewControl)


// ==== TableDataView ====

#include "EditPalette.h"
#include "TableDataView.h"

#include "mci/Class.h"
#include "ScrollPort.h"
#include "TableViewControl.h"

//----------------------------------------------------------------------
// TableDataView Implementation
//----------------------------------------------------------------------

TableDataView::TableDataView(TreeItem* viewContext, ShvSyncMode sm)
:	DataView(viewContext)
{
}


TableViewControl* TableDataView::GetTableViewControl()       
{
	return debug_cast<TableViewControl*>(GetContents().get());
}

const TableViewControl* TableDataView::GetTableViewControl() const
{
	return debug_cast<const TableViewControl*>(GetContents().get());
}

ExportInfo TableDataView::GetExportInfo()
{
	return GetTableControl()->GetExportInfo();
}

TableControl*  TableDataView::GetTableControl()
{
	return GetTableViewControl()->GetTableControl();
}

const TableControl*  TableDataView::GetTableControl() const
{
	return GetTableViewControl()->GetTableControl();
}

bool TableDataView::CanContain(const TreeItem* viewCandidate) const
{
	return GetTableControl()->CanContain(viewCandidate);
}

void TableDataView::AddLayer(const TreeItem* viewItem, bool isDragging)
{
	dms_assert(viewItem);
	if (!CanContain(viewItem))
		viewItem->throwItemError("Cannot be represented in this view");
	if (GetTableControl()->NrEntries() == 0)
	{
		auto domain = SHV_DataContainer_GetDomain(viewItem, 1, HasAdminMode());
		auto firstItem = SHV_DataContainer_GetItem(viewItem, domain, 0, 1, HasAdminMode());
		GetTableControl()->AddIdColumn(domain, firstItem);
	}
	GetTableControl()->AddLayer(viewItem, isDragging);
}

IMPL_RTTI_CLASS(TableDataView);


// ==== WmsLegendControl ====

#include "ShvBase.h"

#include "WmsLegendControl.h"
#include "WmsLayer.h"

#include "GraphVisitor.h"
#include "DrawContext.h"
#include "geom/PointOrder.h"

WmsLegendControl::WmsLegendControl(MovableObject* owner, std::shared_ptr<WmsLayer> layer)
	:	base_type(owner)
	,	m_Layer(std::move(layer))
{}

CrdPoint WmsLegendControl::CalcMaxSize() const
{
	// CalcMaxSize is the full (client+border) size; add the border so the *client*
	// area equals the image and nothing gets clipped (#405).
	CrdPoint border = Size(GetBorderLogicalExtents());
	if (m_Layer && m_Layer->EnsureLegendImage())
	{
		WPoint sz = m_Layer->LegendSize();
		if (sz.Col() > 0 && sz.Row() > 0)
			return shp2dms_order(Float64(sz.Col()), Float64(sz.Row())) + border;
	}
	return shp2dms_order(Float64(120), Float64(16)) + border; // "(legend unavailable)" placeholder
}

bool WmsLegendControl::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawBackground())
		return false;

	auto clientAbsRect = ScaleCrdRect(GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos(), d.GetSubPixelFactors());
	auto clientIntRect = CrdRect2GRect(clientAbsRect);
	auto* dc = d.GetDrawContext();
	if (!dc)
		return false;

	dc->FillRect(clientIntRect, CombineRGB(255, 255, 255)); // legends usually assume a white backdrop

	if (m_Layer && m_Layer->EnsureLegendImage() && m_Layer->LegendPixels())
	{
		WPoint sz = m_Layer->LegendSize();
		Int32 w = Int32(sz.Col()), h = Int32(sz.Row());
		// Scale into the full client rect (which CalcMaxSize sized to the image): at
		// 100% DPI this is 1:1, at higher DPI it scales up uniformly. Filling the rect
		// avoids the right-edge clipping seen when blitting at native pixel width.
		dc->DrawImage(clientIntRect, m_Layer->LegendPixels(), w, h, 32, nullptr, 0); // rows pre-flipped to bottom-up in EnsureLegendImage
	}
	else
	{
		CharPtr msg = "(legend unavailable)";
		dc->TextOut(GPoint(clientIntRect.left + 2, clientIntRect.top), msg, StrLen(msg), GraphicObject::GetDefaultTextColor());
	}
	return false;
}


// ==== ActivationInfo ====

#include "ActivationInfo.h"
#include "MovableObject.h"

//----------------------------------------------------------------------
// struct ActivationInfo
//----------------------------------------------------------------------

sharedPtrMovableObject ActivationInfo::ActiveChild()
{
	while ((*this) && !(*this)->AllVisible())
	{
		dms_assert((*this)->IsActive());
		(*this)->SetActive(false);
		sharedPtrMovableObject::operator=(
			(*this)->GetOwner().lock()
		);
	}
	return (*this);
}

bool ActivationInfo::OnKeyDown(UInt32 virtKey)
{
	sharedPtrMovableObject curr = ActiveChild();
	while (curr)
	{
		if (curr->OnKeyDown(virtKey))
			return true;
		curr = curr->GetOwner().lock();
	}
	return false;
}



// ==== IdleTimer ====

#include "IdleTimer.h"
#include "DataView.h"

//----------------------------------------------------------------------
// struct IdleTimer implementation
//----------------------------------------------------------------------

static UInt32   s_IdleTimerCount    = 0;
static std::vector<std::weak_ptr<DataView>> s_DataViewArray;


IdleTimer::IdleTimer()
{
	assert(IsMainThread());
	if (!s_IdleTimerCount++)
	{
		assert(s_DataViewArray.empty());
	}
}

IdleTimer::~IdleTimer()
{
	assert(IsMainThread());
	if (--s_IdleTimerCount)
		return;

	for (auto wpdv : s_DataViewArray)
	{
		if (auto spdv = wpdv.lock())
			spdv->SetUpdateTimer();
	}
	s_DataViewArray.clear();
}

bool IdleTimer::IsInIdleMode() // is any IdleTimer active?
{
	assert(IsMainThread());
	return s_IdleTimerCount;
}

void IdleTimer::Subscribe  (std::weak_ptr<DataView> dv)
{
	assert(IsMainThread());
	s_DataViewArray.emplace_back(dv);
}


// ==== Cmds ====

#include "Cmds.h"
#include "ViewPort.h"
#include "GridLayer.h"

//----------------------------------------------------------------------
// class  : CmdSelectDistrict
//----------------------------------------------------------------------

GraphVisitState CmdSelectDistrict::DoGridLayer(GridLayer* dgl)
{
	dms_assert(dgl);

	dgl->SelectDistrict(m_WorldPoint, EventID(0) );

	return GVS_Handled;
}



// ==== Wrapper ====

#include "Wrapper.h"

#include "dbg/DebugContext.h"
#include "mci/Class.h"

#include "TreeItem.h"

#include "ShvUtils.h"
#include "DataView.h"

//----------------------------------------------------------------------
// class  : Wrapper
//----------------------------------------------------------------------

Wrapper::Wrapper(MovableObject* owner, DataView* dv, CharPtr caption)
	:	base_type(owner)
	,	m_DataView(dv ? dv->shared_from_this() : nullptr)
	,	m_Caption(caption)
{
}

void Wrapper::SetContents(sharedPtrGO contents)
{
	m_Contents = contents;
}

void Wrapper::ClearContents()
{
	m_Contents = nullptr;
}

//	overrid GraphicObject interface for composition of GraphicObjects (composition pattern)
gr_elem_index Wrapper::NrEntries() const
{
	return 1; 
}

GraphicObject* Wrapper::GetEntry(SizeT i)       
{
	dms_assert(m_Contents);
	return m_Contents.get(); 
}

SharedStr Wrapper::GetCaption() const
{
	return m_Caption; 
}

bool Wrapper::OnCommand(ToolButtonID id)
{
	switch (id)
	{
		case TB_Export: 
			Export(); 
			return true;
	}
	return base_type::OnCommand(id);
}

static StaticLateTokenID s_ContentsTokenID("Contents");

void Wrapper::Sync(TreeItem* context, ShvSyncMode sm) 
{
	ObjectContextHandle contextHandle(context, "Wrapper::Sync");


	base_type::Sync(context, sm);
	const TreeItem* contents = FindTreeItemByID(context, s_ContentsTokenID);
	if (!contents)
		contents= context->CreateItem(s_ContentsTokenID).get();
	m_Contents->Sync(const_cast<TreeItem*>(contents), sm);
}

#if defined(MG_DEBUG)

bool Wrapper::DataViewOK() const 
{
	auto dv = GetDataView().lock();
	return dv && dv->GetViewHost();
}

#endif

IMPL_ABSTR_CLASS(Wrapper)

