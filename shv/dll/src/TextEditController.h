// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__SHV_TEXTEDITCONTROLLER_H)
#define __SHV_TEXTEDITCONTROLLER_H

#include "vt/color.h"
#include "ptr/SharedStr.h"
#include "Xml/XmlTreeOut.h"
class AbstrTextEditControl;

//----------------------------------------------------------------------
// class  : SelRange
//----------------------------------------------------------------------

struct SelRange {

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

	SizeT m_Begin = UNDEFINED_VALUE(SizeT);
	SizeT m_Curr = UNDEFINED_VALUE(SizeT);
	SizeT m_End = UNDEFINED_VALUE(SizeT);
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
	void AbandonEditing();
	void InvalidateDraw();

	void InvalidateCaretPos() { InvalidateDraw(); }

	CharPtr GetCurrText()     const { return m_CurrText.c_str(); }
	UInt32  GetCurrSize()     const { return m_CurrText.ssize(); }
	bool    IsEditing  ()     const { return m_IsEditing; }
	bool    CurrTextControlIs(const AbstrTextEditControl* tc) const { return m_CurrTextControl == tc; }
	SizeT   GetCurrRec ()     const { return m_CurrRec; }

	const SelRange& GetCurrSelRange() const { return m_SelRange; }
//	UInt32  GetCurrSelBegin() const { return m_SelRange.m_Begin; }
//	UInt32  GetCurrSel()      const { return m_SelRange.m_Curr;  }
//	UInt32  GetCurrSelEnd()   const { return m_SelRange.m_End;   }

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
