// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#ifndef __SHV_GRIDLAYERBASE_H
#define __SHV_GRIDLAYERBASE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GraphicLayer.h"
#include "GridCoord.h"

//----------------------------------------------------------------------
// class  : GridLayerBase
//----------------------------------------------------------------------

struct GridLayerBase : GraphicLayer
{
	using GraphicLayer::GraphicLayer;
	typedef GraphicLayer base_type;

protected:
	GridCoordPtr GetGridCoordInfo(ViewPort* vp) const; friend class ViewPort;

	void FillLcMenu(MenuData& menuData) override;

	virtual void Zoom1To1(ViewPort* vp) = 0;
	void Zoom1To1Caller() { Zoom1To1(GetViewPort()); }
	mutable GridCoordPtr m_GridCoord;
};


#endif // __SHV_GRIDLAYERBASE_H

