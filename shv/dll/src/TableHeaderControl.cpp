// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"


#include "TableHeaderControl.h"

#include "mci/Class.h"

#include "DataItemColumn.h"
#include "DataView.h"
#include "KeyFlags.h"
#include "MouseEventDispatcher.h"
#include "TableControl.h"
#include "TextControl.h"

//----------------------------------------------------------------------
// class  : ColumnHeaderDragger interface
//----------------------------------------------------------------------

#include "Carets.h"
#include "Controllers.h"
#include "CaretOperators.h"
#include "geo/PointOrder.h"
class ColumnHeaderControl;

class ColumnHeaderDragger : public DualPointCaretController
{
	typedef DualPointCaretController base_type;
public:
	ColumnHeaderDragger(DataView* owner, ColumnHeaderControl* target, GPoint origin);

protected:
	bool Move(EventInfo& eventInfo) override;
	bool Exec(EventInfo& eventInfo) override;

private:
	std::shared_ptr<ColumnHeaderControl> m_HooverObj;
	GRect                          m_HooverRect;
	bool                           m_Activated;
	bool                           m_Before = false;
};

//----------------------------------------------------------------------
// class  : ColumnHeaderControl interface
//----------------------------------------------------------------------

class ColumnHeaderControl : public TextControl
{
	typedef TextControl base_type;
public:
	ColumnHeaderControl(MovableObject* owner) : TextControl(owner) {}

	UInt32 ColumnNr() const { return m_Dic->ColumnNr(); }

	void SetDic(std::shared_ptr<DataItemColumn> dic)
	{
		m_Dic = dic;
		auto aa = m_Dic->GetActiveAttr();
		if (aa)
			SetText(SharedStr(aa->GetID()));
		else
			SetText(SharedStr("dummyColumn"));
	}
	bool OnKeyDown(UInt32 virtKey) override
	{
		if (!KeyInfo::IsCtrl(virtKey))
			return false;
		switch (KeyInfo::CharOf(virtKey)) {
		case 'F': break;
		case 'G': break;
		default: return false;
		}

		auto dic = GetDic(); if (!dic) return false;
		return dic->OnKeyDown(virtKey);
	}
	std::shared_ptr<DataItemColumn> GetDic() const { return m_Dic;  }

protected:
//	override AbstrTextEditControl callbacks
	bool      MouseEvent(MouseEventDispatcher& med)      override;
	void      FillMenu(MouseEventDispatcher& med)        override;

private:
	std::shared_ptr<DataItemColumn> m_Dic;
};

//----------------------------------------------------------------------
// class  : ColumnHeaderDragger implementation
//----------------------------------------------------------------------

ColumnHeaderDragger::ColumnHeaderDragger(DataView* owner, ColumnHeaderControl* target, GPoint origin)
	:	DualPointCaretController(owner, new RectCaret, target, origin
		,	EventID::MOUSEDRAG|EventID::LBUTTONUP, EventID::LBUTTONUP, EventID::CLOSE_EVENTS, ToolButtonID::TB_Undefined)
	,	m_HooverRect( owner->ViewDeviceRect() )
	,	m_Activated(false)
{}

bool ColumnHeaderDragger::Move(EventInfo& eventInfo)
{
	auto dv = GetOwner().lock(); if (!dv) return true;
	auto to = GetTargetObject().lock(); if (!to) return true;
	dms_assert(m_Caret);
	std::shared_ptr<MovableObject> hooverObj = GraphObjLocator::Locate(dv.get(), eventInfo.m_Point)->shared_from_this();
	while	(	hooverObj 
			&&	(	!dynamic_cast<ColumnHeaderControl*>(hooverObj.get())
				||	to->IsOwnerOf(hooverObj->GetOwner().lock().get())
				)
			)
		hooverObj = hooverObj->GetOwner().lock();

	if	(hooverObj != m_HooverObj)
	{
		m_Activated = true;

		m_HooverObj = std::static_pointer_cast<ColumnHeaderControl>(hooverObj);

		if (m_HooverObj && m_HooverObj != to)
		{
			m_Before = (m_HooverObj->ColumnNr() < debug_cast<ColumnHeaderControl*>(to.get())->ColumnNr());
			m_HooverRect = CrdRect2GRect( m_HooverObj->GetCurrFullAbsDeviceRect() );
//				m_HooverRect.second.Row() = m_HooverRect.first.Row() +  6;
			if (m_Before)
				MakeMin(m_HooverRect.right, m_HooverRect.left  + 12);
			else
				MakeMax(m_HooverRect.left,  m_HooverRect.right - 12);
		}
		else 
			m_HooverRect = GRect(0, 0, 0, 0);

		dv->MoveCaret(m_Caret, DualPointCaretOperator(m_HooverRect.RightBottom(), m_HooverRect.RightBottom(), m_HooverObj.get()));
	}
	return true;
}

