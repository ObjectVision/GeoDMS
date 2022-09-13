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

#if !defined(__SHV_MOUSEEVENTDISPATCHER_H)
#define __SHV_MOUSEEVENTDISPATCHER_H

#include "GraphVisitor.h"

#include "ptr/OwningPtr.h"

#include "MenuData.h"
#include "ActivationInfo.h"

//----------------------------------------------------------------------
// class MouseEventDispatcher
//----------------------------------------------------------------------

class MouseEventDispatcher : public GraphVisitor
{
	typedef GraphVisitor base_type;
public:
	MouseEventDispatcher(DataView* owner, EventInfo& eventInfo);

	GraphVisitState DoObject    (GraphicObject* go ) override;
	GraphVisitState DoMovable   (MovableObject* obj) override;
	GraphVisitState DoViewPort  (ViewPort*      vp ) override;

	const MenuData&  GetMenuData()  const { return m_MenuData;  } 
	const EventInfo& GetEventInfo() const { return r_EventInfo; }
	      EventInfo& GetEventInfo()       { return r_EventInfo; }
		  std::weak_ptr<DataView> GetOwner()     const { return m_Owner;     }
		  bool       IsActivating() const;

protected:
	virtual bool ReverseLayerVisitationOrder() const { return true; }

public:
	std::weak_ptr<DataView>   m_Owner;
	EventInfo&                r_EventInfo;
	MenuData                  m_MenuData;
	CrdPoint                  m_WorldCrd; 

public:
	std::shared_ptr<MovableObject>  m_ActivatedObject;
	std::shared_ptr<GraphicObject>  m_FoundObject;
};

#endif // __SHV_MOUSEEVENTDISPATCHER_H

