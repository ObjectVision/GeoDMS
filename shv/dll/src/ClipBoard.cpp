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

#include "ShvDllPch.h"

#include "geo/StringBounds.h"

#include "ClipBoard.h"

static UInt32    s_ClipBoardOpenCount = 0;
static SharedStr s_ClipBoardBuff;

//----------------------------------------------------------------------
// GlobalLockHandle
//----------------------------------------------------------------------

GlobalLockHandle::GlobalLockHandle(HANDLE hMem)
	:	m_hMem(hMem)
	,	m_DataPtr(GlobalLock(hMem))
{
}

HGLOBAL CheckedGlobalAlloc(_In_ UINT uFlags,
	_In_ SIZE_T dwBytes
)
{
	auto result = GlobalAlloc(uFlags, dwBytes);
	if (!result)
		throwLastSystemError("CheckedGlobalAlloc(%s, %s bytes)", uFlags, dwBytes);
	return result;
}


GlobalLockHandle::GlobalLockHandle(UInt32 size, UInt32 uFlags)
	:	m_hMem(CheckedGlobalAlloc(uFlags, size))
	,	m_DataPtr(GlobalLock(m_hMem))
{
}

GlobalLockHandle::~GlobalLockHandle()
{
	GlobalUnlock(m_hMem);
}

//----------------------------------------------------------------------
// class  : ClipBoard
//----------------------------------------------------------------------

bool LockClipBoard(bool wait)
{
	do {
		if (OpenClipboard(NULL))
			return true;
		if (!wait)
			return false;
		SleepEx(50, true);
	} while (true);
}

ClipBoard::ClipBoard(bool wait) 
{ 
	m_Success = s_ClipBoardOpenCount || LockClipBoard(wait); 
	if (m_Success) 
		++s_ClipBoardOpenCount;
	else
		MessageBeep(-1);
	dms_assert(m_Success || !wait);
}

ClipBoard::~ClipBoard() 
{ 
	if (m_Success)
	{
		if (!--s_ClipBoardOpenCount)
			CloseClipboard(); 
	}
}


void ClipBoard::Clear()
{
	dms_assert(m_Success);
	CheckedGdiCall(EmptyClipboard(), "EmptyClipboard");
}

SharedStr ClipBoard::GetText() const
{
	void* handle = GetClipboardData(CF_TEXT);
	if (handle)
		return SharedStr( reinterpret_cast<CharPtr>( handle ) );
	return SharedStr();
}

void ClipBoard::SetText(CharPtr text)
{
	SetText(text, text+StrLen(text));
}

bool IsUtf8TrailingByte(char x)
{
	return (x & 0xC0) == 0x80; // see https://en.wikipedia.org/wiki/UTF-8#Encoding 00 or 40 results are normal 7 bits utf8 ascii characters; C0
}

SizeT UTF8ToWideChar(CharPtr utf8Begin, CharPtr utf8End, wchar_t* destBuffer, SizeT destSize)
{
	const int MAX_MB2WC_CAPACITY = (1 << 30) - 1;
	SizeT wCharSize = 0;
	while (utf8Begin != utf8End)
	{
		SizeT utf8Size = (utf8End - utf8Begin);
		if (utf8Size > MAX_MB2WC_CAPACITY)
		{
			utf8Size = MAX_MB2WC_CAPACITY;
			while (IsUtf8TrailingByte(utf8Begin[utf8Size]))
			{
				utf8Size--;
				if (utf8Size < MAX_MB2WC_CAPACITY - 4)
					throwErrorF("UTF8ToWideChar", "Unexpected sequence of non-leading UTF8 bytes");
			}
		}
		assert(utf8Size < std::numeric_limits<int>::max());
		int destCurrMaxSize = std::numeric_limits<int>::max();
		if (destSize < destCurrMaxSize)
			destCurrMaxSize = destSize;
		int ccWideChar = MultiByteToWideChar(CP_UTF8, 0, utf8Begin, int(utf8Size), destBuffer, destCurrMaxSize);
		if (destBuffer != nullptr)
		{
			assert(ccWideChar <= destSize);
			destBuffer += ccWideChar;
			destSize -= ccWideChar;
		}
		utf8Begin += utf8Size;
		wCharSize += ccWideChar;
	}
	assert(destSize == 0);
	return wCharSize;
}

void ClipBoard::SetText(CharPtr utf8Begin, CharPtr utf8End)
{
	Clear();

	dms_assert(m_Success);
	SizeT textSize = (utf8End - utf8Begin);
	MakeMin(textSize, MAX_VALUE(int));

	auto ccWideChar = UTF8ToWideChar(utf8Begin, utf8End, nullptr, 0);
	GlobalLockHandle data( GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, (ccWideChar + 1)*sizeof(wchar_t)) );
	LPWSTR unicodeText = reinterpret_cast<wchar_t*>(data.GetDataPtr());
	UTF8ToWideChar(utf8Begin, utf8End, unicodeText, ccWideChar);
	unicodeText[ccWideChar] = 0;

	SetClipboardData(CF_UNICODETEXT, data.GetHandle());
}

void ClipBoard::SetData(UINT uFormat, const GlobalLockHandle& data )
{
	dms_assert(m_Success);
	Clear();

	SetClipboardData(uFormat, data.GetHandle());
}

HANDLE ClipBoard::GetData(UINT uFormat) const
{
	dms_assert(m_Success);

	return GetClipboardData(uFormat);
}

void ClipBoard::SetData(UINT uFormat, CharPtr begin, CharPtr end)
{
	Clear();

	UInt32 dataSize = end - begin;

	GlobalLockHandle data( GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, dataSize) );
	
	memcpy(data.GetDataPtr(), begin, dataSize);

	ClipBoard::SetData(uFormat, data);
}

void ClipBoard::SetDIB(HBITMAP hBitmap)
{
	Clear();
	dms_assert(m_Success);
	SetClipboardData(CF_DIB, hBitmap);
}

void ClipBoard::SetBitmap(HBITMAP hBitmap)
{
	Clear();
	dms_assert(m_Success);
	SetClipboardData(CF_BITMAP, hBitmap);
}

void ClipBoard::AddText(CharPtr text)
{
	dms_assert(m_Success);

	s_ClipBoardBuff += text;
	SetText( s_ClipBoardBuff.c_str() );
}


void ClipBoard::AddTextLine(CharPtr text)
{
	dms_assert(m_Success);

	s_ClipBoardBuff += text;
	s_ClipBoardBuff += "\n";
	SetText( s_ClipBoardBuff.c_str() );
}


void ClipBoard::ClearBuff()
{
	s_ClipBoardBuff = SharedStr();
}

UINT ClipBoard::GetCurrFormat()
{
	ClipBoard xxx(false);
	if (!xxx.IsOpen())
		return 0;
	return EnumClipboardFormats(0);

}
