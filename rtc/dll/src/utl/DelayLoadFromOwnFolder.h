// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __RTC_UTL_DELAYLOADFROMOWNFOLDER_H
#define __RTC_UTL_DELAYLOADFROMOWNFOLDER_H

// The delay-load hook of a DLL that delay-loads another DLL: Stg delay-loads GDAL (DelayLoadDLLs
// in its .vcxproj), and Clc and Geo reach GDAL only through Stg (#1381). Include this header in
// exactly one source file of each such DLL; it defines that DLL's __pfnDliNotifyHook2.
//
// The hook loads a delay-loaded DLL from the folder of the DLL that imports it, when the file is
// there, instead of through the default search order of the delay-load helper (the folder of the
// exe, System32, the current folder, PATH). For GeoDmsGuiQt and GeoDmsRun, which live in that
// folder, nothing changes. It matters when the exe lives elsewhere: python.exe loading the binding
// from a setup folder that is only on sys.path, and a wheel, where delvewheel put the DLLs in
// geodms.libs under hashed names and patched the delay-import names to match (#1355). The file is
// loaded by its full path with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR, so the DLLs it imports in turn
// are also looked for in that folder first. When the file is not there, or does not load, the hook
// returns nullptr and the helper falls back to its default search.

#if defined(_MSC_VER)

#include <windows.h>
#include <delayimp.h>

#include <string>

namespace delay_load_from_own_folder
{
	namespace
	{
		const char s_anchor = 0; // an address inside the DLL that includes this header

		FARPROC WINAPI Hook(unsigned dliNotify, PDelayLoadInfo pdli)
		{
			if (dliNotify != dliNotePreLoadLibrary || !pdli || !pdli->szDll)
				return nullptr;

			HMODULE self = nullptr;
			if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&s_anchor), &self))
				return nullptr;

			std::wstring path(32768, L'\0'); // the longest path Windows accepts
			const DWORD length = GetModuleFileNameW(self, path.data(), static_cast<DWORD>(path.size()));
			if (!length || length >= path.size())
				return nullptr;
			path.resize(length);
			const auto slash = path.find_last_of(L"\\/");
			if (slash == std::wstring::npos)
				return nullptr;
			path.resize(slash + 1);
			for (const char* c = pdli->szDll; *c; ++c) // the names in an import table are ASCII
				path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));

			if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
				return nullptr;
			return reinterpret_cast<FARPROC>(LoadLibraryExW(path.c_str(), nullptr,
				LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS));
		}
	}
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = delay_load_from_own_folder::Hook;

#endif // defined(_MSC_VER)

#endif // __RTC_UTL_DELAYLOADFROMOWNFOLDER_H
