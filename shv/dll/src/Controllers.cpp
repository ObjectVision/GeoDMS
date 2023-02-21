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

#include "act/MainThread.h"
#include "dbg/debug.h"

#include "Param.h"

#include "Controllers.h"
#include "MovableContainer.h"
#include "DataView.h"
#include "CaretOperators.h"
#include "GraphVisitor.h"
#include "LayerSet.h"

//----------------------------------------------------------------------
// class  : AbstrCmd
//----------------------------------------------------------------------

#include "AbstrCmd.h"

GraphVisitState AbstrCmd::DoMovableContainer(MovableContainer* mc)
{
	// forward to active item if available
	MovableObject* mo = mc->GetActiveEntry();
	if (mo && (Visit(mo) != GVS_UnHandled))
		return GVS_Handled;
	return DoMovable(mc);
}

GraphVisitState AbstrCmd::DoLayerSet(LayerSet* obj)
{
	// forward to active item if available
	ScalableObject* so = obj->GetActiveEntry();
	if (so->IsActive() && (Visit(so) != GVS_UnHandled))
		return GVS_Handled;
	return DoScalable(obj);
}

//----------------------------------------------------------------------
// class  : AbstrController
//----------------------------------------------------------------------

//	AbstrController is Inherited by
//	*	PointCaretController
//			*	EnlargeController
//			*	ZoomMoreController
//			*	ZoomLessController
// 			*	SelectController
//	*	DualPointController
//		*	DualPointCaretController
//			*	ZoomInController
//			*	SelectRectController
// 			*	SelCircController
// 			?	RouteController
//		*	ZoomOutController
//		*	PanController
//		*	RectPanController
//	*	SelectObjectController
//	*	SelectDistrictController
//	*	TieCursorController
//	*	DrawPolygonController
//	*	ColumnSizerDragger
//	*	PasteGridController


AbstrController::AbstrController(
	DataView*      owner, 
	GraphicObject* target, 
	UInt32         moveEvents,
	UInt32         execEvents,
	UInt32         stopEvents
)	:	m_Owner(owner->weak_from_this())
	,	m_TargetObject(target->weak_from_this())
	,	m_MoveEvents(moveEvents)
	,	m_ExecEvents(execEvents)
	,	m_StopEvents(stopEvents)
{
}

AbstrController::~AbstrController()
{}

struct ControllerStopper
{
	ControllerStopper(AbstrController* self) :m_Self(self) 
	{}
	~ControllerStopper()
	{
		if (m_Self)
			m_Self->Stop();
	}
	AbstrController* m_Self;
};


bool AbstrController::Event(EventInfo& eventInfo)
{
	bool result = eventInfo.m_EventID & m_StopEvents;

	// STOP EVENTS
	ControllerStopper finalizer(result ? this : nullptr );

	if (eventInfo.m_EventID & m_MoveEvents)
	{
		result = Move(eventInfo);
	}
	if (eventInfo.m_EventID & m_ExecEvents)
	{
		result = Exec(eventInfo);
	}
	return result;
}

bool AbstrController::Move(EventInfo& eventInfo)
{
	return false; // Default: NOP
}

bool AbstrController::Exec(EventInfo& eventInfo)
{
	return false; // Default: NOP
}

void AbstrController::Stop()
{
	auto dv = GetOwner().lock();
	if (dv)
		dv->RemoveController(this); // WARNING: Make sure that *this is not accessed anymore after this call, or that it is reference-counted
}

//----------------------------------------------------------------------
// class  : PointCaretController
//----------------------------------------------------------------------

PointCaretController::PointCaretController(
	DataView* owner, AbstrCaret* caret,
	GraphicObject* target,
	UInt32 moveEvents,
	UInt32 execEvents,
	UInt32 stopEvents
)	:	AbstrController(owner, target, moveEvents, execEvents, stopEvents)
	,	m_Caret(caret)
{
	dms_assert(m_Caret);
	dms_assert(target);
	owner->InsertCaret(caret);
}

