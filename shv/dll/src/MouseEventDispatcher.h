// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
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

	auto GetGeoPoint() const { return GetTransformation().Reverse(GPoint2CrdPoint(GetEventInfo().m_Point)); }

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

struct TooltipCollector : EventInfo, MouseEventDispatcher
{
	TooltipCollector(DataView* owner, GPoint pt)
		: EventInfo(EventID::HOVERED, 0, pt)
		, MouseEventDispatcher(owner, *this)
		, m_Stream(&m_Buff, FormattingFlags::ThousandSeparator)
	{
	}

	GraphVisitState DoObject(GraphicObject* go);

	VectorOutStreamBuff m_Buff;
	FormattedOutStream  m_Stream;

	bool    m_WantsMoreContext = true;
	CrdRect m_DevRect;
};

#endif // __SHV_MOUSEEVENTDISPATCHER_H

