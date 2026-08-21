// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __CARETOPERATORS_H
#define __CARETOPERATORS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

class AbstrCaret;
class GraphicObject;

#include "geom/Geometry.h"
#include "ShvUtils.h"

//----------------------------------------------------------------------
// class  : AbstrCaretOperator
//----------------------------------------------------------------------

//	AbstrCaretOperator is Inherited by
//      TPointCaretOperator
//			TDualPointCaretOperator
//			TPolyLineCaretOperator
//  Implementation can be found in Carets.cpp

class AbstrCaretOperator
{
  public:
  	virtual void operator() (AbstrCaret*) const= 0;
};

//----------------------------------------------------------------------
// class  : PointCaretOperator
//----------------------------------------------------------------------

class PointCaretOperator : public AbstrCaretOperator
{
public:
	PointCaretOperator(GPoint point, GraphicObject* givenObject);
  	void operator() (AbstrCaret* caret) const override;

private:
	GraphicObject*            m_GivenObject;
  	GPoint                    m_StartPoint;
};

//----------------------------------------------------------------------
// class  : DualPointCaretOperator
//----------------------------------------------------------------------

class DualPointCaretOperator : public PointCaretOperator
{
	typedef PointCaretOperator base_type;

public:
	DualPointCaretOperator(GPoint start, GPoint end, GraphicObject* givenObject);
  	void operator() (AbstrCaret* caret) const override;

private:
  	GPoint m_EndPoint;
};

//----------------------------------------------------------------------
// class  : NeedleCaretOperator
//----------------------------------------------------------------------

class NeedleCaretOperator : public PointCaretOperator
{
	typedef PointCaretOperator base_type;

public:
	NeedleCaretOperator(const GPoint& start, const GRect& viewRect, GraphicObject* givenObject);
  	void operator() (AbstrCaret* caret) const override;

private:
  	GRect m_ViewRect;
};

#endif // __CARETOPERATORS_H


