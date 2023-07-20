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

#if !defined(__RTC_INTERFACE_H)
#define __RTC_INTERFACE_H

#include "RtcBase.h"

//----------------------------------------------------------------------
// C style Interface functions for Exception handling
//----------------------------------------------------------------------

extern "C" {

RTC_CALL CharPtr DMS_CONV DMS_GetLastErrorMsg();

RTC_CALL void DMS_CONV DMS_SetGlobalCppExceptionTranslator(TCppExceptionTranslator trFunc);

// The following typedef defines the type TReduceResourcesFunc
// which is a pointer to a function type that takes 
//		clientHandle: a client suppplied DWord to identify a client object that handles the message
// and returns nothing. It is used before calling an external proc to reduce locks etc.
// ODBCStorageManagers subscribe here to close recordsets and database connections.


typedef bool (DMS_CONV *TReduceResourcesFunc)(ClientHandle clientHandle);
RTC_CALL void DMS_CONV DMS_RegisterReduceResourcesFunc(TReduceResourcesFunc fcb, ClientHandle clientHandle);
RTC_CALL void DMS_CONV DMS_ReleaseReduceResourcesFunc (TReduceResourcesFunc fcb, ClientHandle clientHandle);
RTC_CALL bool DMS_CONV DMS_ReduceResources();
RTC_CALL void DMS_CONV DMS_FreeResources();
RTC_CALL void DMS_CONV DMS_Terminate();

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

RTC_CALL const ValueClass*    DMS_CONV DMS_GetValueType(ValueClassID);
RTC_CALL ValueClassID         DMS_CONV DMS_ValueType_GetID(const ValueClass*);

// Helper function: gives the name of a runtime class
RTC_CALL CharPtr DMS_CONV DMS_ValueType_GetName(const ValueClass*);
RTC_CALL Float64 DMS_CONV DMS_ValueType_GetUndefinedValueAsFloat64(const ValueClass*);
RTC_CALL Int32   DMS_CONV DMS_ValueType_GetSize(const ValueClass*);
RTC_CALL UInt8   DMS_CONV DMS_ValueType_NrDims(const ValueClass*);
RTC_CALL bool    DMS_CONV DMS_ValueType_IsNumeric(const ValueClass*);
RTC_CALL bool    DMS_CONV DMS_ValueType_IsIntegral(const ValueClass*);
RTC_CALL bool    DMS_CONV DMS_ValueType_IsSigned(const ValueClass*);
RTC_CALL bool    DMS_CONV DMS_ValueType_IsSequence(const ValueClass*);
RTC_CALL const ValueClass* DMS_CONV DMS_ValueType_GetCrdClass  (const ValueClass*);

RTC_CALL CharPtr DMS_CONV DMS_GetVersion();
RTC_CALL Float64 DMS_CONV DMS_GetVersionNumber();
RTC_CALL UInt32 DMS_CONV DMS_GetMajorVersionNumber();
RTC_CALL UInt32 DMS_CONV DMS_GetMinorVersionNumber();
RTC_CALL UInt32 DMS_CONV DMS_GetPatchNumber();
RTC_CALL CharPtr DMS_CONV DMS_GetPlatform();
RTC_CALL CharPtr DMS_CONV DMS_GetBuildConfig();

typedef void (DMS_CONV *VersionComponentCallbackFunc)(ClientHandle clientHandle, UInt32 componentLevel, CharPtr componentName);

RTC_CALL void DMS_CONV DMS_VisitVersionComponents(ClientHandle clientHandle, VersionComponentCallbackFunc callBack);
RTC_CALL IStringHandle DMS_ReportChangedFiles(bool updateFileTimes);

RTC_CALL void DMS_CONV DMS_Rtc_Load();
RTC_CALL bool DMS_CONV DMS_RTC_Test();

} // end extern "C"

RTC_CALL auto ReportChangedFiles(bool updateFileTimes) -> VectorOutStreamBuff;

RTC_CALL extern bool g_IsTerminating;

#endif // __RTC_INTERFACE_H
