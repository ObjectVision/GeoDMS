// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <cmath>

#include "act/ActorVisitor.h"
#include "geom/Transform.h"
#include "vt/Conversions.h"

#include "Param.h"
#include "TreeItemClass.h"

#include "ShvUtils.h"

#include "CounterStacks.h"
#include "GraphicGrid.h"
#include "GraphVisitor.h"


GraphicGrid::GraphicGrid(ScalableObject* owner)
	:	base_type(owner)
	,	m_BrushColor( CombineRGB(0xFF, 0xFF, 0x80) )
	,	m_PenColor  ( CombineRGB(0x0,  0x0,  0xFF) )
{
}

void GraphicGrid::Sync(TreeItem* viewContext, ShvSyncMode sm)
{
	base_type::Sync(viewContext, sm);
	SyncRef(m_Source_TL, viewContext, GetTokenID_mt("TL"), sm);
	SyncRef(m_Source_BR, viewContext, GetTokenID_mt("BR"), sm);
}


//	override GraphicObject virtuals for size & display of GraphicObjects
void GraphicGrid::DoUpdateView()
{
	dms_assert(m_Source_TL->GetAbstrDomainUnit()->GetCount() == 1);
	dms_assert(m_Source_BR->GetAbstrDomainUnit()->GetCount() == 1);

	PreparedDataReadLock tlLock(m_Source_TL.get_ptr(), "GraphicGrid::DoUpdateView()");
	PreparedDataReadLock brLock(m_Source_BR.get_ptr(), "GraphicGrid::DoUpdateView()");

	SetWorldClientRect(
		CrdRect(
			m_Source_TL->GetRefObj()->GetValueAsDPoint(0), 
			m_Source_BR->GetRefObj()->GetValueAsDPoint(0)
		)
	);
}

bool GraphicGrid::Draw(GraphDrawer& d) const
{
	auto* drawCtx = d.GetDrawContext();
	const CrdTransformation& tr = d.GetTransformation();
	CrdRect wr = CalcWorldClientRect();

	wr &= d.GetWorldClipRect();

	const bool separable = tr.IsAxisSeparable();

	ResumableCounter counter(d.GetCounterStacks(), false);

	if (counter.Value() == 0)
	{
		if (separable)
			drawCtx->FillRect(DRect2GRect(wr, tr), m_BrushColor);
		else
		{
			// rotated/tilted: the world-client rect maps to a (possibly skewed) quad, not an
			// axis-aligned device rect, so fill its four transformed corners as a polygon.
			GPoint quad[4] = {
				DPoint2GPoint(wr.first, tr),
				DPoint2GPoint(shp2dms_order<CrdType>(wr.second.X(), wr.first .Y()), tr),
				DPoint2GPoint(wr.second, tr),
				DPoint2GPoint(shp2dms_order<CrdType>(wr.first .X(), wr.second.Y()), tr),
			};
			drawCtx->DrawPolygon(quad, 4, m_BrushColor);
		}
	}
	++counter;
	if (counter.MustBreakOrSuspend())
		return true;

	wr = Convert<CrdRect>(Convert<SRect>(wr)); // round-off to integers;

	if (separable)
	{
		// ---- level-c device-axis-aligned path (byte-identical to the historical code) ----
		GRect sr = DRect2GRect(wr, tr);

		CrdPoint factor = Abs( tr.Factor() );
		assert(factor.first  > 0);
		assert(factor.second > 0);
		MakeMax(factor, CrdPoint(1, 1));

		CrdType right  = sr.right  + factor.first;
		CrdType bottom = sr.bottom + factor.second;
		if (counter.Value() == 1)
		{
			for (CrdType i=sr.top;  i <= bottom; i += factor.second)
				drawCtx->DrawLine(GPoint(sr.left, i), GPoint(right, i), m_PenColor);
			++counter;
			if (counter.MustBreakOrSuspend())
				return true;
		}
		if (counter.Value() == 2)
		{
			for (CrdType j=sr.left; j <= right;  j += factor.first)
				drawCtx->DrawLine(GPoint(j, sr.top), GPoint(j, bottom), m_PenColor);
		}
		counter.Close();
		return false;
	}

	// ---- rotation/tilt path: grid lines run along the WORLD axes (constant x, constant y),
	// each drawn as one device line between its two transformed endpoints. A projective transform
	// maps straight lines to straight lines, so two endpoints suffice even under tilt. ----
	CrdType xLo = Min<CrdType>(wr.first.X(), wr.second.X());
	CrdType xHi = Max<CrdType>(wr.first.X(), wr.second.X());
	CrdType yLo = Min<CrdType>(wr.first.Y(), wr.second.Y());
	CrdType yHi = Max<CrdType>(wr.first.Y(), wr.second.Y());

	// One line per world unit, coarsened so device spacing stays >= ~1px when zoomed out
	// (mirrors the level-c MakeMax(factor,1) cap), and capped so a steep tilt / huge clip rect
	// can never spawn an unbounded number of lines.
	CrdType localScale = tr.LocalScaleAt(Center(wr)); // device px per world unit
	CrdType step = (localScale > 0 && localScale < 1.0) ? std::ceil(1.0 / localScale) : 1.0;
	const CrdType MAX_LINES_PER_AXIS = 2000;
	MakeMax(step, (xHi - xLo) / MAX_LINES_PER_AXIS);
	MakeMax(step, (yHi - yLo) / MAX_LINES_PER_AXIS);
	if (!(step > 0))
		step = 1.0;

	if (counter.Value() == 1)
	{
		for (CrdType y = yLo; y <= yHi; y += step)
			drawCtx->DrawLine(
				DPoint2GPoint(shp2dms_order<CrdType>(xLo, y), tr),
				DPoint2GPoint(shp2dms_order<CrdType>(xHi, y), tr),
				m_PenColor);
		++counter;
		if (counter.MustBreakOrSuspend())
			return true;
	}
	if (counter.Value() == 2)
	{
		for (CrdType x = xLo; x <= xHi; x += step)
			drawCtx->DrawLine(
				DPoint2GPoint(shp2dms_order<CrdType>(x, yLo), tr),
				DPoint2GPoint(shp2dms_order<CrdType>(x, yHi), tr),
				m_PenColor);
	}
	counter.Close();
	return false;
}


//	override virtual of Actor
ActorVisitState GraphicGrid::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	if (visitor.Visit(m_Source_TL.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;
	if (visitor.Visit(m_Source_BR.get_ptr()) == AVS_SuspendedOrFailed) return AVS_SuspendedOrFailed;

	return base_type::VisitSuppliers(svf, visitor);
}

#include "LayerClass.h"

IMPL_DYNC_SHVCLASS(GraphicGrid,ScalableObject)
