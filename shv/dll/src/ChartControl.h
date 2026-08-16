// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_CHARTCONTROL_H
#define __SHV_CHARTCONTROL_H

#include "ShvBase.h"

#include "ShvUtils.h"
#include "ViewControl.h"

class ViewPort;
class ScrollPort;
class AxisControl;

//----------------------------------------------------------------------
// class  : ChartControl
//----------------------------------------------------------------------
// Composition of a chart view (issue #75), analogous to MapControl:
// a plot-area ViewPort (FitMode::Stretch over a synthetic chart-space world unit)
// flanked by a vertical and a horizontal AxisControl, plus the LayerControlSet
// panel that doubles as the chart legend.

class ChartControl : public ViewControl
{
	typedef ViewControl base_type;
public:
	ChartControl(DataView* dv);
	~ChartControl();
	void Init(DataView* dv);

	GraphicClassFlags GetGraphicClassFlags() const override { return GraphicClassFlags::ChildCovered; };

	bool OnCommand(ToolButtonID id) override;
	CommandStatus OnCommandEnable(ToolButtonID id) const override;
	void DoUpdateView() override;

	void Sync(TreeItem* viewContext, ShvSyncMode sm) override;

	const ViewPort* GetViewPort() const { return m_ViewPort.get(); }
	      ViewPort* GetViewPort()       { return m_ViewPort.get(); }
	const LayerSet* GetLayerSet() const { return m_LayerSet.get(); }
	      LayerSet* GetLayerSet()       { return m_LayerSet.get(); }

	ScrollPort*      GetScrollPort()    { return m_ScrollPort.get(); }
	LayerControlSet* GetLayerControls() { return m_LayerControlSet.get(); }

	void ToggleLayerControl();

protected:
	bool OnKeyDown(UInt32 nVirtKey) override;
	void ProcessSize(CrdPoint chartControlSize) override;

private:
	void SetLayout(ToolButtonID id);

	std::shared_ptr<ViewPort>        m_ViewPort;
	std::shared_ptr<LayerSet>        m_LayerSet;

	std::shared_ptr<AxisControl>     m_XAxis, m_YAxis;

	std::shared_ptr<ScrollPort>      m_ScrollPort;
	std::shared_ptr<LayerControlSet> m_LayerControlSet;

	DECL_RTTI(, Class)
};

#endif // __SHV_CHARTCONTROL_H
