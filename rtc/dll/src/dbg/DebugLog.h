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

#ifndef __RTC_DBG_DEBUGLOG_H
#define __RTC_DBG_DEBUGLOG_H

#include "DbgInterface.h"
#include "ser/FileStreamBuff.h"
#include "ser/FormattedStream.h"


// *****************************************************************************
// CDebugLog
// *****************************************************************************

class CDebugLog {
public:
	RTC_CALL CDebugLog(WeakStr name);
	RTC_CALL ~CDebugLog();

private:
	static void DMS_CONV DebugMsgCallback(ClientHandle clientHandle, const MsgData* msgData);

	FileOutStreamBuff   m_FileBuff;
	FormattedOutStream  m_Stream;
};

#endif  // __RTC_DBG_DEBUGLOG_H