PointCaretController::~PointCaretController()
{
	dms_assert(m_Caret);
	auto dv = GetOwner().lock();
	if (dv)
		dv->RemoveCaret(m_Caret);
}

bool PointCaretController::Move(EventInfo& eventInfo)
{
	auto dv = GetOwner().lock(); if (!dv) return true;
	auto to = GetTargetObject().lock(); if (!to) return true;

	dv->MoveCaret(m_Caret, PointCaretOperator(eventInfo.m_Point, to.get()));
	return false;
}

void PointCaretController::Stop()
{
	auto dv = GetOwner().lock(); if (!dv) return;
	auto to = GetTargetObject().lock(); if (!to) return;

	dv->MoveCaret(m_Caret, PointCaretOperator(UNDEFINED_VALUE(GPoint), to.get()));
	base_type::Stop();
}

//----------------------------------------------------------------------
// class  : DualPointControlller
//----------------------------------------------------------------------

DualPointController::DualPointController(
	DataView*                owner,
	GraphicObject*           target, 
	const GPoint&            origin,
	UInt32                   moveEvents,
	UInt32                   execEvents,
	UInt32                   stopEvents
)	:	AbstrController(owner, target, moveEvents, execEvents, stopEvents)
	,	m_Origin(origin)
{
	dms_assert(target);
}

//----------------------------------------------------------------------
// class  : DualPointCaretControlller
//----------------------------------------------------------------------

DualPointCaretController::DualPointCaretController(
		DataView*                owner, 
		AbstrCaret*              caret,
		GraphicObject*           target, 
		const GPoint&            origin,
    	UInt32                   moveEvents, 
    	UInt32                   execEvents, 
		UInt32                   stopEvents
)	:	DualPointController(owner, target, origin, moveEvents, execEvents, stopEvents)
	,	m_Caret(caret)
{
	dms_assert(m_Caret);
	dms_assert(owner);
	owner->InsertCaret(caret);
}

DualPointCaretController::~DualPointCaretController()
{
	dms_assert(m_Caret);
	auto dv = GetOwner().lock();
	if (dv)
		dv->RemoveCaret(m_Caret);
}


bool DualPointCaretController::Move(EventInfo& eventInfo)
{
	auto dv = GetOwner().lock(); if (!dv) return true;
	auto to = GetTargetObject().lock(); if (!to) return true;
	if (m_Caret)
	  dv->MoveCaret(m_Caret, DualPointCaretOperator(m_Origin, eventInfo.m_Point, to.get() ));
	return false;
}

void DualPointCaretController::Stop()
{
	auto dv = GetOwner().lock();
	if (dv)
		dv->MoveCaret(m_Caret, DualPointCaretOperator(UNDEFINED_VALUE(GPoint), UNDEFINED_VALUE(GPoint), 0));
	base_type::Stop();
}


//----------------------------------------------------------------------
// class  : TieCursorController
//----------------------------------------------------------------------

TieCursorController::TieCursorController(
		DataView*      owner, 
		GraphicObject* target, 
		const GRect&   tieRect,   
		UInt32         moveEvents, // = EID_MOUSEDRAG,
		UInt32         stopEvents  // = EID_CLOSE_EVENTS
	)
	:	AbstrController(owner, target, moveEvents, 0, stopEvents)
	,	m_TieRect(tieRect)
{
	if (m_TieRect.top < m_TieRect.bottom) --m_TieRect.bottom;
	if (m_TieRect.left< m_TieRect.right ) --m_TieRect.right;
}

bool TieCursorController::Move (EventInfo& eventInfo)
{
	MakeUpperBound( eventInfo.m_Point, m_TieRect.TopLeft    ());
	MakeLowerBound( eventInfo.m_Point, m_TieRect.BottomRight());
	GPoint mousePoint = eventInfo.m_Point;
	auto dv = GetOwner().lock();
	if (dv)
		dv->SetCursorPos(mousePoint);
	return false;
}

//----------------------------------------------------------------------
// class  : ZoomInControlller
//----------------------------------------------------------------------

#include "geo/Transform.h"
#include "geo/Conversions.h"
#include "ViewPort.h"
#include "Carets.h"

