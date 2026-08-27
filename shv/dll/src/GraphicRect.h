// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_GRAPHICRECT_H
#define __SHV_GRAPHICRECT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ScalableObject.h"
#include "DcHandle.h"

//#define DMS_SUPPORT_WINNT40
#if defined(DMS_SUPPORT_WINNT40)
#	define SHV_ALPHABLEND_NOT_SUPPORTED
#endif

//----------------------------------------------------------------------
// class  : RoiTracker
//----------------------------------------------------------------------

struct RoiTracker
{
	virtual void AdjustTargetVieport() =0;
};

//----------------------------------------------------------------------
// class  : GraphicRect
//----------------------------------------------------------------------

class GraphicRect : public ScalableObject, RoiTracker
{
private:
	typedef ScalableObject base_type;
public:
//	Constructor
	GraphicRect(ScalableObject* owner);
	~GraphicRect();

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ClipExtents; };

	void SetROI(const CrdRect& roi);

//	override GraphicObject virtuals for size & display of GraphicObjects
	void UpdateExtents();
	void    DoUpdateView() override;
	TRect   GetBorderLogicalExtents() const override;
	bool Draw(GraphDrawer& d) const override;

	bool MouseEvent(MouseEventDispatcher& med) override;
	
	void MoveWorldRect(const CrdPoint& point);

protected: // override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

private:
	std::weak_ptr<MapControl> GetMapControl();
	std::weak_ptr<ViewPort> GetSourceVP  ();
	std::weak_ptr<ViewPort> GetTargetVP  ();

	void AdjustTargetVieport() override;
	bool DrawRect(GraphDrawer& d, const CrdRect& wr, const CrdRect& cr, GRect clientRect) const;

private: friend class LayerSet;
	DmsColor                       m_PenColor;
	DmsColor                       m_BlendColor;
	CrdRect                        m_ROI;
	mutable std::weak_ptr<ViewPort> m_SourceVP;
	mutable std::weak_ptr<ViewPort> m_TargetVP;


	DECL_RTTI(, ShvClass)
};

#endif // __SHV_GRAPHICRECT_H

