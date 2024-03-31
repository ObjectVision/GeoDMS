// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#include "ScaleBar.h"

#include "act/ActorVisitor.h"
#include "geo/PointOrder.h"
#include "ser/AsString.h"
#include "utl/mySPrintF.h"

#include "AbstrUnit.h"
#include "Metric.h"

#include "DataView.h"
#include "GraphVisitor.h"
#include "ShvUtils.h"
#include "TextControl.h"
#include "ViewPort.h"

//----------------------------------------------------------------------
// class  : ScaleBarBase interface
//----------------------------------------------------------------------

ScaleBarBase::ScaleBarBase(const ViewPort* vp)
	:	m_ViewPort(vp), m_Factor(UNDEFINED_VALUE(CrdType))
{}

ScaleBarBase::~ScaleBarBase()
{}

const UInt32 MIN_MEASURE_SIZE = 16;
const UInt32 MAX_TEXT_HEIGHT = 12;

bool ScaleBarBase::Draw(HDC dc, CrdRect devAbsRect) const
{
	GRect clientAbsRect = CrdRect2GRect(devAbsRect);
	GPoint clientSize = clientAbsRect.Size();
	if (clientSize.y < 30)
		return false;

	const Float64 maxTextHeight = MAX_TEXT_HEIGHT * GetWindowDip2PixFactorY(WindowFromDC(dc));
	CrdType measureGroupSize = m_MeasureSize * m_MeasureGroupCount;
	UInt32  nrGroups         = clientSize.x / measureGroupSize; if (nrGroups == 0) return false;
	GType   top              = clientSize.y / 3;
	GType   textTop          = Max<GType>(top - GType(maxTextHeight+2), GType(0));
	GType   bottom           = clientSize.y - top;
	GType   textBottom       = Min<GType>(bottom + GType(maxTextHeight +2), clientSize.y);
	GType   mid              = (top+bottom)/2;
	CrdType left             = 0.5*(clientSize.x - measureGroupSize * nrGroups);
	Float64 topVal           = 0.0;
	Float64 botVal           = 0.0;

	GdiHandle<HBRUSH> brw( CreateSolidBrush( DmsColor2COLORREF(CombineRGB(0xFF, 0xFF, 0xFF)) ) );
	GdiHandle<HBRUSH> br1( CreateSolidBrush( DmsColor2COLORREF(0) ) );
	GdiHandle<HBRUSH> br2( CreateSolidBrush( DmsColor2COLORREF(CombineRGB(0xFF, 0, 0)) ) );
	GPoint topLeft = clientAbsRect.LeftTop();
	for (; nrGroups; --nrGroups)
	{
		{
			GRect bottomRect = GRect(left, bottom, left + measureGroupSize, textBottom) + topLeft;
			DrawRectDmsColor(dc, bottomRect, CombineRGB(0, 0, 0xFF));
			bottomRect.Shrink(1);
			FillRectWithBrush(dc, bottomRect, brw);

			botVal += m_MeasureValue * m_MeasureGroupCount;
			DrawText(
				dc, 
				mySSPrintF("%1.0lf%s", botVal, m_UnitLabel.c_str()).c_str(),
				-1,
				&bottomRect,
				DT_CENTER|DT_VCENTER
			);
		}
		{
			for (UInt32 i=0; i!= m_MeasureGroupCount; ++i)
			{
				CrdType right = left + m_MeasureSize;
				GRect topRect = GRect(left, textTop, right, top) + topLeft;
				DrawRectDmsColor(dc, topRect, CombineRGB(0, 0, 0xFF));
				topRect.Shrink(1);
				FillRectWithBrush(dc, topRect, brw);
				topVal += m_MeasureValue;
				DrawText(
					dc, 
					mySSPrintF("%1.0lf", topVal).c_str(),
					-1,
					&topRect,
					DT_CENTER|DT_VCENTER
				);

				FillRectWithBrush(dc, GRect(left, top, right, mid   ) + topLeft, br1);
				FillRectWithBrush(dc, GRect(left, mid, right, bottom) + topLeft, br2);
				br1.swap(br2);
				left = right;
			}
		}
	}
	return false;
}

