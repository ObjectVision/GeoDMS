// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#pragma hdrstop

#include <numbers>

#include "act/MainThread.h"
#include "dbg/debug.h"
#include "geo/Area.h"
#include "geo/DynamicPoint.h"

#include "Param.h"
#include "Projection.h"

#include "Controllers.h"
#include "MovableContainer.h"
#include "DataView.h"
#include "CaretOperators.h"
#include "GraphVisitor.h"
#include "LayerSet.h"
#include "ViewPort.h"

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


AbstrController::AbstrController(DataView* owner, GraphicObject* target
	, EventID moveEvents, EventID execEvents, EventID stopEvents
	, ToolButtonID toolID)
	:	m_Owner(owner->weak_from_this())
	,	m_TargetObject(target->weak_from_this())
	,	m_MoveEvents(moveEvents), m_ExecEvents(execEvents), m_StopEvents(stopEvents)
	,	m_ToolID(toolID)
{}

AbstrController::~AbstrController()
{}

static TokenID meterToken1 = GetTokenID_st("metre");
static TokenID meterToken2 = GetTokenID_st("meter");
static TokenID kmToken = GetTokenID_st("km");

static TokenID degreeToken = GetTokenID_st("degree");
static TokenID minuteToken = GetTokenID_st("min");
static TokenID secondToken = GetTokenID_st("sec");

bool isMeterToken(TokenID token)
{

	return token == meterToken1 || token == meterToken2;
}

bool isDegreeToken(TokenID token)
{

	return token == degreeToken;
}

bool AbstrController::SendStatusText(CharPtr format, CrdType dst, CrdType dst2) const
{
	auto dv = GetOwner().lock();
	if (!dv)
		return false;

	auto to = GetTargetObject().lock();

	UnitLabelScalePair lsPair;
	if (!m_UnitLabelScalePtr)
	{
		m_UnitLabelScalePtr = std::make_unique<UnitLabelScalePair>();
		if (to)
			if (auto vp = dynamic_cast<ViewPort*>(to.get()))
				if (auto worldCrdUnit = vp->GetWorldCrdUnit())
					if (auto projection = worldCrdUnit->GetProjection())
						*m_UnitLabelScalePtr = projection->GetUnitlabeledScalePair();
					else
						*m_UnitLabelScalePtr = AsUnit(worldCrdUnit->GetUltimateItem())->GetUnitlabeledScalePair();
	}

	MG_CHECK(m_UnitLabelScalePtr);
	TokenID unitToken = m_UnitLabelScalePtr->first;

	if (isMeterToken(unitToken))
	{
		if (dst > 1000)
		{
			dst /= 1000;
			dst2 /= 1000000;
			unitToken = kmToken;
		}
	}
	if (isDegreeToken(unitToken))
	{
		if (abs(dst) < 1.0)
		{
			dst *= 60;
			dst2 *= 60*60;
			unitToken = minuteToken;
		}
		if (abs(dst) < 1.0)
		{
			dst *= 60;
			dst2 *= 60 * 60;
			unitToken = secondToken;
		}
	}

	// insert some replacements here

	auto unitLabel = unitToken.GetStr();

	char buffer[201];
	auto nrBytesWritten = myFixedBufferWrite(buffer, 200, format, dst, unitLabel.c_str(), dst2, unitLabel.c_str());
	buffer[nrBytesWritten] = char(0); // truncate
	dv->SendStatusText(SeverityTypeID::ST_MinorTrace, buffer);
	return true;
}

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

PointCaretController::PointCaretController(DataView* owner, AbstrCaret* caret, GraphicObject* target
	,	EventID moveEvents, EventID execEvents, EventID stopEvents, ToolButtonID toolID)
	:	AbstrController(owner, target, moveEvents, execEvents, stopEvents, toolID)
	,	m_Caret(caret)
{
	assert(m_Caret);
	assert(target);
	owner->InsertCaret(caret);
}

