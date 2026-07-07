// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ViewPort.h"
#include "../res/Resource.h"

#include "act/InvalidationBlock.h"
#include "act/ActorVisitor.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/DebugCast.h"
#include "geo/Conversions.h"
#include "mci/Class.h"
#include "mci/ValueComposition.h"
#include "set/Token.h"
#include "utl/mySPrintF.h"

#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Unit.h"
#include "UnitClass.h"

#include "StgBase.h"
#include "gdal/gdal_base.h"

#include "CaretOperators.h"
#include "Carets.h"
#include "ClipBoard.h"
#include "Controllers.h"
#include "DataView.h"
#include "GraphicContainer.h"
#include "GraphicRect.h"
#include "GridCoord.h"
#include "IndexCollector.h"
#include "KeyFlags.h"
#include "LayerSet.h"
#include "MouseEventDispatcher.h"
#include "ScaleBar.h"
#include "SelCaret.h"
#include "WmsLayer.h"

//----------------------------------------------------------------------
// class  : ViewPoint
//----------------------------------------------------------------------

#include "ser/MoreStreamBuff.h"

ViewPoint::ViewPoint(CharPtrRange viewPointStr)
{
	auto streamWrap = MemoInpStreamBuff(viewPointStr.first, viewPointStr.second);
	FormattedInpStream inp(&streamWrap);
	inp >> "X=" >> center.Col() >> "; Y=" >> center.Row() >> "; ZL=" >> zoomLevel;

}

bool ViewPoint::WriteAsString(char* buffer, SizeT len, FormattingFlags flags)
{
	auto streamWrap = SilentMemoOutStreamBuff(ByteRange(buffer, len));
	//FormattedOutStream out(&streamWrap, flags);

	static SharedStr format = SharedStr("X= {: 10.2f}; Y= {: 10.2f}; ZL= {: 10.2f}");

	auto nrBytesWritten = myFixedBufferWrite(buffer, len, format.c_str(), center.Col(), center.Row(), zoomLevel);
	buffer[nrBytesWritten] = char(0); // truncate

	//out << "X=" << center.Col() << "; Y=" << center.Row() << "; ZL=" << zoomLevel;

	if (streamWrap.CurrPos() >= len)
		return false;
	//out << char(0);
	return true;
}


//----------------------------------------------------------------------
// class  : ViewPort
//----------------------------------------------------------------------

ViewPort::ViewPort(MovableObject* owner, DataView* dv, CharPtr caption) 
	:	Wrapper(owner, dv, caption)
	,	m_Orientation(OrientationType::NegateY)
	,	m_NeedleCaret(nullptr)
	,	m_ScaleBarCaret(nullptr)
	,	m_BrushOrg(0, 0)
	,	m_BkColor(DmsColor2COLORREF( STG_Bmp_GetDefaultColor(CI_BACKGROUND) )) // White, adjustable by tools->options->Current configuration
{
	SetViewPortCursor(DmsCursor::Arrow);
#ifdef _WIN32
	SetViewPortCursor(LoadCursor(NULL, IDC_ARROW));
#endif
	m_State.Set(GOF_IgnoreActivation);
}

ViewPort::~ViewPort()
{
	auto dv = GetDataView().lock();
	if (m_NeedleCaret && dv)
	{
		dbg_assert( DataViewOK() );
		dv->RemoveCaret(m_NeedleCaret);
	}
	if (m_ScaleBarCaret && dv)
	{
		dbg_assert( DataViewOK() );
		dv->RemoveCaret(m_ScaleBarCaret);
	}
	ClearContents(); // make sure all SelCarets and GridCoords (indirect property) are destroyed
}

#define ROI_TL_NAME "RoiTopLeft"
#define ROI_BR_NAME "RoiBottomRight"
static TokenID t_RoiTL = GetTokenID_st(ROI_TL_NAME);
static TokenID t_RoiBR = GetTokenID_st(ROI_BR_NAME);
static TokenID t_WCU = GetTokenID_st("WorldCrdUnit");
static TokenID vpminsID = GetTokenID_st("ViewPortMinSize");
static TokenID vpmaxsID = GetTokenID_st("ViewPortMaxSize");
static TokenID vpdefsID = GetTokenID_st("ViewPortDefaultSize");

// Fallback view size (in metres, scaled to coordinate units) when zooming to a
// single-point / zero-area extent with no background WMS layer to snap to (issue #374).
constexpr CrdType DEFAULT_POINT_VIEW_SIZE_IN_METERS = 1000.0;

Float64 GetSubItemValue(const TreeItem* context, TokenID id, Float64 defaultVal); // defined below

void ViewPort::Sync(TreeItem* context, ShvSyncMode sm) 
{
	ObjectContextHandle contextHandle(context, "ViewPort::Sync");

	base_type::Sync(context, sm);
	if (sm == SM_Save)
		return;
	const AbstrDataItem* roiTL = dynamic_cast<const AbstrDataItem*>(
		FindTreeItemByID(context, t_RoiTL )
	);
	if (roiTL)
		InitWorldCrdUnit(roiTL->GetAbstrValuesUnit());
}


void ViewPort::InitWorldCrdUnit(const AbstrUnit* worldCrdUnit) 
{
	dms_assert(!SuspendTrigger::DidSuspend());
	worldCrdUnit = GetWorldCrdUnitFromGeoUnit(worldCrdUnit);
	if (m_WorldCrdUnit)
		if (m_WorldCrdUnit.get() == worldCrdUnit || !worldCrdUnit)
			return;

	TreeItem* context = GetContext();
	if (worldCrdUnit)
	{
		if (m_WorldCrdUnit)
			m_WorldCrdUnit->UnifyValues(worldCrdUnit, "Already set WorldCrdUnit", "new WorldCrdUnit", UnifyMode(UM_AllowTypeDiff | UM_Throw | UM_AllowDefaultLeft)); // worldCrdUnit must not have less metrc/projection
		m_WorldCrdUnit = make_shared_tree(worldCrdUnit, existing_obj{});
	}
	else
	{
		dms_assert(!m_WorldCrdUnit); // follows from not returning prematurely
		m_WorldCrdUnit = make_shared_tree(
				Unit<CrdPoint>::GetStaticClass()->CreateUnit(
					context
				,	t_WCU
				).get(), existing_obj{});
	}
	Invalidate();
}

CrdTransformation ViewPort::CalcWorldToClientTransformation() const
{
	UpdateView();
	return GetCurrWorldToClientTransformation();
}

CrdTransformation ViewPort::CalcCurrWorldToClientTransformation() const
{
	CrdRect clientRect(Point<CrdType>(0, 0), GetCurrClientSize());
	CrdTransformation base(
		const_cast<ViewPort*>(this)->GetROI(),
		clientRect,
		m_Orientation,
		m_FitMode
	);
	// Parity: a north-up, untilted view stays byte-identical to the historical level-c transform.
	if (m_Rotation == 0.0 && m_Tilt == 0.0)
		return base;

	// View rotation/tilt are applied in CLIENT space about the view centre, so they compose AFTER the
	// world->client fit. f*g = g(f(x)): apply base, then yaw, then tilt. Result is level-d (yaw) / level-f
	// (tilt). All downstream consumers read m_w2vTr via Apply/Reverse/GetGrid2DeviceTransformation.
	CrdPoint clientCentre = Center(clientRect);
	CrdTransformation tr = base;
	if (m_Rotation != 0.0)
		tr *= CrdTransformation::Rotation(m_Rotation, clientCentre);
	if (m_Tilt != 0.0)
		tr *= CrdTransformation::Tilt(m_Tilt, clientCentre, GetCurrClientSize().Row());
	return tr;
}

const CrdType ViewPort::s_MaxTilt = 1.308996939; // 75 degrees in radians (beyond ~90 deg the image flips upside down)

