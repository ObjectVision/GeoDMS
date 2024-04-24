#include "ShvDllPch.h"

#include "GraphVisitor.h"

#include "act/MainThread.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DebugContext.h"
#include "dbg/SeverityType.h"
#include "geo/PointOrder.h"
#include "geo/Conversions.h"
#include "geo/Round.h"
#include "mci/Class.h"
#include "ser/RangeStream.h"

#include "AbstrDataItem.h"

#include "act/ActorVisitor.h"
#include "ActivationInfo.h"
#include "Carets.h"
#include "Controllers.h"
#include "ClipBoard.h"
#include "CounterStacks.h"
#include "DataItemColumn.h"
#include "DataView.h"
#include "DcHandle.h"
#include "FeatureLayer.h"
#include "GraphicContainer.h"
#include "GridLayer.h"
#include "LayerControl.h"
#include "LayerSet.h"
#include "MapControl.h"
#include "ScrollPort.h"
#include "ThemeReadLocks.h"
#include "ViewPort.h"

#include <typeinfo>

//----------------------------------------------------------------------
// SelectPoint2Rect
//----------------------------------------------------------------------

inline GRect SelectPoint2Rect(GPoint clipPoint)
{
	return GRect(
		clipPoint, 
		clipPoint + GPoint(1,1) 
	);
}

//----------------------------------------------------------------------
// AbstrVisitor members
//----------------------------------------------------------------------

GraphVisitState AbstrVisitor::DoObject(GraphicObject* obj)
{
	dms_assert(obj);

	return GVS_Continue; // processing not completed
}

GraphVisitState AbstrVisitor::DoMovable(MovableObject* obj)
{
	return DoObject(obj);
}

GraphVisitState AbstrVisitor::DoMovableContainer(MovableContainer* obj)
{
	dms_assert(obj);
	dms_assert(!HasCounterStacks());

	SizeT n = obj->NrEntries();
	for (SizeT i = 0; i<n ; i++)
	{
		GraphicObject* entry = obj->GetEntry(i);
		dms_assert(entry);
		if (Visit( entry ) != GVS_Continue)
			return GVS_Break;
	}
	// processing not completed
	return DoMovable(obj);
}

GraphVisitState AbstrVisitor::DoLayerSet(LayerSet* obj)
{
	dms_assert(obj);
	dms_assert(!HasCounterStacks());

	gr_elem_index n = obj->NrEntries();
	for (gr_elem_index i = 0; i<n ; i++)
	{
		GraphicObject* gEntry = obj->GetEntry(i);
		dms_assert(gEntry);
		if (Visit( gEntry ) != GVS_Continue)
			return GVS_Break;
	}
	// processing not completed
	return DoScalable(obj);
}

GraphVisitState AbstrVisitor::DoScalable  (ScalableObject*   obj) { return DoObject(obj); }

GraphVisitState AbstrVisitor::DoDataItemColumn(DataItemColumn* dic)  { return DoMovable(dic); }
void  AbstrVisitor::DoElement(DataItemColumn* dic, SizeT i, const GRect& absElemDeviceRect) { }

GraphVisitState AbstrVisitor::DoWrapper(Wrapper* obj)
{
	assert(obj->GetContents());

	if (Visit( obj->GetContents() ) != GVS_Continue)
		return GVS_Break;
	return DoMovable(obj);
}

GraphVisitState AbstrVisitor::DoViewPort    (ViewPort*     vp) { return DoWrapper(vp); }
GraphVisitState AbstrVisitor::DoScrollPort  (ScrollPort*   sp) { return DoWrapper(sp); }
GraphVisitState AbstrVisitor::DoLayer       (GraphicLayer* gl) { return DoScalable(gl); }
GraphVisitState AbstrVisitor::DoGridLayer   (GridLayer*    gl) { return DoLayer  (gl); }
GraphVisitState AbstrVisitor::DoFeatureLayer(FeatureLayer* fl) { return DoLayer  (fl); }
GraphVisitState AbstrVisitor::DoMapControl  (MapControl*   mc) { return DoMovableContainer(mc); }

