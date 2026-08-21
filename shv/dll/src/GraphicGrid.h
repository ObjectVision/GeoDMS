// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_GRAPHICGRID_H
#define __SHV_GRAPHICGRID_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ScalableObject.h"
#include "vt/color.h"

//----------------------------------------------------------------------
// class  : GraphicGrid
//----------------------------------------------------------------------

class GraphicGrid : public ScalableObject
{
private:
	typedef ScalableObject base_type;
public:
//	Constructor
	GraphicGrid(ScalableObject* owner);

//	override GraphicObject virtuals for size & display of GraphicObjects
	void DoUpdateView() override;
	bool Draw(GraphDrawer& d) const override;
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;

protected:
//	override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

public:
	SharedDataItemInterestPtr m_Source_TL;
	SharedDataItemInterestPtr m_Source_BR;
private:
	DmsColor m_BrushColor;
	DmsColor m_PenColor;

	DECL_RTTI(, ShvClass)
};

#endif // __SHV_GRAPHICGRID_H

