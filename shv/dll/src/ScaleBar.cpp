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

#include "ScaleBar.h"

#include "act/ActorVisitor.h"
#include "geo/PointOrder.h"
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

bool ScaleBarBase::Draw(HDC dc, const GRect& clientAbsRect) const
{
	GPoint clientSize = clientAbsRect.Size();
	if (clientSize.y < 30)
		return false;

	const Float64 maxTextHeight = MAX_TEXT_HEIGHT * GetDcDIP2pixFactorY(dc);
	CrdType measureGroupSize = m_MeasureSize * m_MeasureGroupCount;
	UInt32  nrGroups         = clientSize.x / measureGroupSize; if (nrGroups == 0) return false;
	TType   top              = clientSize.y / 3;
	TType   textTop          = Max<TType>(top - TType(maxTextHeight +2), TType(0));
	TType   bottom           = clientSize.y - top;
	TType   textBottom       = Min<TType>(bottom + TType(maxTextHeight +2), clientSize.y);
	TType   mid              = (top+bottom)/2;
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

		;
	}
	return false;
}

bool ScaleBarBase::MustUpdateView() const
{
	dms_assert(m_ViewPort);
	CrdType          currFactor  = m_ViewPort->GetCurrZoomLevel();
	const AbstrUnit* currCrdUnit = m_ViewPort->GetWorldCrdUnit();

	return m_Factor != currFactor || m_CrdUnit != currCrdUnit;
}

bool ScaleBarBase::DoUpdateViewImpl(CrdPoint scaleFactor)
{
	dms_assert(m_ViewPort);
	CrdType          currFactor  = m_ViewPort->GetCurrZoomLevel();
	const AbstrUnit* currCrdUnit = m_ViewPort->GetWorldCrdUnit();

	if (m_Factor == currFactor && m_CrdUnit == currCrdUnit)
		return false;
	m_Factor  = currFactor;  dms_assert(IsDefined(m_Factor));
	m_CrdUnit = currCrdUnit; dms_assert(m_CrdUnit);

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
	dms_assert(IsUpdated());
	auto absLogicalRect = GetCurrClientRelLogicalRect() + d.GetClientLogicalOffset();
	return m_Impl.Draw(d.GetDC(), TRect2GRect(absLogicalRect, GetScaleFactors()));
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
		m_Impl.DoUpdateViewImpl(GetDcDIP2pixFactorXY(dc));
		m_Impl.Draw(dc, GetCurrBoundingBox());
	}
}

GRect ScaleBarCaret::GetCurrBoundingBox() const
{
	return m_Rect;
}

TPoint ScaleBarBase::GetSize(Float64 subPixelFactor) const
{
	return shp2dms_order<TType>(250 * subPixelFactor, 48 * subPixelFactor);
}	

GRect ScaleBarBase::DetermineBoundingBox(const MovableObject* owner, CrdPoint subPixelFactors) const
{
	GRect rect = owner->GetCurrClientAbsDeviceRect();
	rect *=subPixelFactors;
	MakeMax(rect.left, rect.right- subPixelFactors.first );
	MakeMax(rect.top, rect.bottom - subPixelFactors.second);
	return rect;
}

void ScaleBarObj::DetermineAndSetBoundingBox(TPoint currTL, TPoint currPageSize, Float64 subPixelFactor)
{
	SetClientRect( TRect(currPageSize - m_Impl.GetSize(subPixelFactor), currPageSize) - currTL );
	UpdateView();
}

void ScaleBarCaret::DetermineAndSetBoundingBox(CrdPoint scaleFactor)
{
	m_Rect = m_Impl.DetermineBoundingBox(m_Impl.GetViewPort(), scaleFactor);
}

void ScaleBarCaret::Move(const AbstrCaretOperator& caretOper, HDC dc)
{
	if (m_Impl.MustUpdateView())
		base_type::Move(caretOper, dc);
}


void ScaleBarCaretOperator::operator() (AbstrCaret* c) const
{
	assert(c);
	auto obj = c->UsedObject(); if (!obj) return;
	auto dv = obj->GetDataView().lock(); if (!dv) return;

	debug_cast<ScaleBarCaret*>(c)->DetermineAndSetBoundingBox(dv->GetScaleFactors());
	c->SetStartPoint(GPoint(0, 0) );
}