ZoomInController::ZoomInController(
	DataView* owner, 
	ViewPort* target, 
	const CrdTransformation& transformation, 
	const GPoint& origin
)	:	DualPointCaretController(
			owner, 
			new RoiCaret, 
			target, 
			origin,
			EID_MOUSEDRAG,
			EID_LBUTTONUP,
			EID_CLOSE_EVENTS
		)
	,	m_Transformation(transformation)
{
}

// override DualPointCaretController callback
bool ZoomInController::Exec(EventInfo& eventInfo)
{
//	auto dv = GetOwner().lock(); if (!dv) return;
	auto to = GetTargetObject().lock(); if (!to) return true;

	ViewPort* view = debug_cast<ViewPort*>(to.get()); 
	dms_assert(view);

	GPoint size = Abs(eventInfo.m_Point - m_Origin);
	if (size == GPoint(0, 0))
	{
		CrdPoint oldWorldPoint = m_Transformation.Reverse(Convert<CrdPoint>(eventInfo.m_Point)); // curr World location of click location
		view->ZoomIn1();
		auto tr = view->CalcWorldToClientTransformation();
		CrdPoint newWorldPoint = tr.Reverse(Convert<CrdPoint>(eventInfo.m_Point)); // new World location of click location
		view->Pan(oldWorldPoint - newWorldPoint);
		return true;
	}
	CrdRect worldRect = m_Transformation.Reverse(
			Convert<CrdRect>(
				GRect(
					m_Origin, 
					eventInfo.m_Point
				)
			)
		);
	view->SetROI(worldRect);
	return true;
}

//----------------------------------------------------------------------
// class  : ZoomOutControlller
//----------------------------------------------------------------------

ZoomOutController::ZoomOutController(DataView* owner, ViewPort* target, const CrdTransformation& transformation)
:	AbstrController(
			owner, 
			target, 
			0,
			EID_LBUTTONUP,
			EID_CLOSE_EVENTS
		)
	,	m_Transformation(transformation)
{
}

// override DualPointCaretController callback
bool ZoomOutController::Exec(EventInfo& eventInfo)
{
	//	auto dv = GetOwner().lock(); if (!dv) return;
	auto to = GetTargetObject().lock(); if (!to) return true;
	ViewPort* view = debug_cast<ViewPort*>(to.get()); 
	dms_assert(view);

	CrdPoint oldWorldPoint = m_Transformation.Reverse(Convert<CrdPoint>(eventInfo.m_Point)); // curr World location of click location
	view->ZoomOut1();
	auto tr = view->CalcWorldToClientTransformation();
	CrdPoint newWorldPoint = tr.Reverse(Convert<CrdPoint>(eventInfo.m_Point)); // new World location of click location
	view->Pan(oldWorldPoint - newWorldPoint);
	return true;
}

//----------------------------------------------------------------------
// class  : PanControlller
//----------------------------------------------------------------------

PanController::PanController(
	DataView* owner, 
	ViewPort* target, 
	const GPoint& origin
)	:	DualPointController(
			owner, 
			target, 
			origin,
			0,
			EID_LBUTTONUP|EID_MOUSEDRAG,
			EID_LBUTTONUP|EID_CAPTURECHANGED|EID_MOUSEMOVE
		)
	,	m_DidDrag(false)
{}

// override DualPointCaretController callback
bool PanController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	ViewPort* view = debug_cast<ViewPort*>(to.get()); 
	dms_assert(view);

	if (eventInfo.m_Point == m_Origin)
		return m_DidDrag;

	m_DidDrag = true;
	view->Scroll(eventInfo.m_Point - m_Origin);
	m_Origin = eventInfo.m_Point;

	return true;
}

//----------------------------------------------------------------------
// class  : RectPanControlller
//----------------------------------------------------------------------

#include "GraphicRect.h"

