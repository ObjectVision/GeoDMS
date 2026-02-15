// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_INTERFACE_H)
#define __RTC_INTERFACE_H

#include "RtcBase.h"

//----------------------------------------------------------------------
// C style Interface functions for Exception handling
//----------------------------------------------------------------------

extern "C" {

RTC_CALL CharPtr DMS_CONV DMS_GetLastErrorMsg();

RTC_CALL void DMS_CONV DMS_SetGlobalCppExceptionTranslator(TCppExceptionTranslator trFunc);

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

RTC_CALL void DMS_CONV DMS_Rtc_Load();
RTC_CALL bool DMS_CONV DMS_RTC_Test();

} // end extern "C"

RTC_CALL auto ReportChangedFiles() -> VectorOutStreamBuff;
RTC_CALL void ReportCurrentConfigFileList(OutStreamBase& os);

RTC_CALL extern bool g_IsTerminating;

#endif // __RTC_INTERFACE_H
