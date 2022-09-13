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

/***************************************************************************
 *
 * File:      ABSTRCARET.H
 * Descr:     ReversedCarets are used to show temporary carets
 *            ReversedCaretOperators change their properties
 *            MouseControl direct caret change
*/

#ifndef __ABSTRCARET_H
#define __ABSTRCARET_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/PersistentSharedObj.h"
#include "geo/Geometry.h"

class AbstrCaretOperator;
class GraphicObject;

//----------------------------------------------------------------------
// class  : AbstrCaret
//----------------------------------------------------------------------

//	AbstrCaret is Inherited by
//      NeedleCaret
//		RectCaret
//      CircleCaret
//		PolyLineCaret

class AbstrCaret : public PersistentSharedObj
{
	typedef PersistentSharedObj base_type;
public:
  	AbstrCaret();
  	virtual void Reverse(HDC dc, bool newVisibleState) =0;
	virtual void Move(const AbstrCaretOperator& caret_operator, HDC dc);

	void SetUsedObject(GraphicObject* givenObject) { m_UsedObject = givenObject; }
	void SetStartPoint(const GPoint& p)            { m_StartPoint = p; }
	bool IsVisible() const;

protected:
	GraphicObject* m_UsedObject;
	GPoint         m_StartPoint;

	DECL_ABSTR(SHV_CALL, Class);
};

#endif // __ABSTRCARET_H


