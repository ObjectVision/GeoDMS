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

#include "TextControl.h"

#include "geo/PointOrder.h"
#include "geo/StringBounds.h"
#include "mci/Class.h"

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
void DrawSymbol(
	HDC dc, 
	GRect rect,
	HFONT hFont,
	DmsColor color,
	DmsColor bkClr,
	WCHAR   symbolIndex
)
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

void TextOutLimited(
	HDC     dc,
	int     x, 
	int     y, 
	CharPtr txt, 
	SizeT   strLen
)
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
UInt32 DrawTextWithCursor(
	HDC      dc, 
	GRect    rect,
	HFONT    hFont,
	DmsColor color,
	DmsColor bkClr,
	CharPtr  txt,
	UInt32   selPosBegin,
	UInt32   selPosCurr,
	UInt32   selPosEnd,
	UInt32   strLen
)
{

	dms_assert(selPosBegin <= selPosCurr);
	dms_assert(selPosCurr  <= selPosEnd );
	dms_assert(selPosEnd   <= strLen);
	dms_assert(strLen      <= StrLen(txt) );

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

	if (selPosBegin)
		TextOutLimited( dc, 0, 0, txt, selPosBegin);
	GPoint ptBegin; ::GetCurrentPositionEx(dc, &ptBegin);
	UInt32 result = ptBegin.x;
	if (selPosBegin < selPosEnd)
	{
		TextOutLimited( dc, 0, 0, txt+selPosBegin, selPosEnd-selPosBegin);
		GPoint ptEnd; ::GetCurrentPositionEx(dc, &ptEnd);
		GRect selRect(ptBegin.x, rect.top, ptEnd.x, rect.bottom);
		FillRect(dc, &selRect, (HBRUSH) (COLOR_HIGHLIGHT+1) );

		::MoveToEx(dc, ptBegin.x, ptBegin.y, 0);

		DcTextColorSelector selColorSelector(dc, COLORREF2DmsColor( ::GetSysColor(COLOR_HIGHLIGHTTEXT) ) );
		if (selPosBegin < selPosCurr)
			TextOutLimited( dc, 0, 0, txt+selPosBegin, selPosCurr-selPosBegin);
		::GetCurrentPositionEx(dc, &ptEnd);
		result = ptEnd.x;
		if (selPosCurr  < selPosEnd)
			TextOutLimited( dc, 0, 0, txt+selPosCurr,  selPosEnd -selPosCurr );
	}
	if (selPosEnd < strLen)
		TextOutLimited( dc, 0, 0, txt+selPosEnd, strLen-selPosEnd);

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
	SetClientSize(TPoint(ctrlWidth, ctrlHeight));
}


void TextControl::SetWidth (UInt32 width)
{
	SetClientSize(TPoint(width, GetCurrClientSize().y()));
}

void TextControl::SetHeight(UInt32 height)
{
	SetClientSize(TPoint(GetCurrClientSize().x(), height));
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
		GRect clientAbsRect = TRect2GRect(GetCurrClientRelRect() + d.GetClientOffset());
		DrawText(
			d.GetDC(),
			clientAbsRect,
			0, // // use font of current DC
			GetColor(),
			GetBkColor(),
			GetCaption().c_str()
		);
		if (m_IsInverted)
			InvertRect(d.GetDC(), &clientAbsRect);
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
			UInt32 caretX =
				DrawTextWithCursor(
					dc, 
					rect,
					hFont,
					color,
					bkClr,
					tec.GetCurrText(),
					tec.GetCurrSelBegin(),
					tec.GetCurrSel(),
					tec.GetCurrSelEnd(),
					tec.GetCurrSize()
				);
			dv->SetTextCaret(GPoint(caretX, rect.top) );
		}
		else
		{
			UInt32  currTextLen = StrLen(txt);
			DrawTextWithCursor(
				dc, 
				rect,
				hFont,
				color,
				bkClr,
				txt,
				0,           // selBegin
				currTextLen, // caretPos
				currTextLen, // selEnd
				currTextLen  // strLen
			);
		}
	}
	else
		DrawText(
			dc, 
			rect,
			hFont,
			color,
			bkClr,
			txt
		);
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
		GRect clientAbsRect = TRect2GRect( GetCurrClientRelRect() + d.GetClientOffset() );
		DrawEditText(
			d.GetDC(), 
			clientAbsRect,
			0, // // use font of current DC
			GetColor(),
			GetBkColor(),
			GetCaption().c_str(),
			IsActive()
		);
		if (m_IsInverted)
			InvertRect(d.GetDC(), &clientAbsRect);
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
		TPoint relClientPos = TPoint(med.GetEventInfo().m_Point) - (med.GetClientOffset() + GetCurrClientRelPos());
		if (HasBorder())
		{
			if (relClientPos.x() < 0) goto skip;
			if (relClientPos.y() < 0) goto skip;
		}
		dms_assert(relClientPos.x() >= 0);
		dms_assert(relClientPos.y() >= 0);
		if (!IsStrictlyLower(relClientPos, GetCurrClientSize())) goto skip;

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

