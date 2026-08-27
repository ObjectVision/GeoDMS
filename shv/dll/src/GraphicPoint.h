// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __GRAPHICPOINT_H
#define __GRAPHICPOINT_H

#include "Types.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicObject.h"

//----------------------------------------------------------------------
// class  : GraphicPoint: example (mag naar andere klasse)
//----------------------------------------------------------------------

struct GraphicPoint : GraphicObject
{
//	override GraphicObject virtuals for size & display of GraphicObjects
	bool Draw(GraphDrawer& d) const override 
	{
		CPoint p = Convert2CPoint(d.GetTransformationPtr()->Apply(m_Point));
		d.GetDrawContext()->DrawEllipse(GRect(p.x-3, p.y-3, p.x+3, p.y+3), CombineRGB(0, 0, 0));
		return false;
	};
  	void DoUpdateView() override
	{
    	SetWorldClientRect( CrdRect(m_Point, m_Point) );
	}
private:
	CrdPoint m_Point;
};

#endif // __GRAPHICPOINT_H