RectPanController::RectPanController(
	DataView*    owner, 
	GraphicRect* target, 
	const CrdTransformation& transformation, 
	const GPoint& origin
)	:	DualPointController(owner, target, 
			origin,
			0,
			EID_LBUTTONUP|EID_MOUSEDRAG,
			EID_CLOSE_EVENTS
		)
	,	m_Transformation(transformation)
	,	m_OrgWP (transformation.Reverse( Convert<CrdPoint>(origin) ) )
{
}

// override DualPointController callback
bool RectPanController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	GraphicRect* view = debug_cast<GraphicRect*>(to.get());
	dms_assert(view);

	CrdPoint worldPoint = m_Transformation.Reverse(
		Convert<CrdPoint>(eventInfo.m_Point)
	);
	if (worldPoint == m_OrgWP)
		return false;

	view->MoveWorldRect( worldPoint - m_OrgWP );
	m_OrgWP = worldPoint;

	return true;
}

//----------------------------------------------------------------------
// class  : InfoController
//----------------------------------------------------------------------

#include "act/InvalidationBlock.h"
#include "Theme.h"

void SelectPoint(GraphicLayer* layer, const CrdPoint& worldPoint, EventID eventID)
{
	dms_assert(layer);
	if (IsDefined(worldPoint))
		layer->SelectPoint(worldPoint, eventID);
	else
	{
		InvalidationBlock lock(layer);
		DataWriteLock writeLock(
			const_cast<AbstrDataItem*>(layer->CreateSelectionsTheme()->GetThemeAttr()),
			CompoundWriteType(eventID)
		);
		if (layer->SelectFeatureIndex(writeLock, UNDEFINED_VALUE(SizeT), eventID))
		{
			writeLock.Commit();
			lock.ProcessChange();
		}
	}
}

#include "Theme.h"
#include "GridLayer.h"

void SelectPoints(LayerSet* ls, GraphicLayer* activeLayer, const CrdPoint& worldPoint, EventID eventID)
{
	dms_assert(ls);
	if (!ls->DetailsVisible())
		return;

	SizeT i=ls->NrEntries(); 
	while (i)
	{
		ScalableObject* layerSetElem = ls->GetEntry(--i);
		if (layerSetElem == activeLayer)
			continue;
		LayerSet* subSet = dynamic_cast<LayerSet*>(layerSetElem);
		if (subSet)
			SelectPoints(subSet, activeLayer, worldPoint, eventID);
		else
		{
			GridLayer* gridLayer = dynamic_cast<GridLayer*>(layerSetElem);
			if (gridLayer)
				SelectPoint(gridLayer, worldPoint, eventID);
		}
	}
}

void InfoController::SelectFocusElem(LayerSet* ls, const CrdPoint& worldPoint, EventID eventID)
{
	dms_assert(IsMainThread());
	dms_assert(ls);

	static bool s_LastLBUTTONDBLCLK = false;
	bool
		lButtonUp = (eventID & EID_LBUTTONUP) && !s_LastLBUTTONDBLCLK,
		lButtonDblClk = (eventID & EID_LBUTTONDBLCLK);

	if (lButtonUp || lButtonDblClk)
	{
		GraphicLayer* layer = ls->GetActiveLayer();
		if (layer)
			SelectPoint(layer, worldPoint, eventID);

		if (lButtonUp)
			SelectPoints(ls, layer, worldPoint, eventID);
	}
	s_LastLBUTTONDBLCLK = lButtonDblClk;
}

//----------------------------------------------------------------------
// class  : SelectObjectController
//----------------------------------------------------------------------

SelectObjectController::SelectObjectController(
	DataView* owner, 
	ViewPort* target, 
	const CrdTransformation& transformation
)	:	AbstrController(owner, target, 0, EID_MOUSEDRAG|EID_LBUTTONUP, EID_CLOSE_EVENTS)
	,	m_Transformation(transformation)
{}

// override PointCaretController callback
bool SelectObjectController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;

	LayerSet* ls = debug_cast<ViewPort*>(to.get())->GetLayerSet();
	if (!ls)
		return false;

	GraphicLayer* layer = ls->GetActiveLayer();
	if (!layer)
		return false;

	layer	->	SelectPoint(
					m_Transformation.Reverse(
						Convert<CrdPoint>(eventInfo.m_Point)
					),
					EventID(eventInfo.m_EventID | UInt32(EID_REQUEST_SEL))
				);
	return true;
}

