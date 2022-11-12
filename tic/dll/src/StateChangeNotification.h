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

#if !defined(__TIC_STATECHANGENOTIFICATION_H)
#define __TIC_STATECHANGENOTIFICATION_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"
enum NotificationCode {
	NC2_Invalidated    = 0,
	NC2_DetermineChange= 1,
	NC2_DetermineState = 2,
	NC2_InterestChange = 3,
	NC2_MetaReady   = 4,
	NC2_DataReady   = 5,
	NC2_Validated   = 6,
	NC2_Committed   = 7,

	NC2_FailedFlag   = 8,
	NC2_DetermineFailed      = NC2_FailedFlag|NC2_DetermineState,
	NC2_InterestChangeFailed = NC2_FailedFlag|NC2_InterestChange,
	NC2_MetaFailed   = NC2_FailedFlag|NC2_Invalidated,
	NC2_DataFailed   = NC2_FailedFlag|NC2_MetaReady,
	NC2_CheckFailed  = NC2_FailedFlag|NC2_DataReady,
	NC2_CommitFailed = NC2_FailedFlag|NC2_DataReady,

	NC2_Updating          = 16,
	NC2_UpdatingElsewhere = 17,

	// The following states are never assigned to a tree-item, 
	// but are used to notify a StateChange listener of existential events
	NC_Creating =18,     // called from TreeItem constructor, note that GetDynamicClass==TreeItem::GetStaticClass != final type
	NC_Deleting =19,     // called from TreeItem destructor,  note that GetDynamicClass==TreeItem::GetStaticClass != original type
	NC_PropValueChanged  = 20,

	CC_Activate          = 21,
	CC_ShowStatistics    = 22,
	CC_CreateMdiChild    = 23,

	CC_First             = 21,
};

//----------------------------------------------------------------------
CharPtr UpdateStateName(UInt32 nc);
TIC_CALL void NotifyStateChange(const TreeItem* item, UInt32 state);

#endif // __TIC_STATECHANGENOTIFICATION_H
