// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_CONTROLLERS_H
#define __SHV_CONTROLLERS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geom/Geometry.h"

#include "AbstrController.h"
#include "MenuData.h"

//----------------------------------------------------------------------
// class  : PointCaretController
//----------------------------------------------------------------------

class PointCaretController : public AbstrController
{
	typedef AbstrController base_type;
  public:
	PointCaretController(DataView* owner, AbstrCaret* caret, GraphicObject* target
	, EventID moveEvents, EventID execEvents, EventID stopEvents, ToolButtonID toolID);
	~PointCaretController();

protected:
	bool Move(EventInfo& eventInfo) override;
	void Stop() override;

private:
  	AbstrCaret*  m_Caret;
};

//----------------------------------------------------------------------
// class  : DualPointController
//----------------------------------------------------------------------

class DualPointController : public AbstrController
{
	typedef AbstrController base_type;
public:
	DualPointController(DataView* owner, GraphicObject* target, const GPoint& origin
	, EventID moveEvents  // = EID_MOUSEDRAG,
	, EventID execEvents  // = EID_LBNUTTONUP,
	, EventID stopEvents  // = EID_CLOSE_EVENTS
	, ToolButtonID toolID
	);

protected:
	GPoint  m_Origin;
};

//----------------------------------------------------------------------
// class  : DualPointCaretController
//----------------------------------------------------------------------

class DualPointCaretController : public DualPointController
{
	typedef DualPointController base_type;
public:
	DualPointCaretController(DataView* owner, AbstrCaret* caret
	, GraphicObject* target, const GPoint& origin
	, EventID moveEvents  // = EID_MOUSEDRAG,
	, EventID execEvents  // = EID_LBNUTTONUP,
	, EventID stopEvents  // = EID_CLOSE_EVENTS
	, ToolButtonID toolID
	);
	~DualPointCaretController();

protected:
	bool Move(EventInfo& eventInfo) override;
	void Stop() override;

  	AbstrCaret* m_Caret;
};

//----------------------------------------------------------------------
// class  : TieCursorController
//----------------------------------------------------------------------

class TieCursorController : public AbstrController
{
	typedef AbstrController base_type;
public:
	TieCursorController(DataView* owner, GraphicObject* target, GRect tieRect
	, EventID moveEvents  // = EID_MOUSEDRAG,
	, EventID stopEvents  // = EID_CLOSE_EVENTS
	);

protected:
	bool Move (EventInfo& eventInfo) override;

private:
	GRect m_TieRect;
};

//----------------------------------------------------------------------
// class  : ZoomInController
//----------------------------------------------------------------------

class ZoomInController : public DualPointCaretController // DualPointController // <ViewPort>
{
	typedef DualPointCaretController base_type;
public:
	// rightButtonMarquee: drive the marquee with the right mouse button (RBUTTONUP) instead of the
	// left, and treat a sub-threshold drag as a click (returns false from Exec) so the caller's
	// context menu still appears. Used by Alt+drag (left, default) and right-button drag.
	ZoomInController(DataView* owner, ViewPort* target
	,	const CrdTransformation& transformation, const GPoint& origin
	,	bool rightButtonMarquee = false
	);

protected: // override TDualPointController callback
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
	bool              m_RightButton;
};

//----------------------------------------------------------------------
// class  : OrbitController (rotate / tilt about the view centre)
//----------------------------------------------------------------------

// Shift+drag orbit. STUB: the world->view CrdTransformation is still affine-only (level c), so
// composing a rotation/tilt is a no-op until the Transformation complexity work lands
// (see Transformation_complexity_plan.md). Wired now so the gesture routing is in place.
class OrbitController : public DualPointController // <ViewPort>
{
	typedef DualPointController base_type;
public:
	OrbitController(DataView* owner, ViewPort* target, const GPoint& origin);

protected:
	bool Exec(EventInfo& eventInfo) override;
};