//----------------------------------------------------------------------
// class  : SelectRectController
//----------------------------------------------------------------------

SelectRectController::SelectRectController(
	DataView*                owner, 
	ViewPort*                target, 
	const CrdTransformation& transformation, 
	const GPoint&            origin
)	:	DualPointCaretController(
			owner, 
			new RectCaret, 
			target, 
			origin,
			EID_MOUSEDRAG|EID_LBUTTONUP,
			EID_LBUTTONUP,
			EID_CLOSE_EVENTS
		)
	,	m_Transformation(transformation)
{
}

// override PointCaretController callback
bool SelectRectController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	LayerSet* ls = debug_cast<ViewPort*>(to.get())->GetLayerSet();
	if (!ls)
		return false;
	GraphicLayer* layer = ls->GetActiveLayer();
	if (!layer)
		return false;

	layer	->	SelectRect(
					m_Transformation.Reverse(
						Convert<CrdRect>( GRect(eventInfo.m_Point, m_Origin) )
					),
					EventID(eventInfo.m_EventID | UInt32(EID_REQUEST_SEL))
				);
	return true;
}

//----------------------------------------------------------------------
// class  : SelectCircleController
//----------------------------------------------------------------------

SelectCircleController::SelectCircleController(
	DataView*                owner, 
	ViewPort*                target, 
	const CrdTransformation& transformation, 
	const GPoint&            origin
)	:	DualPointCaretController(
			owner, 
			new CircleCaret,
			target, 
			origin,
			EID_MOUSEDRAG|EID_LBUTTONUP,
			EID_LBUTTONUP,
			EID_CLOSE_EVENTS
		)
	,	m_Transformation(transformation)
{
}

// override PointCaretController callback
bool SelectCircleController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	LayerSet* ls = debug_cast<ViewPort*>(to.get())->GetLayerSet();
	if (!ls)
		return false;
	GraphicLayer* layer = ls->GetActiveLayer();
	if (!layer)
		return false;
	CrdPoint orgPoint = m_Transformation.Reverse( Convert<CrdPoint>(m_Origin         ) );
	CrdPoint dstPoint = m_Transformation.Reverse( Convert<CrdPoint>(eventInfo.m_Point) );
	layer	->	SelectCircle(
					orgPoint,
					sqrt( 
						SqrDist<CrdType>(
							Convert<CrdPoint>(orgPoint), 
							Convert<CrdPoint>(dstPoint)
						)
					),
					EventID(eventInfo.m_EventID | UInt32(EID_REQUEST_SEL))
				);
	return true;
}

//----------------------------------------------------------------------
// class  : SelectDistrictController
//----------------------------------------------------------------------

SelectDistrictController::SelectDistrictController(
	DataView* owner, 
	ViewPort* target, 
	const CrdTransformation& transformation
)	:	AbstrController(owner, target, 0, EID_MOUSEDRAG|EID_LBUTTONUP, EID_CLOSE_EVENTS)
	,	m_Transformation(transformation)
{}

// override PointCaretController callback
bool SelectDistrictController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	LayerSet* ls = debug_cast<ViewPort*>(to.get())->GetLayerSet();
	if (!ls)
		return false;
	GraphicLayer* layer = ls->GetActiveLayer();
	if (!layer)
		return false;
	layer	->	SelectDistrict(
					m_Transformation.Reverse(
						Convert<CrdPoint>(eventInfo.m_Point)
					),
					EventID(eventInfo.m_EventID)
				);
	return true;
}

//----------------------------------------------------------------------
// class  : DrawPolygonController
//----------------------------------------------------------------------
#include "Carets.h"

