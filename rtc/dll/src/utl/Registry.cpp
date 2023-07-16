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

#include "RtcPCH.h"
#pragma hdrstop

#include "act/MainThread.h"
#include "utl/Registry.h"
#include "utl/Environment.h"

#include <windows.h>

//  -----------------------------------------------------------------------

bool IsRelativeKey(CharPtr key)
{
	dms_assert(key && *key != '/');
	return *key && *key != '\\';
}

HKEY OpenKeyReadOnly(HKEY baseKey, CharPtr key)
{
	dms_assert(IsRelativeKey(key));
	HKEY tempKey = 0;
	RegOpenKeyEx(baseKey, key, 0, KEY_READ, &tempKey);
	return tempKey;
}

HKEY OpenKey(HKEY baseKey, CharPtr key)
{
	dms_assert(IsRelativeKey(key));
	HKEY tempKey = 0;

	RegCreateKeyEx(baseKey, key, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &tempKey, NULL);

	return tempKey;
}

RegDataType DataTypeToRegDataImpl(UInt32 dataType)
{
	switch (dataType)
	{
		case REG_SZ:        return rdString;
		case REG_EXPAND_SZ: return rdExpandString;
		case REG_BINARY:    return rdBinary;
		case REG_DWORD:     return rdDWORD;
		case REG_MULTI_SZ:  return rdMultiString;
	}
	return rdUnknown;
}

RegDataType DataTypeToRegData(UInt32 dataType)
{
	RegDataType result = DataTypeToRegDataImpl(dataType);
	dms_assert(result == dataType);
	return result;
}

//  -----------------------------------------------------------------------

RegistryHandle::RegistryHandle(HKEY key)
	:	m_Key(key)
{}

RegistryHandle::~RegistryHandle()
{
	RegCloseKey(m_Key);
}

bool RegistryHandle::ValueExists(CharPtr name) const
{
	DWORD dataType;
	return RegQueryValueEx(m_Key, name, NULL, &dataType, NULL, NULL) == ERROR_SUCCESS;
}

UInt32 RegistryHandle::GetDataSize(CharPtr name) const
{
	DWORD dataSize;
	if (RegQueryValueEx(m_Key, name, NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS)
		throwLastSystemError("GetDataSize failed for RegKey '%s'", name);
	return dataSize;
}

RegDataType RegistryHandle::GetDataType(CharPtr name) const
{
	DWORD dataType;
	if (RegQueryValueEx(m_Key, name, NULL, &dataType, NULL, NULL) != ERROR_SUCCESS)
		throwLastSystemError("GetDataType failed for RegKey '%s'", name);
	return DataTypeToRegData(dataType);
}

UInt32 RegistryHandle::GetData(CharPtr name, BYTE* buffer, DWORD bufSize, RegDataType& regDataType) const
{
	DWORD dataType = REG_NONE;
	if (RegQueryValueEx(m_Key, name, NULL, &dataType, buffer, &bufSize) != ERROR_SUCCESS)
		throwLastSystemError("GetData failed for RegKey '%s'", name);
	regDataType = DataTypeToRegData(dataType);
	return bufSize;
}

UInt32 RegistryHandle::GetData(CharPtr name, std::vector<BYTE> &buffer, DWORD bufSize, RegDataType& regDataType) const
{
	auto retCode = ::RegGetValue(m_Key, NULL, name, RRF_RT_REG_MULTI_SZ, nullptr, &buffer[0], &bufSize);
	return bufSize;
}

SharedStr RegistryHandle::ReadString(CharPtr name) const
{
	UInt32 len = GetDataSize(name);
	if (!len)
		return SharedStr();
	SharedCharArray* result = SharedCharArray::CreateUninitialized(len);
	SharedStr resultStr = SharedStr( result );

	RegDataType regDataType;
    GetData(name, reinterpret_cast<BYTE*>(result->begin()), len, regDataType);
    if ((regDataType != rdString) && (regDataType != rdExpandString))
		throwErrorF("RegistryHandle.ReadString", "key '%s' has a non string type", name);
	dms_assert(result->back() == char(0));
	result->back() = char(0);
	return resultStr;
}

void RegistryHandle::WriteString(CharPtr name, std::string str) const
{
	RegSetValueEx(m_Key, name, NULL, REG_SZ, (const BYTE*)str.data(), str.size());
}

void RegistryHandle::DeleteValue(CharPtr name) const
{
	RegDeleteValue(m_Key, name);
}


void RegistryHandle::WriteString(CharPtr name, CharPtrRange str) const
{
	RegSetValueEx(m_Key, name, NULL, REG_SZ, (const BYTE*)str.begin(), str.size());
}

std::vector<std::string> RegistryHandle::ReadMultiString(CharPtr name) const
{
	// get registry entry as byte array
	RegDataType regDataType;
	DWORD len = GetDataSize(name);
	std::vector<BYTE> buf;
	buf.resize(len/sizeof(BYTE));
	GetData(name, buf, len, regDataType);

	// separate strings
	std::vector<std::string> result;
	std::string word;
	for (auto& c : buf)
	{
		if (c=='\0' && not word.empty())
		{
			result.push_back(word);
			word.clear();
			continue;
		}
		word+=c;
	}
	return result;
}

std::vector<BYTE> RegistryHandle::PackVectorStringAsVectorBytes(std::vector<std::string> strings) const
{
	std::vector<BYTE> result;
	for (auto& s : strings)
	{
		for (auto& c : s)
		{
			result.push_back(c);
		}
		result.push_back('\0');
	}
	result.push_back('\0');
	return result;
}

bool RegistryHandle::WriteMultiString(CharPtr name, std::vector<std::string> strings) const
{
	auto reg_value = PackVectorStringAsVectorBytes(strings);
	RegSetValueEx(m_Key, name, NULL, REG_MULTI_SZ, &reg_value[0], reg_value.size());
	return true;
}
  
DWORD RegistryHandle::ReadDWORD(CharPtr name) const
{
	RegDataType regDataType;
	DWORD       regData;
    if ((GetData(name, reinterpret_cast<BYTE*>(&regData), sizeof(DWORD), regDataType) != sizeof(DWORD)) || (regDataType != rdDWORD))
		throwErrorF("RegistryHandle.ReadDWORD", "key '%s' has a non DWORD type", name);
	return regData;
}

bool RegistryHandle::WriteDWORD(CharPtr name, DWORD dw) const
{
	RegSetValueEx(m_Key, name, NULL, REG_DWORD, (const BYTE*)& dw, sizeof(dw));
	return true;
}

//  -----------------------------------------------------------------------

RegistryHandleCurrentUserRO::RegistryHandleCurrentUserRO()
	: RegistryHandle(OpenKeyReadOnly(HKEY_CURRENT_USER, "Software\\ObjectVision\\DMS"))
{}

SharedStr GetLocalMachinePath(CharPtr section)
{
	static SharedStr s_Result = "Software\\ObjectVision\\" + PlatformInfo::GetComputerNameA() + "\\GeoDMS";
	if (!section || !*section)
		return s_Result;
	return s_Result + "\\" + section;
}


RegistryHandleLocalMachineRO::RegistryHandleLocalMachineRO(CharPtr section)
	: RegistryHandle(OpenKeyReadOnly(HKEY_CURRENT_USER, GetLocalMachinePath(section).c_str()))
{}

//  -----------------------------------------------------------------------

RegistryHandleLocalMachineRW::RegistryHandleLocalMachineRW(CharPtr section)
	: RegistryHandle(OpenKey(HKEY_CURRENT_USER, GetLocalMachinePath(section).c_str()))
{}


