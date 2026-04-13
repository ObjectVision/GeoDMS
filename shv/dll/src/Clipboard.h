// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_CLIPBOARD_H)
#define __SHV_CLIPBOARD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

//----------------------------------------------------------------------
// class  : ClipBoard (portable text interface via QClipboard)
//----------------------------------------------------------------------

struct ClipBoard
{
	ClipBoard(bool wait = true);
	~ClipBoard();

	bool IsOpen() const { return m_Success; }

	SharedStr GetText() const;

	void Clear();
	void SetText(CharPtr text);
	void SetText(CharPtr textBegin, CharPtr textEnd);
	void AddText(CharPtr text);
	void AddTextLine(CharPtr text);

	static void ClearBuff();

#ifdef _WIN32
	// Win32-only: bitmap and raw data clipboard operations
	void SetDIB   (HBITMAP hBitmap);
	void SetBitmap(HBITMAP hBitmap);

	void SetData(UINT uFormat, CharPtr begin, CharPtr end);
	HANDLE GetData(UINT uFormat) const;
	static UINT GetCurrFormat();
#endif // _WIN32

private:
	bool m_Success;
};

#ifdef _WIN32
//----------------------------------------------------------------------
// GlobalLockHandle (Win32-only: GlobalAlloc/GlobalLock RAII wrapper)
//----------------------------------------------------------------------

struct GlobalLockHandle
{
	GlobalLockHandle(HANDLE hMem);
	GlobalLockHandle(UInt32 size, UInt32 uFlags = GMEM_MOVEABLE|GMEM_DDESHARE);

	~GlobalLockHandle();

	LPVOID GetDataPtr() const { return m_DataPtr; }
	HANDLE GetHandle()  const { return m_hMem;    }

	HANDLE m_hMem;
	LPVOID m_DataPtr;
};

void ClipBoard_SetData(ClipBoard& cb, UINT uFormat, const GlobalLockHandle& data);
#endif // _WIN32


#endif // __SHV_CLIPBOARD_H