bool ColumnHeaderDragger::Exec(EventInfo& eventInfo)
{
	if (!m_Activated || m_HooverObj == GetTargetObject().lock())
		return false;

	//	insert m_TargetObj as last of the main LayerControlSet
	auto srcLayer = debug_cast<ColumnHeaderControl*>(GetTargetObject().lock().get())->GetDic();
	auto srcOwner = srcLayer->GetOwner().lock();

	if (m_HooverObj && srcOwner)
	{
		DataItemColumn* dstLayer = m_HooverObj->GetDic().get();
		TableControl*   dstOwner = dstLayer->GetTableControl().lock().get();
		dms_assert(!srcLayer->IsOwnerOf(dstOwner));
		debug_cast<TableControl*>(srcOwner.get())->MoveEntry(srcLayer.get(), dstOwner, dstOwner->GetEntryPos(dstLayer) + (m_Before ? 0 : 1));
	}
	return true;
}

//----------------------------------------------------------------------
// class  : ColumnHeaderControl implementation
//----------------------------------------------------------------------

bool ColumnHeaderControl::MouseEvent(MouseEventDispatcher& med)
{
	auto medOwner = med.GetOwner().lock(); if (!medOwner) return true;

	if ((med.GetEventInfo().m_EventID & EventID::SETCURSOR ))
	{
		if (GetControlDeviceRegion(med.GetEventInfo().m_Point.x) != RG_MIDDLE )
		{
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			return true;
		}
	}

	if ((med.GetEventInfo().m_EventID & EventID::LBUTTONDOWN) && m_Dic)
	{
		GPoint mousePoint = med.GetEventInfo().m_Point;
		if (GetControlDeviceRegion(mousePoint.x) != RG_MIDDLE)
			return m_Dic->MouseEvent(med);

		//	Controls for columnn ordering
		m_Dic->SelectCol();
		auto owner = GetOwner().lock(); if (!owner) return true;
		medOwner->InsertController(
			new TieCursorController(medOwner.get(), owner.get()
			,	CrdRect2GRect( owner->GetCurrClientAbsDeviceRect() )
			,	EventID::MOUSEDRAG, EventID::CLOSE_EVENTS
			)
		);

		medOwner->InsertController(
			new DualPointCaretController(medOwner.get(), new BoundaryCaret(this)
			,	this, mousePoint
			,	EventID::MOUSEDRAG, EventID::NONE, EventID::CLOSE_EVENTS, ToolButtonID::TB_Undefined
			)
		);
		medOwner->InsertController(
			new DualPointCaretController(medOwner.get(), new BoundaryCaret(m_Dic.get())
			,	this, mousePoint
			,	EventID::MOUSEDRAG, EventID::NONE, EventID::CLOSE_EVENTS, ToolButtonID::TB_Undefined
			)
		);
		medOwner->InsertController(
			new ColumnHeaderDragger(medOwner.get(), this, mousePoint)
		);
	}

	return base_type::MouseEvent(med);
}

void ColumnHeaderControl::FillMenu(MouseEventDispatcher& med)
{
	if (m_Dic)
		m_Dic->SelectCol();
	base_type::FillMenu(med);
	if (m_Dic)
		m_Dic->FillMenu(med);
}

//----------------------------------------------------------------------
// class  : TableHeaderControl implementation
//----------------------------------------------------------------------

