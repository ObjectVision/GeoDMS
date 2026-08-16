// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __SHV_GRIDLAYER_H
#define __SHV_GRIDLAYER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "GridLayerBase.h"
#include "ptr/SharedTreePtr.h"

template <typename T> struct AbstrRowProcessor;
#ifdef _WIN32
struct PasteHandler;
#endif
struct GridColorPalette;
struct GridCoord;

//----------------------------------------------------------------------
// class  : GraphicGridLayer
//----------------------------------------------------------------------

class GridLayer : public GridLayerBase
{
	typedef GraphicLayer base_type;
	// gridAttr:  CrdDomain -> E   // condition: CrdDomain or Domain(GetBasisGrid(gridAttr)) is SPoint or IPoint 

	// themeAttr: E->V
	// classificationAttr: ClsID->V
public:
	GridLayer(GraphicObject* owner);
	~GridLayer();

	void SelectDistrict(CrdPoint pnt, EventID eventID) override;

protected:
//	override virtuals of GraphicObject
	void  DoUpdateView() override;
	TRect GetBorderLogicalExtents() const override;
	bool  Draw(GraphDrawer& d) const override;
	GraphVisitState InviteGraphVistor(class AbstrVisitor&) override;
	bool OnCommand(ToolButtonID id) override;

//	override virtuals of GraphicLayer
	void FillMenu(MouseEventDispatcher& med) override;
	void SelectPoint  (CrdPoint pnt, EventID eventID) override;
	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID, const JacobianMatrix<CrdType>* worldEllipse = nullptr) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;
	bool GetTooltipText(TooltipCollector& ttc) const override;

	CrdRect CalcSelectedFullWorldRect() const    override;

	const AbstrDataItem* GetGridAttr() const;
	const AbstrUnit* GetGeoCrdUnit() const       override;
	void  InvalidateFeature(SizeT selectedID) override;

	void AssignValues(sequence_traits<Bool>::cseq_t selData);
	void AssignSelValues();
	void CopySelValuesToBitmap();


	void CopySelValues ();
#ifdef _WIN32
	void PasteSelValuesDirect();
	void PasteSelValues();
	void PasteNow();
	void ClearPaste();
#endif

public:
	template <typename T> void SelectRegion(CrdRect worldRect, const AbstrRowProcessor<T>& rowProcessor, AbstrDataItem* selAttr, EventID eventID);

private:
#ifdef _WIN32
	void InvalidatePasteArea(); friend class PasteGridController;
#endif

	CrdRect GetWorldExtents(feature_id featureIndex) const;
	bool DrawAllRects(GraphDrawer& d, const GridColorPalette& colorPalette) const;
	// >c (rotated/projective grid->device) path: render each tile at native resolution and issue one
	// transformed blit per tile. Selected when the composite grid->device is !IsAxisSeparable().
	bool DrawAllRectsTransformed(GraphDrawer& d, const GridColorPalette& colorPalette
		, GridCoord* drawGridCoords, const AbstrDataItem* grid, const AbstrUnit* gridDomain
		, GPoint viewportDeviceOffset) const;
#ifdef _WIN32
	void DrawPaste   (GraphDrawer& d, const GridColorPalette& colorPalette) const;
#endif

	void Zoom1To1(ViewPort* vp) override;

//	GridCoordPtr GetGridCoordInfo(ViewPort* vp) const; friend class ViewPort;
	void CreateSelCaretInfo () const;
	IRect CalcSelectedGeoRect()  const;

	mutable std::shared_ptr<const AbstrUnit>    m_GeoCoordUnit;
	mutable SelCaretPtr                   m_SelCaret;
#ifdef _WIN32
	mutable std::unique_ptr<PasteHandler> m_PasteHandler;
#endif

	DECL_RTTI(, LayerClass)
};


#endif // __SHV_GRIDLAYER_H

