// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "utl/scoped_exit.h"

#include "TextEditController.h"
#include "KeyFlags.h"

#include "DataView.h"
#include "TextControl.h"

//#include "TableControl.h"

//----------------------------------------------------------------------
// class  : SelRange
//----------------------------------------------------------------------

void SelRange::Go(bool shift, SizeT p)
{
	dms_assert(m_Begin <= m_Curr && m_Curr <= m_End);
	if (shift && IsDefined())
	{
		if (p < m_Begin)
		{
			if (m_Curr == m_End)
				m_End = m_Begin;
			m_Begin = p;
		}
		else if (p > m_End)
		{
			if (m_Curr == m_Begin)
				m_Begin = m_End;
			m_End = p;
		}
		else {
			if (m_Curr == m_End)
				m_End = p;
			else if (m_Curr == m_Begin)
				m_Begin = p;
		}
		m_Curr = p;
	}
	else
		CloseAt(p);
}

void SelRange::GoLeft(bool shift, SizeT c)
{
	while (c && m_Curr)
	{
		--c;
		--m_Curr;
		if (shift)
		{
			if (m_Begin > m_Curr)
				m_Begin = m_Curr;
			else
				m_End = m_Curr;
		}
		else
		{
			if (m_Begin < m_End)
				m_End = m_Curr = m_Begin;
			else
				m_Begin = m_End = m_Curr;
		}
	}
}


void SelRange::GoRight(bool shift, SizeT c, SizeT n)
{
	while (c && m_Curr < n)
	{
		--c;
		++m_Curr;
		if (shift)
		{
			if (m_End < m_Curr)
				m_End = m_Curr;
			else
				m_Begin = m_Curr;
		}
		else
			if (m_Begin < m_End)
				m_Begin = m_Curr = m_End;
			else
				m_Begin = m_End = m_Curr;
	}
}

void SelRange::GoHome(bool shift)
{
	if (shift)
	{
		if (m_Curr > m_Begin)
			m_End = m_Begin;
	}
	else
		m_End= 0;
	m_Begin = m_Curr = 0;
}

void SelRange::GoEnd(bool shift, SizeT size)
{
	if (shift)
	{
		if (m_Curr < m_End)
			m_Begin = m_End;
	}
	else
		m_Begin = size;
	m_End = m_Curr = size;
}

//----------------------------------------------------------------------
// class  : TextEditController
//----------------------------------------------------------------------

TextEditController::TextEditController() 
	:	m_CurrTextControl(0)
	,	m_CurrRec(0)
	,	m_IsEditing(false)
{}