// SetRotation/SetTilt change a CalcCurrWorldToClientTransformation input. They must NOT call
// DoUpdateView() directly: that re-inits every GridCoord and could collide with a suspended/in-flight
// draw (a threading race -> crash). Instead defer like SetClientSize (another transform change):
// InvalidateView() clears GOF_IsUpdated so the next UpdateView pass (main thread, try/catch-guarded,
// before the draw) rebuilds m_w2vTr + re-inits the GridCoords; InvalidateDraw() schedules that repaint.
void ViewPort::SetRotation(CrdType yawRad)
{
	if (m_Rotation == yawRad)
		return;
	m_Rotation = yawRad;
	InvalidateView();
	InvalidateDraw();
	UpdateScaleBar();
}

void ViewPort::SetTilt(CrdType pitchRad)
{
	MakeMin(pitchRad, s_MaxTilt); // cap at 75 deg: beyond ~90 deg the plane would flip upside down
	MakeMax(pitchRad, CrdType(0)); // 0 deg = top view (pure affine); never tilt below 0 (no horizon below the view centre)
	if (m_Tilt == pitchRad)
		return;
	m_Tilt = pitchRad;
	InvalidateView();
	InvalidateDraw();
	UpdateScaleBar();
}

void ViewPort::DoUpdateView()
{
	dms_assert(GetContents());

	if (!GetContents())
		return;
	if (dynamic_cast<LayerSet*>(GetContents()) && !GetContents()->NrEntries())
		return;

	if (m_Tracker)
		m_Tracker->AdjustTargetVieport();

	if (GetROI().empty() || GetCurrClientSize().X() == 0 || GetCurrClientSize().Y() == 0)
		return;

	CrdTransformation w2vTr = CalcCurrWorldToClientTransformation();

	if (m_w2vTr == w2vTr)
		return;

	m_w2vTr = w2vTr;

	InvalidateDraw();
	UpdateScaleBar();
	m_cmdTransformChanged();

	auto sf = GetScaleFactors();
	auto deviceSize = ScaleCrdPoint(GetCurrClientSize(), sf);
	auto w2dTr = m_w2vTr;
	w2dTr *= CrdTransformation(CrdPoint(0.0, 0.0), sf);

	for (auto& gc: m_GridCoordMap)
		if (auto gridCoord = gc.second.lock())
			gridCoord->Init(CrdPoint2GPoint(deviceSize), w2dTr);

	for (auto& sc: m_SelCaretMap)
	{
		if (auto selCaret = sc.second.lock())
			selCaret->OnZoom();
	}
}

bool ViewPort::Draw(GraphDrawer& d) const
{
	if (!d.IsInOnPaint())	// when drawing from OnPaint, ReverseCarets undoes this
		for (const auto& sc: m_SelCaretMap)
			if (auto selCaret = sc.second.lock())
				selCaret->UpdateRgn(d.GetAbsClipRegion());

	return false;
}

void ViewPort::SetClientSize(CrdPoint newSize)
{
	if (GetCurrClientSize() == newSize)
		return;

	base_type::SetClientSize(newSize);

	InvalidateView();
	InvalidateDraw();
}

void ViewPort::UpdateScaleBar()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	if (m_ScaleBarCaret)
		dv->MoveCaret(m_ScaleBarCaret, ScaleBarCaretOperator() );
}

void ViewPort::InvalidateOverlapped()
{
	auto dv = GetDataView().lock(); if (!dv) return;
	if (m_ScaleBarCaret)
		dv->InvalidateDeviceRect(CrdRect2GRect(m_ScaleBarCaret->GetCurrDeviceExtents()));
}

void ViewPort::OnChildSizeChanged()
{
}


void ViewPort::ToggleNeedleController()
{
	m_State.Toggle(VPF_NeedleVisible);
}

void ViewPort::SetNeedle(CrdTransformation* tr, GPoint trackPoint)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	assert(tr);
	dbg_assert(DataViewOK());

	if (IsNeedleVisible())
	{
		if (!m_NeedleCaret)
			dv->InsertCaret(m_NeedleCaret = new NeedleCaret);

		NeedleCaretOperator co(trackPoint, CrdRect2GRect(GetCurrClientAbsDeviceRect()), this);
		dv->MoveCaret(m_NeedleCaret, co);
	}
	else
	{
		if (m_NeedleCaret)
		{
			dv->RemoveCaret(m_NeedleCaret);
			m_NeedleCaret = nullptr;
		}
	}
}

ScalableObject* ViewPort::GetContents()
{
	return debug_cast<ScalableObject*>(base_type::GetContents()); 
}

const ScalableObject* ViewPort::GetContents() const
{
	return debug_cast<const ScalableObject*>(base_type::GetContents()); 
}

const AbstrUnit* ViewPort::GetWorldCrdUnit() const
{
	return m_WorldCrdUnit.get();
}

CrdRect WorldRect_AddBorderPixels(CrdRect result, CrdRect viewPort, const ScalableObject* go, OrientationType orientation)
{
	if (!result.empty() && !result.is_max())
	{
		TRect border = go->GetBorderLogicalExtents();

		if (	not border.empty()
			&&	IsStrictlyLower(Convert<CrdPoint>(border.Size()), Size(viewPort)))
		{
			CrdRect crdBorder = Convert<CrdRect>(border);
			auto tr = CrdTransformation(result, Convert<CrdRect>(viewPort) - crdBorder, orientation);
			if (!tr.IsSingular())
			{
				auto worldBorder = tr.WorldScale(crdBorder);
				result += worldBorder;
			}
		}
	}
	return result;
}

static ViewPort* g_CurrVpZoom = nullptr;

CrdRect calcWorldFullRect(CrdRect viewPort, const ScalableObject* go, OrientationType orientation)
{
	auto wcr = go->CalcWorldClientRect();
	return WorldRect_AddBorderPixels(wcr, viewPort, go, orientation);
}

CrdRect calcSelectedWorldFullRect(CrdRect viewPort, const GraphicLayer* gl, OrientationType orientation)
{
	CrdRect selectRect = gl->CalcSelectedFullWorldRect();
	if (! selectRect.inverted()) 
		return WorldRect_AddBorderPixels(selectRect, viewPort, gl, orientation);
	return selectRect;
}

CrdRect getCurrWorldFullRect(CrdRect viewPort, const ScalableObject* go, OrientationType orientation)
{
	return WorldRect_AddBorderPixels(go->GetCurrWorldClientRect(), viewPort, go, orientation);
}

const LayerSet* ViewPort::GetLayerSet() const
{
	return debug_cast<const LayerSet*>(GetContents());
}


LayerSet* ViewPort::GetLayerSet()
{
	return debug_cast<LayerSet*>(GetContents());
}

GraphicLayer* ViewPort::GetActiveLayer()
{
	LayerSet* ls = GetLayerSet();
	if (!ls) return 0;
	return ls->GetActiveLayer();
}

const GraphicLayer* ViewPort::GetActiveLayer() const
{
	const LayerSet* ls = GetLayerSet();
	if (!ls) return 0;
	return ls->GetActiveLayer();
}

void ViewPort::ZoomWorldFullRect(CrdRect relClientRect)
{
	const ScalableObject* gc = GetContents();
	dms_assert(gc);

	CrdRect roi;
	
	for (gr_elem_index n = gc->NrEntries(); n--; )
	{
		const ScalableObject* go = gc->GetConstEntry(n);
		if (!go->HasNoExtent())
		{
			auto cwfr = calcWorldFullRect(relClientRect, go, m_Orientation);
			roi |= cwfr;
		}
	}

	if (roi.empty())
		roi = Convert<CrdRect>(relClientRect);
	ZoomToTargetRoi(roi);
}

CrdRect ViewPort::GetCurrWorldFullRect() const
{
	const ScalableObject* gc = GetContents();
	assert(gc);

	gr_elem_index n = gc->NrEntries();
	assert(n);

	auto relClientRect = GetCurrClientRelLogicalRect();
	CrdRect roi;
	for (gr_elem_index i = 0; i != n; ++i)
	{
		const ScalableObject* go = gc->GetConstEntry(i);
		if (!go->HasNoExtent())
			roi |= getCurrWorldFullRect(relClientRect, go, m_Orientation);
	}
	return roi;
}


CrdRect ViewPort::GetCurrWorldClientRect() const
{
	return m_w2vTr.Reverse( CrdRect(Point<CrdType>(0, 0), GetCurrClientSize()) );
}

