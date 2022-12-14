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
#pragma once

#if !defined(__SHV_GRAPHVISITOR_H)
#define __SHV_GRAPHVISITOR_H

#include "ptr/WeakPtr.h"
#include "ptr/SharedPtr.h"
#include "ptr/PersistentSharedObj.h"

#include "Region.h"
#include "ShvUtils.h"

//----------------------------------------------------------------------
// class  : AbstrVisitor
//----------------------------------------------------------------------

class AbstrVisitor
{
public:
	virtual GraphVisitState Visit       (GraphicObject*    go );

	virtual GraphVisitState DoObject    (GraphicObject*    go);
  	virtual GraphVisitState DoMovable(MovableObject*   obj);
  	virtual GraphVisitState DoMovableContainer(MovableContainer* obj);
  	virtual GraphVisitState DoVarRows   (GraphicVarRows*   goc);
  	virtual GraphVisitState DoVarCols   (GraphicVarCols*   goc);
  	virtual GraphVisitState DoDataItemColumn(DataItemColumn* dic);

	virtual GraphVisitState DoWrapper   (Wrapper*    wr);
	virtual GraphVisitState DoViewPort  (ViewPort*    vp);
	virtual GraphVisitState DoScrollPort(ScrollPort*  sp);

  	virtual GraphVisitState DoScalable    (ScalableObject*  gl);
	virtual GraphVisitState DoLayerSet    (LayerSet*       ls);
  	virtual GraphVisitState DoLayer       (GraphicLayer*    gl);
  	virtual GraphVisitState DoFeatureLayer(FeatureLayer*  gfl);
  	virtual GraphVisitState DoGridLayer   (GridLayer*  dgl);

  	virtual GraphVisitState DoLayerControlBase (LayerControlBase*  lcb);
  	virtual GraphVisitState DoLayerControl     (LayerControl*      lc);
  	virtual GraphVisitState DoLayerControlSet  (LayerControlSet*   lcs);
  	virtual GraphVisitState DoLayerControlGroup(LayerControlGroup* lcg);

	virtual GraphVisitState DoMapControl(MapControl* mc);

	virtual void DoElement(DataItemColumn* dic, SizeT i, const GRect& absElemRect);
	virtual WeakPtr<CounterStacks> GetCounterStacks() const;

	bool HasCounterStacks() const { return ! GetCounterStacks().is_null(); }
	bool MustBreak() const;

protected:
	AbstrVisitor() {}
	virtual ~AbstrVisitor() {}
};

//----------------------------------------------------------------------
// class  : GraphVisitor
//----------------------------------------------------------------------

// 	GraphVisitor(clipRect) is inherited by
//  	GraphDrawer           (HDC, clipRect, doneGraphics)
//		GraphInvalidator      (canvas, lastPaintTS, clipRect)
//		UpdateViewProcessor
//		MouseEventDispatcher
//		GraphObjectLocator


class GraphVisitor : public AbstrVisitor
{
	typedef AbstrVisitor base_type;
public:
	GraphVisitState Visit              (GraphicObject* go) override;

  	GraphVisitState DoLayerSet         (LayerSet*   goc) override;
	GraphVisitState DoViewPort         (ViewPort*    vp) override;
	GraphVisitState DoScrollPort       (ScrollPort*  sp) override;

  	GraphVisitState DoMovableContainer(MovableContainer* goc) override;
	GraphVisitState DoDataItemColumn  (DataItemColumn*   dic) override;

	const CrdTransformation& GetTransformation() const { return m_Transformation; }
	const TPoint&            GetClientOffset  () const { return m_ClientOffset;   }
	const GRect&             GetAbsClipRect   () const { return m_ClipRect;       }
	CrdRect                  GetWorldClipRect () const;

	Float64 GetSubPixelFactor() const;

protected:
	GraphVisitor(const GRect& clipRect, CrdType subPixelFactor);

	virtual bool ReverseLayerVisitationOrder() const { return false;  }
  	CrdTransformation m_Transformation;
	TPoint            m_ClientOffset;
	GRect             m_ClipRect; // in client coordinates 
	CrdType           m_SubPixelFactor;

	friend struct AddTransformation;
	friend struct AddClientOffset;
	friend struct VisitorRectSelector;
};

