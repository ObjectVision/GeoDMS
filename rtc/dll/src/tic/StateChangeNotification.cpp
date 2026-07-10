// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"
#pragma hdrstop

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include <set>
#include <utility>

#include "StateChangeNotification.h"

#include "dbg/DmsCatch.h"
#include "ptr/StaticPtr.h"
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