//----------------------------------------------------------------------
// class  : ZoomOutController
//----------------------------------------------------------------------

class ZoomOutController : public AbstrController // <ViewPort>
{
	typedef DualPointCaretController base_type;
public:
	ZoomOutController(DataView* owner, ViewPort* target, const CrdTransformation& transformation);

protected: // override TDualPointController callback
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
};

//----------------------------------------------------------------------
// class  : PanController
//----------------------------------------------------------------------

struct PanController : DualPointController // <ViewPort>
{
	typedef DualPointCaretController base_type;
	PanController(DataView* owner, ViewPort* target, const GPoint& origin);

protected: // override TDualPointController callback
	bool Exec(EventInfo& eventInfo) override;

private:
	bool m_DidDrag;
};

//----------------------------------------------------------------------
// class  : RectPanController
//----------------------------------------------------------------------

class RectPanController : public DualPointController // <ViewPort>
{
	typedef DualPointCaretController base_type;
public:
	RectPanController(DataView* owner, GraphicRect* target
	,	const CrdTransformation& transformation, const GPoint&origin);

protected: // override TDualPointController callback
	bool Exec(EventInfo& eventInfo) override;

private:
	CrdTransformation m_Transformation;

	CrdPoint m_OrgWP;
};

//----------------------------------------------------------------------
// class  : InfoController
//----------------------------------------------------------------------

namespace InfoController
{
	void SelectFocusElem(LayerSet* ls, const CrdPoint& worldPoint, EventID eventID);
};

//----------------------------------------------------------------------
// class  : SelectObjectController
//----------------------------------------------------------------------

class SelectObjectController : public AbstrController
{
	typedef AbstrController base_type;
public:
	SelectObjectController(DataView* owner, ViewPort* target
	, const CrdTransformation& transformation
	);

protected: // override TPointCaretController callback
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
};

//----------------------------------------------------------------------
// class  : SelectRectController
//----------------------------------------------------------------------

class SelectRectController : public DualPointCaretController
{
	typedef DualPointCaretController base_type;
public:
	SelectRectController(DataView* owner, ViewPort* target
	,	const CrdTransformation& transformation, const GPoint& origin
	);

protected: // override PointController callback
	bool Move(EventInfo& eventInfo) override;
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
};

//----------------------------------------------------------------------
// class  : SelectCircleController
//----------------------------------------------------------------------

class SelectCircleController : public DualPointCaretController
{
	typedef DualPointCaretController base_type;
public:
	SelectCircleController(DataView* owner, ViewPort* target
		, const CrdTransformation& transformation, const GPoint& origin
	);

protected: // override PointController callback
	bool Move(EventInfo& eventInfo) override;
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
};

//----------------------------------------------------------------------
// class  : SelectDistrictController
//----------------------------------------------------------------------

class SelectDistrictController : public AbstrController
{
	typedef AbstrController base_type;
public:
	SelectDistrictController(
		DataView* owner, 
		ViewPort* target,
		const CrdTransformation& transformation
	);

protected: // override PointController callback
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
};

//----------------------------------------------------------------------
// class  : DrawPolygonController
//----------------------------------------------------------------------

class DrawPolygonController : public AbstrController
{
	typedef AbstrController base_type;
public:
	DrawPolygonController(
		DataView*                owner, 
		ViewPort*                target,
		const CrdTransformation& transformation, 
		const GPoint&            origin,
		bool                     drawEndLine = true
	);
	~DrawPolygonController();

protected:
	bool Move(EventInfo& eventInfo) override;
	bool Exec(EventInfo& eventInfo) override;

private:
	ViewPort* GetViewPort();
	void AddPoint(GPoint pnt);

	CrdTransformation        m_Transformation;
	std::vector<GPoint>      m_Points;
	std::vector<AbstrCaret*> m_LineCarets;
	AbstrCaret*              m_EndLineCaret;
	AbstrCaret*              m_PolygonCaret;
};

#endif // __SHV_CONTROLLERS_H


