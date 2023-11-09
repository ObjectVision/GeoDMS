// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
	MovableRectCaret(GRect objRect);

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


