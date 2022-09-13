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

	GraphicClassFlags GetGraphicClassFlags() const override { dms_assert(!base_type::GetGraphicClassFlags()); return GCF_ChildCovered; };

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
	void ProcessSize(TPoint mapControlSize) override;

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
