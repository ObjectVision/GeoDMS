// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "dbg/DebugCast.h"

#include "CaretOperators.h"
#include "Carets.h"

//----------------------------------------------------------------------
// class  : PointCaretOperator
//----------------------------------------------------------------------

PointCaretOperator::PointCaretOperator(GPoint point, GraphicObject* givenObject)
   :	m_StartPoint(point) 
	,	m_GivenObject(givenObject)
{}

void PointCaretOperator::operator() (AbstrCaret* caret) const
{
	caret->SetUsedObject(m_GivenObject);
	caret->SetStartPoint(m_StartPoint);
}

//----------------------------------------------------------------------
// class  : DualPointCaretOperator
//----------------------------------------------------------------------

DualPointCaretOperator::DualPointCaretOperator(GPoint start, GPoint end, GraphicObject* givenObject
)  :	PointCaretOperator(start, givenObject)
	,	m_EndPoint(end) 
{}

void DualPointCaretOperator::operator() (AbstrCaret* caret) const
{
	base_type::operator()(caret);
	debug_cast<DualPointCaret*>(caret)->SetEndPoint(m_EndPoint);
}

//----------------------------------------------------------------------
// class  : NeedleCaretOperator
//----------------------------------------------------------------------

NeedleCaretOperator::NeedleCaretOperator(
	const GPoint& start, 
	const GRect& viewRect, 
	GraphicObject* givenObject
)	:	PointCaretOperator(start, givenObject)
	,	m_ViewRect(viewRect)
{}

void NeedleCaretOperator::operator() (AbstrCaret* caret) const
{
	base_type::operator()(caret);
	debug_cast<NeedleCaret*>(caret)->SetViewRect(m_ViewRect);
}

