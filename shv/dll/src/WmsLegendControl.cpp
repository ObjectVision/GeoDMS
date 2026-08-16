// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ShvBase.h"

#include "WmsLegendControl.h"
#include "WmsLayer.h"

#include "GraphVisitor.h"
#include "DrawContext.h"
#include "geom/PointOrder.h"

WmsLegendControl::WmsLegendControl(MovableObject* owner, std::shared_ptr<WmsLayer> layer)
	:	base_type(owner)
	,	m_Layer(std::move(layer))
{}

CrdPoint WmsLegendControl::CalcMaxSize() const
{
	// CalcMaxSize is the full (client+border) size; add the border so the *client*
	// area equals the image and nothing gets clipped (#405).
	CrdPoint border = Size(GetBorderLogicalExtents());
	if (m_Layer && m_Layer->EnsureLegendImage())
	{
		WPoint sz = m_Layer->LegendSize();
		if (sz.Col() > 0 && sz.Row() > 0)
			return shp2dms_order(Float64(sz.Col()), Float64(sz.Row())) + border;
	}
	return shp2dms_order(Float64(120), Float64(16)) + border; // "(legend unavailable)" placeholder
}

bool WmsLegendControl::Draw(GraphDrawer& d) const
{
	if (!d.DoDrawBackground())
		return false;

	auto clientAbsRect = ScaleCrdRect(GetCurrClientRelLogicalRect() + d.GetClientLogicalAbsPos(), d.GetSubPixelFactors());
	auto clientIntRect = CrdRect2GRect(clientAbsRect);
	auto* dc = d.GetDrawContext();
	if (!dc)
		return false;

	dc->FillRect(clientIntRect, CombineRGB(255, 255, 255)); // legends usually assume a white backdrop

	if (m_Layer && m_Layer->EnsureLegendImage() && m_Layer->LegendPixels())
	{
		WPoint sz = m_Layer->LegendSize();
		Int32 w = Int32(sz.Col()), h = Int32(sz.Row());
		// Scale into the full client rect (which CalcMaxSize sized to the image): at
		// 100% DPI this is 1:1, at higher DPI it scales up uniformly. Filling the rect
		// avoids the right-edge clipping seen when blitting at native pixel width.
		dc->DrawImage(clientIntRect, m_Layer->LegendPixels(), w, h, 32, nullptr, 0); // rows pre-flipped to bottom-up in EnsureLegendImage
	}
	else
	{
		CharPtr msg = "(legend unavailable)";
		dc->TextOut(GPoint(clientIntRect.left + 2, clientIntRect.top), msg, StrLen(msg), GraphicObject::GetDefaultTextColor());
	}
	return false;
}
