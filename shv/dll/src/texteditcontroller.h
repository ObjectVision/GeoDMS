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

#if !defined(__SHV_TEXTEDITCONTROLLER_H)
#define __SHV_TEXTEDITCONTROLLER_H

#include "geo/Color.h"
#include "ptr/SharedStr.h"
#include "xml/XmlTreeOut.h"
class AbstrTextEditControl;

//----------------------------------------------------------------------
// class  : SelRange
//----------------------------------------------------------------------

struct SelRange {

	SelRange();

	SizeT Size    () const { return m_End - m_Begin; }
	bool  IsClosed() const { return m_Begin == m_End; }
	bool  IsInRange(SizeT p) const{ return m_Begin <= p && p <=m_End && IsDefined(); }
	bool  IsDefined() const { return ::IsDefined(m_Curr); }

	void Close  ()        { m_Begin = m_End = m_Curr; }
	void CloseAt(SizeT p) { m_Curr = p; Close(); }

	void Go     (bool shift, SizeT p);
	void GoLeft (bool shift, SizeT c);
	void GoRight(bool shift, SizeT c, SizeT n);
	void GoHome (bool shift);
	void GoEnd  (bool shift, SizeT size);

	SizeT m_Begin;
	SizeT m_Curr;
	SizeT m_End;
};

inline bool EqualRange(const SelRange& a, const SelRange& b) { return a.m_Begin == b.m_Begin && a.m_End == b.m_End; }

//----------------------------------------------------------------------
// class  : TextEditController
//----------------------------------------------------------------------

class TextEditController
{
public:
	TextEditController();

	bool OnKeyDown (AbstrTextEditControl* srcTC, SizeT srcRec, UInt32 virtKey);
	void OnActivate(AbstrTextEditControl* srcTC, SizeT srcRec);
	void CloseCurr();

	void StartEdit();
	void Abandon();
	void InvalidateDraw();

	void InvalidateCaretPos() { InvalidateDraw(); }

	CharPtr GetCurrText()     const { return m_CurrText.c_str(); }
	UInt32  GetCurrSize()     const { return m_CurrText.ssize(); }
	bool    IsEditing  ()     const { return m_IsEditing; }

	UInt32  GetCurrSelBegin() const { return m_SelRange.m_Begin; }
	UInt32  GetCurrSel()      const { return m_SelRange.m_Curr;  }
	UInt32  GetCurrSelEnd()   const { return m_SelRange.m_End;   }

#if defined(MG_DEBUG)
	void CheckCurrTec(AbstrTextEditControl* currTec) { dms_assert(!IsEditing() || currTec == m_CurrTextControl); }
#endif

private:
	AbstrTextEditControl* m_CurrTextControl;
	SizeT                 m_CurrRec;

	SelRange              m_SelRange;

	SharedStr             m_CurrText, m_OrgText;
	bool                  m_IsEditing;
};

#endif //!defined(__SHV_TEXTEDITCONTROLLER_H)