GraphVisitState AbstrVisitor::DoLayerControlBase (LayerControlBase*  lcb) { return DoMovableContainer(lcb); }
GraphVisitState AbstrVisitor::DoLayerControl     (LayerControl* lc)       { return DoLayerControlBase(lc); }
GraphVisitState AbstrVisitor::DoLayerControlSet  (LayerControlSet*   lcs) { return DoMovableContainer(lcs); }
GraphVisitState AbstrVisitor::DoLayerControlGroup(LayerControlGroup* lcg) { return DoLayerControlBase(lcg); }

WeakPtr<CounterStacks> AbstrVisitor::GetCounterStacks() const
{
	return nullptr;
}

GraphVisitState AbstrVisitor::Visit(GraphicObject* go)
{
	dms_assert(go);

	if (!go->IsVisible())
		return GVS_UnHandled;
	dms_assert(go->AllVisible());

//	go->UpdateView();

	dms_assert(!SuspendTrigger::DidSuspend());
	return go->InviteGraphVistor(*this);
}

bool AbstrVisitor::MustBreak() const
{
	return HasCounterStacks() && GetCounterStacks()->MustBreak(); 
}

//----------------------------------------------------------------------
// GraphVisitor members
//----------------------------------------------------------------------

GraphVisitor::GraphVisitor(const GRect& clipDeviceRect, DPoint scaleFactors)
	:	m_ClipDeviceRect(clipDeviceRect)
	,	m_ClientLogicalAbsPos(Point<TType>(0, 0))
	,	m_Transformation(CrdPoint(0, 0), scaleFactors)
	,	m_SubPixelFactors(scaleFactors)
{}

GraphVisitState GraphVisitor::Visit(GraphicObject* obj)
{
	DBG_START(typeid(*this).name(), "Visit", MG_DEBUG_VIEWINVALIDATE);
	DBG_TRACE(("%s %x: %x", obj->GetDynamicClass()->GetName().c_str(), obj, &*(obj->GetOwner().lock())));
	DBG_TRACE(("Cliprect     : %s", AsString(m_ClipDeviceRect).c_str()));

	assert(obj);
	assert(obj->AllVisible());

	assert(!SuspendTrigger::DidSuspend());
	obj->UpdateView();
	assert(!SuspendTrigger::DidSuspend());

	assert(obj->AllVisible());

	if (MustBreak())
		return GVS_Break; // equals GVS_Handled

	if (obj->InviteGraphVistor(*this) == GVS_Handled)
		return GVS_Handled; // return;
	assert(!SuspendTrigger::DidSuspend());

	obj->UpdateView(); // retry
	assert(!SuspendTrigger::DidSuspend());

	assert(obj->IsUpdated() || obj->WasFailed());
	return GVS_UnHandled;
}

GraphVisitState GraphVisitor::DoMovableContainer(MovableContainer* gc)
{
	assert(gc);
	assert(!MustBreak());

	SizeT n = gc->NrEntries();

	ResumableCounter counter( GetCounterStacks(), false); if (counter.MustBreakOrSuspend()) return GVS_Break;
	{
		AddClientLogicalOffset localBase(this, gc->GetCurrClientRelPos());

		while (counter < n)
		{
			MovableObject* gEntry = gc->GetEntry( counter.Value() );
			assert(gEntry);
			if ( !gEntry->IsVisible() ) 
				goto nextEntry;
			else
			{	
				if (gEntry->GetClippedCurrFullAbsDeviceRect(*this).empty())
					goto nextEntry;
			}
			if (Visit(gEntry))
				return GVS_Break; // suspend processing, come back here with the same counter

		nextEntry:
			if (!counter.Inc())
				return GVS_Break; // euqals GVS_Handled
		}
	}
	if (DoMovable(gc) != GVS_UnHandled)
		return GVS_Break;

	counter.Close();
	return GVS_Continue;
}