CrdRect ViewPort::CalcCurrWorldClientRect() const
{
	return CalcCurrWorldToClientTransformation().Reverse(  CrdRect(Point<CrdType>(0, 0), GetCurrClientSize()) );
}

CrdRect ViewPort::CalcWorldClientRect() const
{
	UpdateView();
	return GetCurrWorldClientRect();
}

CrdType ViewPort::GetCurrLogicalZoomLevel() const
{
	// >c-safe: the isotropic scale at the VIEW CENTRE, not m_w2vTr.ZoomLevel() which samples the world origin
	// (0,0) - bogus under tilt/rotation. Byte-identical at <=c (Abs(Factor().first) == ZoomLevel()).
	return Abs(GetCurrLogicalZoomFactors().first);
}

// Per-axis logical zoom factors. Factor() only exists for axis-separable transforms (levels a..c);
// under view rotation/tilt (>c) there is no single per-axis factor, so fall back to the isotropic
// local scale at the world centre. Byte-identical at <=c.
static CrdPoint LogicalZoomFactorsOf(const CrdTransformation& tr, CrdPoint worldCenter)
{
	if (tr.IsAxisSeparable())
		return tr.Factor();
	auto s = tr.LocalScaleAt(worldCenter);
	return CrdPoint(s, s);
}

CrdPoint ViewPort::GetCurrLogicalZoomFactors() const
{
	return LogicalZoomFactorsOf(m_w2vTr, Center(GetROI()));
}

CrdPoint ViewPort::CalcCurrWorldToDeviceFactors() const
{
	return LogicalZoomFactorsOf(CalcCurrWorldToClientTransformation(), Center(GetROI())) * GetScaleFactors();
}

void ViewPort::ZoomAll()
{
	ScalableObject* gc = GetContents();
	if (!gc || !gc->NrEntries()) 
		return;
	ZoomWorldFullRect(GetCurrClientRelLogicalRect());
}

ViewPort* ViewPort::g_CurrZoom = nullptr;

void ViewPort::AL_ZoomAll()
{
	tmp_swapper<ViewPort*> currVpSetter(g_CurrZoom, this);

	ScalableObject* go = GetActiveLayer();
	if (!go)
		return;
	auto dv = GetDataView().lock();
	if (!dv)
		return;

	ZoomToTargetRoi(calcWorldFullRect(CalcClientRelRect(), go, m_Orientation));
}

void ViewPort::AL_ZoomSel()
{
	GraphicLayer* al = GetActiveLayer();
	if (!al)
		return;

	CrdRect selectRect = calcSelectedWorldFullRect(CalcClientRelRect(), al, m_Orientation);
	if (! selectRect.empty())
		ZoomToTargetRoi(selectRect); // a single selected point would otherwise over-zoom (issue #374)
}

void ViewPort::AL_SelectAllObjects(bool select)
{
	GraphicLayer* al = GetActiveLayer();
	if (al)
		al->SelectAll(select);
}

auto FindWmsLayer(LayerSet* ls) -> WmsLayer*
{
	for (gr_elem_index i = 0, n = ls->NrEntries(); i!=n; ++i)
	{
		auto* layerBase = ls->GetEntry(i);
		auto asWmsLayer = dynamic_cast<WmsLayer*>(layerBase);
		if (asWmsLayer)
			return asWmsLayer;
		auto asSubset = dynamic_cast<LayerSet*>(layerBase);
		if (!asSubset)
			continue;
		asWmsLayer = FindWmsLayer(asSubset);
		if (asWmsLayer)
			return asWmsLayer;
	}
	return nullptr;
}

auto ViewPort::FindBackgroundWmsLayer() -> WmsLayer*
{
	auto ls = GetLayerSet();
	return FindWmsLayer(ls);
}

void ViewPort::ZoomIn1()
{
	auto layer = FindBackgroundWmsLayer();
	if (layer && layer->ZoomIn(this))
		return;
	ZoomFactor(0.5); // zoom in on half of org ROI
}

void ViewPort::ZoomFactor(CrdType factor)
{
	if (!m_WorldCrdUnit)
		return;

	CrdPoint p = Center(GetROI());
	CrdPoint s = Size(GetROI()) * (factor * 0.5);

	SetROI(CrdRect(p-s, p+s)); // zoom in on half of org ROI
}

void ViewPort::ZoomToTargetRoi(CrdRect roi)
{
	// A non-degenerate extent zooms as before.
	if (Size(roi).X() > 0.0 && Size(roi).Y() > 0.0)
	{
		SetROI(roi);
		return;
	}

	// Degenerate (single-point / zero-area) extent: don't let SetROI over-zoom it to
	// ViewPortMinSize. Snap to a background WMS layer's finest tile level if present,
	// else to a fixed default extent (issue #374).
	CrdPoint center = Center(roi);

	if (auto* wms = FindBackgroundWmsLayer())
	{
		SetROI(CrdRect(center, center)); // set the center; ZoomToFinestLevel sizes around the ROI center
		if (wms->ZoomToFinestLevel(this))
			return;
	}

	InitWorldCrdUnit(0);
	CrdType metersPerUnit = m_WorldCrdUnit ? GetUnitSizeInMeters(m_WorldCrdUnit.get()) : 1.0;
	CrdType defSize = m_WorldCrdUnit ? GetSubItemValue(m_WorldCrdUnit.get(), vpdefsID, -1.0) : -1.0;
	if (defSize <= 0.0)
		defSize = DEFAULT_POINT_VIEW_SIZE_IN_METERS / metersPerUnit;

	CrdPoint h = CrdPoint(defSize, defSize) * 0.5;
	SetROI(CrdRect(center - h, center + h));
}


void ViewPort::ZoomOut1()
{
	auto layer = FindBackgroundWmsLayer();
	if (layer && layer->ZoomOut(this, false))
		return;
	ZoomFactor(2.0); // zoom in on twice the org ROI
}

void ViewPort::Pan(CrdPoint delta)
{
	CrdPoint center = Center(GetROI());
	auto newCenter = center + delta;
	PanTo(newCenter);
}

void ViewPort::PanTo(CrdPoint newCenter)
{
	CrdPoint sz = Size(GetROI()) * 0.5;

	SetROI(CrdRect(newCenter - sz, newCenter + sz));
}

void ViewPort::PanToClipboardLocation()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;

	SharedStr clipboardText = clipBoard.GetText();
	auto viewPoint = ViewPoint(clipboardText.AsRange());
	if (IsDefined(viewPoint.center.first) && IsDefined(viewPoint.center.second))
		PanTo(viewPoint.center);
}

void ViewPort::ZoomToClipboardLocation()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;

	SharedStr clipboardText = clipBoard.GetText();
	auto viewPoint = ViewPoint(clipboardText.AsRange());
	if (IsDefined(viewPoint.zoomLevel))
		SetCurrZoomLevel(viewPoint.zoomLevel);
}

void ViewPort::PanAndZoomToClipboardLocation()
{
	ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;

	SharedStr clipboardText = clipBoard.GetText();
	auto viewPoint = ViewPoint(clipboardText.AsRange());
	if (IsDefined(viewPoint.center.first) && IsDefined(viewPoint.center.second))
		PanTo(viewPoint.center);
	if (IsDefined(viewPoint.zoomLevel))
		SetCurrZoomLevel(viewPoint.zoomLevel);
}

void ViewPort::CopyLocationAndZoomlevelToClipboard()
{
	auto roi = GetROI();
	auto center = Center(roi);
	auto zoomLevel = GetCurrLogicalZoomLevel();
	auto viewPoint = ViewPoint(center, zoomLevel, {});
	char buffer[201];;
	if (viewPoint.WriteAsString(buffer, 200, FormattingFlags::None))
	{
		ClipBoard clipBoard(false); if (!clipBoard.IsOpen()) return;
		ClipBoard::ClearBuff();
		clipBoard.AddTextLine(buffer);
	}
}