PointCaretController::~PointCaretController()
{
	assert(m_Caret);
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

DualPointController::DualPointController(DataView* owner, GraphicObject* target, const GPoint& origin
	, EventID moveEvents, EventID execEvents, EventID stopEvents, ToolButtonID toolID)
	:	AbstrController(owner, target, moveEvents, execEvents, stopEvents, toolID)
	,	m_Origin(origin)
{
	assert(target);
}

//----------------------------------------------------------------------
// class  : DualPointCaretControlller
//----------------------------------------------------------------------

DualPointCaretController::DualPointCaretController(DataView* owner, AbstrCaret* caret
	,	GraphicObject* target,  const GPoint& origin
	, EventID moveEvents, EventID execEvents, EventID stopEvents
	,	ToolButtonID toolID)
	:	DualPointController(owner, target, origin, moveEvents, execEvents, stopEvents, toolID)
	,	m_Caret(caret)
{
	assert(m_Caret);
	assert(owner);
	owner->InsertCaret(caret);
}

DualPointCaretController::~DualPointCaretController()
{
	assert(m_Caret);
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

TieCursorController::TieCursorController(DataView* owner, GraphicObject* target
	,	GRect tieRect
	, EventID moveEvents /*EventID::MOUSEDRAG*/, EventID stopEvents  /*EventID::CLOSE_EVENTS*/ )
	:	AbstrController(owner, target, moveEvents, EventID::NONE, stopEvents, ToolButtonID::TB_Undefined)
	,	m_TieRect(tieRect)
{
	if (m_TieRect.top < m_TieRect.bottom) --m_TieRect.bottom;
	if (m_TieRect.left< m_TieRect.right ) --m_TieRect.right;
}

bool TieCursorController::Move (EventInfo& eventInfo)
{
	MakeUpperBound( eventInfo.m_Point, m_TieRect.LeftTop());
	MakeLowerBound( eventInfo.m_Point, m_TieRect.RightBottom());
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

ZoomInController::ZoomInController(DataView* owner, ViewPort* target
	,	const CrdTransformation& transformation, const GPoint& origin)
	:	DualPointCaretController(owner, new RoiCaret, target, origin
		, EventID::MOUSEDRAG, EventID::LBUTTONUP, EventID::CLOSE_EVENTS, ToolButtonID::TB_ZoomIn2)
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
		CrdPoint oldWorldPoint = m_Transformation.Reverse(g2dms_order<Float64>(eventInfo.m_Point)); // curr World location of click location
		view->ZoomIn1();
		auto tr = view->CalcWorldToClientTransformation();
		CrdPoint newWorldPoint = tr.Reverse(g2dms_order<Float64>(eventInfo.m_Point)); // new World location of click location
		view->Pan(oldWorldPoint - newWorldPoint);
		return true;
	}
	auto deviceRect = CrdRect(g2dms_order<Float64>(m_Origin), g2dms_order<Float64>(eventInfo.m_Point));
	auto worldRect = m_Transformation.Reverse(deviceRect);
	view->SetROI(worldRect);
	return true;
}

//----------------------------------------------------------------------
// class  : ZoomOutControlller
//----------------------------------------------------------------------

ZoomOutController::ZoomOutController(DataView* owner, ViewPort* target, const CrdTransformation& transformation)
	:	AbstrController(owner, target, EventID::NONE, EventID::LBUTTONUP, EventID::CLOSE_EVENTS, ToolButtonID::TB_ZoomOut2)
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

	CrdPoint oldWorldPoint = m_Transformation.Reverse(g2dms_order<Float64>(eventInfo.m_Point)); // curr World location of click location
	view->ZoomOut1();
	auto tr = view->CalcWorldToClientTransformation();
	CrdPoint newWorldPoint = tr.Reverse(g2dms_order<Float64>(eventInfo.m_Point)); // new World location of click location
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
)	:	DualPointController(owner, target, origin
		,	EventID::NONE
		,	EventID::LBUTTONUP|EventID::MOUSEDRAG
		,	EventID::LBUTTONUP|EventID::CAPTURECHANGED|EventID::MOUSEMOVE
		,	ToolButtonID::OBSOLETE_TB_Pan)
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
	view->ScrollDevice(eventInfo.m_Point - m_Origin);
	m_Origin = eventInfo.m_Point;

	return true;
}

