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

#include "ShvDllPch.h"

#include "dbg/DebugCast.h"

#include "CaretOperators.h"
#include "Carets.h"

//----------------------------------------------------------------------
// class  : PointCaretOperator
//----------------------------------------------------------------------

PointCaretOperator::PointCaretOperator(const GPoint& point, GraphicObject* givenObject)
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

DualPointCaretOperator::DualPointCaretOperator(
	const GPoint& start, const GPoint& end, GraphicObject* givenObject
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