bool ViewPort::MouseEvent(MouseEventDispatcher& med)
{
	auto medOwner = med.GetOwner().lock(); if (!medOwner) return true;
	const EventInfo& eventInfo = med.GetEventInfo();
	EventID          eventID = eventInfo.GetID();
	if (eventID & EventID::MOUSEWHEEL )
	{
		int wheelDelta = GET_WHEEL_DELTA_WPARAM(eventInfo.m_wParam);
		wheelDelta /= WHEEL_DELTA;
		if (wheelDelta)
		{
			if (eventID & EventID::SHIFTKEY)
			{
				// Shift+Wheel: raise/lower viewpoint (eye height). STUB until the world->view
				// transform supports perspective (Transformation_complexity_plan.md §7).
				// TODO(transform-f): adjust eye height by wheelDelta.
				return true;
			}
			CrdPoint oldWorldPoint = CalcWorldToClientTransformation().Reverse(g2dms_order<CrdType>(eventInfo.m_Point)); // curr World location of click location
			if (wheelDelta > 0)
			{
				MakeMin<int>(wheelDelta, 5);
				while (wheelDelta--)
					ZoomIn1();
			}
			else 
			{
				assert(wheelDelta < 0);
				MakeMax<int>(wheelDelta, -5);
				while (wheelDelta++)
					ZoomOut1();
			}
			CrdPoint newWorldPoint = CalcWorldToClientTransformation().Reverse(g2dms_order<CrdType>(eventInfo.m_Point)); // new World location of click location
			Pan(oldWorldPoint - newWorldPoint);
			return true;
		}
	}
	if (eventID & (EventID::LBUTTONDOWN | EventID::LBUTTONUP | EventID::LBUTTONDBLCLK) )
	{
		AddClientLogicalOffset  viewportOffset(&med, GetCurrClientRelPos());
		auto w2dTr = CalcWorldToClientTransformation() + Convert<CrdPoint>(med.GetClientLogicalAbsPos());
		w2dTr *= CrdTransformation(CrdPoint(0, 0), med.GetSubPixelFactors());

		if (MustQuery(medOwner->m_ControllerID))
			InfoController::SelectFocusElem(GetLayerSet(), w2dTr.Reverse(g2dms_order<CrdType>(eventInfo.m_Point)), eventID);

		// Alt+drag = marquee zoom, regardless of the active tool (navigation override).
		if ((eventID & EventID::LBUTTONDOWN) && (eventID & EventID::ALTKEY))
		{
			medOwner->InsertController(
				new ZoomInController(medOwner.get(), this, w2dTr, eventInfo.m_Point)
			);
			return true;
		}

		if (eventID & EventID::LBUTTONDOWN) switch (medOwner->m_ControllerID)
		{
			case TB_ZoomIn2:
			{
				medOwner->InsertController(
					new ZoomInController(medOwner.get(), this, w2dTr,  eventInfo.m_Point)
				);
				return true;
			}
			case TB_ZoomOut2:
				medOwner->InsertController(
					new ZoomOutController(medOwner.get(), this, w2dTr)
				);
				return true;

			case TB_SelectObject:
				medOwner->InsertController(
					new SelectObjectController(medOwner.get(), this, w2dTr)
				);
				return true;
			case TB_SelectRect:
				medOwner->InsertController(
					new SelectRectController(medOwner.get(), this, w2dTr,  eventInfo.m_Point)
				);
				return true;

			case TB_SelectCircle:
				medOwner->InsertController(
					new SelectCircleController(medOwner.get(), this, w2dTr, eventInfo.m_Point)
				);
				return true;

			case TB_SelectPolygon:
				medOwner->InsertController(
					new DrawPolygonController(medOwner.get(), this, w2dTr, eventInfo.m_Point)
				);
				return true;

			case TB_SelectDistrict:
				medOwner->InsertController(
					new SelectDistrictController(medOwner.get(), this, CalcWorldToClientTransformation() + Convert<CrdPoint>(med.GetClientLogicalAbsPos() )
					)
				);
				return true;

			case TB_Neutral:
			{
				bool ctrl  = med.GetEventInfo().m_EventID & EventID::CTRLKEY;
				bool shift = med.GetEventInfo().m_EventID & EventID::SHIFTKEY;
				if (ctrl && shift)
				{
					// Ctrl+Shift+click: copy coordinate to clipboard (fresh copy). Fall through
					// (no return) so the visit reaches DoViewPort, which consumes COPYCOORD.
					ClipBoard::ClearBuff();
					med.GetEventInfo().m_EventID |= EventID::COPYCOORD;
				}
				else if (ctrl)
				{
					// Ctrl+click/drag: apply the active selection tool (default = point selection).
					medOwner->InsertController(
						new SelectObjectController(medOwner.get(), this, w2dTr)
					);
					return true;
				}
				else if (shift)
				{
					// Shift+drag: orbit (rotate/tilt) about the view centre. STUB until the
					// world->view transform supports rotation (Transformation_complexity_plan.md).
					medOwner->InsertController(
						new OrbitController(medOwner.get(), this, eventInfo.m_Point)
					);
					return true;
				}
				else
					medOwner->InsertController(
						new PanController(medOwner.get(), this, eventInfo.m_Point)
					);
			}
		}
	}
	// Right-button: a drag is a marquee zoom; a click (sub-threshold) is the context menu.
	// Start the right-button marquee controller on RBUTTONDOWN; it decides on RBUTTONUP whether the
	// movement was a drag (commit zoom, consume the event) or a click (leave the event unhandled so
	// the later FillMenu shows the context menu).
	if (eventID & EventID::RBUTTONDOWN)
	{
		AddClientLogicalOffset viewportOffset(&med, GetCurrClientRelPos());
		auto w2dTr = CalcWorldToClientTransformation() + Convert<CrdPoint>(med.GetClientLogicalAbsPos());
		w2dTr *= CrdTransformation(CrdPoint(0, 0), med.GetSubPixelFactors());
		medOwner->InsertController(
			new ZoomInController(medOwner.get(), this, w2dTr, eventInfo.m_Point, /*rightButtonMarquee*/ true)
		);
		return true;
	}

	return Wrapper::MouseEvent(med); // returns false
}

#include "DataArray.h" 

CrdType interpolate(CrdType min, CrdType max, UInt32 i, UInt32 n)
{
	return (i*max + (n-i) *min) / n;
}

#include "FilePtrHandle.h"

#ifdef _WIN32

void SaveBitmap(WeakStr filename, HBITMAP hBitmap)
{
	BITMAPINFO        bmpInfo;
	BITMAPFILEHEADER  bmpFileHeader; 

	DcHandleBase hdc(NULL);
	ZeroMemory(&bmpInfo,sizeof(BITMAPINFO));
	bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
	GetDIBits(hdc,hBitmap,0,0,nullptr,&bmpInfo,DIB_RGB_COLORS); 
	bmpInfo.bmiHeader.biBitCount = 3*8;
//	if(bmpInfo.bmiHeader.biSizeImage<=0)
		bmpInfo.bmiHeader.biSizeImage
			=	abs(bmpInfo.bmiHeader.biHeight) * ((bmpInfo.bmiHeader.biWidth * (bmpInfo.bmiHeader.biBitCount+7)/8 + 0x03) & ~0x03);

	OwningPtrSizedArray<BYTE> pBuf( bmpInfo.bmiHeader.biSizeImage, dont_initialize MG_DEBUG_ALLOCATOR_SRC("SaveBitmap"));

	bmpInfo.bmiHeader.biCompression=BI_RGB;
	GetDIBits(hdc,hBitmap,0,bmpInfo.bmiHeader.biHeight,pBuf.begin(), &bmpInfo, DIB_RGB_COLORS);

	FilePtrHandle fh;
	auto r = fh.OpenFH(filename, FCM_CreateAlways, false, NR_PAGES_DATFILE);
	if (!r)
		r.Throw("SaveBitmap");
	bmpFileHeader.bfReserved1=0;
	bmpFileHeader.bfReserved2=0;
	bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
	bmpFileHeader.bfType='MB';
	bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
	fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fh);
	fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fh);
	fwrite(pBuf.begin(),bmpInfo.bmiHeader.biSizeImage,1,fh); 
}

#endif // _WIN32 (SaveBitmap)

//===================================== ViewPort::GetExportInfo (portable)
ExportInfo ViewPort::GetExportInfo()
{
	return ExportInfo(this);
}