GraphVisitState GraphVisitor::DoLayerSet(LayerSet* gc)
{
	dms_assert(gc);
	dms_assert(!MustBreak());

	SizeT n = gc->NrEntries();

	ResumableCounter counter( GetCounterStacks(), false ); 
	gc->CalcWorldClientRect();
	if (counter.MustBreakOrSuspend())
		return GVS_Break; // euqals GVS_Handled

	while (counter.Value() < n)
	{
		SizeT i = counter.Value();
		if (ReverseLayerVisitationOrder())
			i = n - i - 1;
		ScalableObject* gEntry = gc->GetEntry( i );
		dms_assert(gEntry);
		if ( !gEntry->IsVisible() ) 
			goto nextEntry;
		else
		{	
			if (gEntry->GetClippedCurrFullAbsDeviceRect(*this).empty() && gEntry->HasDefinedExtent())
				goto nextEntry;
		}
		if (Visit(gEntry) != GVS_Continue)
			return GVS_Break; // suspend processing, come back here with the same counter

	nextEntry:
		if (! counter.Inc())
			return GVS_Break; // euqals GVS_Handled
	}
	if (DoObject(gc) != GVS_Continue)
		return GVS_Break; // euqals GVS_Handled

	counter.Close();
	return GVS_UnHandled;
}

GraphVisitState GraphVisitor::DoDataItemColumn(DataItemColumn* dic)
{
	assert(!SuspendTrigger::DidSuspend());
	auto tc = dic->GetTableControl().lock(); if (!tc) return GVS_Continue;
	bool isColOriented = tc->IsColOriented();
	SizeT n = tc->NrRows();
	if (!n)
		return GVS_Continue;

	{
		auto sf = GetSubPixelFactors();
		AddClientLogicalOffset localBase(this, dic->GetCurrClientRelPos());
		TPoint elemSize = Convert<TPoint>(dic->ElemSize());
		if (dic->HasElemBorder())
		{
			elemSize.X() += DOUBLE_BORDERSIZE;
			elemSize.Y() += DOUBLE_BORDERSIZE;
		}

		auto scaleFactor = isColOriented ? sf.second : sf.first;
		TType rowLogicalDelta = (elemSize.FlippableY(isColOriented) + dic->RowSepHeight());
		CrdType rowDeviceDelta = rowLogicalDelta * scaleFactor;
		CrdType clientDeviceRow = m_ClientLogicalAbsPos.FlippableY(isColOriented) * scaleFactor;

		auto clipDeviceStart = isColOriented ? m_ClipDeviceRect.Top()    : m_ClipDeviceRect.Left();
		auto clipDeviceEnd   = isColOriented ? m_ClipDeviceRect.Bottom() : m_ClipDeviceRect.Right();
		SizeT firstRecNo = (clipDeviceStart > clientDeviceRow)
			?	(clipDeviceStart - clientDeviceRow) / rowDeviceDelta
			:	0;

		dms_assert(!SuspendTrigger::DidSuspend());
		ResumableCounter counter(GetCounterStacks(), true);
		SizeT recNo = counter.Value() + firstRecNo;
		TType
			currRow = recNo * rowLogicalDelta + dic->RowSepHeight(),
			clipEndRow = clipDeviceEnd / scaleFactor - m_ClientLogicalAbsPos.FlippableY(isColOriented); // in device pixel units
		if (currRow < clipEndRow)
		{
			auto clientLogicalAbsPos = m_ClientLogicalAbsPos;
			auto clientLogicalEnd = clientLogicalAbsPos.FlippableY(isColOriented) + clipEndRow;
			clientLogicalAbsPos.FlippableY(isColOriented) += currRow;
			MakeMin(n, recNo + (clipEndRow - currRow)/ rowLogicalDelta + 1);
			while (recNo < n)
			{
				assert(!SuspendTrigger::DidSuspend());
				auto absElemDeviceRect = ScaleCrdRect(CrdRect(clientLogicalAbsPos, clientLogicalAbsPos + Convert<CrdPoint>(elemSize)), sf);
				auto intElemDeviceRect = CrdRect2GRect(absElemDeviceRect);
				VisitorDeviceRectSelector clipper(this, intElemDeviceRect);

				assert(!SuspendTrigger::DidSuspend());
				if (!clipper.empty())
					DoElement(dic, recNo, intElemDeviceRect);
				if (SuspendTrigger::DidSuspend())
					return GraphVisitState::GVS_Break;

				++recNo;
				++counter;

				clientLogicalAbsPos.FlippableY(isColOriented) += rowLogicalDelta;
			}
		}
		counter.Close();
	}
	return base_type::DoDataItemColumn(dic); 
}

