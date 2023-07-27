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

#ifndef __SHV_MOVABLEOBJECT_H
#define __SHV_MOVABLEOBJECT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "dbg/DebugCast.h"

#include "GraphicObject.h"
#include "ShvUtils.h"

enum ControlRegion { RG_MIDDLE, RG_LEFT, RG_RIGHT };
enum GdMode;

//----------------------------------------------------------------------
// class  : MovableObject
//----------------------------------------------------------------------

class MovableObject: public GraphicObject
{
	typedef GraphicObject base_type;
public:
	MovableObject(MovableObject* owner);
	MovableObject(const MovableObject& src);

	bool HasBorder          () const { return  m_State.Get(MOF_HasBorder); }
	bool RevBorder          () const { return  m_State.Get(MOF_RevBorder); }
	void SetBorder(bool hasBorder);
	void SetRevBorder(bool revBorder);

	virtual void MoveTo(TPoint newClientRelPos); // SetClientRelPos
	virtual void SetClientSize(TPoint newRelPos);
	void SetClientRect(const TRect& r);
	void SetFullRelRect(TRect r);

	TPoint GetCurrClientRelPos () const { return m_RelPos; }
	TPoint GetCurrClientSize   () const { return m_ClientLogicalSize; }
	TPoint GetCurrFullSize     () const { return m_ClientLogicalSize + GetBorderLogicalSize(); }
	TRect  GetCurrClientRelLogicalRect() const { return TRect(m_RelPos, m_RelPos+m_ClientLogicalSize); }

	TPoint CalcClientSize() const;
	virtual TPoint CalcMaxSize() const;
	TRect  CalcClientRelRect() const { return TRect(m_RelPos, m_RelPos + CalcClientSize()); }
	TRect  CalcFullRelRect  () const { return CalcClientRelRect() + GetBorderLogicalExtents(); }

	TPoint GetCurrClientAbsLogicalPos () const;
	GPoint GetCurrClientAbsDevicePos() const { return TPoint2GPoint(GetCurrClientAbsLogicalPos(), GetScaleFactors(), GetCumulativeScrollSlack()).first; }
	TRect  GetCurrClientAbsLogicalRect() const { TPoint pos = GetCurrClientAbsLogicalPos(); return TRect(pos, pos + m_ClientLogicalSize); }
	GRect  GetCurrClientAbsDeviceRect() const { return TRect2GRect(GetCurrClientAbsLogicalRect(), GetScaleFactors(), GetCumulativeScrollSlack()); }

	TPoint GetCurrClientAbsLogicalPos(const GraphVisitor& v) const;
	GPoint GetCurrClientAbsDevicePos(const GraphVisitor& v) const;
	TRect  GetCurrClientAbsLogicalRect(const GraphVisitor& v) const;
	GRect  GetCurrClientAbsDeviceRect(const GraphVisitor& v) const;

	GRect GetDrawnClientAbsDeviceRect() const;
	GRect GetDrawnNettAbsDeviceRect() const override;

	TRect GetCurrFullRelLogicalRect() const { return GetCurrClientRelLogicalRect() + GetBorderLogicalExtents(); }
	TRect GetCurrFullAbsLogicalRect() const { return GetCurrClientAbsLogicalRect() + GetBorderLogicalExtents(); }

	GRect GetCurrFullRelDeviceRect() const { return TRect2GRect( GetCurrFullRelLogicalRect(), GetScaleFactors(), GetCumulativeScrollSlack()); }
	GRect GetCurrFullAbsDeviceRect() const { return TRect2GRect( GetCurrFullAbsLogicalRect(), GetScaleFactors(), GetCumulativeScrollSlack()); }
	CrdPoint GetCumulativeScrollSlack() const;

	GRect GetParentClipAbsRect() const;

	virtual TPoint GetCurrNettLogicalSize()  const { return m_ClientLogicalSize; } // for ScrollPorts this is excluding the scrollbar sizes if visible
	TRect GetCurrNettRelLogicalRect() const { return TRect(m_RelPos, m_RelPos+GetCurrNettLogicalSize()); }

	TRect GetCurrNettAbsLogicalRect() const;
	TRect GetCurrNettAbsLogicalRect(const GraphVisitor& v) const;
	TRect GetCurrFullAbsLogicalRect(const GraphVisitor& v) const { return GetCurrClientAbsLogicalRect(v) + GetBorderLogicalExtents(); }
	GRect GetCurrFullAbsDeviceRect(const GraphVisitor& v) const override;

	TRect  GetBorderLogicalExtents() const;
	TPoint GetBorderLogicalSize   () const;

	void CopyToClipboard(DataView* dv);
	HBITMAP GetAsDDBitmap(DataView* dv, CrdType subPixelFactor = 1.0, MovableObject* extraObj= nullptr);

	ControlRegion GetControlDeviceRegion(GType absX) const;

//	non-virtual override of GetOwner
	std::weak_ptr<MovableObject> GetOwner()             { return std::static_pointer_cast<MovableObject>(base_type::GetOwner().lock());	}
	std::weak_ptr<const MovableObject> GetOwner() const { return std::static_pointer_cast<const MovableObject>(base_type::GetOwner().lock()); }

	void InvalidateClientRect(TRect rect) const;
	virtual void GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem = nullptr);
	virtual void GrowVer(TType deltaX, TType relPosX, const MovableObject* sourceItem = nullptr);

//	override GraphicObject
	void SetIsVisible(bool value) override;
	void SetDisconnected() override;
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	bool MouseEvent(MouseEventDispatcher& med);

	std::shared_ptr<MovableObject> shared_from_this() { return shared_from_base<MovableObject>(); }
#if defined(MG_DEBUG)
	void CheckState() const override;
#endif

	friend class GraphicVarCols;
	friend class GraphicVarRows;

protected:
	HCURSOR SetViewPortCursor(HCURSOR hCursor); friend class PasteGridController;
private:
	bool UpdateCursor() const;

public:
	CrdPoint m_ScrollSlack = { 0.0, 0.0 };
private:
	TPoint m_RelPos            = Point<TType>(0, 0); // position of clients (0,0) in parents coordinate system, managed by container
	TPoint m_ClientLogicalSize = Point<TType>(0, 0); // should be determined by DoUpdateView
	HCURSOR m_Cursor;
};

#endif // __SHV_MOVABLEOBJECT_H