#ifdef _WIN32
void ViewPort::Export()
{
	auto dv = GetDataView().lock(); if (!dv) return;

	FixedContextHandle context("while Exporting full ViewPort contents");
	FencedInterestRetainContext resultHolder("ViewPort::Export()");

	dms_assert(!SuspendTrigger::DidSuspend()); // DEBUG.
	dbg_assert(DataViewOK());

	ExportInfo info(this);

	bool isBmp = true; // false would indicate tiff

	auto tmpVP = std::make_shared<ViewPort>(nullptr, nullptr, ""); // , info.m_SubPixelFactor);
	tmpVP->SetContents(GetContents()->shared_from_this());
	tmpVP->InitWorldCrdUnit(GetWorldCrdUnit());
	tmpVP->SetClientSize(Convert<TPoint>(info.GetNrSubDotsPerTile()));

	std::shared_ptr<ScaleBarObj> optionalScaleBar;
	if (m_ScaleBarCaret)
		optionalScaleBar = std::make_shared<ScaleBarObj>(tmpVP.get(), tmpVP.get());

	// and go
	UInt32 row, col;
	CrdRect tileRoi; 
	
	for (	tileRoi.first .Row() = info.m_ROI.first.Row(), row = 0
		;	tileRoi.second.Row() = interpolate(info.m_ROI.first.Row(), info.m_ROI.second.Row(), row+1, info.m_NrTiles.Row()), row != UInt32(info.m_NrTiles.Row())
		;	tileRoi.first .Row() = tileRoi.second.Row(), ++row)
	{
		UInt32 rowFromTop = info.m_NrTiles.Row() - row - 1;
		for (	tileRoi.first .Col() = info.m_ROI.first.Col(), col = 0
			;	tileRoi.second.Col() = interpolate(info.m_ROI.first.Col(), info.m_ROI.second.Col(), col+1, info.m_NrTiles.Col()), col != UInt32(info.m_NrTiles.Col())
			;	tileRoi.first. Col() = tileRoi.second.Col(), ++col)
		{
			tmpVP->SetROI( tileRoi );
			if (optionalScaleBar)
				optionalScaleBar->DetermineAndSetLogicalBoundingBox(
					Convert<TPoint>(info.GetNrDotsPerTile()) * shp2dms_order<TType>(col, rowFromTop),
					Convert<TPoint>(info.GetNrDotsPerPage())
				);

			GdiHandle<HBITMAP> bitmap( tmpVP->GetAsDDBitmap(dv.get(), info.m_SubPixelFactor, optionalScaleBar.get()) );

			dms_assert(!SuspendTrigger::DidSuspend());

			if (isBmp)
			{
				SharedStr fileName = mySSPrintF("{}_{}_{}.bmp", info.m_FullFileNameBase.c_str(), rowFromTop, col);
				SaveBitmap(fileName, bitmap);
			}
			else
			{
	//				TifImp fh; fh.Open(mySSPrintF("{}_{}_{}.tif", fullFileNameBase.c_str(), row, col).c_str(), TIF_WRITE);  // NYI
			}
		}
	}
}

#else // !_WIN32

void ViewPort::Export()
{
	// TODO: implement bitmap export on Linux (Qt-based rendering)
}

#endif // _WIN32 (Export)

#if defined(_WIN32) && (WINVER < 0x0500)
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif /* WINVER >= 0x0500 */

int ScrollStepSize()
{
	return (GetKeyState(VK_SHIFT)& 0x8000)
		?	PAGE_SCROLL_STEP
		:	LINE_SCROLL_STEP; 
}

bool ViewPort::OnKeyDown(UInt32 virtKey)
{
	// N = restore north-up (yaw->0), T = restore untilted (pitch->0). Handle here, before the IsSpec
	// branch, so they work on BOTH key paths: the native Win32 path delivers WM_KEYDOWN (VK_N/VK_T, no
	// Char flag), while the Qt path delivers letters qtKeyToVK does not translate as WM_CHAR-equivalents
	// with KeyInfo::Flag::Char set -- which makes IsSpec() (CcmOf==0) false, so the keys were dead on the
	// Qt build. Match the keydown VK ('N'/'T') and the char codes ('n'/'t', and shifted 'N'/'T'); only when
	// no Ctrl/Alt is held (a map view has no text-cell editing, so Char delivery is safe to treat as a command).
	if (!(virtKey & (KeyInfo::Flag::Ctrl | KeyInfo::Flag::Menu)))
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case 'N': case 'n': return OnCommand(TB_RestoreNorth);    // restore north-up (yaw=0)
			case 'T': case 't': return OnCommand(TB_RestoreUntilted); // restore untilted (pitch=0)
		}
	}

	if (KeyInfo::IsSpec(virtKey))
	{
		switch (KeyInfo::CharOf(virtKey)) {
			case VK_ADD:      return OnCommand(TB_ZoomIn1);
			case VK_SUBTRACT: return OnCommand(TB_ZoomOut1);

			case VK_RIGHT:    ScrollLogical(shp2dms_order<TType>(-ScrollStepSize(), 0)); return true;
			case VK_LEFT:     ScrollLogical(shp2dms_order<TType>( ScrollStepSize(), 0)); return true;
			case VK_UP:       ScrollLogical(shp2dms_order<TType>(0,  ScrollStepSize())); return true;
			case VK_DOWN:     ScrollLogical(shp2dms_order<TType>(0, -ScrollStepSize())); return true;
		}
	} else if (KeyInfo::IsShift(virtKey)) {
		// Shift+arrows: step rotate (yaw) / tilt (pitch). STUB setters until the world->view
		// transform supports rotation/tilt (Transformation_complexity_plan.md §7).
		const CrdType ROT_STEP  = 0.2617993878; // ~15 degrees in radians
		const CrdType TILT_STEP = 0.1745329252; // ~10 degrees in radians
		switch (KeyInfo::CharOf(virtKey)) {
			case VK_LEFT:  SetRotation(GetRotation() + ROT_STEP);  return true; // match Google-Maps feel: Left rotates view CCW
			case VK_RIGHT: SetRotation(GetRotation() - ROT_STEP);  return true; // Right rotates view CW
			case VK_UP:    SetTilt    (GetTilt()     - TILT_STEP); return true; // viewpoint higher -> less tilt (toward top-down)
			case VK_DOWN:  SetTilt    (GetTilt()     + TILT_STEP); return true; // viewpoint lower  -> more tilt
		}
	} else if (KeyInfo::IsCtrl(virtKey)) {
		switch (KeyInfo::CharOf(virtKey)) {
			case 'L': return OnCommand(TB_ZoomAllLayers);
			case 'Y': return OnCommand(TB_ZoomActiveLayer);
			case 'F': return OnCommand(TB_ZoomSelectedObj);
			case 'N': return OnCommand(TB_Export);
			case 'G': return OnCommand(TB_GotoClipboardLocation);
			case '1': return OnCommand(TB_GotoClipboardLocation);
			case '2': return OnCommand(TB_GotoClipboardZoomlevel);
			case '3': return OnCommand(TB_GotoClipboardLocationAndZoomlevel);
			case '0': return OnCommand(TB_CopyLocationAndZoomlevelToClipboard);
		}
	}
	else if (KeyInfo::IsShiftCtrl(virtKey)) {
		switch (KeyInfo::CharOf(virtKey)) {
			case '1': BroadcastCommandToAllDataViews(TB_GotoClipboardLocation); return true;
			case '2': BroadcastCommandToAllDataViews(TB_GotoClipboardZoomlevel); return true;
			case '3': BroadcastCommandToAllDataViews(TB_GotoClipboardLocationAndZoomlevel); return true;
		}
	} else if (KeyInfo::IsCtrlAlt(virtKey)) {
		switch (KeyInfo::CharOf(virtKey)) {
			case 'X': return OnCommand(TB_CutSel);
			case 'C': return OnCommand(TB_CopySel);
			case 'P': return OnCommand(TB_PasteSelDirect);
			case 'D': return OnCommand(TB_DeleteSel);
		}
	}
	return base_type::OnKeyDown(virtKey);
}