GraphVisitState GraphVisitor::DoViewPort(ViewPort* vp)
{
	assert(vp);
	if (vp->GetWorldCrdUnit())
	{
		auto vpRect = m_Transformation.Apply(vp->GetCurrClientRelLogicalRect());
		VisitorDeviceRectSelector clipper(this, CrdRect2GRect(vpRect));
		if (clipper.empty()) 
			return GVS_UnHandled;

		AddClientLogicalOffset viewportOffset(this, vp->GetCurrClientRelPos());
		assert(!HasCounterStacks() || vp->IsUpdated());
		AddTransformation transformHolder(
			this
		, 	vp->GetCurrWorldToClientTransformation() + Convert<CrdPoint>(m_ClientLogicalAbsPos)
		);

		if (Visit(vp->GetContents()) != GVS_UnHandled) // this is what DoWrapper does 
			return GVS_Handled;
	}
	return DoMovable(vp);
}

GraphVisitState GraphVisitor::DoScrollPort(ScrollPort* sp)
{
	dms_assert(sp); 
	{
		DBG_START("GraphVisitor", "DoScrollPort", MG_DEBUG_VIEWINVALIDATE);

		auto absNettLogicalRect = sp->GetCurrNettAbsLogicalRect(*this);
		auto absNettDeviceRect = m_Transformation.Apply(absNettLogicalRect);
		VisitorDeviceRectSelector clipper( this, CrdRect2GRect(absNettDeviceRect));
		DBG_TRACE(("clipperEmpty: %d", clipper.empty()));

		if (clipper.empty()) 
			return GVS_UnHandled;

		AddClientLogicalOffset localOffset(this, sp->GetCurrClientRelPos());

		if (Visit(sp->GetContents()) != GVS_UnHandled) // this is what DoWrapper does 
			return GVS_Handled;
	}
	return DoMovable(sp);
}

CrdRect GraphVisitor::GetWorldClipRect() const
{
	return m_Transformation.Reverse(g2dms_order<CrdType>( GetAbsClipDeviceRect() ) ); 
}

GPoint GraphVisitor::GetDeviceSize(TPoint relPoint) const
{
	return GPoint(relPoint.X() * m_SubPixelFactors.first, relPoint.Y() * m_SubPixelFactors.second );
}

TPoint GraphVisitor::GetLogicalSize(GPoint devicePoint) const
{
	return shp2dms_order<TType>(devicePoint.x / m_SubPixelFactors.first, devicePoint.y / m_SubPixelFactors.second);
}

CrdPoint GraphVisitor::GetSubPixelFactors() const
{
	return m_SubPixelFactors;
}

CrdType GraphVisitor::GetSubPixelFactor() const
{
	return 0.5 * (m_SubPixelFactors.first + m_SubPixelFactors.second);
}

//----------------------------------------------------------------------
// class  : GraphObjLocator
//----------------------------------------------------------------------

GraphObjLocator::GraphObjLocator(GPoint pnt, CrdPoint scaleFactor)
	:	GraphVisitor(SelectPoint2Rect(pnt), scaleFactor) 
{}

MovableObject* GraphObjLocator::Locate(DataView* dv, GPoint pnt)
{
	assert(dv);
	GraphObjLocator locator(pnt, dv->GetScaleFactors());
	locator.Visit(dv->GetContents().get());
	return locator.m_TheOne.lock().get();
}

GraphVisitState GraphObjLocator::DoMovable(MovableObject* obj)
{
	dms_assert(!m_TheOne.lock());
	m_TheOne = obj->shared_from_this();
	return GVS_Handled;
}

//----------------------------------------------------------------------
// GraphDrawer members
//----------------------------------------------------------------------

