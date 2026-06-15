// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_AXISCONTROL_H
#define __SHV_AXISCONTROL_H

#include "MovableObject.h"
#include "ShvSignal.h"

class ViewPort;

//----------------------------------------------------------------------
// class  : AxisControl
//----------------------------------------------------------------------
// Tick + label band along the bottom (Horizontal) or left (Vertical) edge of a
// chart's plot-area ViewPort. Reads the ViewPort's world-to-client transformation
// at draw time, so it is always consistent with the plotted contents; redraws are
// triggered by the ViewPort's m_cmdTransformChanged signal.

enum class AxisKind { Horizontal, Vertical };

class AxisControl : public MovableObject
{
	typedef MovableObject base_type;

public:
	AxisControl(MovableObject* owner, ViewPort* vp, AxisKind kind);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ClipExtents; }

//	override virtuals of GraphicObject
	bool Draw(GraphDrawer& d) const override;
	COLORREF GetBkColor() const override;

private:
	ViewPort* m_ViewPort; // the sibling plot-area ViewPort; both are owned by the same ChartControl
	AxisKind  m_Kind;

	ScopedConnection m_connTransformChanged;

	DECL_RTTI(SHV_CALL, Class)
};

#endif // __SHV_AXISCONTROL_H