bool ViewPort::OnCommand(ToolButtonID id)
{
	auto dv = GetDataView().lock(); if (!dv) return false;
	switch (id)
	{
		case TB_ZoomIn1:  ZoomIn1 (); return true;
		case TB_ZoomOut1: ZoomOut1(); return true;

		case TB_ZoomAllLayers:    ZoomAll();   return true;
		case TB_ZoomActiveLayer:  AL_ZoomAll(); return true;
		case TB_ZoomSelectedObj:  AL_ZoomSel(); return true;

		case TB_RestoreNorth:     SetRotation(0.0); return true; // yaw->0, leave any tilt (STUB)
		case TB_RestoreUntilted:  SetTilt(0.0);     return true; // pitch->0, leave any rotation (STUB)
		case TB_ShowFirstSelectedRow: AL_ZoomSel(); return true;
		case TB_Neutral:
			SetViewPortCursor(DmsCursor::Arrow);
#ifdef _WIN32
			SetViewPortCursor( LoadCursor(NULL, IDC_ARROW) );
#endif
			goto setControllerID;

		case TB_ZoomIn2:
			SetViewPortCursor(DmsCursor::ZoomIn);
#ifdef _WIN32
			SetViewPortCursor( LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_ZOOMIN) ) );
#endif
			goto setControllerID;

		case TB_ZoomOut2:
			SetViewPortCursor(DmsCursor::ZoomOut);
#ifdef _WIN32
			SetViewPortCursor( LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_ZOOMOUT) ) );
#endif
			goto setControllerID;

		case TB_SelectObject:
		case TB_SelectRect:
		case TB_SelectCircle:
		case TB_SelectPolygon:
		case TB_SelectDistrict:
			SetViewPortCursor(DmsCursor::SelectDiamond);
#ifdef _WIN32
			SetViewPortCursor( LoadCursor(NULL, MAKEINTRESOURCE(IDC_SELECTDIAMOND) ) );
#endif
			goto setControllerID;

		case TB_SelectAll : AL_SelectAllObjects( true); return true;
		case TB_SelectNone: AL_SelectAllObjects(false); return true;

		case TB_NeedleOn:
		case TB_NeedleOff:
			if (IsNeedleVisible() != (id == TB_NeedleOn))
				ToggleNeedleController();
			return true;

		case TB_ScaleBarOn:
			if (!m_ScaleBarCaret)
				dv->InsertCaret(m_ScaleBarCaret = new ScaleBarCaret(this) );
			UpdateScaleBar();
			return true;

		case TB_ScaleBarOff:
			InvalidateOverlapped();
			if (m_ScaleBarCaret)
			{
				dv->RemoveCaret(m_ScaleBarCaret); 
				m_ScaleBarCaret = nullptr;
			}
			return true;

		case TB_SP_All:
		case TB_SP_Active:
		case TB_SP_None:
			return GetContents()->OnCommand(id);

		case TB_CutSel:
		case TB_CopySel:
		case TB_PasteSelDirect:
		case TB_PasteSel:
		case TB_DeleteSel:
		case TB_ShowSelOnlyOn:
		case TB_ShowSelOnlyOff:
			{
				GraphicLayer* al = GetActiveLayer();
				if (!al)
					throwItemError("No layer is activated");
				return al->OnCommand(id);
			}
		case TB_GotoClipboardLocation:
			PanToClipboardLocation();
			return true;

		case TB_GotoClipboardZoomlevel:
			ZoomToClipboardLocation();
			return true;

		case TB_GotoClipboardLocationAndZoomlevel:
			PanAndZoomToClipboardLocation();
			return true;

		case TB_CopyLocationAndZoomlevelToClipboard:
			CopyLocationAndZoomlevelToClipboard();
			return true;
	}
	return base_type::OnCommand(id);

setControllerID:
	dms_assert(dv);
	dv->m_ControllerID = id;
	return true;
}

void ViewPort::FillMenu(MouseEventDispatcher& med)
{
	//	Goto & Find
	static SharedStr msg0 = SharedStr("Ctrl-0: Copy Location and Zoomlevel as ViewPoint to Clipboard");
	static SharedStr msg1 = SharedStr("Ctrl-1: Pan to ViewPoint location in Clipboard");
	static SharedStr msg2 = SharedStr("Ctrl-2: Zoom to ViewPoint zoom-level");
	static SharedStr msg3 = SharedStr("Ctrl-3: Pan and Zoom to ViewPoint in Clipboard");
	med.m_MenuData.push_back(MenuItem(msg0, make_MembFuncCmd(&ViewPort::CopyLocationAndZoomlevelToClipboard), this));
	med.m_MenuData.push_back(MenuItem(msg1, make_MembFuncCmd(&ViewPort::PanToClipboardLocation), this));
	med.m_MenuData.push_back(MenuItem(msg2, make_MembFuncCmd(&ViewPort::ZoomToClipboardLocation), this));
	med.m_MenuData.push_back(MenuItem(msg3, make_MembFuncCmd(&ViewPort::PanAndZoomToClipboardLocation), this));

	static SharedStr msgS1 = SharedStr("Shift-Ctrl-1: Pan all MapViews to ViewPoint location in Clipboard");
	static SharedStr msgS2 = SharedStr("Shift-Ctrl-2: Zoom all MapViews to ViewPoint zoom-level");
	static SharedStr msgS3 = SharedStr("Shift-Ctrl-3: Pan and Zoom all MapViews to ViewPoint in Clipboard");
	med.m_MenuData.push_back(MenuItem(msgS1, make_MembFuncCmd(&ViewPort::OnKeyDown, KeyInfo::Flag::Ctrl | KeyInfo::Flag::Shift | '1'), this));
	med.m_MenuData.push_back(MenuItem(msgS2, make_MembFuncCmd(&ViewPort::OnKeyDown, KeyInfo::Flag::Ctrl | KeyInfo::Flag::Shift | '2'), this));
	med.m_MenuData.push_back(MenuItem(msgS3, make_MembFuncCmd(&ViewPort::OnKeyDown, KeyInfo::Flag::Ctrl | KeyInfo::Flag::Shift | '3'), this));

	base_type::FillMenu(med);
}

bool isSelectionTool(ToolButtonID id)
{
	switch (id)
	{
	case TB_CutSel:
	case TB_CopySel:
	case TB_PasteSelDirect:
	case TB_PasteSel:
	case TB_DeleteSel:
	case TB_ZoomSelectedObj:
	case TB_SelectObject:
	case TB_SelectRect:
	case TB_SelectCircle:
	case TB_SelectPolygon:
	case TB_SelectDistrict:
	case TB_SelectRows:
	case TB_SelectAll:
	case TB_SelectNone:
	case TB_ShowSelOnlyOn:
	case TB_ShowSelOnlyOff:
		return true;
	default: return false;
	}
}

CommandStatus ViewPort::OnCommandEnable(ToolButtonID id) const
{
	switch (id) {
		case TB_NeedleOn: return IsNeedleVisible() ? CommandStatus::DOWN : CommandStatus::ENABLED;
		case TB_ScaleBarOn: return m_ScaleBarCaret ? CommandStatus::DOWN : CommandStatus::ENABLED;
	}

	if (isSelectionTool(id))
	{
		const GraphicLayer* al = GetActiveLayer();
		if (!al || al->OnCommandEnable(id) == CommandStatus::DISABLED)
			return CommandStatus::DISABLED;
	}

	auto dv = GetDataView().lock();
	if (dv && dv->m_ControllerID == id)
		return CommandStatus::DOWN;

	return base_type::OnCommandEnable(id);
}

//============================
// PasteGridController
//============================

#include "GridLayer.h"
#include "GridDrawer.h"

#ifdef _WIN32

