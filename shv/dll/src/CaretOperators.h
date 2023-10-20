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

#ifndef __CARETOPERATORS_H
#define __CARETOPERATORS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

class AbstrCaret;
class GraphicObject;

#include "geo/Geometry.h"
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


