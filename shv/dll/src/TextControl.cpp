// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TextControl.h"

#include "geo/PointOrder.h"
#include "geo/StringBounds.h"
#include "mci/Class.h"
#include "ser/AsString.h"
#include "ser/FormattedStream.h"

#include "DataView.h"
#include "GraphVisitor.h"
#include "MouseEventDispatcher.h"
#include "ShvUtils.h"
#include "TextEditController.h"

//----------------------------------------------------------------------
// class  : TextControl
//----------------------------------------------------------------------

TextControl::TextControl(MovableObject* owner,
		UInt32 ctrlWidth,
		UInt32 ctrlHeight,
		CharPtr caption,
		COLORREF textColor, 
		COLORREF bkColor
)	:	base_type(owner)
	,	m_Caption  ( caption)
	,	m_TextColor( textColor )
	,	m_BkColor  ( bkColor )
	, m_IsInverted(false)
{
	SetClientSize(shp2dms_order<TType>(ctrlWidth, ctrlHeight));
}


void TextControl::SetWidth (UInt32 width)
{
	SetClientSize(shp2dms_order<TType>(width, GetCurrClientSize().Y()));
}

void TextControl::SetHeight(UInt32 height)
{
	SetClientSize(shp2dms_order<TType>(GetCurrClientSize().X(), height));
}

void TextControl::SetText(WeakStr caption)
{
	if (m_Caption == caption) return;

	m_Caption = caption;
	InvalidateDraw();
}

void TextControl::SetTextColor(COLORREF clr)
{
	if (m_TextColor == clr) return;

	m_TextColor = clr;
	InvalidateDraw();
}

void TextControl::SetBkColor(COLORREF clr)
{
	if (m_BkColor == clr) return;

	m_BkColor = clr;
	InvalidateDraw();
}

SharedStr TextControl::GetCaption() const
{
	return m_Caption;
}

COLORREF TextControl::GetColor() const
{
	return m_TextColor;
}

COLORREF TextControl::GetBkColor() const
{
	return m_BkColor;
}

void TextControl::SetIsInverted(bool v)
{
	if (v == m_IsInverted) return;
	m_IsInverted = v;
	InvalidateDraw();
}


//	override virtual of GraphicObject
bool TextControl::Draw(GraphDrawer& d) const 
{
	if (d.DoDrawBackground())
	{
		auto clientAbsRect = ScaleCrdRect(GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos(), d.GetSubPixelFactors());
		auto clientIntRect = CrdRect2GRect(clientAbsRect);
		auto* dc = d.GetDrawContext();
		DmsColor bk = GetBkColor();
		if (IsDefined(bk))
			dc->FillRect(clientIntRect, bk);
		DmsColor clr = GetColor();
		if (!IsDefined(clr))
			clr = GraphicObject::GetDefaultTextColor();
		dc->TextOut(GPoint(clientIntRect.left, clientIntRect.top), GetCaption().c_str(), StrLen(GetCaption().c_str()), clr);
		if (m_IsInverted)
			dc->InvertRect(clientIntRect);
	}
	return false;
}

bool TextControl::GetTooltipText(TooltipCollector& ttc) const
{
	ttc.m_WantsMoreContext = false;
	base_type::GetTooltipText(ttc);
	ttc.m_Stream << GetCaption();
	return true;
}

IMPL_RTTI_CLASS(TextControl)

//----------------------------------------------------------------------
// class  : EditableTextControl
//----------------------------------------------------------------------

EditableTextControl::EditableTextControl(MovableObject* owner,
	bool isEditable
,	TType ctrlWidth
,	TType ctrlHeight
,	CharPtr caption
,	DmsColor textColor
)	:	TextControl(owner, ctrlWidth, ctrlHeight, caption, textColor, isEditable ? DmsWhite : UNDEFINED_VALUE(UInt32))
	,	m_IsEditable(isEditable)
{}

void EditableTextControl::SetIsEditable(bool v)
{
	m_IsEditable = v; InvalidateDrawnActiveElement(); 
}

void EditableTextControl::InvalidateDrawnActiveElement()
{
	InvalidateDraw();
}

bool EditableTextControl::IsEditable(AspectNr a) const
{
	return a == AN_LabelText && m_IsEditable;
}

bool EditableTextControl::Draw(GraphDrawer& d) const
{
	if (d.DoDrawBackground())
	{
		auto clientAbsRect = ScaleCrdRect(GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos(), GetScaleFactors() );
		auto clientIntRect = CrdRect2GRect(clientAbsRect);
		auto* dc = d.GetDrawContext();

		DmsColor bk = GetBkColor();
		if (IsDefined(bk))
			dc->FillRect(clientIntRect, bk);
		DmsColor clr = GetColor();
		if (!IsDefined(clr))
			clr = GraphicObject::GetDefaultTextColor();
		dc->TextOut(GPoint(clientIntRect.left, clientIntRect.top), GetCaption().c_str(), StrLen(GetCaption().c_str()), clr);

		if (m_IsInverted)
			dc->InvertRect(clientIntRect);
	}
	return false;
}

void EditableTextControl::SetActive(bool newState)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	dms_assert(newState != IsActive());
	if (!newState)
	{
		MG_DEBUGCODE ( dv->m_TextEditController.CheckCurrTec(this); )
		dv->m_TextEditController.CloseCurr();
	}
	InvalidateDrawnActiveElement();
	base_type::SetActive(newState);
}

bool EditableTextControl::MouseEvent(MouseEventDispatcher& med)
{
	if ((med.GetEventInfo().m_EventID & EventID::SETCURSOR ) && IsEditable(AN_LabelText))
	{
		auto dv = GetDataView().lock();
		if (dv && dv->GetViewHost())
			dv->GetViewHost()->VH_SetCursor(DmsCursor::IBeam);
#ifdef _WIN32
		else
			SetCursor(LoadCursor(NULL, IDC_IBEAM));
#endif
		return true;
	}
	if(med.GetEventInfo().m_EventID & (EventID::LBUTTONDOWN|EventID::LBUTTONDBLCLK) )
	{
		CrdPoint relClientPos = Convert<CrdPoint>(med.GetLogicalSize(med.GetEventInfo().m_Point)) - (med.GetClientLogicalAbsPos() + GetCurrClientRelPos());
		if (HasBorder())
		{
			if (relClientPos.X() < 0) goto skip;
			if (relClientPos.Y() < 0) goto skip;
		}
		dms_assert(relClientPos.X() >= 0);
		dms_assert(relClientPos.Y() >= 0);
		if (!IsStrictlyLower(relClientPos, Convert<CrdPoint>(GetCurrClientSize()))) goto skip;

		if(med.GetEventInfo().m_EventID & EventID::LBUTTONDBLCLK )
			OnKeyDown(VK_F2);
		return true;  // don't continue processing LBUTTONDBLCLK
	}
skip:
	return base_type::MouseEvent(med);
}

bool EditableTextControl::OnKeyDown(UInt32 virtKey)
{
	auto dv = GetDataView().lock(); if (!dv) return true;
	if (dv->m_TextEditController.OnKeyDown(this, 0, virtKey))
		return true;

	return base_type::OnKeyDown(virtKey);
}

void EditableTextControl::SetRevBorder(bool revBorder)
{
	base_type::SetRevBorder(revBorder);
}