class PasteGridController : public AbstrController
{
	typedef AbstrController base_type;
public:
	PasteGridController(
		DataView*      owner, 
		ViewPort*      vp,
		GridLayer*     target, 
		GridCoordPtr    gridCoords,
		SelValuesData* selValues
	)	:	AbstrController(owner, target 
			,	EventID::LBUTTONDOWN|EventID::MOUSEMOVE|EventID::MOUSEDRAG
			,	EventID::LBUTTONUP
			,	EventID::LBUTTONUP|EventID::RBUTTONDOWN|EventID::RBUTTONUP|EventID::CAPTURECHANGED|EventID::SCROLLED
			,	ToolButtonID::TB_PasteSel
			)
		,	m_ViewPort(vp)
		,	m_GridLayer(target)
		,	m_GridCoords(gridCoords)
		,	m_SelValues(selValues)
	{
		dms_assert(target);
		dms_assert(selValues);
	}
	~PasteGridController()
	{
		dms_assert(!m_OrgCursor);
	}

protected:
	bool Move (EventInfo& eventInfo) override
	{
		m_GridCoords->UpdateUnscaled();
		IPoint gridNewBase = (IsDefined(eventInfo.m_Point))			
			?	m_GridCoords->GetExtGridCoordFromAbs(eventInfo.m_Point)
			:	UNDEFINED_VALUE(IPoint); 

		if (gridNewBase == m_SelValues->m_Rect.first)
			return false;

		m_GridLayer->InvalidatePasteArea();
		m_SelValues->MoveTo(gridNewBase);
		m_GridLayer->InvalidatePasteArea();
		return true;
	}
	bool Exec(EventInfo& eventInfo) override
	{
		m_GridLayer->PasteNow();
		return true;
	}

	void Stop () override
	{
		m_ViewPort->SetViewPortCursor(m_OrgCursor);
		m_OrgCursor = nullptr;
		m_GridLayer->ClearPaste();
 		base_type::Stop(); // !!! destroys this
	}

private:
	std::shared_ptr<ViewPort>    m_ViewPort;
	std::shared_ptr<GridLayer>   m_GridLayer;
	GridCoordPtr           m_GridCoords;
	WeakPtr<SelValuesData> m_SelValues;
	HCURSOR                m_OrgCursor = nullptr; friend class ViewPort;
};

//============================

void ViewPort::PasteGrid(SelValuesData* svd, GridLayer* gl)
{
	auto dv = GetDataView().lock(); if (!dv) return;
	SharedPtr<PasteGridController> pasteController = new PasteGridController(dv.get(), this, gl, gl->GetGridCoordInfo(this), svd);
	dv->InsertController(pasteController.get());
	pasteController->m_OrgCursor = SetViewPortCursor(LoadCursor(g_ShvDllInstance, MAKEINTRESOURCE(IDC_PAN)));
}

#endif // _WIN32 (PasteGridController, PasteGrid)

void ViewPort::ScrollDevice(GPoint delta)
{
	DBG_START("ViewPort", "Scroll", MG_DEBUG_SCROLL);

	auto dv = GetDataView().lock(); if (!dv) return;

	dbg_assert(DataViewOK());

	if (delta == GPoint(0, 0) )
		return;

	UpdateView();

	{
		InvalidationBlock lock(this);
		auto w2c = CalcCurrWorldToClientTransformation();
		CrdPoint worldDelta;
		if (w2c.IsAxisSeparable())
		{
			// exact historical path (per-axis device factors)
			CrdTransformation w2d(CrdPoint(0.0, 0.0), w2c.Factor() * GetScaleFactors());
			worldDelta = w2d.WorldScale(g2dms_order<CrdType>(delta));
		}
		else
		{
			// rotated/projective view: convert the device pan delta to a world delta via the inverse of
			// the world->device transform (Reverse-difference), which is valid at any complexity.
			auto w2d = w2c * CrdTransformation(CrdPoint(0.0, 0.0), GetScaleFactors());
			auto dDelta = g2dms_order<CrdType>(delta);
			worldDelta = w2d.Reverse(dDelta) - w2d.Reverse(CrdPoint(0.0, 0.0));
		}
		SetROI(GetROI() - worldDelta);
	}
	auto newW2vTr = CalcCurrWorldToClientTransformation();

	InvalidateOverlapped();

	if (newW2vTr.IsProjective())
	{
		// Tilted (projective) view: a world pan is NOT a uniform device translation (perspective makes
		// near features shift more than far ones), so the rendered pixels cannot be blit-scrolled and the
		// cached per-tile GridCoords are not simply delta-offsettable. Rebuild + repaint the whole viewport:
		// InvalidateView() defers a main-thread UpdateView that recomputes m_w2vTr and re-inits every
		// GridCoord for the new ROI (the same safe deferred path SetRotation/SetTilt use -- never call
		// DoUpdateView() directly here, that would race an in-flight draw); InvalidateDraw() schedules the
		// paint. Affine views (north-up AND pure yaw) keep the fast, pixel-exact translative scroll below.
		//
		// Do NOT assign m_w2vTr here: DoUpdateView only re-inits the GridCoords (which place the WMS/raster
		// tiles) when its freshly computed transform DIFFERS from m_w2vTr (the `if (m_w2vTr == w2vTr) return;`
		// guard). Pre-assigning short-circuits that guard, leaving the WMS tiles at the old ROI while the
		// vector layers -- which read m_w2vTr at draw time -- move to the new one (the pan-under-tilt skew).
		InvalidateView();
		InvalidateDraw();
		return;
	}

	m_w2vTr = newW2vTr;

	auto deviceExtents = ScaleCrdRect( CalcClientRelRect(), GetScaleFactors() );
	auto intExtents = CrdRect2GRect(deviceExtents);

	dv->ScrollDevice(delta, intExtents, intExtents, this);

	m_BrushOrg += delta;

	DBG_TRACE(("delta {}",       AsString(delta).c_str()));
	DBG_TRACE(("viewExtents {}", AsString(deviceExtents).c_str()));

	for (auto& gc: m_GridCoordMap)
		if (auto gridCoord = gc.second.lock())
			gridCoord->OnDeviceScroll(delta);

	for (auto& sc: m_SelCaretMap)
		if (auto selCaret = sc.second.lock())
			selCaret->OnDeviceScroll(delta);
}

void ViewPort::InvalidateWorldRect(CrdRect rect, TRect borderExtents) const
{
	if (!IsDefined(rect) || rect.inverted())
		return;

	auto devRect = GetCurrWorldToClientTransformation().Apply(rect);
	devRect += Convert<CrdRect>(borderExtents);
	InvalidateClientRect(devRect);
}

bool CreatePointParam(SharedMutableDataItemInterestPtr& param, ViewPort* vp, TokenID nameID)
{
	if (param || !vp->GetContext())
		return false;
	param = CreateDataItem(vp->GetContext(), nameID, 	
		Unit<Void>::GetStaticClass()->CreateDefault(), 
		vp->GetWorldCrdUnit(), ValueComposition::Single
	);
	return true;
}

AbstrDataItem* GetPointParam(SharedMutableDataItemInterestPtr& param, ViewPort* vp, TokenID nameID)
{
	if (!param && vp->GetContext())
		param = AsDynamicDataItem( vp->GetContext()->GetSubTreeItemByID(nameID) );
	return param;
}

Float64 GetSubItemValue(const TreeItem* context, TokenID id, Float64 defaultVal)
{
	dms_assert(context);
	auto si = context->GetConstSubTreeItemByID(id);
	if (!si || !IsDataItem(si))
		return defaultVal;

	InterestPtr<const TreeItem*> tii(si.get());
	PreparedDataReadLock drl(AsDataItem(si.get()), "GetSubItemValue");
	return AsDataItem(si)->GetRefObj()->GetValueAsFloat64(0);
}

void LimitRange(Float64& start, Float64& end, Float64 minSize, Float64 maxSize)
{
	dms_assert(start <= end );
	dms_assert(minSize <= maxSize);
	Float64 size = end - start;
	if (size >= minSize && size <= maxSize)
		return;
	Float64 center = (start + end) * 0.5;
	Float64 radius = ((size < minSize) ? minSize : maxSize) * 0.5;
	start = center - radius;
	end   = center + radius;
}

