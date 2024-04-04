// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_FEATURELAYER_H
#define __SHV_FEATURELAYER_H

#include "GraphicLayer.h"

#include "ptr/OwningPtr.h"
#include "ptr/WeakPtr.h"
#include "geo/color.h"

#include "DataArray.h"

#include "LockedIndexCollectorPtr.h"
#include "FontIndexCache.h"
#include "FontRole.h"
#include "PenIndexCache.h"
#include "AbstrBoundingBoxCache.h"

class AbstrThemeValueGetter;
class AbstrDataItem;
class FeatureLayer;
struct FontIndexCache;
struct PenIndexCache;

//----------------------------------------------------------------------
// struct  : FeatureDrawer
//----------------------------------------------------------------------

struct FeatureDrawer
{
	FeatureDrawer(const FeatureLayer* layer, GraphDrawer&  drawer);

	WeakPtr<const IndexCollector> GetIndexCollector() const { return m_IndexCollector; }
	bool                          HasLabelText     () const;
	const FontIndexCache*         GetUpdatedLabelFontIndexCache() const;

	WeakPtr<const FeatureLayer> m_Layer;
	GraphDrawer&                m_Drawer;
	const Float64               m_WorldZoomLevel;

	LockedIndexCollectorPtr    m_IndexCollector;

	std::optional<DataArray<SelectionID>::locked_cseq_t> m_SelValues;
};

//----------------------------------------------------------------------
// class  : FeatureLayer
//----------------------------------------------------------------------

class FeatureLayer : public GraphicLayer 
{
	typedef GraphicLayer base_type;

	// theme: (E->V, ClsID->V, ClsID->Aspect)

	// featrueAttr: FeatureID -> *CrdDomain
	// featureID:   FeatureID -> EK
	// geoRelAttr:  E         -> EK

	// theme: (E->V, Cls->V, Cls->Aspect)
	//	theme == featureTheme when V := EK and Cls := FeatureID and Aspect := *CrdDomain

	// featrueData: FeatureID -> *CrdDomain
	// featureAttr: FeatureID -> E
	// theme:   (E->V, ClsID->V, ClsID->Aspect)
	// feature: (E->EK, FtrId->EK, FtrId->*Crd)

	GraphVisitState InviteGraphVistor(class AbstrVisitor&) override;

public:
	void Init();
	const AbstrUnit*     GetThemeDomainEntity () const;
	const AbstrDataItem* GetFeatureAttr       () const;
	Int32                GetMaxLabelStrLen    () const;

	DmsColor& GetDefaultPointColor() const;
	DmsColor& GetDefaultBrushColor() const;
	DmsColor& GetDefaultArcColor() const;
	DmsColor GetDefaultOrThemeColor(AspectNr an) const;

//	override virtual of GraphicLayer
	const AbstrUnit* GetGeoCrdUnit     () const override;
	CrdRect CalcSelectedFullWorldRect  () const override;

//	override virtual of GraphicObject
	TRect   GetBorderLogicalExtents() const override;
	bool    OnCommand(ToolButtonID id)        override;
	void    FillLcMenu(MenuData& menuData) override;

	virtual  CrdRect GetFeatureWorldExtents   () const;
	virtual  TRect   GetFeatureLogicalExtents() const;
	virtual  CrdRect CalcSelectedClientWorldRect() const;

  	CrdRect GetExtentsInflator(const CrdTransformation& tr) const;
  	CrdRect GetWorldClipRect  (const GraphDrawer& d) const;

protected: friend FeatureDrawer; friend struct LabelDrawer;
	FeatureLayer(GraphicObject* owner, const LayerClass* cls);

//	new interface
	virtual bool DrawImpl(FeatureDrawer& fd) const =0;
	virtual SizeT FindFeatureByPoint(const CrdPoint& geoPnt);
	virtual SizeT FindNextFeatureByPoint(const CrdPoint& geoPnt, SizeT featureIndex);

	std::shared_ptr<const AbstrBoundingBoxCache> GetBoundingBoxCache() const;

	FontIndexCache* GetFontIndexCache(FontRole fr) const;
	PenIndexCache*  GetPenIndexCache(DmsColor defaultColor) const;
	bool ShowSelectedOnlyEnabled() const             override;
	void UpdateShowSelOnly      ()                   override;

//	override GraphicDrawer
	void SelectPoint(CrdPoint pnt, EventID eventID) override;

//	override virtual of GraphicObject
	void DoUpdateView() override;
	bool Draw(GraphDrawer& d) const override;

//	override virtuals of Actor
	void DoInvalidate () const override;

public:
	mutable std::shared_ptr<const AbstrBoundingBoxCache> m_BoundingBoxCache;

private:
	DmsColor& UpdateDefaultColor(DmsColor& mutableDefaultPaletteColor) const;

	CrdRect                                  m_FeatureDataExtents;
	mutable OwningPtr<FontIndexCache>        m_FontIndexCaches[FR_Count];
	mutable OwningPtr<PenIndexCache>         m_PenIndexCache;
	mutable UInt32                           m_MaxLabelStrLen;
protected:
	mutable DmsColor m_DefaultPointColor = -2, m_DefaultArcColor=-2, m_DefaultBrushColor=-2;
	DECL_ABSTR(SHV_CALL, Class);
};