#include "utl/IncrementalLock.h"

GraphDrawer::GraphDrawer(HDC hDC, CounterStacks& doneGraphics, DataView* dv, GdMode gdMode, CrdPoint scaleFactors)
	:	GraphVisitor( doneGraphics.CurrRegion().BoundingBox(), scaleFactors)
//	,	SuspendTrigger::FencedBlocker("@GraphDrawer")
	,	m_hDC(hDC)
	,	m_AbsClipRegion(doneGraphics.CurrRegion().Clone())
	,	m_DoneGraphics(&doneGraphics)
	,	m_ViewPtr(dv)
	,	m_GdMode(gdMode)
{
	dms_assert( doneGraphics.NoActiveCounters() );
	dms_assert( IsSuspendible() );

	dms_assert(! m_AbsClipRegion.Empty());
	dms_assert(DoUpdateData() || !DoDrawData());
	dms_assert( (GetDC() != 0) ==  (DoDrawBackground() || DoDrawData()) );
}

GraphDrawer::GraphDrawer(HDC hDC, const Region&  rgn, DataView* dv, GdMode gdMode, CrdPoint scaleFactors)
	:	GraphVisitor(rgn.BoundingBox(), scaleFactors)
//	,	SuspendTrigger::FencedBlocker("@GraphDrawer")
	,	m_hDC(hDC)
	,	m_AbsClipRegion( rgn.Clone() )
	,	m_DoneGraphics(0)
	,	m_ViewPtr(dv)
	,	m_GdMode(gdMode)
{
	dms_assert(! IsSuspendible() );

	dms_assert(! m_AbsClipRegion.Empty());
	dms_assert(DoUpdateData() || !DoDrawData());
	dms_assert( (GetDC() != 0) ==  (DoDrawBackground() || DoDrawData()) );
}

GraphVisitState GraphDrawer::Visit(GraphicObject* go)
{
	DBG_START("GraphDrawer", "Visit", MG_DEBUG_VIEWINVALIDATE);
	DBG_TRACE(("%s %x: %x", go->GetDynamicClass()->GetName().c_str(), go, &*(go->GetOwner().lock())));
	DBG_TRACE(("ClipRect     : %s", AsString(m_ClipDeviceRect).c_str()));

	bool suspendible = IsSuspendible();

	dms_assert(go);
	dms_assert(go->AllVisible()); // PRECONDITION

	if (MustBreak())
		return GVS_Break;

	if (SuspendTrigger::DidSuspend() )
	{
		dms_assert(suspendible);
		return GVS_Break;
	}

	if (suspendible)
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		go->SuspendibleUpdate(PS_Committed);
		if (SuspendTrigger::DidSuspend())
			return GVS_Break; // suspend processing, thus: done =  true
	}
	dms_assert(!SuspendTrigger::DidSuspend());

	auto absFullRect = go->GetCurrFullAbsDeviceRect(*this);
	assert(!SuspendTrigger::DidSuspend());
	assert(suspendible || go->IsUpdated() || (m_GdMode & GD_OnPaint));

	bool hasDefinedExtent = go->HasDefinedExtent();
	if (absFullRect.empty() && hasDefinedExtent)
		return GVS_Continue;

	VisitorDeviceRectSelector clipper(this, CrdRect2GRect(absFullRect));
	if (clipper.empty() && hasDefinedExtent)
		return GVS_Continue;

	if (DoStoreRect())
	{
		auto owner = go->GetOwner().lock();
		dms_assert(!owner || owner->IsDrawn());

		// CLIP ON ANCHESTORS BOUNDARIES, BUT NOT ON OnPaint Areas. 
		auto ownerRect = owner ? owner->GetDrawnNettAbsDeviceRect() : GRect2CrdRect( m_ViewPtr->ViewDeviceRect() );
		if (hasDefinedExtent)
			ownerRect &= absFullRect;

		if (go->IsDrawn())
			go->ClipDrawnRect(ownerRect);
		go->m_DrawnFullAbsRect = ownerRect;
	}

	if ( (DoDrawData() && go->MustClip()) || (DoDrawBackground() && go->MustFill()))
	{
		assert(hasDefinedExtent); // implied by go->MustClip() ?
		if (clipper.empty())
			return GVS_Continue;

		DcClipRegionSelector clipRegionSelector(GetDC(), m_AbsClipRegion, m_ClipDeviceRect);

		if (clipRegionSelector.empty())
			return GVS_Continue; // processing completed
		return Visit2(go);
	}
	return Visit2(go);
}

