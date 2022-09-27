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

#if !defined(__RTC_UTL_REGISTRY_H)
#define __RTC_UTL_REGISTRY_H

#include "cpc/Types.h"
#include "ptr/OwningPtr.h"

WINDECL_HANDLE(HKEY);
typedef unsigned char BYTE;

enum RegDataType { rdUnknown, rdString, rdExpandString, rdBinary, rdDWORD, rdDWORDBigEndian, rdLINK, rdMultiString};

struct RegistryHandle
{
public:
	bool						ValueExists(CharPtr name) const;
	UInt32						GetDataSize(CharPtr name) const;
	RegDataType					GetDataType(CharPtr name) const;
	UInt32						GetData(CharPtr name, BYTE* buffer, DWORD bufSize, RegDataType& regDataType)  const;
	UInt32						GetData(CharPtr name, std::vector<BYTE>& buffer, DWORD bufSize, RegDataType& regDataType) const;
	SharedStr					ReadString(CharPtr name) const;
	std::vector<std::string>	ReadMultiString(CharPtr name) const;
	bool						WriteMultiString(CharPtr name, std::vector<std::string> strings) const;
	DWORD						ReadDWORD (CharPtr name) const;
	bool						WriteDWORD(CharPtr name, DWORD dw) const;

protected:
	RegistryHandle(HKEY key);
	~RegistryHandle();

private:
	std::vector<BYTE> PackVectorStringAsVectorBytes(std::vector<std::string> strings) const;
	HKEY m_Key;
};


//  -----------------------------------------------------------------------

struct RegistryHandleCurrentUserRO : RegistryHandle
{
	RegistryHandleCurrentUserRO();
};

struct RegistryHandleLocalMachineRO : RegistryHandle
{
	RegistryHandleLocalMachineRO();
};

struct RegistryHandleLocalMachineRW : RegistryHandle
{
	RegistryHandleLocalMachineRW();
};

#endif // __RTC_UTL_REGISTRY_H
