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
	GRect GetBorderPixelExtents(CrdType subPixelFactor) const override;
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
	void  _InvalidateFeature(SizeT selectedID) override;

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

