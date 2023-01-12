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

#ifndef __SHV_GRAPHICLAYER_H
#define __SHV_GRAPHICLAYER_H

//----------------------------------------------------------------------
// Module  : GraphicLayers
//----------------------------------------------------------------------

#include "GraphicObject.h"
#include "ThemeSet.h"
#include "ScalableObject.h"

struct IndexCollector;

//----------------------------------------------------------------------
// class  : GraphicLayer
//----------------------------------------------------------------------

/* 
a GraphicLayer is a GraphicObject that combines 
a GeographicReference<SPoint, E> of entity E,
with a set of Aspects.

The GeographicReference<SPoint, E> is
- DataArray<E, S/F/DPoint>   for GraphicPointLayers
- DataArray<SPoint, E>       for GraphicGridLayers; optionally with a SPoint -> CrdPoint world coordinate transformation
- DataArray<E, S/F/DPolygon> for GraphicPolygonLayers
- DataArray<E, S/F/DArc>     for GraphicArcLayers
- DataArray<E, S/F/DElement> for GraphicElementLayers

GraphicPointLayers   have the following Aspects: Color, Symbol, Size, 
GraphicGridLayers    have the following Aspects: Color
GraphicPolygonLayers have the following Aspects: Color, Hatching, AspectRatio
GraphicArcLayers     have the following Aspects: Color, LineStyle, Width

TextLayers are a specialization of PointLayers or ArcLayers.

*/

class GraphicLayer : public ScalableObject, public ThemeSet
{
	typedef ScalableObject base_type;

public:
	GraphicLayer(GraphicObject* owner, const LayerClass* cls);
	~GraphicLayer(); // hide destruction of data members

	void ChangeTheme(Theme* theme);
	void SetThemeAndActivate(Theme* theme, const AbstrDataItem* focus);

//	override virtual methods of GraphicObject
	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;
	void FillMenu(MouseEventDispatcher& med)         override;
  	void SetActive(bool newState)                    override;
	bool OnCommand(ToolButtonID id)                  override;

//	interface extension
	const AbstrUnit* GetWorldCrdUnit() const;
	virtual const AbstrUnit* GetGeoCrdUnit() const = 0;
	CrdTransformation GetGeoTransformation() const { return ::GetGeoTransformation( GetGeoCrdUnit() ); }

//	override ScalableObject
	void              FillLcMenu(MenuData& menuData)          override;
	std::shared_ptr<LayerControlBase> CreateControl(MovableObject* owner) override;
	SharedStr         GetCaption()                                       const override;

	const LayerClass* GetLayerClass() const;

	virtual TokenID GetID() const override;

	virtual void SelectPoint(CrdPoint pnt, EventID eventID) {};
	virtual void SelectRect(CrdRect worldRect, EventID eventID) {};
	virtual void SelectCircle(CrdPoint worldPnt, CrdType worldRadius, EventID eventID) {};
	virtual void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) {};
	virtual void SelectDistrict(CrdPoint pnt, EventID eventID);

	virtual CrdRect CalcSelectedFullWorldRect() const = 0;

	virtual void _InvalidateFeature(SizeT featureIndex) = 0;

	std::weak_ptr<LayerSet> GetLayerSet();

	void EnableAspectGroup(AspectGroup ag, bool enable);

	void SelectAll(bool select);

//	feature support (feature ID -> entity ID in ExtKey, in GridLayer, feature==rastercell
	bool SelectFeatureIndex(AbstrDataObject* selAttrObj, SizeT featureIndex, EventID eventID);
	bool IsFeatureSelected(SizeT featureIndex) const;

	bool VisibleLevel(Float64 currNrPixelsPerUnit) const;
	bool VisibleLevel(GraphDrawer& d) const;

	bool HasEntityIndex() const;
	bool HasEntityAggr () const;

	SizeT Feature2EntityIndex(SizeT featureIndex) const;
	SizeT Entity2FeatureIndex(SizeT entityIndex ) const;

	bool HasEditAttr() const;
	bool HasClassIdAttr() const;

protected:
	const IndexCollector*  GetIndexCollector() const;
//	const IndexCollector*  GetFeatureIndexCollector() const;

	bool SelectEntityIndex  (AbstrDataObject* selAttrObj, SizeT selectedIndex, EventID eventID);
	bool SetFocusEntityIndex(SizeT focussedIndex, bool showDetails);
	bool IsEntitySelected   (SizeT entityID) const;

	std::shared_ptr<const Theme> GetEditTheme     () const;
	AbstrDataItem* GetEditAttr      () const;
	ClassID        GetCurrClassID   () const;
	SharedStr      GetCurrClassLabel() const;

//	override Actor callbacks
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	void DoInvalidate () const override;

private:
	void OnFocusElemChanged(SizeT selectedID, SizeT oldSelectedID);

	mutable SharedPtr<IndexCollector> m_EntityIndexCollector;
	mutable SharedDataItemInterestPtr m_ActiveThemeValuesUnitLabelLock;

protected:
	SharedPtr<const AbstrUnit>   m_SelEntity;
	SharedDataItemInterestPtr    m_SelIndexAttr;
	
	DECL_ABSTR(SHV_CALL, Class);
};

#endif // __SHV_GRAPHICLAYER_H

