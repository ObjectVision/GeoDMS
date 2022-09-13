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

#ifndef __SER__STREAMBUFFINTERFACE_H
#define __SER__STREAMBUFFINTERFACE_H

#include "RtcBase.h"

//----------------------------------------------------------------------
// Creating and reading OutStreamBuff's
//----------------------------------------------------------------------

extern "C" {

	RTC_CALL VectorOutStreamBuff* DMS_CONV DMS_VectOutStreamBuff_Create();
	RTC_CALL FileOutStreamBuff*   DMS_CONV DMS_FileOutStreamBuff_Create(CharPtr fileName, bool isAsciiFile);

	RTC_CALL void DMS_CONV DMS_OutStreamBuff_Destroy(OutStreamBuff* );
	RTC_CALL void DMS_CONV DMS_OutStreamBuff_WriteBytes(OutStreamBuff* self, const Byte* source, streamsize_t sourceSize);
	RTC_CALL void DMS_CONV DMS_OutStreamBuff_WriteChars(OutStreamBuff* self, CharPtr source);

	// note: apply the following function only on VectOutstreamBuffs
	// note: the returning string might not be null-terminated; use CurrPos as length-indicator to avoid 'GPF'
	RTC_CALL CharPtr DMS_CONV DMS_VectOutStreamBuff_GetData(VectorOutStreamBuff* self);
	RTC_CALL streamsize_t DMS_CONV DMS_OutStreamBuff_CurrPos(OutStreamBuff* self);

} // extern "C"

#endif // __SER__STREAMBUFFINTERFACE_H