TableHeaderControl::TableHeaderControl(MovableObject* owner, TableControl* tableControl)
	:	base_type(owner, tableControl->m_Orientation)
	,	m_TableControl(tableControl)
	,	m_connElemSetChanged(tableControl->m_cmdElemSetChanged.connect([this]() { this->InvalidateView(); } ))
{
	assert(tableControl);
	assert(owner);
	auto dv = owner->GetDataView().lock(); assert(dv);
	SetMaxSize((IsColOriented() ? DEF_TEXT_PIX_HEIGHT : DEF_TEXT_PIX_WIDTH) + DOUBLE_BORDERSIZE);
}

void TableHeaderControl::DoUpdateView()
{
	auto dv = GetDataView().lock(); if (!dv) return;

	SetSepSize(m_TableControl->m_SepSize);

	bool isGroupedBy = m_TableControl->m_GroupByEntity.get_ptr();

	gr_elem_index
		n = m_TableControl->NrEntries(),
		m = NrEntries();

	MovableObject* activeDic = nullptr;
	if (IsActive())
		activeDic = m_TableControl->GetActiveDIC();

	bool isColOriented = m_TableControl->IsColOriented();

	for (gr_elem_index i=0; i!=n; ++i)
	{
		DataItemColumn* dic = m_TableControl->GetColumn(i);
		dms_assert(dic);

		std::shared_ptr<ColumnHeaderControl> columnHeader;
		if (i < m)
			columnHeader = GetEntry(i)->shared_from_base<ColumnHeaderControl>();
		else
		{
			// copy immutable properties only once
			columnHeader = std::make_shared<ColumnHeaderControl>(this);
			InsertEntry(columnHeader.get());
			columnHeader->SetBorder(true); 
		}
		
		if (dic != columnHeader->GetDic().get())
		{
//			dms_assert(!dic->GetActiveAttr() || !dic->GetActiveAttr()->IsCacheItem()); // requirement for ChangeName of columnHeader
			columnHeader->SetDic( dic->shared_from_base<DataItemColumn>() );
		}
		columnHeader->SetText(dic->Caption());
		auto headerWidth = dic->CalcClientSize().FlippableX(isColOriented) 
			+ Size(dic->GetBorderLogicalExtents()).FlippableX(isColOriented) 
			- Size(columnHeader->GetBorderLogicalExtents()).FlippableX(isColOriented);
		auto headerSize = prj2dms_order<TType>(headerWidth, isColOriented ? DEF_TEXT_PIX_HEIGHT : DEF_TEXT_PIX_WIDTH, isColOriented);
		columnHeader->SetClientSize(headerSize);
		columnHeader->SetIsInverted(m_TableControl->m_Cols.IsInRange(i));
		if (activeDic == dic)
			dv->Activate(columnHeader.get());
	}

	while (n < m)
		RemoveEntry(--m);

	base_type::DoUpdateView();
}

bool TableHeaderControl::MouseEvent(MouseEventDispatcher& med)
{
	if ((med.GetEventInfo().m_EventID & EventID::LBUTTONDOWN)  && med.m_FoundObject.get() ==  this)
	{
		auto sf = med.GetSubPixelFactors();
		// find child that is left of position
		for (UInt32 i=0, n=NrEntries(); i!=n; ++i)
		{
			MovableObject* chc = GetEntry(i);
			if (IsColOriented())
			{
				auto curX = med.GetEventInfo().m_Point.x / sf.first;
				auto x = chc->GetCurrFullAbsLogicalRect().second.X();
				if ((x <= curX) && (curX < x + CrdType(m_SepSize)))
				{
					auto dic = debug_cast<ColumnHeaderControl*>(chc)->GetDic();
					if (dic)
						dic->StartResize(med);
					return true;
				}
			}
			else
			{
				auto curY = med.GetEventInfo().m_Point.y / sf.second;
				auto y = chc->GetCurrFullAbsLogicalRect().second.Y();
				if ((y <= curY) && (curY < y + CrdType(m_SepSize)))
				{
					auto dic = debug_cast<ColumnHeaderControl*>(chc)->GetDic();
					if (dic)
						dic->StartResize(med);
					return true;
				}
			}
		}

	}
	return base_type::MouseEvent(med);
}

bool TableHeaderControl::OnKeyDown(UInt32 virtKey)
{
	return m_TableControl->OnKeyDown(virtKey);
}