//----------------------------------------------------------------------
// class  : GraphicPointLayer
//----------------------------------------------------------------------

class GraphicPointLayer : public FeatureLayer
{
	typedef FeatureLayer base_type;

public:
	GraphicPointLayer(GraphicObject* owner, const LayerClass* cls = GetStaticClass());

	TRect   GetFeatureLogicalExtents() const override;
	CrdRect GetFeatureWorldExtents() const override;

protected:
//	override virtual of FeatureLayer
	bool DrawImpl(FeatureDrawer& fd) const override;

	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

	CrdRect CalcSelectedClientWorldRect() const override; // specialization that doesn't use BoundinBox

	SizeT FindFeatureByPoint(const CrdPoint& geoPnt) override;
	void   InvalidateFeature(SizeT featureIndex) override;

private:
	void InvalidatePoint(UInt32 selectedID);

	DECL_RTTI(SHV_CALL, LayerClass);
};

//----------------------------------------------------------------------
// class  : GraphicNetworkLayer
//----------------------------------------------------------------------

class GraphicNetworkLayer : public GraphicPointLayer
{
	typedef GraphicPointLayer base_type;
//	override virtual of FeatureLayer
	bool DrawImpl(FeatureDrawer& fd) const override;

public:
	GraphicNetworkLayer(GraphicObject* owner, const LayerClass* cls = GetStaticClass()) : base_type(owner, cls) {}

	DECL_RTTI(SHV_CALL, LayerClass);
};

//----------------------------------------------------------------------
// class  : GraphicArcLayer
//----------------------------------------------------------------------

class GraphicArcLayer : public FeatureLayer
{
	typedef FeatureLayer base_type;

public:
	GraphicArcLayer(GraphicObject* owner);

	TRect   GetFeatureLogicalExtents() const override;
	CrdRect GetFeatureWorldExtents() const override;

protected:

//	override virtual of FeatureLayer
	bool DrawImpl(FeatureDrawer& fd) const override;

	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

	SizeT FindFeatureByPoint(const CrdPoint& geoPnt) override;
	void  InvalidateFeature(SizeT featureIndex) override;

	DECL_RTTI(SHV_CALL, LayerClass);
};

//----------------------------------------------------------------------
// class  : GraphicPolygonLayer
//----------------------------------------------------------------------

class GraphicPolygonLayer : public FeatureLayer
{
	typedef FeatureLayer base_type;

public:
	GraphicPolygonLayer(GraphicObject* owner);

	TRect   GetFeatureLogicalExtents() const override;
	CrdRect GetFeatureWorldExtents() const override;

protected:

//	override virtual of FeatureLayer
	bool DrawImpl(FeatureDrawer& fd) const override;

	void SelectRect   (CrdRect worldRect, EventID eventID) override;
	void SelectCircle (CrdPoint worldPnt, CrdType worldRadius, EventID eventID) override;
	void SelectPolygon(const CrdPoint* first, const CrdPoint* last, EventID eventID) override;

	SizeT FindNextFeatureByPoint(const CrdPoint& geoPnt, SizeT currFeatureIndex) override;
	void  InvalidateFeature(SizeT featureIndex) override;

	DECL_RTTI(SHV_CALL, LayerClass);
};

//----------------------------------------------------------------------
// AbstrBoundingBoxCache
//----------------------------------------------------------------------

#include "BoundingBoxCache.h"

template <typename ScalarType>
const SequenceBoundingBoxCache<ScalarType>*
GetSequenceFeatureBoundingBoxCache(const FeatureLayer* layer)
{
	return GetSequenceBoundingBoxCache<ScalarType>(layer->m_BoundingBoxCache, layer->GetFeatureAttr(), true);
}

template <typename ScalarType>
const PointBoundingBoxCache<ScalarType>*
GetPointFeautureBoundingBoxCache(const FeatureLayer* layer)
{
	return GetPointBoundingBoxCache<ScalarType>(layer->m_BoundingBoxCache, layer->GetFeatureAttr(), true);
}

template <typename ScalarType>
std::shared_ptr<const SequenceBoundingBoxCache<ScalarType>>
GetSequenceBoundingBoxCache(const FeatureLayer* layer)
{
	assert(IsMetaThread());
	if (!layer->m_BoundingBoxCache)
		layer->m_BoundingBoxCache = GetSequenceBoundingBoxCache<ScalarType>(layer->GetFeatureAttr(), true);
	return { layer->m_BoundingBoxCache, dynamic_cast<const SequenceBoundingBoxCache<ScalarType>*>(layer->m_BoundingBoxCache.get()) };
}

template <typename ScalarType>
std::shared_ptr<const PointBoundingBoxCache<ScalarType>>
GetPointBoundingBoxCache(const FeatureLayer* layer)
{
	assert(IsMetaThread());
	if (!layer->m_BoundingBoxCache)
		layer->m_BoundingBoxCache = GetPointBoundingBoxCache<ScalarType>(layer->GetFeatureAttr(), true);
	return { layer->m_BoundingBoxCache, dynamic_cast<const PointBoundingBoxCache<ScalarType>*>(layer->m_BoundingBoxCache.get()) };
}

#endif // __SHV_FEATURELAYER_H

