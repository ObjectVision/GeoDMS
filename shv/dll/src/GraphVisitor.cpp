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

GraphVisitState AbstrVisitor::DoVarRows   (GraphicVarRows*   goc) { return DoMovableContainer(goc); }
GraphVisitState AbstrVisitor::DoVarCols   (GraphicVarCols*   goc) { return DoMovableContainer(goc); }
GraphVisitState AbstrVisitor::DoScalable  (ScalableObject*   obj) { return DoObject(obj); }

GraphVisitState AbstrVisitor::DoDataItemColumn(DataItemColumn* dic)  { return DoMovable(dic); }
void  AbstrVisitor::DoElement       (DataItemColumn* dic, SizeT i, const GRect& absElemRect) { }

GraphVisitState AbstrVisitor::DoWrapper(Wrapper* obj)
{
	dms_assert(obj->GetContents());

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

GraphVisitState AbstrVisitor::DoLayerControlBase (LayerControlBase*  lcb) { return DoVarRows(lcb); }
GraphVisitState AbstrVisitor::DoLayerControl     (LayerControl* lc)       { return DoLayerControlBase(lc); }
GraphVisitState AbstrVisitor::DoLayerControlSet  (LayerControlSet*   lcs) { return DoVarRows(lcs); }
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

GraphVisitor::GraphVisitor(const GRect& clipRect, CrdType subPixelFactor)
	:	m_ClipRect(clipRect)
	,	m_Transformation()
	,	m_ClientOffset(0, 0)
	,	m_SubPixelFactor(subPixelFactor)
{}

GraphVisitState GraphVisitor::Visit(GraphicObject* obj)
{
	DBG_START(typeid(*this).name(), "Visit", MG_DEBUG_VIEWINVALIDATE);
	DBG_TRACE(("%s %x: %x", obj->GetDynamicClass()->GetName().c_str(), obj, &*(obj->GetOwner().lock())));
	DBG_TRACE(("Cliprect     : %s", AsString(m_ClipRect).c_str()));

	dms_assert(obj);
	dms_assert(obj->AllVisible());

	dms_assert(!SuspendTrigger::DidSuspend());
	obj->UpdateView();
	dms_assert(!SuspendTrigger::DidSuspend());

	dms_assert(obj->AllVisible());

	if (MustBreak())
		return GVS_Break; // equals GVS_Handled

	if (obj->InviteGraphVistor(*this) == GVS_Handled)
		return GVS_Handled; // return;
	dms_assert(!SuspendTrigger::DidSuspend());

	obj->UpdateView(); // retry
	dms_assert(!SuspendTrigger::DidSuspend());

	dms_assert(obj->IsUpdated() || obj->WasFailed());
	return GVS_UnHandled;
}

GraphVisitState GraphVisitor::DoMovableContainer(MovableContainer* gc)
{
	dms_assert(gc);
	dms_assert(!MustBreak());

	SizeT n = gc->NrEntries();

	ResumableCounter counter( GetCounterStacks(), false); if (counter.MustBreakOrSuspend()) return GVS_Break;
	{
		AddClientOffset localBase(this, gc->GetCurrClientRelPos());

		while (counter.Value() < n)
		{
			MovableObject* gEntry = gc->GetEntry( counter.Value() );
			dms_assert(gEntry);
			if ( !gEntry->IsVisible() ) 
				goto nextEntry;
			else
			{	
//				dms_assert(!HasCounterStacks() || gEntry->IsUpdated());
				if (gEntry->GetClippedCurrFullAbsRect(*this).empty())
					goto nextEntry;
			}
			if (Visit(gEntry))
				return GVS_Break; // suspend processing, come back here with the same counter

		nextEntry:
			if (! counter.Inc())
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
			dms_assert(!HasCounterStacks() || gEntry->IsUpdated() || gEntry->WasFailed(FR_Data));
			if (gEntry->GetClippedCurrFullAbsRect(*this).empty() && gEntry->HasDefinedExtent())
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
	dms_assert(!SuspendTrigger::DidSuspend());
	dms_assert(m_Transformation.Factor() == CrdPoint(1, 1));
	auto tc = dic->GetTableControl().lock(); if (!tc) return GVS_Continue;
	SizeT n = tc->NrRows();
	if (!n)
		return GVS_Continue;

	{
		AddClientOffset localBase(this, dic->GetCurrClientRelPos());
		GPoint elemSize = dic->ElemSize();
		if (dic->HasElemBorder())
		{
			elemSize.x += 2*BORDERSIZE;
			elemSize.y += 2*BORDERSIZE;
		}

		TType rowDelta = elemSize.y + dic->RowSepHeight();

		SizeT firstRecNo = (m_ClipRect.Top() > m_ClientOffset.y())
			?	(m_ClipRect.Top() - m_ClientOffset.y()) / rowDelta
			:	0;

		dms_assert(!SuspendTrigger::DidSuspend());
		ResumableCounter counter(GetCounterStacks(), true);
		SizeT recNo = counter.Value() + firstRecNo;
		TType
			currRow    = recNo * rowDelta + dic->RowSepHeight(),
			clipEndRow = m_ClipRect.Bottom() - m_ClientOffset.y();

		while ( recNo < n && currRow < clipEndRow)
		{
			dms_assert(!SuspendTrigger::DidSuspend());
			AddClientOffset localRowBase(this, TPoint(0, currRow));

			GRect absElemRect = GRect(TPoint2GPoint(m_ClientOffset), TPoint2GPoint(m_ClientOffset + TPoint(elemSize)) );
			VisitorRectSelector clipper(this, absElemRect );

			dms_assert(!SuspendTrigger::DidSuspend());
			if (!clipper.empty())
				DoElement(dic, recNo, absElemRect);
			if (SuspendTrigger::DidSuspend())
				return GraphVisitState::GVS_Break;

			++recNo;
			++counter; 

			currRow += rowDelta;
		}
		counter.Close();
	}
	return base_type::DoDataItemColumn(dic); 
}

GraphVisitState GraphVisitor::DoViewPort(ViewPort* vp)
{
	dms_assert(vp);
	if (vp->GetWorldCrdUnit())
	{
		VisitorRectSelector clipper(this, TRect2GRect( vp->GetCurrClientRelRect() + m_ClientOffset ));
		if (clipper.empty()) 
			return GVS_UnHandled;

		AddClientOffset viewportOffset(this, vp->GetCurrClientRelPos());
		dms_assert(!HasCounterStacks() || vp->IsUpdated());
		AddTransformation transformHolder(
			this
		, 	vp->GetCurrWorldToClientTransformation() + Convert<CrdPoint>(m_ClientOffset)
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

		VisitorRectSelector clipper( this, TRect2GRect( sp->GetCurrNettAbsRect(*this) ) );
		DBG_TRACE(("clipperEmpty: %d", clipper.empty()));

		if (clipper.empty()) 
			return GVS_UnHandled;

		AddClientOffset localOffset(this, sp->GetCurrClientRelPos());

		if (Visit(sp->GetContents()) != GVS_UnHandled) // this is what DoWrapper does 
			return GVS_Handled;
	}
	return DoMovable(sp);
}

CrdRect GraphVisitor::GetWorldClipRect() const
{
	return m_Transformation.Reverse(Convert<CrdRect>( GetAbsClipRect() ) ); 
}

Float64 GraphVisitor::GetSubPixelFactor() const
{
	return m_SubPixelFactor;
}

//----------------------------------------------------------------------
// class  : GraphObjLocator
//----------------------------------------------------------------------

GraphObjLocator::GraphObjLocator(GPoint pnt, CrdType subPixelFactor)
	:	GraphVisitor(SelectPoint2Rect(pnt), subPixelFactor) 
{}

MovableObject* GraphObjLocator::Locate(DataView* view, GPoint pnt, CrdType subPixelFactor)
{
	GraphObjLocator locator(pnt, subPixelFactor);
	locator.Visit(view->GetContents().get());
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

GraphDrawer::GraphDrawer(
		HDC            hDC
	,	CounterStacks& doneGraphics
	,	DataView*      viewPtr
	,	GdMode         gdMode
	,	Float32        subPixelFactor
	)	:	GraphVisitor( doneGraphics.CurrRegion().BoundingBox(), subPixelFactor)
		,	m_hDC(hDC)
		,	m_AbsClipRegion(doneGraphics.CurrRegion().Clone())
		,	m_DoneGraphics(&doneGraphics)
		,	m_ViewPtr(viewPtr)
		,	m_GdMode(gdMode)
{
	dms_assert( doneGraphics.NoActiveCounters() );
	dms_assert( IsSuspendible() );

	dms_assert(! m_AbsClipRegion.Empty());
	dms_assert(DoUpdateData() || !DoDrawData());
	dms_assert( (GetDC() != 0) ==  (DoDrawBackground() || DoDrawData()) );
}

GraphDrawer::GraphDrawer(
		HDC            hDC
	,	const Region&  rgn
	,	DataView*      viewPtr
	,	GdMode         gdMode
	,	Float32        subPixelFactor
	)	:	GraphVisitor(rgn.BoundingBox(), subPixelFactor)
		,	m_hDC(hDC)
		,	m_AbsClipRegion( rgn.Clone() )
		,	m_DoneGraphics(0)
		,	m_ViewPtr(viewPtr)
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
	DBG_TRACE(("ClipRect     : %s", AsString(m_ClipRect).c_str()));

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

	GRect absFullRect = TRect2GRect(go->GetCurrFullAbsRect(*this));
	dms_assert(!SuspendTrigger::DidSuspend());
	dms_assert(suspendible || go->IsUpdated() || (m_GdMode & GD_OnPaint));

	bool hasDefinedExtent = go->HasDefinedExtent();
	if (absFullRect.empty() && hasDefinedExtent)
		return GVS_Continue;

	VisitorRectSelector clipper(this, absFullRect);
	if (clipper.empty() && hasDefinedExtent)
		return GVS_Continue;

	if (DoStoreRect())
	{
		auto owner = go->GetOwner().lock();
		dms_assert(!owner || owner->IsDrawn());

		// CLIP ON ANCHESTORS BOUNDARIES, BUT NOT ON OnPaint Areas. 
		GRect ownerRect = owner ? owner->GetDrawnNettAbsRect() : m_ViewPtr->ViewRect();
		if (hasDefinedExtent)
			ownerRect &= absFullRect;

		if (go->IsDrawn())
			go->ClipDrawnRect(ownerRect);
		go->m_DrawnFullAbsRect = ownerRect;
	}

	if ( (DoDrawData() && go->MustClip()) || (DoDrawBackground() && go->MustFill()))
	{
		dms_assert(hasDefinedExtent); // implied by go->MustClip() ?
		if (clipper.empty())
			return GVS_Continue;

		DcClipRegionSelector clipRegionSelector(
			GetDC(), 
			m_AbsClipRegion, 
			m_ClipRect // absFullRect
		);
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
		auto extents = TRect2GRect(obj->GetCurrFullAbsRect(*this));

		DrawBorder(
			GetDC(),
			extents,
			obj->RevBorder()
		);
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

void GraphDrawer::DoElement(DataItemColumn* dic, SizeT i, const GRect& absElemRect)
{
	dms_assert(DoDrawData());
	dic->DrawElement(*this, i, absElemRect, m_TileLocks);
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

GraphUpdater::GraphUpdater(const GRect& clipRect, CrdType subPixelFactor)
	:	GraphVisitor(clipRect, subPixelFactor) 
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
:	GraphVisitor(SelectPoint2Rect(eventInfo.m_Point), GetDesktopDIP2pixFactor())
	,	m_Owner(owner->shared_from_this())
	,	r_EventInfo(eventInfo)
{
	dms_assert((eventInfo.m_EventID & EID_OBJECTFOUND) == 0);
}


bool MouseEventDispatcher::IsActivating() const
{
	return r_EventInfo.m_EventID & EID_ACTIVATE;
}

GraphVisitState MouseEventDispatcher::DoObject(GraphicObject* go)
{
	if ((r_EventInfo.m_EventID & EID_OBJECTFOUND) == 0)
	{
		m_WorldCrd    = m_Transformation.Reverse(Convert<CrdPoint>(r_EventInfo.m_Point));
		m_FoundObject = go->shared_from_this();
		r_EventInfo.m_EventID |= EID_OBJECTFOUND;


		if (r_EventInfo.m_EventID & EID_LBUTTONDOWN && !IsActivating() && ! go->IgnoreActivation())
		{
			auto owner = m_Owner.lock();
			if (owner->m_ActivationInfo.ActiveChild().get() != go)
				r_EventInfo.m_EventID |= EID_ACTIVATE;
		}

	}
	dms_assert(r_EventInfo.m_EventID & EID_OBJECTFOUND);

	if (r_EventInfo.m_EventID & EID_RBUTTONUP)
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

GraphVisitState MouseEventDispatcher::DoViewPort(ViewPort*  vp)
{
	vp->SetNeedle(&m_Transformation, r_EventInfo.m_Point);

	GraphVisitState result = base_type::DoViewPort(vp);

	dms_assert(r_EventInfo.m_EventID & EID_OBJECTFOUND);

	dms_assert(IsMainThread());
	static ExternalVectorOutStreamBuff::VectorType buffer;

	buffer.clear();
	buffer.reserve(30);
	ExternalVectorOutStreamBuff strBuff(buffer);
	auto dv = m_Owner.lock();
	if (dv)
	{
		FormattedOutStream out(&strBuff, FormattingFlags::ThousandSeparator);
		out << "X=" << m_WorldCrd.Col() << "; Y=" << m_WorldCrd.Row() << char(0);
		dv->SendStatusText(SeverityTypeID::ST_MinorTrace, &*buffer.begin());
	}
	buffer.clear();
	if (r_EventInfo.m_EventID & EID_COPYCOORD )
	{
		FormattedOutStream out(&strBuff, FormattingFlags::None);
		out << "[ X=" << m_WorldCrd.Col() << "; Y=" << m_WorldCrd.Row() << "]" << char(0);
		ClipBoard clp;
		clp.AddTextLine(&*buffer.begin());
	}

	return result;
}

