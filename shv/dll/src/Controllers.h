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

#ifndef __SHV_CONTROLLERS_H
#define __SHV_CONTROLLERS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Geometry.h"

#include "AbstrController.h"
#include "MenuData.h"

//----------------------------------------------------------------------
// class  : PointCaretController
//----------------------------------------------------------------------

class PointCaretController : public AbstrController
{
	typedef AbstrController base_type;
  public:
	PointCaretController(
		DataView*                owner,
		AbstrCaret*              caret,
		GraphicObject*           target,
		UInt32                   moveEvents, //= EventID(EID_MOUSEMOVE|EID_MOUSEDRAG) 
		UInt32                   execEvents,
		UInt32                   stopEvents
	);
	~PointCaretController();

protected:
	bool Move(EventInfo& eventInfo) override;
	void Stop() override;

private:
  	AbstrCaret* m_Caret;
};

//----------------------------------------------------------------------
// class  : DualPointController
//----------------------------------------------------------------------

class DualPointController : public AbstrController
{
	typedef AbstrController base_type;
public:
	DualPointController(
		DataView*                owner, 
		GraphicObject*           target, 
		const GPoint&            origin,
		UInt32 moveEvents, // = EID_MOUSEDRAG,
		UInt32 execEvents, // = EID_LBNUTTONUP,
		UInt32 stopEvents  // = EID_CLOSE_EVENTS
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
	DualPointCaretController(
		DataView*                owner, 
		AbstrCaret*              caret,
		GraphicObject*           target, 
		const GPoint&            origin,
		UInt32 moveEvents, // = EID_MOUSEDRAG,
		UInt32 execEvents, // = EID_LBNUTTONUP,
		UInt32 stopEvents  // = EID_CLOSE_EVENTS
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
	TieCursorController(
		DataView*      owner, 
		GraphicObject* target, 
		const GRect&   tieRect,   
		UInt32         moveEvents, // = EID_MOUSEDRAG,
		UInt32         stopEvents  // = EID_CLOSE_EVENTS
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
	ZoomInController(
		DataView*                owner, 
		ViewPort*                target,
		const CrdTransformation& transformation, 
		const GPoint&            origin
	);

protected: // override TDualPointController callback
	bool Exec(EventInfo& eventInfo) override;

	CrdTransformation m_Transformation;
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
	PanController(
		DataView*                owner, 
		ViewPort*                target,
		const GPoint&            origin
	);

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
	RectPanController(
		DataView*                owner, 
		GraphicRect*             target,
		const CrdTransformation& transformation, 
		const GPoint&            origin
	);

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
	SelectObjectController(
		DataView* owner, 
		ViewPort* target,
		const CrdTransformation& transformation
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
	SelectRectController(
		DataView*                owner, 
		ViewPort*                target,
		const CrdTransformation& transformation, 
		const GPoint&            origin
	);

protected: // override PointController callback
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
	SelectCircleController(
		DataView*                owner, 
		ViewPort*                target,
		const CrdTransformation& transformation, 
		const GPoint&            origin
	);

protected: // override PointController callback
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


