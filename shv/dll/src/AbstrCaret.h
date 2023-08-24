// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
	using base_type = PersistentSharedObj;
public:

  	virtual void Reverse(HDC dc, bool newVisibleState) =0;
	virtual void Move(const AbstrCaretOperator& caret_operator, HDC dc);

	void SetUsedObject(GraphicObject* givenObject) { m_UsedObject = givenObject; }
	void SetStartPoint(const GPoint& p)            { m_StartPoint = p; }
	bool IsVisible() const;
	auto UsedObject() const { assert(m_UsedObject);  return m_UsedObject; }

protected:
	GraphicObject* m_UsedObject = nullptr;
	GPoint         m_StartPoint = UNDEFINED_VALUE(GPoint);

	DECL_ABSTR(SHV_CALL, Class);
};

#endif // __ABSTRCARET_H


