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
	TPoint GetCurrClientSize   () const { return m_ClientSize; }
	TPoint GetCurrFullSize     () const { return m_ClientSize + TPoint(GetBorderPixelSize()); }
	TRect  GetCurrClientRelRect() const { return TRect(m_RelPos, m_RelPos+m_ClientSize); }

	TPoint CalcClientSize() const;
	virtual TPoint CalcMaxSize() const;
	TRect  CalcClientRelRect() const { return TRect(m_RelPos, m_RelPos + CalcClientSize()); }
	TRect  CalcFullRelRect  () const { return CalcClientRelRect() + TRect(GetBorderPixelExtents()); }

	TPoint GetCurrClientAbsPos () const;
	TRect  GetCurrClientAbsRect() const { TPoint pos = GetCurrClientAbsPos(); return TRect(pos, pos + m_ClientSize); }

	GRect GetDrawnClientAbsRect() const;
	GRect GetDrawnNettAbsRect() const override;

	TRect GetCurrFullRelLogicalRect() const { return GetCurrClientRelRect() + TRect(GetBorderPixelExtents()); }
	TRect GetCurrFullAbsLogicalRect() const { return GetCurrClientAbsRect() + TRect(GetBorderPixelExtents()); }
	GRect GetParentClipAbsRect() const;

	TRect GetCurrFullRelDeviceRect() const { auto rect = GetCurrFullRelLogicalRect(); rect *= GetScaleFactors(); return rect; }
	TRect GetCurrFullAbsDeviceRect() const { auto rect = GetCurrFullAbsLogicalRect(); rect *= GetScaleFactors(); return rect; }

	virtual TPoint GetCurrNettSize()  const { return m_ClientSize; } // for ScrollPorts this is excluding the scrollbar sizes if visible
	TRect GetCurrNettRelRect() const { return TRect(m_RelPos, m_RelPos+GetCurrNettSize()); }
	TRect GetCurrNettAbsRect() const { TPoint pos = GetCurrClientAbsPos(); return TRect(pos, pos + GetCurrNettSize()); }

	TRect GetCurrNettAbsRect(const GraphVisitor& v) const;
	TRect GetCurrFullAbsRect(const GraphVisitor& v) const override;

	GRect  GetBorderPixelExtents() const;
	GPoint GetBorderPixelSize   () const;

	void CopyToClipboard(DataView* dv);
	HBITMAP GetAsDDBitmap(DataView* dv, CrdType subPixelFactor = 1.0, MovableObject* extraObj= nullptr);

	ControlRegion GetControlRegion(TType absX) const;

//	non-virtual override of GetOwner
	std::weak_ptr<MovableObject> GetOwner()             { return std::static_pointer_cast<MovableObject>(base_type::GetOwner().lock());	}
	std::weak_ptr<const MovableObject> GetOwner() const { return std::static_pointer_cast<const MovableObject>(base_type::GetOwner().lock()); }

	void InvalidateClientRect(TRect rect) const;
	virtual void GrowHor(TType deltaX, TType relPosX, const MovableObject* sourceItem);
	virtual void GrowVer(TType deltaX, TType relPosX, const MovableObject* sourceItem);

//	override GraphicObject
	TRect CalcFullAbsRect(const GraphVisitor& v) const override;
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

	TPoint m_RelPos;     // position of clients (0,0) in parents coordinate system, managed by container
	TPoint m_ClientSize; // should be determined by DoUpdateView
	HCURSOR m_Cursor;
};

#endif // __SHV_MOVABLEOBJECT_H