GraphVisitState GraphDrawer::Visit2(GraphicObject* go)
{
	dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon
	if (DoStoreRect() && !(go->m_DrawnFullAbsRect.empty() && go->HasDefinedExtent()))
		go->SetIsDrawn();
	if (DoDrawBackground())           // should not suspend
		go->DrawBackground(*this);
	dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon
	return go->InviteGraphVistor(*this);
}

GraphVisitState GraphDrawer::DoObject(GraphicObject* go)
{
	ObjectContextHandle context(go, "Draw");

	dms_assert(!SuspendTrigger::DidSuspend());

	if (go->Draw(*this) != GVS_Continue)
	{
		dms_assert(HasCounterStacks()); // suspended can only happen if suspendible which is equivalent with HasCounterStacks()
		dms_assert(SuspendTrigger::DidSuspend() || m_DoneGraphics->DidBreak() ); // if failed, Draw must just return false, don't try again.
		return GVS_Break; // suspend processing, thus: retry =  true
	}

	return GVS_Continue; // processing completed; thus retry = false
}

GraphVisitState GraphDrawer::DoMovable(MovableObject* obj)
{
	if ( GraphVisitor::DoMovable(obj) != GVS_UnHandled)
		return GVS_Handled;

	if (obj->HasBorder() && DoDrawBackground())
	{
		auto extents = obj->GetCurrFullAbsLogicalRect(*this);
		auto devExtents = m_Transformation.Apply( extents );
		auto gExtents = CrdRect2GRect(devExtents);
		DrawBorder(GetDC(), gExtents, obj->RevBorder());
	}
	return GVS_UnHandled;
}

GraphVisitState GraphDrawer::DoLayerControlBase(LayerControlBase* lc)
{
	DcTextColorSelector textColorHolder(GetDC(),
		(lc->GetLayerSetElem()->AllVisible())
		?	lc->GetDefaultTextColor()
		:	COLORREF2DmsColor( GetSysColor(COLOR_GRAYTEXT) )
	);

	GdiObjectSelector<HFONT> fontSelector(GetDC(),
		m_ViewPtr->GetDefaultFont(lc->GetFontSizeCategory(), GetSubPixelFactor())
	);

	return base_type::DoLayerControlBase(lc);
}

GraphVisitState GraphDrawer::DoLayer(GraphicLayer* gl)
{
	if (!gl->VisibleLevel(*this))
		return GVS_Continue;

	if (gl->PrepareThemeSetData(gl) == AVS_SuspendedOrFailed && SuspendTrigger::DidSuspend())
		return GVS_Handled; // suspended: break  or failed: continue with the next layer
	return base_type::DoLayer(gl);
}

bool PrepareData(ThemeReadLocks& trl, DataItemColumn* dic, const AbstrDataItem* adi, bool* tryLater)
{
	if (!adi)
		return true;
	if (dic->PrepareDataOrUpdateViewLater(adi))
	{
		trl.push_back(DataReadLock(adi));
		return true;
	}
	if (!adi->WasFailed(FR_Data))
		*tryLater = true;
	if (SuspendTrigger::DidSuspend()) // if failed, Draw must just return false, don't try again.
		return false; // suspend processing, thus: retry =  true

	if (adi->WasFailed())
		dic->Fail(adi);
	return true;
}

