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

#ifndef __ACCESSRUNSQL_H
#define __ACCESSRUNSQL_H

struct DllHandle;

RTC_CALL DllHandle*  RTC_GetDll (CharPtr dllName);
RTC_CALL void*       RTC_GetProc(DllHandle*, CharPtr procName);
RTC_CALL bool        RTC_IsLoaded(const DllHandle*);

RTC_CALL DllHandle*  GetDllChecked(CharPtr dllName);
RTC_CALL void*       GetProcChecked(CharPtr dllName, CharPtr procName);

typedef UInt32 (DMS_CONV* LPFNDLLFUNC0)();
typedef UInt32 (DMS_CONV* LPFNDLLFUNC1)(CharPtr);
typedef UInt32 (DMS_CONV* LPFNDLLFUNC2)(CharPtr, CharPtr);
typedef UInt32 (DMS_CONV* LPFNDLLFUNC3)(CharPtr, CharPtr, CharPtr);

RTC_CALL UInt32 RunDllProc0(CharPtr dllName, CharPtr procName);
RTC_CALL UInt32 RunDllProc1(CharPtr dllName, CharPtr procName, CharPtr arg1);
RTC_CALL UInt32 RunDllProc2(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2);
RTC_CALL UInt32 RunDllProc3(CharPtr dllName, CharPtr procName, CharPtr arg1, CharPtr arg2, CharPtr arg3);

// Example calls
inline void RunAccessSql(CharPtr dbName, CharPtr sqlName, CharPtr tblName)
{
	RunDllProc3("AccessRun.dll", "RunSQLAndWait", dbName, sqlName, tblName);
}

inline void RunAccessProc(CharPtr dbName, CharPtr procName)
{
	RunDllProc2("AccessRun.dll", "RunProcAndWait", dbName, procName);
}

inline void RunAccessAction(CharPtr dbName, CharPtr action)
{
	RunDllProc2("AccessRun.dll", "RunActionAndWait", dbName, action);
}

inline void RunAccessViewAction(CharPtr dbName, CharPtr action)
{
	RunDllProc2("AccessRun.dll", "RunActionVisible", dbName, action);
}

#endif // __ACCESSRUNSQL_H
