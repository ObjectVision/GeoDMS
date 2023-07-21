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

#ifndef __SHV_VIEWPORT_H
#define __SHV_VIEWPORT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

#include "set/QuickContainers.h"

#include "ExportInfo.h"
#include "Wrapper.h"

class NeedleCaret;
class ScaleBarCaret;
struct RoiTracker;
struct SelValuesData;
struct WmsLayer;

enum ToolButtonID;
using SharedRwDataItemInterestPtr = InterestPtr<SharedPtr<AbstrDataItem> >;

//----------------------------------------------------------------------
// class  : ViewPoint
//----------------------------------------------------------------------

struct ViewPoint
{
	CrdPoint center = Undefined();
	CrdType  zoomLevel = UNDEFINED_VALUE(CrdType);
	SharedStr spatialReference;

	ViewPoint(CrdPoint c, CrdType zl, SharedStr sr)
		: center(c), zoomLevel(zl), spatialReference(sr)
	{}
	ViewPoint(CharPtrRange viewPointStr);

	bool WriteAsString(char* buffer, SizeT len, FormattingFlags flags);
};

//----------------------------------------------------------------------
// class  : ViewPort
//----------------------------------------------------------------------

class ViewPort : public Wrapper
{
	typedef Wrapper base_type;
public:
	ViewPort(MovableObject* owner, DataView* dv, CharPtr caption);
	~ViewPort();

//	delayed construction
	const AbstrUnit* GetWorldCrdUnit() const;
	void InitWorldCrdUnit(const AbstrUnit* worldCrdUnit);
	void SetBkColor(COLORREF bkColor) { m_BkColor = bkColor; }

//	nieuwe functions

	const LayerSet* GetLayerSet() const;
	      LayerSet* GetLayerSet();
	const GraphicLayer* GetActiveLayer() const;
		  GraphicLayer* GetActiveLayer();

	void ZoomAll();
	void AL_ZoomAll();
	void ZoomIn1();
	void ZoomFactor(CrdType factor);
	void SetCurrZoomLevel(CrdType scale) { ZoomFactor(GetCurrZoomLevel() / scale); }
	void AL_ZoomSel();

	void ZoomOut1();
	ExportInfo GetExportInfo();
	void Export();
	void Scroll(const GPoint& delta);
	void InvalidateWorldRect(const CrdRect& rect, const GRect& borderExtents) const;

	void Pan  (CrdPoint delta);
	void PanTo(CrdPoint newCenter);

	void PanToClipboardLocation();
	void ZoomToClipboardLocation();
	void PanAndZoomToClipboardLocation();
	void CopyLocationAndZoomlevelToClipboard();

	bool IsNeedleVisible() const { return m_State.Get(VPF_NeedleVisible); }
	void ToggleNeedleController();
	void AL_SelectAllObjects(bool select);

	// props
	void ZoomWorldFullRect(const TRect& relClientRect) ;
	CrdRect GetCurrWorldFullRect() const;
	CrdRect GetCurrWorldClientRect() const;
	CrdRect CalcCurrWorldClientRect() const; // called by GraphicRect::AdjustTargetViewPort, which is called from DoUpdateView
	CrdRect CalcWorldClientRect() const;
	CrdType GetCurrZoomLevel() const;
//	CrdPoint GetSubPixelFactors() const;

  	void        SetROI(const CrdRect& r);
	CrdRect     GetROI() const;
	OrientationType Orientation() const;
	const AbstrDataItem* GetRoiTlParam() const { return m_ROI_TL;}
	const AbstrDataItem* GetRoiBrParam() const { return m_ROI_BR;}

	void SetNeedle(CrdTransformation* tr, GPoint trackPoint);

	void Sync(TreeItem* context, ShvSyncMode sm) override;

	// override virtuals of Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	ActorVisitState DoUpdate(ProgressState ps) override;

//	size, extents and transformations
	CrdTransformation CalcWorldToClientTransformation() const; // calls DoUpdateView -> AdjustTargetViewport
	CrdTransformation CalcCurrWorldToClientTransformation() const; // called by DoUpdateView and GraphicRect::AdjustTargetViewport, uses m_ROI and GetCurrClientSize()
	CrdTransformation GetCurrWorldToClientTransformation() const { return m_w2vTr; }

//	override other virtuals of GraphicObject
  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	bool OnCommand(ToolButtonID id) override;
	bool OnKeyDown(UInt32 virtKey) override;
	void FillMenu(MouseEventDispatcher& med) override;
	CommandStatus OnCommandEnable(ToolButtonID id) const override;
	COLORREF GetBkColor() const override;
	void SetClientSize(TPoint newSize) override;
	bool Draw(GraphDrawer& d) const override;

	      ScalableObject* GetContents();
	const ScalableObject* GetContents() const;

	const GPoint& GetBrushOrg      () const { return m_BrushOrg; }

	GridCoordPtr GetOrCreateGridCoord(const grid_coord_key& geo2worldTr);
	GridCoordPtr GetOrCreateGridCoord(const AbstrUnit* gridCrdUnit);
	SelCaretPtr  GetOrCreateSelCaret (const sel_caret_key& key);

	void PasteGrid(SelValuesData* svd, GridLayer* gl);

	WeakPtr<RoiTracker> m_Tracker;
	auto FindBackgroundWmsLayer() -> WmsLayer*;

private:
	// Override virtuals of GraphicObject
	void DoUpdateView() override;
	void OnChildSizeChanged() override;
	bool MouseEvent(MouseEventDispatcher& med) override;

	bool HasROI() const;
	void UpdateScaleBar();
	void InvalidateOverlapped();

	SharedPtr<const AbstrUnit> m_WorldCrdUnit;
	mutable SharedRwDataItemInterestPtr   m_ROI_TL;   // AOI in world coordinates (TopLeft)
	mutable SharedRwDataItemInterestPtr   m_ROI_BR;   // AOI in world coordinates (BottomRight)
	OrientationType            m_Orientation;
	CrdRect                    m_ROI;
	CrdTransformation          m_w2vTr;
	GPoint                     m_BrushOrg;
	COLORREF                   m_BkColor;

	NeedleCaret*               m_NeedleCaret;
	ScaleBarCaret*             m_ScaleBarCaret;
	grid_coord_map             m_GridCoordMap;
	sel_caret_map              m_SelCaretMap;

	friend GridCoord;
	friend SelCaret;

public:
	static ViewPort* g_CurrZoom;

	DECL_RTTI(SHV_CALL, Class);
};


#endif // __SHV_VIEWPORT_H

