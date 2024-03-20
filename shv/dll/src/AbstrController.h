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

#ifndef __SHV_ABSTRCONTROLLER_H
#define __SHV_ABSTRCONTROLLER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/PersistentSharedObj.h"
#include "ShvUtils.h"

class DataView;
class AbstrCaret;
class GraphicObject;

//----------------------------------------------------------------------
// class  : AbstrController
//----------------------------------------------------------------------

//	AbstrController is Inherited by
//		PointCaretController
//			*		EnlargeController
//			*		ZoomMoreController
//			*		ZoomLessController
// 			*		SelectController
//		DualPointCaretController
// 			*		SelRectController
// 			*		SelCircController
// 			*		RouteController
//			*		ZoomInController
//			*		PanController

// MouseControllerEvents

using EventIdType = UInt32;

enum class EventID : EventIdType  {
	NONE          = 0x000,
	MOUSEMOVE     = 0x001,
	MOUSEDRAG     = 0x002,

	LBUTTONDOWN   = 0x004,
	LBUTTONUP     = 0x008,
	LBUTTONDBLCLK = 0x010,

	RBUTTONDOWN   = 0x020,
	RBUTTONUP     = 0x040,
	RBUTTONDBLCLK = 0x080,

	CAPTURECHANGED= 0x100,
	SCROLLED      = 0x200,
	PANNED        = 0x200,
	CTRLKEY       = 0x400,
	SHIFTKEY      = 0x800,
	OBJECTFOUND   = 0x1000,
	ACTIVATE      = 0x2000,

	REQUEST_INFO  = 0x4000,
	REQUEST_SEL   = 0x8000,

	CLOSE_EVENTS  = LBUTTONUP|CAPTURECHANGED|MOUSEMOVE|SCROLLED,

	HANDLED       = 0x00010000,
	SETCURSOR     = 0x00020000,
	COPYCOORD     = 0x00040000,
	MOUSEWHEEL    = 0x00080000,
	TEXTSENT      = 0x00100000,
};

inline EventID operator | (EventID a, EventID b) { return EventID(EventIdType(a) | EventIdType(b)); }
inline bool    operator & (EventID a, EventID b) { return EventIdType(a) & EventIdType(b); }
inline void operator |= (EventID& a, EventID b) { a = a | b; }
inline EventID operator - (EventID a, EventID b) { return EventID(EventIdType(a) & ~EventIdType(b)); }

inline bool IsCreateNewEvent(EventID eid) { return not(eid & (EventID::SHIFTKEY| EventID::CTRLKEY)); }
inline auto CompoundWriteType(EventID eid) { return DmsRwChangeType(IsCreateNewEvent(eid)); }

struct EventInfo
{
	EventInfo(EventID eventID, UINT wParam, GPoint devicePoint = GPoint())
		:	m_EventID(eventID)
		,	m_wParam(wParam)
		,	m_Point(devicePoint)
	{}

	EventID GetID() const { return m_EventID; }

	EventID m_EventID;
	UINT    m_wParam;
	GPoint  m_Point;
};

class AbstrController : public SharedObj
{
	typedef PersistentSharedObj base_type;
public:
	AbstrController(DataView* owner, GraphicObject* target
	,	EventID moveEvents  // = EventID::MOUSEMOVE|EventID::MOUSEDRAG
	,	EventID execEvents, EventID stopEvents
	,	ToolButtonID toolID
	);

	virtual ~AbstrController();

	std::weak_ptr<DataView>      GetOwner         () const { return m_Owner; }
	std::weak_ptr<GraphicObject> GetTargetObject  () const { return m_TargetObject; }

	bool Event(EventInfo& eventInfo);

	auto GetPressStatus(ToolButtonID id) const -> PressStatus
	{
		if (id == m_ToolID)
			return PressStatus::Dn;
		return PressStatus::DontCare;
	}

	bool SendStatusText(CharPtr format, CrdType dst, CrdType dst2) const;

protected:
	virtual bool Move(EventInfo& eventInfo);
	virtual bool Exec(EventInfo& eventInfo);
	virtual void Stop(); friend struct ControllerStopper;

private:
	std::weak_ptr<DataView>      m_Owner;
	std::weak_ptr<GraphicObject> m_TargetObject;
	EventID                      m_MoveEvents, m_ExecEvents, m_StopEvents;
	ToolButtonID                 m_ToolID;
	mutable std::unique_ptr< UnitLabelScalePair> m_UnitLabelScalePtr;
};

//----------------------------------------------------------------------
// class  : AbstrController
//----------------------------------------------------------------------

struct controller_vector : std::vector<SharedPtr<AbstrController> >
{
	void operator () (EventInfo& eventInfo) const
	{
		UInt32 i = 0;
 		while (i < size())
		{
			SharedPtr<AbstrController> ctrl = begin()[i];
			if (ctrl->Event(eventInfo))
				eventInfo.m_EventID |= EventID::HANDLED;
			if (i < size() && ctrl == begin()[i]) // not yet deleted?
				++i;
		}
	}
};

#endif // __SHV_ABSTRCONTROLLER_H


