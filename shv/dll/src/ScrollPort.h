// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __SHV_SCROLLPORT_H
#define __SHV_SCROLLPORT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "Wrapper.h"
#include "DcHandle.h"

//----------------------------------------------------------------------
// class  : ScrollPort
//----------------------------------------------------------------------

class ScrollPort : public Wrapper
{
	typedef Wrapper base_type;
public:
	ScrollPort(MovableObject* owner, DataView* dv, CharPtr caption, bool disableScrollbars);

//	nieuwe functions
	void ScrollLogical  (CrdPoint delta);
	void ScrollLogicalTo(CrdPoint delta);

	void OnHScroll(UInt16 scollCmd);
	void OnVScroll(UInt16 scollCmd);

	void ScrollHome();
	void ScrollEnd ();
	void MakeLogicalRectVisible(CrdRect rect, TPoint border);
	void Export();
	 
	      MovableObject* GetContents();
	const MovableObject* GetContents() const;

	void CalcNettSize();

	CrdPoint CalcMaxSize() const override;
	CrdPoint GetCurrNettLogicalSize() const override { return m_NettSize; }

	void MoveTo(CrdPoint newRelPos) override;

private:
//	Override virtuals of GraphicObject
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	bool OnKeyDown(UInt32 nVirtKey) override;
	void DoUpdateView() override;
	void OnChildSizeChanged() override;
	bool MouseEvent(MouseEventDispatcher& med) override;

	void GrowHor(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem = nullptr) override;
	void GrowVer(CrdType xDelta, CrdType xRelPos, const MovableObject* sourceItem = nullptr) override;

private:
	void SetScrollX(bool horScroll);
	void SetScrollY(bool verScroll);
	void SetScrollBars();

public:
	MG_DEBUGCODE ( UInt32 md_ScrollRecursionCount; )

	CmdSignal  m_cmdOnScrolled;

private:
	CrdPoint m_NettSize = Point<CrdType>(0, 0);
	GPoint m_NrLogicalUnitsPerTumpnailTick;
#ifdef _WIN32
	HWND   m_HorScroll =0, m_VerScroll =0;
#else
	bool   m_HorScroll = false, m_VerScroll = false;
#endif

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_SCROLLPORT_H

