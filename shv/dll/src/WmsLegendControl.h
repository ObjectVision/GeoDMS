// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_WMSLEGENDCONTROL_H
#define __SHV_WMSLEGENDCONTROL_H

#include "TextControl.h"

struct WmsLayer;

//----------------------------------------------------------------------
// class  : WmsLegendControl  (issue #405)
//----------------------------------------------------------------------
// Shows a WMS/WMTS legend image in the layer control, occupying the slot the
// palette would otherwise take for a normal layer. The legend raster is fetched
// and decoded lazily by the owning WmsLayer. Derives from TextControl only to
// reuse its concrete control plumbing; the text/caption is unused.

class WmsLegendControl : public TextControl
{
	typedef TextControl base_type;
public:
	WmsLegendControl(MovableObject* owner, std::shared_ptr<WmsLayer> layer);

//	override virtuals of GraphicObject / MovableObject
	bool     Draw(GraphDrawer& d) const override;
	CrdPoint CalcMaxSize() const override;

private:
	std::shared_ptr<WmsLayer> m_Layer;
};

#endif // __SHV_WMSLEGENDCONTROL_H
