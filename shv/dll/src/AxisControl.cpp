// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "AxisControl.h"

#include "ChartLayer.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "mci/Class.h"
#include "utl/mySPrintF.h"

#include "DataView.h"
#include "GraphVisitor.h"
#include "ViewPort.h"

#include <cmath>

//----------------------------------------------------------------------
// class  : AxisControl
//----------------------------------------------------------------------

AxisControl::AxisControl(MovableObject* owner, ViewPort* vp, AxisKind kind)
	:	base_type(owner)
	,	m_ViewPort(vp)
	,	m_Kind(kind)
{
	assert(vp);
	m_State.Set(GOF_IgnoreActivation);
	m_connTransformChanged = vp->m_cmdTransformChanged.connect(
		[this]() { this->InvalidateDraw(); }
	);
}

COLORREF AxisControl::GetBkColor() const
{
	return DmsColor2COLORREF(CombineRGB(255, 255, 255));
}

// step size covering range/maxTicks, rounded up to 1, 2 or 5 times a power of 10
static CrdType NiceStepSize(CrdType range, UInt32 maxTicks)
{
	if (!(range > 0) || !maxTicks)
		return 0;
	CrdType rawStep = range / maxTicks;
	CrdType mag  = std::pow(10.0, std::floor(std::log10(rawStep)));
	CrdType norm = rawStep / mag;
	CrdType step = (norm <= 1.0) ? 1.0 : (norm <= 2.0) ? 2.0 : (norm <= 5.0) ? 5.0 : 10.0;
	return step * mag;
}

const int TICK_LEN = 4;

bool AxisControl::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawBackground())
		return false;

	auto* dc = d.GetDrawContext();
	auto sf  = d.GetSubPixelFactors();
	auto clientAbsRect = ScaleCrdRect(GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos(), sf);
	auto clientIntRect = CrdRect2GRect(clientAbsRect);

	dc->FillRect(clientIntRect, CombineRGB(255, 255, 255));

	DmsColor lineColor = GraphicObject::GetDefaultTextColor();
	bool isHor = (m_Kind == AxisKind::Horizontal);

	// the axis line runs along the edge shared with the plot-area ViewPort
	if (isHor)
		dc->DrawLine(GPoint(clientIntRect.left, clientIntRect.top), GPoint(clientIntRect.right, clientIntRect.top), lineColor, 1);
	else
		dc->DrawLine(GPoint(clientIntRect.right-1, clientIntRect.top), GPoint(clientIntRect.right-1, clientIntRect.bottom), lineColor, 1);

	CrdRect roi = m_ViewPort->GetROI();
	if (roi.empty())
		return false; // no zoomed contents yet: just the band and the axis line

	auto w2v = m_ViewPort->GetCurrWorldToClientTransformation();
	if (w2v.IsSingular())
		return false;
	auto vpAbsDevicePos = m_ViewPort->GetCurrClientAbsDevicePos();

#ifdef _WIN32
	auto fontHeight = GetDefaultFontHeightDIP(FontSizeCategory::SMALL) * (96.0 / 72.0);
	dc->SetFont("Noto Sans Medium", fontHeight, 0);
#else
	auto fontHeight = GetDefaultFontHeightDIP(FontSizeCategory::SMALL) * (72.0 / 96.0);
	dc->SetFont("Noto Sans", fontHeight, 0);
#endif
	dc->SetBkMode(true);

	CrdType worldMin = isHor ? roi.first .X() : roi.first .Y();
	CrdType worldMax = isHor ? roi.second.X() : roi.second.Y();

	// categorical horizontal axis: when the active ChartLayer maps a non-numeric X attribute to
	// ordinal positions, label each ordinal with its category instead of drawing numeric ticks.
	if (isHor)
	{
		auto* chartLayer = dynamic_cast<const ChartLayer*>(m_ViewPort->GetActiveLayer());
		if (chartLayer && !chartLayer->GetXAxisLabels().empty())
		{
			for (const auto& posLabel : chartLayer->GetXAxisLabels())
			{
				CrdType pos = posLabel.first;
				if (pos < worldMin || pos > worldMax)
					continue;
				CrdPoint vpLogical = w2v.Apply(shp2dms_order<CrdType>(pos, roi.first.Y()));
				GType devX = GType(vpAbsDevicePos.X() + vpLogical.X() * sf.X());
				if (devX < clientIntRect.left || devX >= clientIntRect.right)
					continue;
				const SharedStr& text = posLabel.second;
				dc->DrawLine(GPoint(devX, clientIntRect.top), GPoint(devX, clientIntRect.top + TICK_LEN), lineColor, 1);
				dc->TextOut(GPoint(devX + 2, clientIntRect.top + TICK_LEN), text.c_str(), text.ssize(), lineColor);
			}
			return false;
		}
	}

	auto bandDeviceSize = isHor ? clientIntRect.Width() : clientIntRect.Height();
	UInt32 maxTicks = Max<Int32>(bandDeviceSize / (isHor ? 80 : 32), 1);
	CrdType step = NiceStepSize(worldMax - worldMin, maxTicks);
	if (!(step > 0))
		return false;

	CrdType firstTick = std::ceil(worldMin / step) * step;
	for (CrdType t = firstTick; t <= worldMax; t += step)
	{
		// world -> ViewPort-logical -> device, then into our band which shares the relevant device axis
		CrdPoint worldPnt = isHor
			? shp2dms_order<CrdType>(t, roi.first.Y())
			: shp2dms_order<CrdType>(roi.first.X(), t);
		CrdPoint vpLogical = w2v.Apply(worldPnt);

		SharedStr label = mySSPrintF("{:.6g}", t);
		if (isHor)
		{
			GType devX = GType(vpAbsDevicePos.X() + vpLogical.X() * sf.X());
			if (devX < clientIntRect.left || devX >= clientIntRect.right)
				continue;
			dc->DrawLine(GPoint(devX, clientIntRect.top), GPoint(devX, clientIntRect.top + TICK_LEN), lineColor, 1);
			dc->TextOut(GPoint(devX + 2, clientIntRect.top + TICK_LEN), label.c_str(), label.ssize(), lineColor);
		}
		else
		{
			GType devY = GType(vpAbsDevicePos.Y() + vpLogical.Y() * sf.Y());
			if (devY < clientIntRect.top || devY >= clientIntRect.bottom)
				continue;
			dc->DrawLine(GPoint(clientIntRect.right - 1 - TICK_LEN, devY), GPoint(clientIntRect.right - 1, devY), lineColor, 1);
			dc->TextOut(GPoint(clientIntRect.left + 2, devY - GType(fontHeight)), label.c_str(), label.ssize(), lineColor);
		}
	}
	return false;
}

IMPL_RTTI_CLASS(AxisControl)
