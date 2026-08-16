// Copyright (C) 1998-2023 Object Vision b.v. 
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
enum GdMode : int;

// Smallest element (content) width an interactive resize-drag may produce, per column.
constexpr CrdType MIN_COL_ELEM_WIDTH = 6;

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
	virtual void SetElemWidth(UInt16 width);
	virtual void SetElemHeight(UInt16 height); // vertical counterpart, used for row-oriented tables (issue #1150)

	// Apply an interactive border-resize drag whose pointer is at logical X.
	// The default resizes this single element to fit the pointer. DataItemColumn
	// overrides it so that, when the dragged column is part of a multi-column
	// selection, all selected columns adopt the same width (issue #1121).
	virtual void ResizeDragTo(CrdType mouseLogicalX);
	// Vertical counterpart: applied when a row-oriented table's band boundary is dragged (issue #1150).
	virtual void ResizeDragToVer(CrdType mouseLogicalY);
	// Device-space left bound for the resize cursor-tie (how far left the dragged
	// border may travel). DataItemColumn widens it for a pooled multi-column drag
	// so the whole block can shrink to count*MIN_COL_ELEM_WIDTH from the leftmost
	// selected column's edge (issue #1121).
	virtual GType ResizeTieLeftDevice(CrdPoint subPixelFactors) const;
	GType ResizeTieTopDevice(CrdPoint subPixelFactors) const; // vertical counterpart (issue #1150)
	void InvalidateResizedCaret(); // wipe the resize-caret XOR artifact at the column's right/bottom edge

	void SetClientRect(CrdRect r);
	void SetFullRelRect(CrdRect r);

	CrdPoint GetCurrClientRelPos () const { return m_RelPos; }
	CrdPoint GetCurrClientSize   () const { return m_ClientLogicalSize; }
	CrdPoint GetCurrFullSize     () const { return m_ClientLogicalSize + GetBorderLogicalSize(); }
	CrdRect  GetCurrClientRelLogicalRect() const { return CrdRect(m_RelPos, m_RelPos + Convert<CrdPoint>(m_ClientLogicalSize)); }

	CrdPoint CalcClientSize() const;
	CrdPoint CalcFullSize() const { return CalcClientSize() + GetBorderLogicalSize();  }
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
#ifdef _WIN32
	HBITMAP GetAsDDBitmap(DataView* dv, CrdType subPixelFactor = 1.0, MovableObject* extraObj= nullptr);
#endif

	ControlRegion GetControlDeviceRegion(GType absX, bool isColOriented) const;

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

	friend class AutoSizeContainer;
	// Start an interactive border-resize drag. alongX = true drags the right edge
	// horizontally (column-oriented tables); alongX = false drags the bottom edge
	// vertically (row-oriented tables, issue #1150).
	void StartResize(MouseEventDispatcher& med, bool alongX = true);
	void SetElemBorder(bool hasBorder) { m_State.Set(DIC_HasElemBorder, hasBorder); }
	bool HasElemBorder() const { return m_State.Get(DIC_HasElemBorder); }

protected:
	DmsCursor SetViewPortCursor(DmsCursor cursor);
#ifdef _WIN32
	HCURSOR SetViewPortCursor(HCURSOR hCursor); friend class PasteGridController;
#endif
	bool GetTooltipText(TooltipCollector& ttc) const;

private:
	bool UpdateCursor() const;

private:
	CrdPoint m_RelPos            = Point<CrdType>(0, 0); // position of clients (0,0) in parents coordinate system, managed by container
	CrdPoint m_ClientLogicalSize = Point<CrdType>(0, 0); // should be determined by DoUpdateView
	DmsCursor m_DmsCursor = DmsCursor::Arrow;
#ifdef _WIN32
	HCURSOR m_Cursor;
#endif

	// =============================================== ToolTip
	bool HitTest(GPoint ptClient) const noexcept override;
};

#endif // __SHV_MOVABLEOBJECT_H

