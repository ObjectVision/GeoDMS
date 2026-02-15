// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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
// enumeration type for File Handling as required by FileHandle and FileMapHandle
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