bool ScaleBarBase::MustUpdateView() const
{
	assert(m_ViewPort);
	CrdType          currFactor  = m_ViewPort->GetCurrWorldToDeviceZoomLevel();
	const AbstrUnit* currCrdUnit = m_ViewPort->GetWorldCrdUnit();

	return m_Factor != currFactor || m_CrdUnit != currCrdUnit;
}

bool ScaleBarBase::DoUpdateViewImpl(CrdPoint scaleFactor)
{
	assert(m_ViewPort);
	CrdType          currFactor  = m_ViewPort->GetCurrWorldToDeviceZoomLevel();
	const AbstrUnit* currCrdUnit = m_ViewPort->GetWorldCrdUnit();

	if (m_Factor == currFactor && m_CrdUnit == currCrdUnit)
		return false;
	m_Factor  = currFactor;  assert(IsDefined(m_Factor));
	m_CrdUnit = currCrdUnit; assert(m_CrdUnit);

	m_MeasureSize = m_Factor;

	const UnitMetric* metric = m_CrdUnit->GetMetric();
	if (metric)
	{
		m_MeasureSize *= metric->m_Factor;
		switch (metric->m_BaseUnits.size())
		{
			case 0:
				m_UnitLabel = "m";
				break;
			case 1:
				m_UnitLabel = metric->m_BaseUnits.begin()->first;
				dms_assert(metric->m_BaseUnits.begin()->second == 1);
				break;
			default:
				UnitMetric tmp = *metric; tmp.m_Factor = 1;
				m_UnitLabel = tmp.AsString(FormattingFlags::ThousandSeparator);
				break;
		}
	}
	if (m_UnitLabel != "m")
	{
		if (m_UnitLabel == "mm") { m_UnitLabel = "m"; m_MeasureSize *= 0.001;  }
		if (m_UnitLabel == "cm") { m_UnitLabel = "m"; m_MeasureSize *= 0.01;   }
		if (m_UnitLabel == "dm") { m_UnitLabel = "m"; m_MeasureSize *= 0.1;    }
		if (m_UnitLabel == "km") { m_UnitLabel = "m"; m_MeasureSize *= 1000.0; }
	}

	UInt32 measureMantissa   = 1;
	Int32  measureExponent   = 0;

	const Float64 minMeasureSize = MIN_MEASURE_SIZE * scaleFactor.first;

	m_MeasureGroupCount = 5;
	while (m_MeasureSize >  2.0*minMeasureSize) { m_MeasureSize *= 0.1; --measureExponent; }
	while (m_MeasureSize <= 0.2*minMeasureSize) { m_MeasureSize *= 10 ; ++measureExponent; }
	if    (m_MeasureSize <= minMeasureSize) { m_MeasureSize *= 2.0; measureMantissa = 2; }
	if    (m_MeasureSize <= minMeasureSize) { m_MeasureSize *= 2.5; measureMantissa = 5; }

	dms_assert(m_MeasureSize > minMeasureSize);
	dms_assert(m_MeasureSize * m_MeasureGroupCount <= 12.5*minMeasureSize);

	if (m_UnitLabel == "m")
	{
		if (measureExponent >=3)      { measureExponent -= 3; m_UnitLabel = "km"; }
		else if (measureExponent <-3) { measureExponent += 9; m_UnitLabel = "nm"; }
		else if (measureExponent <-2) { measureExponent += 3; m_UnitLabel = "mm"; }
		else if (measureExponent < 0) { measureExponent += 2; m_UnitLabel = "cm"; }
	}

	m_MeasureValue = measureMantissa;
	while (measureExponent > 0) { m_MeasureValue *= 10;    --measureExponent;  }
	while (measureExponent <-2) { m_MeasureValue *= 0.001; measureExponent+=3; }
	while (measureExponent < 0) { m_MeasureValue *= 0.1;    ++measureExponent; }
	dms_assert(measureExponent == 0);

	return true;
}