DrawPolygonController::DrawPolygonController(
		DataView*                owner, 
		ViewPort*                target,
		const CrdTransformation& transformation, 
		const GPoint&            origin,
		bool                     drawEndLine
	)
	:	AbstrController(owner, target
		,	EID_MOUSEMOVE|EID_MOUSEDRAG|EID_LBUTTONDOWN|EID_LBUTTONUP|EID_LBUTTONDBLCLK
		,	EID_LBUTTONDBLCLK
		,	EID_LBUTTONDBLCLK|EID_RBUTTONDOWN|EID_CAPTURECHANGED|EID_SCROLLED
		)
	,	m_EndLineCaret(0)
	,	m_PolygonCaret(0)
	,	m_Transformation(transformation)
{
	dms_assert(owner);
	AddPoint(origin);
	if (drawEndLine)
	{
		m_EndLineCaret = new LineCaret(CombineRGB(0xFF, 0x00, 0x00)) ;
		owner->InsertCaret( m_EndLineCaret );
		owner->MoveCaret(
			m_EndLineCaret, 
			DualPointCaretOperator(origin, origin, target)
		);
	}
}

DrawPolygonController::~DrawPolygonController()
{
	auto dv = GetOwner().lock(); if (!dv) return;
	std::vector<AbstrCaret*>::iterator
		b = m_LineCarets.begin(),
		e = m_LineCarets.end();
	for (; b!=e; ++b)
		dv->RemoveCaret(*b);
	if (m_EndLineCaret)
		dv->RemoveCaret(m_EndLineCaret);
	if (m_PolygonCaret)
		dv->RemoveCaret(m_PolygonCaret);
}

bool DrawPolygonController::Move(EventInfo& eventInfo)
{
	auto dv = GetOwner().lock(); if (!dv) return true;
	auto to = GetTargetObject().lock(); if (!to) return true;
	dv->MoveCaret(
		m_LineCarets.back(), 
		DualPointCaretOperator(m_Points.back(), eventInfo.m_Point, to.get() )
	);
	if (m_EndLineCaret)
	dv->MoveCaret(
		m_EndLineCaret, 
		DualPointCaretOperator(eventInfo.m_Point, m_Points.front(), to.get() )
	);

	if (eventInfo.m_EventID & (EID_LBUTTONDOWN|EID_LBUTTONUP|EID_LBUTTONDBLCLK))
		AddPoint(eventInfo.m_Point);
	return true;
}

bool DrawPolygonController::Exec(EventInfo& eventInfo)
{
	dms_assert(!m_PolygonCaret);

	auto dv = GetOwner().lock(); if (!dv) return true;

	LayerSet* ls = GetViewPort()->GetLayerSet();
	if (!ls)
		return false;
	GraphicLayer* layer = ls->GetActiveLayer();
	if (!layer)
		return false;

	GPoint
		*i = begin_ptr(m_Points),
		*e = end_ptr  (m_Points);

	m_PolygonCaret = new PolygonCaret(i, e);
	m_PolygonCaret->SetStartPoint(GPoint(0, 0));
	dv->InsertCaret(m_PolygonCaret );

	std::vector<CrdPoint> worldPoints;
	worldPoints.reserve(m_Points.size());

	while (i!=e)
		worldPoints.push_back(m_Transformation.Reverse( Convert<CrdPoint>(*i++) ) );

	layer	->	SelectPolygon(
					begin_ptr(worldPoints),
					end_ptr  (worldPoints),
					EventID(eventInfo.m_EventID | UInt32(EID_REQUEST_SEL))
				);
	return true;
}

void DrawPolygonController::AddPoint(GPoint pnt)
{
	auto dv = GetOwner().lock(); if (!dv) return;
	auto to = GetTargetObject().lock(); if (!to) return;

	if (m_Points.size() && m_Points.back() == pnt)
		return;

	m_Points.push_back(pnt);
	m_LineCarets.push_back(new LineCaret(CombineRGB(0x00, 0x00, 0xFF)));
	dv->InsertCaret(m_LineCarets.back() );
	dv->MoveCaret(
		m_LineCarets.back(), 
		DualPointCaretOperator(pnt, pnt, to.get() )
	);
}

ViewPort* DrawPolygonController::GetViewPort()
{
	auto to = GetTargetObject().lock(); if (!to) return nullptr;
	return debug_cast<ViewPort*>(to.get());
}