//----------------------------------------------------------------------
// class  : RectPanControlller
//----------------------------------------------------------------------

#include "GraphicRect.h"

RectPanController::RectPanController(DataView* owner, GraphicRect* target
	,	const CrdTransformation& transformation, const GPoint& origin)
	:	DualPointController(owner, target, origin
		, EventID::NONE
		, EventID::LBUTTONUP|EventID::MOUSEDRAG
		, EventID::CLOSE_EVENTS
		, ToolButtonID::OBSOLETE_TB_Pan)
	,	m_Transformation(transformation)
	,	m_OrgWP (transformation.Reverse(g2dms_order<Float64>(origin)) )
{}

// override DualPointController callback
bool RectPanController::Exec(EventInfo& eventInfo)
{
	auto to = GetTargetObject().lock(); if (!to) return true;
	GraphicRect* view = debug_cast<GraphicRect*>(to.get());
	dms_assert(view);

	CrdPoint worldPoint = m_Transformation.Reverse(g2dms_order<Float64>(eventInfo.m_Point));
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
		lButtonUp = (eventID & EventID::LBUTTONUP) && !s_LastLBUTTONDBLCLK,
		lButtonDblClk = (eventID & EventID::LBUTTONDBLCLK);

	if (lButtonUp || lButtonDblClk)
	{
		ObjectMsgGenerator thisMsgGenerator(nullptr, "SelectPoint");
		Waiter waiter(&thisMsgGenerator);
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
)	:	AbstrController(owner, target
		, EventID::NONE
		, EventID::MOUSEDRAG|EventID::LBUTTONUP
		, EventID::CLOSE_EVENTS
		, ToolButtonID::TB_SelectObject)
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
					m_Transformation.Reverse(g2dms_order<Float64>(eventInfo.m_Point))
				,	eventInfo.m_EventID | EventID::REQUEST_SEL
				);
	return true;
}

//----------------------------------------------------------------------
// class  : SelectRectController
//----------------------------------------------------------------------

SelectRectController::SelectRectController(DataView* owner, ViewPort* target
	,	const CrdTransformation& transformation, const GPoint& origin)
	:	DualPointCaretController(owner, new RectCaret
		,	target, origin
		,	EventID::MOUSEDRAG|EventID::LBUTTONUP, EventID::LBUTTONUP, EventID::CLOSE_EVENTS, ToolButtonID::TB_SelectRect)
	,	m_Transformation(transformation)
{}

bool SelectRectController::Move(EventInfo& eventInfo)
{
	auto result = DualPointCaretController::Move(eventInfo);
	CrdPoint orgPoint = m_Transformation.Reverse(g2dms_order<CrdType>(m_Origin));
	CrdPoint dstPoint = m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point));
	CrdPoint deltaPoint = dstPoint - orgPoint;
	CrdType dst = sqrt(Norm<CrdType>(deltaPoint));
	CrdType dst2 = CrdType(deltaPoint.first) * CrdType(deltaPoint.second);

	SendStatusText("Diagonal: % 10.2f[% s]; Area: % 10.2f[% s ^ 2]", dst, dst2);
	eventInfo.m_EventID |= EventID::TEXTSENT;

	return result;
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

	auto deviceRect = CrdRect(g2dms_order<Float64>(eventInfo.m_Point), g2dms_order<Float64>(m_Origin));
	auto worldRect = m_Transformation.Reverse(deviceRect);
	layer->SelectRect(worldRect, eventInfo.m_EventID | EventID::REQUEST_SEL);

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
)	:	DualPointCaretController(owner, new CircleCaret, target,  origin
		,	EventID::MOUSEDRAG|EventID::LBUTTONUP, EventID::LBUTTONUP,EventID::CLOSE_EVENTS, ToolButtonID::TB_SelectCircle)
	,	m_Transformation(transformation)
{
}