//----------------------------------------------------------------------
// class  : ScaleBarObj implementaion
//----------------------------------------------------------------------

ScaleBarObj::ScaleBarObj(MovableObject* owner, const ViewPort* vp)
	:	base_type(owner)
	,	m_Impl(vp)
{
	m_State.Set(GOF_Transparent);
}

ScaleBarObj::~ScaleBarObj()
{}

//	override virtuals of Actor
ActorVisitState ScaleBarObj::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (m_Impl.GetViewPort()->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
		return AVS_SuspendedOrFailed;
	return base_type::VisitSuppliers(svf, visitor);
}

bool ScaleBarObj::Draw(GraphDrawer& d) const
{
	//const_cast<ScaleBarObj*>(this)->DoUpdateView();  // maybe size has changed the factor without invalidating the viewport
	assert(IsUpdated());
	auto absLogicalRect = GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos();
	return m_Impl.Draw(d.GetDC(), ScaleCrdRect(absLogicalRect, GetScaleFactors()));
}

void ScaleBarObj::DoUpdateView()
{
	m_Impl.GetViewPort()->UpdateView();
	assert(m_Impl.GetViewPort()->IsUpdated());

	auto dv = GetDataView().lock();
	if (dv && m_Impl.DoUpdateViewImpl(dv->GetScaleFactors()))
		InvalidateDraw();
}

//----------------------------------------------------------------------
// class  : ScaleBarCaret implementaion
//----------------------------------------------------------------------

ScaleBarCaret::ScaleBarCaret(const ViewPort* vp)
	:	m_Impl(vp)
{
}

ScaleBarCaret::~ScaleBarCaret()
{}


//	override AbstrCaret
void ScaleBarCaret::Reverse(HDC dc, bool newVisibleState)
{
	if (newVisibleState)
	{
		DcMixModeSelector xxxDontXRor(dc, R2_COPYPEN);
		m_Impl.DoUpdateViewImpl(GetWindowDip2PixFactors(WindowFromDC(dc)));
		m_Impl.Draw(dc, GetCurrDeviceExtents());
	}
}

CrdRect ScaleBarCaret::GetCurrDeviceExtents() const
{
	return m_DeviceExtents;
}

CrdPoint ScaleBarBase::GetLogicalSize() const
{
	return shp2dms_order<CrdType>(250, 48);
}

CrdRect ScaleBarBase::DetermineBoundingBox(const MovableObject* owner, CrdPoint subPixelFactors) const
{
	auto rect = owner->GetCurrClientAbsDeviceRect();
	auto logicalSize = GetLogicalSize();
	auto scaleBarSize = ScaleCrdPoint(logicalSize, subPixelFactors);
	MakeMax(rect.first.X(), rect.second.X() - scaleBarSize.X());
	MakeMax(rect.first.Y(), rect.second.Y() - scaleBarSize.Y());
	return rect;
}

void ScaleBarObj::DetermineAndSetLogicalBoundingBox(CrdPoint currTL, CrdPoint currPageSize)
{
	SetClientRect( CrdRect(currPageSize - m_Impl.GetLogicalSize(), currPageSize) - currTL );
	UpdateView();
}

void ScaleBarCaret::DetermineAndSetBoundingBox(CrdPoint scaleFactor)
{
	m_DeviceExtents = m_Impl.DetermineBoundingBox(m_Impl.GetViewPort(), scaleFactor);
}

void ScaleBarCaret::Move(const AbstrCaretOperator& caretOper, HDC dc)
{
	if (m_Impl.MustUpdateView())
		base_type::Move(caretOper, dc);
}


void ScaleBarCaretOperator::operator() (AbstrCaret* c) const
{
	assert(c);
	auto sbc = dynamic_cast<ScaleBarCaret*>(c);
	assert(sbc); if (!sbc) return;
	auto vp = sbc->GetViewPort();
	assert(vp); if (!vp) return;

	auto dv = vp->GetDataView().lock(); if (!dv) return;

	sbc->DetermineAndSetBoundingBox(dv->GetScaleFactors());
	sbc->SetStartPoint(GPoint(0, 0) );
}
