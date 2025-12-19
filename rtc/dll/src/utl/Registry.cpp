// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "utl/Registry.h"
#include "utl/Environment.h"

#if defined(WIN32)
#include <windows.h>

//  -----------------------------------------------------------------------

bool IsRelativeKey(CharPtr key)
{
	assert(key && *key != '/');
	return *key && *key != '\\';
}

HKEY OpenKeyReadOnly(HKEY baseKey, CharPtr key)
{
	assert(IsRelativeKey(key));
	HKEY tempKey = 0;
	auto keyW = Utf8_2_wchar(key);
	RegOpenKeyExW(baseKey, keyW.get(), 0, KEY_READ, &tempKey);
	return tempKey;
}

HKEY OpenKey(HKEY baseKey, CharPtr key)
{
	assert(IsRelativeKey(key));
	HKEY tempKey = 0;

	auto keyW = Utf8_2_wchar(key);
	RegCreateKeyExW(baseKey, keyW.get(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &tempKey, NULL);

	return tempKey;
}

RegDataType DataTypeToRegDataImpl(DWORD dataType)
{
	switch (dataType)
	{
		case REG_SZ:        return RegDataType::String;
		case REG_EXPAND_SZ: return RegDataType::ExpandString;
		case REG_BINARY:    return RegDataType::Binary;
		case REG_DWORD:     return RegDataType::DWORD;
		case REG_MULTI_SZ:  return RegDataType::MultiString;
	}
	return RegDataType::Unknown;
}

RegDataType DataTypeToRegData(DWORD dataType)
{
	RegDataType result = DataTypeToRegDataImpl(dataType);
	assert(static_cast<DWORD>(result) == dataType);
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
	auto nameW = Utf8_2_wchar(name);
	DWORD dataType;
	return RegQueryValueExW(m_Key, nameW.get(), NULL, &dataType, NULL, NULL) == ERROR_SUCCESS;
}

UInt32 RegistryHandle::GetDataSize(CharPtr name) const
{
	auto nameW = Utf8_2_wchar(name);
	DWORD dataSize;
	if (RegQueryValueExW(m_Key, nameW.get(), NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS)
		throwLastSystemError("GetDataSize failed for RegKey '%s'", name);
	return dataSize;
}

RegDataType RegistryHandle::GetDataType(CharPtr name) const
{
	DWORD dataType;
	auto nameW = Utf8_2_wchar(name);
	if (RegQueryValueExW(m_Key, nameW.get(), NULL, &dataType, NULL, NULL) != ERROR_SUCCESS)
		throwLastSystemError("GetDataType failed for RegKey '%s'", name);
	return DataTypeToRegData(dataType);
}

UInt32 RegistryHandle::GetDataW(CharPtr name, BYTE* buffer, DWORD bufSize, RegDataType& regDataType) const
{
	DWORD dataType = REG_NONE;
	auto nameW = Utf8_2_wchar(name);
	if (RegQueryValueExW(m_Key, nameW.get(), NULL, &dataType, buffer, &bufSize) != ERROR_SUCCESS)
		throwLastSystemError("GetData failed for RegKey '%s'", name);
	regDataType = DataTypeToRegData(dataType);
	return bufSize;
}

/*
UInt32 RegistryHandle::GetData(CharPtr name, std::vector<BYTE> &buffer, DWORD bufSize, RegDataType& regDataType) const
{
	auto nameW = Utf8_2_wchar(name);
	auto retCode = ::RegGetValueW(m_Key, NULL, nameW.get(), RRF_RT_REG_MULTI_SZ, nullptr, &buffer[0], &bufSize);
	return bufSize;
}
*/

SharedStr RegistryHandle::ReadString(CharPtr name) const
{
	auto len = GetDataSize(name);
	if (!len)
		return SharedStr();

	auto nr_wchars = (len + sizeof(wchar_t) - 1) / sizeof(wchar_t);
	auto wcharResult = std::make_unique<wchar_t[]>(nr_wchars);

	RegDataType regDataType;
    GetDataW(name, reinterpret_cast<BYTE*>(wcharResult.get()), len, regDataType);
	if ((regDataType != RegDataType::String) && (regDataType != RegDataType::ExpandString))
		throwErrorF("RegistryHandle.ReadString", "key '%s' has a non string type", name);

	auto utf8Str = wchar_2_Utf8(wcharResult.get(), nr_wchars);
	return SharedStr(utf8Str.get());
}

void RegistryHandle::DeleteValue(CharPtr name) const
{
	RegDeleteValue(m_Key, name);
}


void RegistryHandle::WriteString(CharPtr name, CharPtrRange str) const
{
	auto strW = Utf8_2_wchar(str.begin(), static_cast<int>(str.size()));
	auto nameW = Utf8_2_wchar(name);
	RegSetValueExW(m_Key, nameW.get(), NULL, REG_SZ, reinterpret_cast<const BYTE*>(strW.get()), std::wcslen(strW.get()) * sizeof(wchar_t));
}

auto RegistryHandle::ReadMultiString(CharPtr name) const -> std::vector<SharedStr>
{
	// get registry entry as byte array
	RegDataType regDataType;
	DWORD len = GetDataSize(name);
	if (!len)
		return {};

	auto nr_wchars = (len + sizeof(wchar_t) - 1) / sizeof(wchar_t);
	auto wcharResult = std::make_unique<wchar_t[]>(nr_wchars);

	GetDataW(name, reinterpret_cast<BYTE*>(wcharResult.get()), len, regDataType);
	if (regDataType != RegDataType::MultiString)
		throwErrorF("RegistryHandle.ReadMultiString", "key '%s' has a non multi string type", name);

	// separate strings
	std::vector<SharedStr> result;
	std::wstring wcharWord;
	for (auto* wcPtr = wcharResult.get(); nr_wchars; ++wcPtr, --nr_wchars)
	{
		auto wc = *wcPtr;
		if (wc==L'\0' && not wcharWord.empty())
		{
			auto word = wchar_2_Utf8(wcharWord.c_str(), wcharWord.size());
			result.emplace_back(word.get());
			wcharWord.clear();
			continue;
		}
		wcharWord += wc;
	}
	return result;
}

 auto PackVectorStringAsVectorBytes(const std::vector<SharedStr>& strings) -> std::vector<wchar_t>
{
	std::vector<wchar_t> result;
	for (const auto& s : strings)
	{
		auto ws = Utf8_2_wchar(s.c_str());
		result.insert(result.end(), ws.get(), ws.get() + std::wcslen(ws.get()));
		result.emplace_back(L'\0');
	}
	result.emplace_back(L'\0');
	return result;
}

bool RegistryHandle::WriteMultiString(CharPtr name, const std::vector<SharedStr>& strings) const
{
	auto nameW = Utf8_2_wchar(name);
	auto reg_value = PackVectorStringAsVectorBytes(strings);
	RegSetValueExW(m_Key, nameW.get(), NULL, REG_MULTI_SZ, reinterpret_cast<BYTE*>(begin_ptr(reg_value)), reg_value.size() * sizeof(wchar_t));
	return true;
}
  
DWORD RegistryHandle::ReadDWORD(CharPtr name) const
{
	RegDataType regDataType;
	DWORD       regData;
    if ((GetDataW(name, reinterpret_cast<BYTE*>(&regData), sizeof(DWORD), regDataType) != sizeof(DWORD)) || (regDataType != RegDataType::DWORD))
		throwErrorF("RegistryHandle.ReadDWORD", "key '%s' has a non DWORD type", name);
	return regData;
}

bool RegistryHandle::WriteDWORD(CharPtr name, DWORD dw) const
{
	auto nameW = Utf8_2_wchar(name);
	RegSetValueExW(m_Key, nameW.get(), NULL, REG_DWORD, (const BYTE*)&dw, sizeof(dw));
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

#endif //defined(WIN32)

