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

#if !defined(__RTC_SER_FILECREATIONMODE_H)
#define __RTC_SER_FILECREATIONMODE_H

//----------------------------------------------------------------------
// dms_rw_mode
//----------------------------------------------------------------------

enum class dms_rw_mode : Int8
{
	read_only  = 0,
	read_write = 1,      // must keep existing data 
	write_only_mustzero = 2, // MustZero contents when Allocating memory storage and/or reopening existing storage
	write_only_all  = 3, // no need to Zero contents since data has been / will be written to.
	unspecified  = -1,
	check_only = -2,
};

inline bool dms_must_keep(dms_rw_mode rwMode) { return rwMode == dms_rw_mode::read_only || rwMode == dms_rw_mode::read_write; }

//----------------------------------------------------------------------
// enumeration type for File Handling as required by SafeFileWriter, FileHandle and FileMapHandle
//----------------------------------------------------------------------

enum FileCreationMode : UInt8
{
	FCM_Undefined,
	FCM_CreateNew,
	FCM_CreateAlways,
	FCM_Delete,
	FCM_OpenRwGrowable,
	FCM_OpenRwFixed,
	FCM_OpenReadOnly,
};

inline bool IsDefined  (FileCreationMode fcm) { return fcm != FCM_Undefined; }
inline bool UseExisting(FileCreationMode fcm) { return fcm >= FCM_OpenRwGrowable; }
inline bool MakeNew    (FileCreationMode fcm) { return fcm <= FCM_CreateAlways && IsDefined(fcm);   }
inline bool IsWritable (FileCreationMode fcm) { return fcm == FCM_OpenRwGrowable || fcm == FCM_OpenRwFixed || MakeNew(fcm);  }

#endif // __RTC_SER_FILECREATIONMODE_H