bool TextEditController::OnKeyDown(AbstrTextEditControl* srcTC, SizeT srcRec, UInt32 virtKey)
{
	dms_assert(srcTC);
	OnActivate(srcTC, srcRec);

	auto isWmChar = KeyInfo::IsChar(virtKey);
	auto vk = KeyInfo::CharOf(virtKey);

	if (GetKeyState(VK_MENU) & 0x8000)
		return false; // let Qt receive this
	if ((GetKeyState(VK_CONTROL) & 0x8000) && vk != VK_F2)
		return false; // let Qt receive this; Ctrl+F2 is the in-cell edit-start
		              // fallback because plain F2 is consumed app-wide by the
		              // "Step up to FailReason" QShortcut (#1112)

	if (!m_IsEditing)
	{
		if (vk == VK_RETURN)
			return false;

		if (vk == VK_TAB)
			return false;

		if (!isWmChar)
		{
			// we're in WM_KeyDown; these virtual keys may trickle down the GraphicObject tree and then to Qt Controls
			if (vk == VK_UP    ) return false;
			if (vk == VK_DOWN  ) return false;
			if (vk == VK_LEFT  ) return false;
			if (vk == VK_RIGHT ) return false;
			if (vk == VK_PRIOR ) return false; // PgUp must reach TableControl::OnKeyDown for paging
			if (vk == VK_NEXT  ) return false; // PgDn must reach TableControl::OnKeyDown for paging
			if (vk == VK_HOME  ) return false; // Home/End likewise navigate the table when not editing
			if (vk == VK_END   ) return false;
			if (vk == VK_ESCAPE) return false;
			if (vk == VK_DELETE) return false;

			if (vk != VK_F2)
				return true; // mark as handled as we want to process a subsequent WM_CHAR if that would follow, but ignore non-char keys otherwise
		}
		if (m_CurrTextControl->IsEditable(AN_LabelText))
		{
			GuiReadLock lock;
			m_OrgText = srcTC->GetOrgText(srcRec, lock).c_str();
			StartEdit();
			if (vk == VK_F2)
			{
				// F2 / Ctrl+F2 / double-click: pre-load the original text and
				// select it all, so the first char typed replaces the value
				// (standard edit-control convention). Subsequent chars then
				// insert at the collapsed caret.
				m_CurrText = m_OrgText;
				m_SelRange.m_Begin = 0;
				m_SelRange.m_End = m_SelRange.m_Curr = m_CurrText.ssize();
			}
			else
			{
				m_CurrText = SharedStr();
				if (vk != VK_BACK)
					m_CurrText += char(vk);
				m_SelRange.m_End = m_SelRange.m_Begin = m_SelRange.m_Curr = m_CurrText.ssize();
			}
			InvalidateCaretPos();
		}
		else 
			MessageBeep(-1);
		return true;
	}
	assert(m_IsEditing);

	// Several control keys (Backspace, Enter, Tab, Delete) are delivered as both
	// WM_KEYDOWN (vk in the switch below) AND a subsequent WM_CHAR with the
	// same character code (0x08, 0x0D, 0x09, 0x7F). The cases below switch on
	// vk = CharOf(virtKey) which strips the Char flag, so the unreachable
	// `case VK_BACK | Flag::Char` cases below never matched and the WM_CHAR
	// delivery fell into the regular VK_BACK case, causing a second erase
	// per Backspace press (issue #1112). Suppress the WM_CHAR follow-up here.
	if (isWmChar && (vk == VK_BACK || vk == VK_RETURN || vk == VK_TAB || vk == 0x7F))
		return true;

	bool shiftKey = GetKeyState(VK_SHIFT) & 0x8000;
	switch (vk) {
		case VK_BACK: // Backspace
			if (shiftKey)
				return false;
			if (m_SelRange.m_Begin && m_SelRange.IsClosed())
				--m_SelRange.m_Begin;

			m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.Size());
			m_SelRange.CloseAt(m_SelRange.m_Begin);
			InvalidateDraw();
			return true;

		case VK_ESCAPE:
			if (shiftKey)
				return false;
			AbandonEditing();
			return true;

		case VK_RETURN:
			if (shiftKey)
				return false;
			CloseCurr();
			return true;

		case VK_TAB:
		case VK_UP:
		case VK_DOWN:
			CloseCurr();
			return false;

		// swallow subsequent semi control chars as char
		case VK_RETURN | KeyInfo::Flag::Char:
		case VK_BACK | KeyInfo::Flag::Char:
		case VK_TAB | KeyInfo::Flag::Char:
		case 0x7F | KeyInfo::Flag::Char: // DEL char
			return true;

		case VK_LEFT:
			m_SelRange.GoLeft(shiftKey, 1);
			InvalidateCaretPos();
			return true;

		case VK_RIGHT:
			m_SelRange.GoRight(shiftKey, 1, m_CurrText.ssize());
			InvalidateCaretPos();
			return true;

		case VK_HOME:
			m_SelRange.GoHome(shiftKey);
			InvalidateCaretPos();
			return true;

		case VK_END:
			m_SelRange.GoEnd(shiftKey, m_CurrText.ssize());
			InvalidateCaretPos();
			return true;

		case VK_DELETE:
			if (m_SelRange.m_End < m_CurrText.ssize() && m_SelRange.IsClosed())
				++m_SelRange.m_End;

			if (m_SelRange.m_Begin == m_SelRange.m_End)
				return true;

			m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.m_End - m_SelRange.m_Begin);
			m_SelRange.m_End = m_SelRange.m_Curr = m_SelRange.m_Begin;
			InvalidateDraw();
			return true;
	}
	if (isWmChar)
	{
		if (!m_SelRange.IsClosed())
		{
			m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.Size());
			m_SelRange.m_Curr = m_SelRange.m_Begin;
		}
		m_CurrText.insert(m_SelRange.m_Curr++, vk);
		m_SelRange.Close();
		InvalidateDraw();
	}
	return true;
}

void TextEditController::OnActivate(AbstrTextEditControl* srcTC, SizeT srcRec)
{
	if (m_CurrTextControl == srcTC && m_CurrRec == srcRec)
		return;
	CloseCurr();
	m_CurrTextControl = srcTC;
	m_CurrRec = srcRec;
}

void TextEditController::StartEdit()
{
	m_IsEditing = true;
	m_CurrTextControl->SetRevBorder(true);
}

void TextEditController::CloseCurr()
{
	if (!m_IsEditing)
		return;

	auto onExit = make_scoped_exit([self = this]() { self->AbandonEditing(); });

	if (m_CurrText != m_OrgText)
		m_CurrTextControl->SetOrgText(m_CurrRec, GetCurrText());
}

void TextEditController::AbandonEditing()
{
	if (!m_IsEditing)
		return;
	m_IsEditing = false;
	auto dv = m_CurrTextControl->GetDataView().lock();
	if (dv)
		dv->ClearTextCaret();
	m_CurrTextControl->SetRevBorder(false);
	InvalidateDraw();
}

void TextEditController::InvalidateDraw() 
{ 
	m_CurrTextControl->InvalidateDrawnActiveElement(); 
}
