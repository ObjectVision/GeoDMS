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

#ifndef __CARETS_H
#define __CARETS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "AbstrCaret.h"
#include "ShvUtils.h"

//----------------------------------------------------------------------
// class  : NeedleCaret
//----------------------------------------------------------------------

class NeedleCaret : public AbstrCaret
{
	typedef AbstrCaret base_type;
public:	
	NeedleCaret();

//	override AbstrCaret interface
  	void Reverse(HDC dc, bool newVisibleState) override;
	void Move(const AbstrCaretOperator& caret_operator, HDC dc) override;

	void SetViewRect(const GRect& viewRect) { m_ViewRect = viewRect; }

protected:
	GRect m_ViewRect;
};

//----------------------------------------------------------------------
// class  : DualPointCaret
//----------------------------------------------------------------------

class DualPointCaret : public AbstrCaret
{
	typedef AbstrCaret base_type;
public:	
	DualPointCaret();

	void SetEndPoint  (const GPoint& p) { m_EndPoint = p; }

protected:
	GPoint  m_EndPoint;
};

//----------------------------------------------------------------------
// class  : FocusCaret
//----------------------------------------------------------------------

class FocusCaret : public DualPointCaret
{
	typedef DualPointCaret base_type;

public:	
	GRect GetRect() const;

//	override AbstrCaret interface
	void Reverse(HDC dc, bool newVisibleState) override;
};

//----------------------------------------------------------------------
// class  : LineCaret
//----------------------------------------------------------------------

class LineCaret : public DualPointCaret
{
	typedef DualPointCaret base_type;
public:	
	LineCaret(DmsColor clr) : m_LineColor(clr) {}

//	override AbstrCaret interface
	void Reverse(HDC dc, bool newVisibleState) override;

private:
	DmsColor m_LineColor;
};

//----------------------------------------------------------------------
// class  : PolygonCaret
//----------------------------------------------------------------------

class PolygonCaret : public AbstrCaret
{
	typedef DualPointCaret base_type;
public:	
	PolygonCaret(const POINT* first, const POINT* last, DmsColor clr = CombineRGB(0, 0, 0))
	:	m_First(first)
	,	m_Count(last-first)
	,	m_FillColor(clr) {}

//	override AbstrCaret interface
	void Reverse(HDC dc, bool newVisibleState) override;

private:
	const POINT* m_First;
	SizeT        m_Count;
	DmsColor     m_FillColor;
};

//----------------------------------------------------------------------
// class  :	InvertRgnCaret
//----------------------------------------------------------------------

class InvertRgnCaret : public DualPointCaret
{
	typedef DualPointCaret base_type;

public:	
	virtual void GetRgn(Region& rgn, HDC dc) const=0;

//	override AbstrCaret interface
	void Reverse(HDC dc, bool newVisibleState) override;
	void Move(const AbstrCaretOperator& caret_operator, HDC dc) override;

	DECL_ABSTR(SHV_CALL, Class);
};

//----------------------------------------------------------------------
// class  : RectCaret
//----------------------------------------------------------------------

class RectCaret : public InvertRgnCaret
{
	typedef InvertRgnCaret base_type;

public:	
//	override InvertRgnCaret interface
	void GetRgn(Region& rgn, HDC dc) const override;
};

//----------------------------------------------------------------------
// class  : MovableRectCaret
//----------------------------------------------------------------------

class MovableRectCaret : public InvertRgnCaret
{
	typedef InvertRgnCaret base_type;

public:
	MovableRectCaret(const GRect& objRect);

	void GetRgn(Region& rgn, HDC dc) const override;

private:
	GRect m_ObjRect;
	GRect m_SubRect;
};

//----------------------------------------------------------------------
// class  : BoundaryCaret
//----------------------------------------------------------------------

class BoundaryCaret : public MovableRectCaret
{
	typedef MovableRectCaret base_type;

public:
	BoundaryCaret(MovableObject* obj);
};

//----------------------------------------------------------------------
// class  : RoiCaret
//----------------------------------------------------------------------

class RoiCaret : public RectCaret
{
	typedef RectCaret base_type;

public:	
	void GetRgn(Region& rgn, HDC dc) const override;
};

//----------------------------------------------------------------------
// class  : CircleCaret
//----------------------------------------------------------------------

class CircleCaret : public InvertRgnCaret
{
	typedef InvertRgnCaret base_type;

public:	
	GType Radius() const;

//	override AbstrCaret interface
	void GetRgn(Region& rgn, HDC dc) const override;
};

#endif // __CARETS_H


