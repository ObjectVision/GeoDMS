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

#include "TextEditController.h"

#include "KeyFlags.h"

#include "DataView.h"
#include "TextControl.h"

//#include "TableControl.h"

//----------------------------------------------------------------------
// class  : SelRange
//----------------------------------------------------------------------

SelRange::SelRange()
	:	m_Begin(UNDEFINED_VALUE(SizeT))
	,	m_Curr (UNDEFINED_VALUE(SizeT)) 
	,	m_End  (UNDEFINED_VALUE(SizeT))
{}

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

	if (!m_IsEditing) 
	{
		if (KeyInfo::IsChar(virtKey))
		{
			virtKey = KeyInfo::CharOf(virtKey);
			if (virtKey == VK_ESCAPE)
				return false;
			m_CurrText = SharedStr();
			if (virtKey != VK_BACK)
				m_CurrText += char(virtKey);
		}
		else if (virtKey != VK_F2)
			return false;

		if (m_CurrTextControl->IsEditable(AN_LabelText))
		{
			GuiReadLock lock;
			m_OrgText = srcTC->GetOrgText(srcRec, lock).c_str();
			StartEdit();
			if (virtKey == VK_F2)
				m_CurrText = m_OrgText;

			m_SelRange.m_End = m_SelRange.m_Begin = m_SelRange.m_Curr = m_CurrText.ssize();
			InvalidateCaretPos();
		}
		else 
			MessageBeep(-1);
		return true;
	}

	if (m_IsEditing)
	{
		if ( KeyInfo::IsChar(virtKey) )
		{
			virtKey &= KeyInfo::CharMask;
			switch (virtKey) {
				case VK_BACK: // Backspace
					if (m_SelRange.m_Begin && m_SelRange.IsClosed())
						--m_SelRange.m_Begin;

					m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.Size());
					m_SelRange.CloseAt(m_SelRange.m_Begin);
					InvalidateDraw();
					return true;

				case VK_TAB: // Process a tab.  
					dms_assert(false);
			}
			if (! m_SelRange.IsClosed())
			{
				m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.Size());
				m_SelRange.m_Curr = m_SelRange.m_Begin;
			}
			m_CurrText.insert(m_SelRange.m_Curr++, virtKey);
			m_SelRange.Close();
			InvalidateDraw();
			return true;
		}
		else if (KeyInfo::IsSpec(virtKey))
		{
			bool shiftKey = GetKeyState(VK_SHIFT) & 0x8000;
			switch (KeyInfo::CharOf(virtKey)) {
      			case VK_ESCAPE: // Escape 
					Abandon();
					return true;

				case VK_RETURN:
					CloseCurr();
					return true;

				case VK_TAB:
				case VK_UP:
				case VK_DOWN:
					CloseCurr();
					return false;

				case VK_LEFT: 
					m_SelRange.GoLeft(shiftKey, 1 );
					InvalidateCaretPos();
					return true;

				case VK_RIGHT: 
					m_SelRange.GoRight(shiftKey, 1, m_CurrText.ssize() );
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
				{
					if (m_SelRange.m_End < m_CurrText.ssize() && m_SelRange.IsClosed() )
						++m_SelRange.m_End;

					if (m_SelRange.m_Begin == m_SelRange.m_End)
						return true;

					m_CurrText.GetAsMutableCharArray()->erase(m_SelRange.m_Begin, m_SelRange.m_End - m_SelRange.m_Begin);
					m_SelRange.m_End = m_SelRange.m_Curr = m_SelRange.m_Begin;
					InvalidateDraw();
					return true;
				}
			}
		}
	}
	return false;
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
	struct Finalize
	{
		Finalize(TextEditController* self) : m_Self(self) {} 
		~Finalize() { m_Self->Abandon(); } 
		TextEditController* m_Self;
	}	callAbandon(this);
	if (m_CurrText != m_OrgText)
		m_CurrTextControl->SetOrgText(m_CurrRec, GetCurrText());
}

void TextEditController::Abandon()
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
