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
// GlobalLockHandle
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


//----------------------------------------------------------------------
// class  : ClipBoard
//----------------------------------------------------------------------

struct ClipBoard
{
	ClipBoard(bool wait = true);
	~ClipBoard();

	bool IsOpen() const { return m_Success; }

	SharedStr GetText() const;

	void Clear();
	void SetDIB   (HBITMAP hBitmap);
	void SetBitmap(HBITMAP hBitmap);
	void SetText(CharPtr text);
	void SetText(CharPtr textBegin, CharPtr textEnd);
	void AddText(CharPtr text);
	void AddTextLine(CharPtr text);

	void SetData(UINT uFormat, const GlobalLockHandle& data );
	void SetData(UINT uFormat, CharPtr begin, CharPtr end);

	HANDLE GetData(UINT uFormat) const;

	static void ClearBuff();
	static UINT GetCurrFormat();

private:
	bool m_Success;
};



#endif // __SHV_CLIPBOARD_H

