// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__SHV_MAPCONTROL_H)
#define __SHV_MAPCONTROL_H

#include "ShvBase.h"
#include "dbg/DebugCast.h"

#include "ShvUtils.h"

#include "ViewControl.h"

class ViewPort;
class ScrollPort;

//----------------------------------------------------------------------
// class  : MapControl
//----------------------------------------------------------------------

class MapControl : public ViewControl
{
	typedef ViewControl base_type;
public:
	MapControl(DataView* dv);
	~MapControl();
	void Init(DataView* dv);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ChildCovered; };

  	GraphVisitState InviteGraphVistor(AbstrVisitor&) override;
	bool OnCommand(ToolButtonID id) override;
	CommandStatus OnCommandEnable(ToolButtonID id) const override;
	void OnChildSizeChanged() override;

	void Sync(TreeItem* viewContext, ShvSyncMode sm);

	const ViewPort* GetViewPort() const { return m_ViewPort.get(); }
	      ViewPort* GetViewPort()       { return m_ViewPort.get(); }
	const LayerSet* GetLayerSet() const { return m_LayerSet.get(); }
		  LayerSet* GetLayerSet()       { return m_LayerSet.get(); }

	ScrollPort*      GetScrollPort()    { return m_ScrollPort.get(); }
	LayerControlSet* GetLayerControls() { return m_LayerControlSet.get(); }

	ViewPort* GetOverviewPort  () { return m_OvViewPort.get(); }
	LayerSet* GetOverviewLayers() { return m_OvLayerSet.get(); }

	void ToggleOverview();
	void ToggleLayerControl();

protected:
	// Override virtuals of GraphicObject
	bool OnKeyDown(UInt32 nVirtKey) override;
	void ProcessSize(CrdPoint mapControlSize) override;

private:
	void SetLayout(ToolButtonID id);

	std::shared_ptr<ViewPort>        m_ViewPort;
	std::shared_ptr<LayerSet>        m_LayerSet;

	std::shared_ptr<ScrollPort>      m_ScrollPort;
	std::shared_ptr<LayerControlSet> m_LayerControlSet;

	std::shared_ptr<ViewPort>        m_OvViewPort;
	std::shared_ptr<LayerSet>        m_OvLayerSet;

	DECL_RTTI(SHV_CALL, Class);
};

#endif // __SHV_MAPCONTROL_H