//----------------------------------------------------------------------
// class  : GraphObjLocator
//----------------------------------------------------------------------

class GraphObjLocator : public GraphVisitor
{
	typedef GraphVisitor base_type;
public:
	GraphObjLocator(GPoint pnt, CrdType subPixelFactor);

	static MovableObject* Locate(DataView* view, GPoint pnt, CrdType subPixelFactor);

protected:
  	GraphVisitState DoMovable(MovableObject* obj) override;

private:
	std::weak_ptr<MovableObject> m_TheOne;
};

//----------------------------------------------------------------------
// class  : GraphDrawer
//----------------------------------------------------------------------

enum GdMode { 
	GD_DrawBackground = 0x01
,	GD_UpdateData     = 0x02
,	GD_DrawData       = 0x04
,	GD_StoreRect      = 0x08
,	GD_Suspendible    = 0x10
,	GD_OnPaint        = 0x20
};

class GraphDrawer : public GraphVisitor
{
	typedef GraphVisitor base_type;
public:
	GraphDrawer(HDC hDC, CounterStacks& doneGraphics, DataView* viewPtr, GdMode gdMode, Float32 subPixelFactor);
	GraphDrawer(HDC hDC, const Region&  doneGraphics, DataView* viewPtr, GdMode gdMode, Float32 subPixelFactor);

	GraphVisitState Visit(GraphicObject* go) override;

  	GraphVisitState DoObject          (GraphicObject* go) override;
	GraphVisitState DoMovable         (MovableObject* obj) override;
	GraphVisitState DoViewPort        (ViewPort*      vp) override;
  	GraphVisitState DoLayerControlBase(LayerControlBase*  lc) override;
	GraphVisitState DoLayer           (GraphicLayer*    gl) override;
	GraphVisitState DoDataItemColumn  (DataItemColumn* dic) override;

	void DoElement         (DataItemColumn* dic, SizeT i, const GRect& absElemRect) override;
	WeakPtr<CounterStacks> GetCounterStacks() const override;

	HDC GetDC() const { return m_hDC; }
	const Region& GetAbsClipRegion() const { return m_AbsClipRegion; }
	ViewPort*     GetViewPortPtr  () const { return m_ViewPortPtr; }

	bool DoDrawBackground() const { return m_GdMode & GD_DrawBackground; }
	bool IsInOnPaint     () const { return m_GdMode & GD_OnPaint; }
	bool DoUpdateData    () const { return m_GdMode & GD_UpdateData; }
	bool DoDrawData      () const { return m_GdMode & GD_DrawData; }
	bool DoOnlyUpdate    () const { return DoUpdateData() && !DoDrawData(); }
	bool DoStoreRect     () const { return m_GdMode & GD_StoreRect;   }
	bool IsSuspendible   () const { return m_GdMode & GD_Suspendible; }

private:
	GraphVisitState Visit2(GraphicObject* go);


private:
	mutable Region          m_AbsClipRegion;
  	mutable HDC             m_hDC;
	mutable CounterStacks*  m_DoneGraphics;
	DataView*               m_ViewPtr;
	ViewPort*               m_ViewPortPtr;
	mutable GuiReadLockPair m_TileLocks;
public:
	GdMode                 m_GdMode;
};

//----------------------------------------------------------------------
// class  : GraphInvalidator
//----------------------------------------------------------------------

class GraphInvalidator : public AbstrVisitor
{
//	typedef GraphVisitor base_type;
	typedef AbstrVisitor base_type;
public:
  	GraphInvalidator();

	GraphVisitState Visit(GraphicObject* go) override;
};

//----------------------------------------------------------------------
// class  : UpdateViewProcessor
//----------------------------------------------------------------------

class UpdateViewProcessor: public AbstrVisitor
{
	typedef AbstrVisitor base_type;
public:
	GraphVisitState Visit(GraphicObject* go) override;
};

//----------------------------------------------------------------------
// class  : GraphUpdater
//----------------------------------------------------------------------

class GraphUpdater: public GraphVisitor
{
public:
	GraphUpdater(const GRect& clipRect, CrdType subPixelFactor);

	GraphVisitState DoObject(GraphicObject* go) override;
};

#endif // __SHV_GRAPHVISITOR_H

