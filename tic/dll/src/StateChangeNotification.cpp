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
#include "TicPCH.h"
#pragma hdrstop

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "StateChangeNotification.h"

#include "dbg/DmsCatch.h"
#include "set/QuickContainers.h"

#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------

namespace { // local defs

	typedef std::pair<TStateChangeNotificationFunc, ClientHandle> TStateChangeNotificationSink;
	typedef std::set<TStateChangeNotificationSink>               TStateChangeNotificationSinkContainer;
	static_ptr<TStateChangeNotificationSinkContainer>             g_StateChangeNotifications;

	typedef std::pair<const TreeItem*, TStateChangeNotificationSink> TTreeItemStateChangeNotificationSink;
	typedef std::set<TTreeItemStateChangeNotificationSink>         TTreeItemStateChangeNotificationSinkContainer;

	static_ptr<TTreeItemStateChangeNotificationSinkContainer>       g_TreeItemStateChangeNotificationSink;

} // end anonymous namespace

//----------------------------------------------------------------------
// header implementation
//----------------------------------------------------------------------

CharPtr UpdateStateName(UInt32 nc)
{
	switch (nc) {
	case NC2_Invalidated: return "None";
	case NC2_MetaReady:   return "MetaInfoReady";
	case NC2_DataReady:   return "DataReady";
	case NC2_Validated:   return "Validated";
	case NC2_Committed:   return "Validated&Committed";
	}
	return "unrecognized";
}

void NotifyStateChange(const TreeItem* item, UInt32 state)
{
	if (TreeItem::s_NotifyChangeLockCount || (state < CC_First && item->IsPassor()))
		return;

	if (g_StateChangeNotifications)
	{
		TStateChangeNotificationSinkContainer::const_iterator 
			current = g_StateChangeNotifications->begin(),
			last    = g_StateChangeNotifications->end();
		while (current != last)
		{
			ClientHandle clientHandle = current->second;
			(current++)->first(clientHandle, item, NotificationCode(state) );
		}
	}
	if (state >= CC_First || !g_TreeItemStateChangeNotificationSink)
		return;

	TTreeItemStateChangeNotificationSinkContainer::const_iterator
		i = g_TreeItemStateChangeNotificationSink->lower_bound(TTreeItemStateChangeNotificationSink(item, TStateChangeNotificationSink(nullptr,ClientHandle()))),
		e = g_TreeItemStateChangeNotificationSink->end();
	while (g_TreeItemStateChangeNotificationSink && i != e && i->first == item)
	{
		ClientHandle clientHandle = i->second.second;
		(i++)->second.first(clientHandle, item, NotificationCode(state));
	}
}

//----------------------------------------------------------------------
// C-API implementation
//----------------------------------------------------------------------

#include "TicInterface.h"

extern "C" TIC_CALL void DMS_CONV DMS_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		if (!g_StateChangeNotifications) 
			g_StateChangeNotifications.assign( new TStateChangeNotificationSinkContainer );
		dms_assert(g_StateChangeNotifications);
		g_StateChangeNotifications->insert(TStateChangeNotificationSink(fcb, clientHandle));

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_CONV DMS_ReleaseStateChangeNotification(TStateChangeNotificationFunc fcb, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		// Release may be called several times to accomodate various exit paths for client
		if(!g_StateChangeNotifications)
			return;

		g_StateChangeNotifications->erase(TStateChangeNotificationSink(fcb, clientHandle));
		if (g_StateChangeNotifications->empty())
			g_StateChangeNotifications.reset();

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_CONV DMS_TreeItem_RegisterStateChangeNotification(TStateChangeNotificationFunc fcb, const TreeItem* self, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, nullptr, "DMS_TreeItem_RegisterStateChangeNotification");
		if (!g_TreeItemStateChangeNotificationSink) 
			g_TreeItemStateChangeNotificationSink.assign( new TTreeItemStateChangeNotificationSinkContainer );

		assert(g_TreeItemStateChangeNotificationSink);

		g_TreeItemStateChangeNotificationSink->insert(
				TTreeItemStateChangeNotificationSink(self, TStateChangeNotificationSink(fcb, clientHandle))
		);

	DMS_CALL_END
}


extern "C" TIC_CALL void DMS_CONV DMS_TreeItem_ReleaseStateChangeNotification (TStateChangeNotificationFunc fcb, const TreeItem* self, ClientHandle clientHandle)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, nullptr, "DMS_TreeItem_ReleaseStateChangeNotification");
		dms_assert(g_TreeItemStateChangeNotificationSink);
		g_TreeItemStateChangeNotificationSink->erase(TTreeItemStateChangeNotificationSink(self, TStateChangeNotificationSink(fcb, clientHandle)));

		if (g_TreeItemStateChangeNotificationSink->empty())
			g_TreeItemStateChangeNotificationSink.reset();

	DMS_CALL_END
}