GraphVisitState GraphDrawer::DoDataItemColumn(DataItemColumn* dic)
{
	dms_assert(!SuspendTrigger::DidSuspend()); // should have been acted upon

	if (!DoUpdateData())
		return DoMovable(dic);

	if (dic->PrepareThemeSetData(dic) == AVS_SuspendedOrFailed && SuspendTrigger::DidSuspend())
		return GVS_Handled; // suspended: break  or failed: continue with the next layer

	const AbstrDataItem* indexAttr= dic->GetTableControl().lock().get()->GetIndexAttr();

	ThemeReadLocks trl; bool tryLater = false;
	if (!PrepareData(trl, dic, indexAttr, &tryLater))
		return GVS_Break;
	if (!PrepareData(trl, dic, dic->GetActiveTextAttr(), &tryLater))
		return GVS_Break;
	
	if (! trl.push_back(dic, DrlType::Suspendible))
		if (trl.ProcessFailOrSuspend(dic))
			return GVS_Handled; // suspend processing, thus: retry =  true
	dms_assert(!SuspendTrigger::DidSuspend());
	if (!DoDrawData())
		return GVS_Continue;
	if (tryLater)
		return GVS_Continue;
	return base_type::DoDataItemColumn(dic);
}

void GraphDrawer::DoElement(DataItemColumn* dic, SizeT i, const GRect& absElemDeviceRect)
{
	dms_assert(DoDrawData());
	dic->DrawElement(*this, i, absElemDeviceRect, m_TileLocks);
}

GraphVisitState GraphDrawer::DoViewPort(ViewPort* vp)
{
	tmp_swapper<ViewPort*> holdVP(m_ViewPortPtr, vp);
	if (!DoUpdateData())
		return DoMovable(vp); // skip LayerSet when drawing background

	if (!DoDrawData())
		return base_type::DoViewPort(vp);

	DcBrushOrgSelector brushOrgSelector(GetDC(), vp->GetBrushOrg());
	return base_type::DoViewPort(vp);
}

WeakPtr<CounterStacks> GraphDrawer::GetCounterStacks() const
{
	return m_DoneGraphics;
}

//----------------------------------------------------------------------
// GraphInvalidator members
//----------------------------------------------------------------------

#include "DataView.h"

GraphInvalidator::GraphInvalidator()
{
}

GraphVisitState GraphInvalidator::Visit(GraphicObject* go)
{
	go->GetLastChangeTS(); // trigger DoInvalidate if supplier changed, which calls InvalidateView and InvalidateDraw

	return base_type::Visit(go);
}

//----------------------------------------------------------------------
// class  : GraphUpdater
//----------------------------------------------------------------------

GraphUpdater::GraphUpdater(const GRect& clipDeviceRect, CrdPoint subPixelFactors)
	:	GraphVisitor(clipDeviceRect, subPixelFactors) 
{}

GraphVisitState GraphUpdater::DoObject(GraphicObject* go)
{
	dms_assert(go);
	dms_assert(!SuspendTrigger::DidSuspend());

	ActorVisitState ready = go->SuspendibleUpdate(PS_Committed); // returns true if didn't suspend (success or failure, keep going anyway)
	dms_assert((ready == AVS_SuspendedOrFailed) == (SuspendTrigger::DidSuspend() || go->WasFailed(FR_Committed)) );
//	if (ready)
//		go->SuspendibleUpdate(PS_Committed);
	return GVS_BreakOnSuspended();
}

GraphVisitState GraphUpdater::DoDataItemColumn(DataItemColumn* dic)
{
	return DoObject(dic);
}


//----------------------------------------------------------------------
// UpdateViewProcessor members
//----------------------------------------------------------------------

#if defined(MG_DEBUG)
	struct BlockInvalidateView
	{
		BlockInvalidateView(GraphicObject* go) : m_GO(go)
		{
			go->SetBlockInvalidateView(true);
		}
		~BlockInvalidateView()
		{
			m_GO->SetBlockInvalidateView(false);
		}
		
		GraphicObject* m_GO;
	};
#endif

