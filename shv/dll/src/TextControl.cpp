// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "TextControl.h"

#include "geo/PointOrder.h"
#include "geo/StringBounds.h"
#include "mci/Class.h"
#include "ser/AsString.h"

#include "DataView.h"
#include "GraphVisitor.h"
#include "MouseEventDispatcher.h"
#include "ShvUtils.h"
#include "TextEditController.h"

//----------------------------------------------------------------------
// section : defines
//----------------------------------------------------------------------

// see Community Comment at http://msdn.microsoft.com/en-us/library/windows/desktop/dd145133(v=vs.85).aspx

//----------------------------------------------------------------------
// DrawText funcs
//----------------------------------------------------------------------

// TODO: scaled Font size, Set TextAlignMode
void DrawSymbol(HDC dc, GRect rect, HFONT hFont, DmsColor color, DmsColor bkClr, WCHAR   symbolIndex)
{
	bool isSymbol = (symbolIndex != UNDEFINED_WCHAR);

	if (!isSymbol && IsDefined(color) )
		bkClr = color;

	if (IsDefined(bkClr) ) // Draw Color Block filled with background color
		FillRectDmsColor(dc, rect, bkClr);

	if (!isSymbol)
		return;

	if (!IsDefined(color))
		color = GraphicObject::GetDefaultTextColor();

	GdiObjectSelector<HFONT> fontSelector  (hFont            ? dc : NULL, hFont );
	DcTextColorSelector      colorSelector (IsDefined(color) ? dc : NULL, color );

	DcTextAlignSelector selectCenterAlignment(dc, TA_TOP|TA_CENTER|TA_NOUPDATECP);
	CheckedGdiCall(
		TextOutW(
			dc, 
			(rect.left + rect.right) / 2,
			rect.top, 
			&symbolIndex, 1
		) 
	,	"DrawSymbol");
}

void TextOutLimited(HDC dc, int x, int y, CharPtr txt, SizeT strLen)
{
	SizeT textLen = Min<SizeT>(strLen, MAX_TEXTOUT_SIZE);
	bool result;
	do  {
		wchar_t uft16Buff[MAX_TEXTOUT_SIZE];
		textLen = MultiByteToWideChar(utf8CP, 0, txt, textLen, uft16Buff, MAX_TEXTOUT_SIZE);
		result = TextOutW( dc, x, y, uft16Buff, textLen);

		if (result)
			return;
		if (GetLastError() != 1450)
			CheckedGdiCall( result, "TextOut");
		textLen = textLen / 2;
	} while (textLen);
}



// TODO: scaled Font size, Set TextAlignMode
UInt32 DrawTextWithCursor(HDC dc, GRect rect, HFONT hFont, DmsColor color, DmsColor bkClr, CharPtr txt, const SelRange& sp, UInt32 strLen)
{
	assert(sp.m_Begin <= sp.m_Curr);
	assert(sp.m_Curr  <= sp.m_End );
	assert(sp.m_End   <= strLen);
	assert(strLen      <= StrLen(txt) );

	if (IsDefined(bkClr) ) // Draw Color Block filled with background color
	{
		FillRectDmsColor(dc, rect, bkClr);
		if (!IsDefined(color))
			color = GraphicObject::GetDefaultTextColor();
	}

	GdiObjectSelector<HFONT> fontSelector  (hFont            ? dc : NULL, hFont );
	DcTextColorSelector      colorSelector (IsDefined(color) ? dc : NULL, color );

	DcTextAlignSelector selectCenterAlignment(dc, TA_TOP|TA_LEFT|TA_UPDATECP);
	::MoveToEx(dc, rect.left, rect.top, 0);

	if (sp.m_Begin)
		TextOutLimited( dc, 0, 0, txt, sp.m_Begin);
	GPoint ptBegin; ::GetCurrentPositionEx(dc, &ptBegin);
	UInt32 result = ptBegin.x;
	if (sp.m_Begin < sp.m_End)
	{
		TextOutLimited( dc, 0, 0, txt+sp.m_Begin, sp.m_End-sp.m_Begin);
		GPoint ptEnd; ::GetCurrentPositionEx(dc, &ptEnd);
		GRect selRect(ptBegin.x, rect.top, ptEnd.x, rect.bottom);
		FillRect(dc, &selRect, (HBRUSH) (COLOR_HIGHLIGHT+1) );

		::MoveToEx(dc, ptBegin.x, ptBegin.y, 0);

		DcTextColorSelector selColorSelector(dc, COLORREF2DmsColor( ::GetSysColor(COLOR_HIGHLIGHTTEXT) ) );
		if (sp.m_Begin < sp.m_Curr)
			TextOutLimited( dc, 0, 0, txt+sp.m_Begin, sp.m_Curr-sp.m_Begin);
		::GetCurrentPositionEx(dc, &ptEnd);
		result = ptEnd.x;
		if (sp.m_Curr  < sp.m_End)
			TextOutLimited( dc, 0, 0, txt+sp.m_Curr,  sp.m_End -sp.m_Curr );
	}
	if (sp.m_End < strLen)
		TextOutLimited( dc, 0, 0, txt+sp.m_End, strLen-sp.m_End);

	return result;
}