// Returns true if rr is a usable ROI for worldCrdUnit; rr is clamped in place.
// Rejects inverted / undefined / non-finite / out-of-envelope rects so that
// junk values (e.g. corrupted persisted RoiTopLeft/RoiBottomRight) cannot reach
// the drawing path, where Float64->Int32 conversion would yield UNDEFINED_VALUE.
static bool SanitizeRoi(CrdRect& rr, const AbstrUnit* worldCrdUnit, CrdType minSizeOverride = -1.0)
{
	if (rr.inverted() || !IsDefined(rr.first) || !IsDefined(rr.second))
	{
		reportF(SeverityTypeID::ST_MinorTrace, "ViewPort::SanitizeRoi rejected inverted/undefined ROI {}", AsString(rr).c_str());
		return false;
	}
	if (!worldCrdUnit)
		return false;

	CrdType minSize = GetSubItemValue(worldCrdUnit, vpminsID, -1.0);
	CrdType maxSize = GetSubItemValue(worldCrdUnit, vpmaxsID, 40000.0e+9);
	if (minSize == -1.0)
		minSize = (minSizeOverride > 0.0) ? minSizeOverride : 10.0 / GetUnitSizeInMeters(worldCrdUnit);

	// LimitRange only clamps the size of the range; a junk center (e.g. 1e303) survives.
	// Reject if any coordinate is already outside the projection's max envelope.
	if (Abs(rr.first .Row()) > maxSize || Abs(rr.second.Row()) > maxSize
	 || Abs(rr.first .Col()) > maxSize || Abs(rr.second.Col()) > maxSize)
	{
		reportF(SeverityTypeID::ST_MinorTrace, "ViewPort::SanitizeRoi rejected out-of-envelope ROI {} (maxSize={:g})", AsString(rr).c_str(), maxSize);
		return false;
	}

	LimitRange(rr.first.Row(), rr.second.Row(), minSize, maxSize);
	LimitRange(rr.first.Col(), rr.second.Col(), minSize, maxSize);
	return true;
}

void ViewPort::SetROI(const CrdRect& r)
{
	DBG_START("ViewPort", "SetROI", MG_DEBUG_SCROLL);

	DBG_TRACE(("NewRect : {}", AsString(r).c_str() ));

	if (m_ROI == r)
		return;

	InitWorldCrdUnit(0);
	dms_assert(m_WorldCrdUnit); // must be set before

	CrdRect rr = r;
	if (!SanitizeRoi(rr, m_WorldCrdUnit.get(), m_MinRoiSize))
		return;

	bool tlIsNew = CreatePointParam(m_ROI_TL, this, t_RoiTL);
	bool brIsNew = CreatePointParam(m_ROI_BR, this, t_RoiBR);

	DBG_TRACE(("IsNew : {}", tlIsNew || brIsNew ));
	DBG_TRACE(("TlChanged: {}", GetContext() ? m_ROI_TL->GetLastChangeTS() : 0 ));
	DBG_TRACE(("BrChanged: {}", GetContext() ? m_ROI_BR->GetLastChangeTS() : 0 ));

	ChangePoint(m_ROI_TL, rr.first , tlIsNew);
	ChangePoint(m_ROI_BR, rr.second, brIsNew);
	CertainUpdate("ViewPort::SetROI()");
	m_ROI = rr;

	DBG_TRACE(("TlChanged: {}", GetContext() ? m_ROI_TL->GetLastChangeTS() : 0 ));
	DBG_TRACE(("BrChanged: {}", GetContext() ? m_ROI_BR->GetLastChangeTS() : 0 ));
//	DoUpdateView();
	InvalidateView();
	UpdateScaleBar();
}


void SetRoiParam(TreeItem* context, SharedMutableDataItemInterestPtr& param, TokenID id, const AbstrUnit* worldCrdUnit)
{
	dms_assert(context);
	if (!param)
	{
		const AbstrDataItem* res = AsDynamicDataItem( context->GetConstSubTreeItemByID(id).get() );

		if (res && res->GetAbstrValuesUnit()->UnifyValues(worldCrdUnit, res->GetFullName().c_str(), "WorldCrdUnit", UM_AllowDefault))
			param = const_cast<AbstrDataItem*>(res);
	}
}

bool ViewPort::HasROI() const
{
	if (!m_WorldCrdUnit)
		return false;
	TreeItem* context = const_cast<ViewPort*>(this)->GetContext();
	if (!context)
		return false;

	SetRoiParam(context, m_ROI_TL, t_RoiTL, m_WorldCrdUnit.get());
	SetRoiParam(context, m_ROI_BR, t_RoiBR, m_WorldCrdUnit.get());

	return m_ROI_TL && m_ROI_BR; 
}

ActorVisitState ViewPort::DoUpdate()
{
	if ( HasROI() )
	{
		if (!m_ROI_TL->PrepareData() || !m_ROI_BR->PrepareData())
			return AVS_SuspendedOrFailed;

		DataReadLock l1(m_ROI_TL);
		DataReadLock l2(m_ROI_BR);

		CrdRect roi = CrdRect(
			m_ROI_TL->GetRefObj()->GetValueAsDPoint(0),
			m_ROI_BR->GetRefObj()->GetValueAsDPoint(0)
		);
		if (!SanitizeRoi(roi, m_WorldCrdUnit.get(), m_MinRoiSize))
			return AVS_Ready; // persisted ROI was junk; leave m_ROI for ZoomAll/SetROI to populate
		if (m_ROI != roi)
		{
			m_ROI = roi;
			if (!m_State.HasInvalidationBlock())
				InvalidateDraw();
		}
	}
	return AVS_Ready;
}

CrdRect ViewPort::GetROI() const
{ 
	dms_assert(GetContents());
	CertainUpdate("ViewPort::GetROI()");
// ISSUE 262, WIP
//	if (m_ROI.empty())
//		const_cast<ViewPort*>(this)->ZoomAll();
	return m_ROI;
}

COLORREF ViewPort::GetBkColor() const
{
	return m_BkColor;
}

GraphVisitState ViewPort::InviteGraphVistor(AbstrVisitor& v)
{
	dms_assert(!SuspendTrigger::DidSuspend());
	return v.DoViewPort(this);
}

ActorVisitState ViewPort::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (HasROI())
	{
		if (visitor.Visit(GetPointParam(m_ROI_TL, const_cast<ViewPort*>(this), t_RoiTL)) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
		if (visitor.Visit(GetPointParam(m_ROI_BR, const_cast<ViewPort*>(this), t_RoiBR)) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	}
	return base_type::VisitSuppliers(svf, visitor);
}

GridCoordPtr ViewPort::GetOrCreateGridCoord(const grid_coord_key& key)
{
	grid_coord_map::iterator pos = m_GridCoordMap.find(key);
	if (pos != m_GridCoordMap.end() && pos->first == key)
		return pos->second.lock();

	auto result = std::make_shared<GridCoord>(this, key);
	m_GridCoordMap.insert(pos, grid_coord_map::value_type(key, result) );

	auto sf = GetScaleFactors();
	auto deviceSize = ScaleCrdPoint(GetCurrClientSize(), sf);
	auto w2dTr = m_w2vTr;
	w2dTr *= CrdTransformation(CrdPoint(0.0, 0.0), sf);

	result->Init(CrdPoint2GPoint(deviceSize), w2dTr);

	return result;
}

GridCoordPtr ViewPort::GetOrCreateGridCoord(const AbstrUnit* gridCrdUnit)
{
	return GetOrCreateGridCoord(GetGridCoordKey(gridCrdUnit));
}

//----------------------------------------------------------------------
// class  : sel_caret_key
//----------------------------------------------------------------------

const AbstrUnit* GetGridUnit(const sel_caret_key& key)
{
	return key.second ? key.second->GetFeatureDomain() : key.first->GetAbstrDomainUnit();
}

SelCaretPtr  ViewPort::GetOrCreateSelCaret (const sel_caret_key& key)
{
	sel_caret_map::iterator pos = m_SelCaretMap.find(key);
	if (pos != m_SelCaretMap.end() && pos->first == key) 
		return pos->second.lock();

	auto result = std::make_shared<SelCaret>(this, key, GetOrCreateGridCoord( GetGridUnit(key) ) );
	m_SelCaretMap.insert(pos, sel_caret_map::value_type(key, result) );
	return result;
}

IMPL_RTTI_CLASS(ViewPort)