GraphVisitState UpdateViewProcessor::Visit(GraphicObject* go)
{
	if (go->AllUpdated() || !go->IsVisible())
		return GVS_UnHandled;
	dms_assert(go->AllVisible());

	do { 
		go->UpdateView();

revisit:
		if (go->InviteGraphVistor(*this) != GVS_UnHandled)
			return GVS_Break;

		if (!go->IsUpdated())
			return GVS_Continue;

		SizeT n = go->NrEntries();
		while (n)
		{
			GraphicObject* child = go->GetEntry(--n);
			if (child->IsVisible() && (!child->IsFailed(FR_Data)) && !child->AllUpdated())
				goto revisit;
		}

	} while ( !go->IsFailed(FR_Data) && !go->IsUpdated() );

	go->SetAllUpdated();

	return GVS_Continue;
}


//----------------------------------------------------------------------
// class MouseEventDispatcher
//----------------------------------------------------------------------

#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"

#include "AbstrController.h"
#include "DataView.h"
#include "MouseEventDispatcher.h"

MouseEventDispatcher::MouseEventDispatcher(DataView* owner, EventInfo& eventInfo)
:	GraphVisitor(SelectPoint2Rect(eventInfo.m_Point), owner->GetScaleFactors() )
	,	m_Owner(owner->shared_from_this())
	,	r_EventInfo(eventInfo)
{
	assert((eventInfo.m_EventID & EventID::OBJECTFOUND) == 0);
}

bool MouseEventDispatcher::IsActivating() const
{
	return r_EventInfo.m_EventID & EventID::ACTIVATE;
}

GraphVisitState MouseEventDispatcher::DoObject(GraphicObject* go)
{
	if ((r_EventInfo.m_EventID & EventID::OBJECTFOUND) == 0)
	{
		m_WorldCrd    = m_Transformation.Reverse(g2dms_order<CrdType>(r_EventInfo.m_Point));
		m_FoundObject = go->shared_from_this();
		r_EventInfo.m_EventID |= EventID::OBJECTFOUND;


		if (r_EventInfo.m_EventID & EventID::LBUTTONDOWN && !IsActivating() && ! go->IgnoreActivation())
		{
			auto owner = m_Owner.lock();
			if (owner->m_ActivationInfo.ActiveChild().get() != go)
				r_EventInfo.m_EventID |= EventID::ACTIVATE;
		}

	}
	dms_assert(r_EventInfo.m_EventID & EventID::OBJECTFOUND);

	if (r_EventInfo.m_EventID & EventID::RBUTTONUP)
		go->FillMenu(*this);

	return go->MouseEvent(*this) ? GVS_Handled : GVS_UnHandled;
}

GraphVisitState MouseEventDispatcher::DoMovable(MovableObject* obj)
{
	GraphVisitState result = base_type::DoMovable(obj);

	if (IsActivating() && !obj->IgnoreActivation() && !m_ActivatedObject) 
		m_ActivatedObject = obj->shared_from_this();

	return result;
}

GraphVisitState MouseEventDispatcher::DoViewPort(ViewPort* vp)
{
	vp->SetNeedle(&m_Transformation, r_EventInfo.m_Point);

	GraphVisitState result = base_type::DoViewPort(vp);

	assert(r_EventInfo.m_EventID & (EventID::OBJECTFOUND| EventID::SETCURSOR));

	if (r_EventInfo.m_EventID & EventID::SETCURSOR)
		return result; 
	if (!(r_EventInfo.m_EventID & EventID::OBJECTFOUND))
		return result;

	assert(IsMainThread());

	if (!(r_EventInfo.m_EventID & EventID::TEXTSENT))
	{
		auto viewPoint = ViewPoint(m_WorldCrd, vp->GetCurrLogicalZoomLevel(), {});
		char buffer[201];

		if (auto dv = m_Owner.lock())
		{
			if (!viewPoint.WriteAsString(buffer, 200, FormattingFlags::ThousandSeparator))
				buffer[200] = char(0); // truncate
				dv->SendStatusText(SeverityTypeID::ST_MinorTrace, buffer);
		}
		if (r_EventInfo.m_EventID & EventID::COPYCOORD)
		{
			if (viewPoint.WriteAsString(buffer, 200, FormattingFlags::None))
			{
				ClipBoard clp;
				clp.AddTextLine(buffer);
			}
		}
	}

	return result;
}

