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

template <typename T> struct AbstrRowProcessor;
struct PasteHandler;
struct GridColorPalette;

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
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

	CrdRect CalcSelectedFullWorldRect() const    override;

	const AbstrDataItem* GetGridAttr() const;
	const AbstrUnit* GetGeoCrdUnit() const       override;
	void  InvalidateFeature(SizeT selectedID) override;

	void AssignValues(sequence_traits<Bool>::cseq_t selData);
	void AssignSelValues();
	void CopySelValuesToBitmap();


	void CopySelValues ();
	void PasteSelValuesDirect();
	void PasteSelValues();
	void PasteNow();
	void ClearPaste();

private:
	void InvalidatePasteArea(); friend class PasteGridController;

	CrdRect GetWorldExtents(feature_id featureIndex) const;
	template <typename T> void SelectRegion(CrdRect worldRect, const AbstrRowProcessor<T>& rowProcessor, AbstrDataItem* selAttr, EventID eventID);
	bool DrawAllRects(GraphDrawer& d, const GridColorPalette& colorPalette) const;
	void DrawPaste   (GraphDrawer& d, const GridColorPalette& colorPalette) const;

	void Zoom1To1(ViewPort* vp) override;

//	GridCoordPtr GetGridCoordInfo(ViewPort* vp) const; friend class ViewPort;
	void CreateSelCaretInfo () const;
	IRect CalcSelectedGeoRect()  const;

	mutable SharedPtr<const AbstrUnit> m_GeoCoordUnit;
	mutable SelCaretPtr                m_SelCaret;
	mutable OwningPtr<PasteHandler>    m_PasteHandler;

	DECL_RTTI(SHV_CALL, LayerClass);
};


#endif // __SHV_GRIDLAYER_H

