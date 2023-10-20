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

enum EventID {
	EID_MOUSEMOVE     = 0x001,
	EID_MOUSEDRAG     = 0x002,

	EID_LBUTTONDOWN   = 0x004,
	EID_LBUTTONUP     = 0x008,
	EID_LBUTTONDBLCLK = 0x010,

	EID_RBUTTONDOWN   = 0x020,
	EID_RBUTTONUP     = 0x040,
	EID_RBUTTONDBLCLK = 0x080,

	EID_CAPTURECHANGED= 0x100,
	EID_SCROLLED      = 0x200,
	EID_PANNED        = 0x200,
	EID_CTRLKEY       = 0x400,
	EID_SHIFTKEY      = 0x800,
	EID_OBJECTFOUND   = 0x1000,
	EID_ACTIVATE      = 0x2000,

	EID_REQUEST_INFO  = 0x4000,
	EID_REQUEST_SEL   = 0x8000,

	EID_CLOSE_EVENTS  = EID_LBUTTONUP|EID_CAPTURECHANGED|EID_MOUSEMOVE|EID_SCROLLED,

	EID_HANDLED       = 0x00010000,
	EID_SETCURSOR     = 0x00020000,
	EID_COPYCOORD     = 0x00040000,
	EID_MOUSEWHEEL    = 0x00080000,
};
typedef UInt32 EventIdType;

inline bool IsCreateNewEvent(EventID eid) { return (eid & (EID_SHIFTKEY|EID_CTRLKEY)) ==0 ; }
inline auto CompoundWriteType(EventID eid) { return DmsRwChangeType(IsCreateNewEvent(eid)); }

struct EventInfo
{
	EventInfo(EventID eventID, UINT wParam, GPoint devicePoint = GPoint())
		:	m_EventID(eventID)
		,	m_wParam(wParam)
		,	m_Point(devicePoint)
	{}

	EventID GetID() const { return EventID(m_EventID); }

	EventIdType m_EventID;
	UINT        m_wParam;
	GPoint      m_Point;
};

class AbstrController : public SharedObj
{
	typedef PersistentSharedObj base_type;
public:
	AbstrController(DataView* owner, GraphicObject* target
	,	EventIdType moveEvents  // = EID_MOUSEMOVE|EID_MOUSEDRAG
	,	EventIdType execEvents, EventIdType stopEvents
	,	ToolButtonID toolID
	);

	virtual ~AbstrController();

	std::weak_ptr<DataView>      GetOwner         () { return m_Owner; }
	std::weak_ptr<GraphicObject> GetTargetObject  () { return m_TargetObject; }

	bool Event(EventInfo& eventInfo);

	auto GetPressStatus(ToolButtonID id) const -> PressStatus
	{
		if (id == m_ToolID)
			return PressStatus::Dn;
		return PressStatus::DontCare;
	}

protected:
	virtual bool Move(EventInfo& eventInfo);
	virtual bool Exec(EventInfo& eventInfo);
	virtual void Stop(); friend struct ControllerStopper;

private:
	std::weak_ptr<DataView>      m_Owner;
	std::weak_ptr<GraphicObject> m_TargetObject;
	UInt32 m_MoveEvents, m_ExecEvents, m_StopEvents;
	ToolButtonID                 m_ToolID;
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
				eventInfo.m_EventID |= EID_HANDLED;
			if (i < size() && ctrl == begin()[i]) // not yet deleted?
				++i;
		}
	}
};

#endif // __SHV_ABSTRCONTROLLER_H


