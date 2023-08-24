// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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

	virtual void MoveTo(CrdPoint newClientRelPos); // SetClientRelPos
	virtual void SetClientSize(CrdPoint newRelPos);
	void SetClientRect(CrdRect r);
	void SetFullRelRect(CrdRect r);

	CrdPoint GetCurrClientRelPos () const { return m_RelPos; }
	CrdPoint GetCurrClientSize   () const { return m_ClientLogicalSize; }
	CrdPoint GetCurrFullSize     () const { return m_ClientLogicalSize + GetBorderLogicalSize(); }
	CrdRect  GetCurrClientRelLogicalRect() const { return CrdRect(m_RelPos, m_RelPos + Convert<CrdPoint>(m_ClientLogicalSize)); }

	CrdPoint CalcClientSize() const;
	virtual CrdPoint CalcMaxSize() const;
	CrdRect  CalcClientRelRect() const { return CrdRect(m_RelPos, m_RelPos + Convert<CrdPoint>(CalcClientSize())); }
	CrdRect  CalcFullRelRect  () const { return CalcClientRelRect() + Convert<CrdRect>(GetBorderLogicalExtents()); }

	CrdPoint GetCurrClientAbsLogicalPos () const;
	CrdPoint GetCurrClientAbsDevicePos() const { return ScaleCrdPoint(GetCurrClientAbsLogicalPos(), GetScaleFactors()); }
	CrdRect  GetCurrClientAbsLogicalRect() const { auto pos = GetCurrClientAbsLogicalPos(); return CrdRect(pos, pos + Convert<CrdPoint>(m_ClientLogicalSize)); }
	CrdRect  GetCurrClientAbsDeviceRect() const { return ScaleCrdRect(GetCurrClientAbsLogicalRect(), GetScaleFactors()); }

	CrdPoint GetCurrClientAbsLogicalPos(const GraphVisitor& v) const;
	CrdPoint GetCurrClientAbsDevicePos(const GraphVisitor& v) const;
	CrdRect  GetCurrClientAbsLogicalRect(const GraphVisitor& v) const;
	CrdRect  GetCurrClientAbsDeviceRect(const GraphVisitor& v) const;

	CrdRect GetDrawnClientAbsDeviceRect() const;
	CrdRect GetDrawnNettAbsDeviceRect() const override;

	CrdRect GetCurrFullRelLogicalRect() const { return GetCurrClientRelLogicalRect() + Convert<CrdRect>(GetBorderLogicalExtents()); }
	CrdRect GetCurrFullAbsLogicalRect() const { return GetCurrClientAbsLogicalRect() + Convert<CrdRect>(GetBorderLogicalExtents()); }

	CrdRect GetCurrFullRelDeviceRect() const { return ScaleCrdRect( GetCurrFullRelLogicalRect(), GetScaleFactors()); }
	CrdRect GetCurrFullAbsDeviceRect() const { return ScaleCrdRect( GetCurrFullAbsLogicalRect(), GetScaleFactors()); }

	CrdRect GetParentClipAbsRect() const;

	virtual CrdPoint GetCurrNettLogicalSize()  const { return m_ClientLogicalSize; } // for ScrollPorts this is excluding the scrollbar sizes if visible
	CrdRect GetCurrNettRelLogicalRect() const { return CrdRect(m_RelPos, m_RelPos + Convert<CrdPoint>(GetCurrNettLogicalSize())); }

	CrdRect GetCurrNettAbsLogicalRect() const;
	CrdRect GetCurrNettAbsLogicalRect(const GraphVisitor& v) const;
	CrdRect GetCurrFullAbsLogicalRect(const GraphVisitor& v) const { return GetCurrClientAbsLogicalRect(v) + Convert<CrdRect>(GetBorderLogicalExtents()); }
	CrdRect GetCurrFullAbsDeviceRect(const GraphVisitor& v) const override;

	CrdRect  GetBorderLogicalExtents() const;
	CrdPoint GetBorderLogicalSize   () const;

	void CopyToClipboard(DataView* dv);
	HBITMAP GetAsDDBitmap(DataView* dv, CrdType subPixelFactor = 1.0, MovableObject* extraObj= nullptr);

	ControlRegion GetControlDeviceRegion(GType absX) const;

//	non-virtual override of GetOwner
	std::weak_ptr<MovableObject> GetOwner()             { return std::static_pointer_cast<MovableObject>(base_type::GetOwner().lock());	}
	std::weak_ptr<const MovableObject> GetOwner() const { return std::static_pointer_cast<const MovableObject>(base_type::GetOwner().lock()); }

	void InvalidateClientRect(CrdRect rect) const;
	virtual void GrowHor(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem = nullptr);
	virtual void GrowVer(CrdType deltaX, CrdType relPosX, const MovableObject* sourceItem = nullptr);

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

private:
	CrdPoint m_RelPos            = Point<CrdType>(0, 0); // position of clients (0,0) in parents coordinate system, managed by container
	CrdPoint m_ClientLogicalSize = Point<CrdType>(0, 0); // should be determined by DoUpdateView
	HCURSOR m_Cursor;
};

#endif // __SHV_MOVABLEOBJECT_H

