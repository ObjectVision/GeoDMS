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
	GRect   GetBorderPixelExtents(CrdType subPixelFactor) const override;
	bool HasDefinedExtent() const override;

//	override GraphicObject interface
	void DoUpdateView()                               override;
	GraphVisitState InviteGraphVistor(class AbstrVisitor&)       override;
	bool OnCommand(ToolButtonID id)                   override;
	void Sync(TreeItem* themeContext, ShvSyncMode sm) override;
	void ProcessCollectionChange()                    override;

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

