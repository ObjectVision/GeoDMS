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
		,	EID_MOUSEDRAG|EID_LBUTTONUP, EID_LBUTTONUP, EID_CLOSE_EVENTS, ToolButtonID::TB_Undefined)
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
			m_HooverRect = m_HooverObj->GetCurrFullAbsDeviceRect();
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

	if ((med.GetEventInfo().m_EventID & EID_SETCURSOR ))
	{
		if (GetControlDeviceRegion(med.GetEventInfo().m_Point.x) != RG_MIDDLE )
		{
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			return true;
		}
	}

	if ((med.GetEventInfo().m_EventID & EID_LBUTTONDOWN) && m_Dic)
	{
		GPoint mousePoint = med.GetEventInfo().m_Point;
		if (GetControlDeviceRegion(mousePoint.x) != RG_MIDDLE)
			return m_Dic->MouseEvent(med);

		//	Controls for columnn ordering
		m_Dic->SelectCol();
		auto owner = GetOwner().lock(); if (!owner) return true;
		medOwner->InsertController(
			new TieCursorController(medOwner.get(), owner.get()
			,	owner->GetCurrClientAbsDeviceRect()
			,	EID_MOUSEDRAG, EID_CLOSE_EVENTS
			)
		);

		medOwner->InsertController(
			new DualPointCaretController(medOwner.get(), new BoundaryCaret(this)
			,	this, mousePoint
			,	EID_MOUSEDRAG, 0, EID_CLOSE_EVENTS, ToolButtonID::TB_Undefined
			)
		);
		medOwner->InsertController(
			new DualPointCaretController(medOwner.get(), new BoundaryCaret(m_Dic.get())
			,	this, mousePoint
			,	EID_MOUSEDRAG, 0, EID_CLOSE_EVENTS, ToolButtonID::TB_Undefined
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
	:	base_type(owner)
	,	m_TableControl(tableControl)
	, m_connElemSetChanged(tableControl->m_cmdElemSetChanged.connect([this]() { this->InvalidateView(); } ))
{
	assert(tableControl);
	assert(owner);
	auto dv = owner->GetDataView().lock(); assert(dv);
	SetMaxRowHeight((DEF_TEXT_PIX_HEIGHT  + 2* BORDERSIZE));
}

void TableHeaderControl::DoUpdateView()
{
	auto dv = GetDataView().lock(); if (!dv) return;

	SetColSepWidth(m_TableControl->ColSepWidth());

	bool isGroupedBy = m_TableControl->m_GroupByEntity.get_ptr();

	gr_elem_index
		n = m_TableControl->NrEntries(),
		m = NrEntries();

	MovableObject* activeDic = nullptr;
	if (IsActive())
		activeDic = m_TableControl->GetActiveDIC();

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
		auto headerWidth = dic->CalcClientSize().X() + dic->GetBorderLogicalExtents().Width() - columnHeader->GetBorderLogicalExtents().Width();
		auto headerSize = shp2dms_order<TType>(headerWidth, DEF_TEXT_PIX_HEIGHT);
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
	if ((med.GetEventInfo().m_EventID & EID_LBUTTONDOWN)  && med.m_FoundObject.get() ==  this)
	{
		auto sf = med.GetSubPixelFactors();
		TType curX = med.GetEventInfo().m_Point.x / sf.first;
		// find child that is left of position
		for (UInt32 i=0, n=NrEntries(); i!=n; ++i)
		{
			MovableObject* chc = GetEntry(i);
			TType x = chc->GetCurrFullAbsLogicalRect().Right();
			if ((x <= curX) && (curX < x + TType(ColSepWidth())))
			{
				auto dic = debug_cast<ColumnHeaderControl*>(chc)->GetDic();
				if (dic) 
					dic->StartResize(med);
				return true;
			}
		}

	}
	return base_type::MouseEvent(med);
}

bool TableHeaderControl::OnKeyDown(UInt32 virtKey)
{
	return m_TableControl->OnKeyDown(virtKey);
}