bool SelectCircleController::Move(EventInfo& eventInfo)
{
	auto result = DualPointCaretController::Move(eventInfo);
	CrdPoint orgPoint = m_Transformation.Reverse(g2dms_order<CrdType>(m_Origin));
	CrdPoint dstPoint = m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point));
	CrdType dst2 = SqrDist<CrdType>(orgPoint, dstPoint);
	CrdType dst = sqrt(dst2);

	SendStatusText("Radius: % 10.2f[%s]; Area: % 10.2f[%s^2]", dst, std::numbers::pi_v<Float64> *dst2);
	eventInfo.m_EventID |= EventID::TEXTSENT;

	return result;
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
	CrdPoint orgPoint = m_Transformation.Reverse(g2dms_order<CrdType>(m_Origin         ) );
	CrdPoint dstPoint = m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point) );
	auto dist = sqrt(SqrDist<CrdType>(orgPoint, dstPoint));
	layer	->	SelectCircle(orgPoint, dist, eventInfo.m_EventID | EventID::REQUEST_SEL);
	return true;
}

//----------------------------------------------------------------------
// class  : SelectDistrictController
//----------------------------------------------------------------------

SelectDistrictController::SelectDistrictController(
	DataView* owner, 
	ViewPort* target, 
	const CrdTransformation& transformation
)	:	AbstrController(owner, target
		,	EventID::NONE
		,	EventID::MOUSEDRAG|EventID::LBUTTONUP
		,	EventID::CLOSE_EVENTS
		,	ToolButtonID::TB_SelectDistrict)
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
	auto worldPoint = m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point));
	layer	->	SelectDistrict(worldPoint, EventID(eventInfo.m_EventID));
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
		,	EventID::MOUSEMOVE|EventID::MOUSEDRAG|EventID::LBUTTONDOWN|EventID::LBUTTONUP|EventID::LBUTTONDBLCLK
		,	EventID::LBUTTONDBLCLK
		,	EventID::LBUTTONDBLCLK|EventID::RBUTTONDOWN|EventID::CAPTURECHANGED|EventID::SCROLLED
		,	ToolButtonID::TB_SelectPolygon
		)
	,	m_EndLineCaret(0)
	,	m_PolygonCaret(0)
	,	m_Transformation(transformation)
{
	assert(owner);
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

	MG_CHECK(m_Points.size() >= 1);
	if (m_Points.size() == 1)
	{
		auto basePoint = m_Transformation.Reverse(g2dms_order<CrdType>(m_Points[0]));
		auto currPoint = m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point));

		CrdType dst2 = SqrDist<CrdType>(basePoint, currPoint);
		CrdType dst = sqrt(dst2);

		SendStatusText("Distance: % 10.2f[%s]; Area: % 10.2f[%s^2]", dst, 0.0);
	}
	else
	{
		std::vector<CrdPoint> worldPoints; worldPoints.reserve(m_Points.size() + 1);
		for (auto p : m_Points)
			worldPoints.emplace_back(m_Transformation.Reverse(g2dms_order<CrdType>(p)));
		worldPoints.emplace_back(m_Transformation.Reverse(g2dms_order<CrdType>(eventInfo.m_Point)));
		worldPoints.emplace_back(worldPoints.front());

		auto perimeter = ArcLength<Float64>(worldPoints.begin(), worldPoints.end());
		auto area = Area<Float64>(worldPoints.begin(), worldPoints.end());

		SendStatusText("Perimeter: % 10.2f[%s]; Area: % 10.2f[%s^2]", perimeter, area);
	}

	eventInfo.m_EventID |= EventID::TEXTSENT;

	if (eventInfo.m_EventID & (EventID::LBUTTONDOWN|EventID::LBUTTONUP|EventID::LBUTTONDBLCLK))
		AddPoint(eventInfo.m_Point);
	return true;
}

bool DrawPolygonController::Exec(EventInfo& eventInfo)
{
	assert(!m_PolygonCaret);

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
		worldPoints.emplace_back(m_Transformation.Reverse(g2dms_order<CrdType>(*i++) ) );

	layer->SelectPolygon(begin_ptr(worldPoints), end_ptr  (worldPoints), eventInfo.m_EventID | EventID::REQUEST_SEL);

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

