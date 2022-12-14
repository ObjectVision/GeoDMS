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
#pragma once

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
	void Scroll  (TPoint delta);
	void ScrollTo(TPoint delta);

	void OnHScroll(UInt16 scollCmd);
	void OnVScroll(UInt16 scollCmd);

	void ScrollHome();
	void ScrollEnd ();
	void MakeVisible(TRect rect, const TPoint& border);
	void Export();
	 
	      MovableObject* GetContents();
	const MovableObject* GetContents() const;

	void CalcNettSize();

	TPoint CalcMaxSize() const override;
	TPoint GetCurrNettSize() const override { return m_NettSize; }

private:
//	Override virtuals of GraphicObject
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	bool OnKeyDown(UInt32 nVirtKey) override;
	void DoUpdateView() override;
	void OnChildSizeChanged() override;
	void MoveTo(TPoint newRelPos) override;
	bool MouseEvent(MouseEventDispatcher& med) override;

	void GrowHor(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;
	void GrowVer(TType xDelta, TType xRelPos, const MovableObject* sourceItem) override;

private:
	void SetScrollX(bool horScroll);
	void SetScrollY(bool verScroll);
	void SetScrollBars();

public:
	MG_DEBUGCODE ( UInt32 md_ScrollRecursionCount; )

	CmdSignal  m_cmdOnScrolled;

private:
	TPoint m_NettSize;
	GPoint m_NrTPointsPerGPoint;
	HWND   m_HorScroll, m_VerScroll;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_SCROLLPORT_H