void DrawText(
	HDC      dc, 
	GRect    rect,
	HFONT    hFont,
	DmsColor color,
	DmsColor bkClr,
	CharPtr  txt
)
{
	dms_assert(txt);
	if (IsDefined(bkClr) ) // Draw Color Block filled with background color
	{
		FillRectDmsColor(dc, rect, bkClr);
		if (!IsDefined(color))
			color = GraphicObject::GetDefaultTextColor();
	}

	GdiObjectSelector<HFONT> fontSelector  (hFont            ? dc : NULL, hFont );
	DcTextColorSelector      colorSelector (IsDefined(color) ? dc : NULL, color );

	TextOutLimited(
		dc, 
		rect.left, 
		rect.top, 
		txt, StrLen(txt)
	) ;
}

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
		DrawText(d.GetDC(), clientIntRect,
			0, // use font of current DC
			GetColor(),
			GetBkColor(),
			GetCaption().c_str()
		);
		if (m_IsInverted)
			InvertRect(d.GetDC(), &clientIntRect);
	}
	return false;
}

IMPL_RTTI_CLASS(TextControl)

//----------------------------------------------------------------------
// class  : AbstrTextEditControl
//----------------------------------------------------------------------

void AbstrTextEditControl::DrawEditText(
	HDC          dc, 
	const GRect& rect,
	HFONT        hFont,
	DmsColor     color,
	DmsColor     bkClr,
	CharPtr      txt,
	bool         isActive
) const
{
	auto dv = GetDataView().lock(); if (!dv) return;
	if (isActive)
	{
		TextEditController& tec = dv->m_TextEditController;
		if (tec.IsEditing())
		{
			auto caretX =
				DrawTextWithCursor(dc, rect, hFont, color, bkClr, tec.GetCurrText(), tec.GetCurrSelRange(), tec.GetCurrSize());
			dv->SetTextCaret(GPoint(caretX, rect.top) );
		}
		else
		{
			auto currTextLen = StrLen(txt);
			DrawTextWithCursor(dc, rect, hFont, color, bkClr, txt, SelRange{ 0, currTextLen, currTextLen }, currTextLen);
		}
	}
	else
		DrawText(dc, rect, hFont, color, bkClr, txt);
}

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

		DrawEditText(d.GetDC(), clientIntRect,
			0, // // use font of current DC
			GetColor(),
			GetBkColor(),
			GetCaption().c_str(),
			IsActive()
		);
		if (m_IsInverted)
			InvertRect(d.GetDC(), &clientIntRect);
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
	if ((med.GetEventInfo().m_EventID & EID_SETCURSOR ) && IsEditable(AN_LabelText))
	{
		SetCursor(LoadCursor(NULL, IDC_IBEAM));
		return true;
	}
	if(med.GetEventInfo().m_EventID & (EID_LBUTTONDOWN|EID_LBUTTONDBLCLK) )
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

		if(med.GetEventInfo().m_EventID & EID_LBUTTONDBLCLK )
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

