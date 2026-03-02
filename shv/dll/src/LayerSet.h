// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __SHV_LAYERSET_H
#define __SHV_LAYERSET_H

#include "Aspect.h"
#include "GraphicContainer.h"
#include "ScalableObject.h"

struct LayerInfo;

//----------------------------------------------------------------------
// class  : LayerSet
//	ScalableObject that contains ScalableObjeets (GraphicLayers, Rects and other LayerSets)
//----------------------------------------------------------------------

class LayerSet : public GraphicContainer<ScalableObject>
{
	typedef GraphicContainer<ScalableObject> base_type;
public:
	LayerSet(GraphicObject* owner, CharPtr caption = "");

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::PushVisibility; }

//	override ScalableObject --- LayerControl Support
	void              FillLcMenu(MenuData& menuData)          override;
	std::shared_ptr<LayerControlBase> CreateControl(MovableObject* owner) override;
	SharedStr         GetCaption()                                       const override;

//	override ScalableObject --- size and posiitioning
	TRect GetBorderLogicalExtents() const override;
	bool HasDefinedExtent() const override;

//	override GraphicObject interface
	void DoUpdateView()                               override;
	GraphVisitState InviteGraphVistor(class AbstrVisitor&)       override;
	bool OnCommand(ToolButtonID id)                   override;
	void Sync(TreeItem* themeContext, ShvSyncMode sm) override;
	void ProcessCollectionChange()                    override;
	bool GetTooltipText(TooltipCollector& med) const override;

	void ShuffleEntry  (ScalableObject* focalEntry, ElemShuffleCmd esc);

	std::shared_ptr<GraphicLayer> CreateLayer (const AbstrDataItem* viewItem, const LayerInfo& info, DataView* dv, bool foreground);
	std::shared_ptr<LayerSet>     CreateSubset(CharPtr caption);

	const GraphicLayer* GetActiveLayer() const;
	GraphicLayer* GetActiveLayer();

private:
	void ShowPalettes(bool visible, const GraphicLayer* activeLayer);

	SharedStr m_Caption;

	DECL_RTTI(SHV_CALL, ShvClass);
};

#endif // __SHV_LAYERSET_H

