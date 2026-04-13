// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/StringBounds.h"
#include "ptr/SharedStr.h"
#include "utl/Environment.h"

#include "ClipBoard.h"
#include "ShvUtils.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

static SharedStr s_ClipBoardBuff;

//----------------------------------------------------------------------
// class  : ClipBoard (portable text via QClipboard)
//----------------------------------------------------------------------

ClipBoard::ClipBoard(bool wait)
	: m_Success(QGuiApplication::instance() != nullptr)
{
}

ClipBoard::~ClipBoard()
{
}

void ClipBoard::Clear()
{
	dms_assert(m_Success);
	auto* cb = QGuiApplication::clipboard();
	if (cb) cb->clear();
}

SharedStr ClipBoard::GetText() const
{
	auto* cb = QGuiApplication::clipboard();
	if (!cb) return SharedStr();
	QByteArray utf8 = cb->text().toUtf8();
	return SharedStr(CharPtrRange(utf8.constData(), utf8.constData() + utf8.size()));
}

void ClipBoard::SetText(CharPtr text)
{
	SetText(text, text + StrLen(text));
}

void ClipBoard::SetText(CharPtr utf8Begin, CharPtr utf8End)
{
	dms_assert(m_Success);
	auto* cb = QGuiApplication::clipboard();
	if (!cb) return;
	cb->setText(QString::fromUtf8(utf8Begin, utf8End - utf8Begin));
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

//----------------------------------------------------------------------
// Win32-only: GlobalLockHandle, bitmap and raw data clipboard
//----------------------------------------------------------------------

#ifdef _WIN32

GlobalLockHandle::GlobalLockHandle(HANDLE hMem)
	:	m_hMem(hMem)
	,	m_DataPtr(GlobalLock(hMem))
{
}

static HGLOBAL CheckedGlobalAlloc(UINT uFlags, SIZE_T dwBytes)
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

static bool LockWin32Clipboard(bool wait)
{
	do {
		if (OpenClipboard(NULL))
			return true;
		if (!wait)
			return false;
		SleepEx(50, true);
	} while (true);
}

void ClipBoard::SetDIB(HBITMAP hBitmap)
{
	dms_assert(m_Success);
	if (!LockWin32Clipboard(false)) return;
	EmptyClipboard();
	SetClipboardData(CF_DIB, hBitmap);
	CloseClipboard();
}

void ClipBoard::SetBitmap(HBITMAP hBitmap)
{
	dms_assert(m_Success);
	if (!LockWin32Clipboard(false)) return;
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, hBitmap);
	CloseClipboard();
}

void ClipBoard_SetData(ClipBoard& cb, UINT uFormat, const GlobalLockHandle& data)
{
	dms_assert(cb.IsOpen());
	if (!LockWin32Clipboard(false)) return;
	EmptyClipboard();
	SetClipboardData(uFormat, data.GetHandle());
	CloseClipboard();
}

void ClipBoard::SetData(UINT uFormat, CharPtr begin, CharPtr end)
{
	UInt32 dataSize = end - begin;
	GlobalLockHandle data( GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, dataSize) );
	memcpy(data.GetDataPtr(), begin, dataSize);
	ClipBoard_SetData(*this, uFormat, data);
}

HANDLE ClipBoard::GetData(UINT uFormat) const
{
	dms_assert(m_Success);
	if (!LockWin32Clipboard(false)) return nullptr;
	HANDLE result = GetClipboardData(uFormat);
	CloseClipboard();
	return result;
}

UINT ClipBoard::GetCurrFormat()
{
	if (!OpenClipboard(NULL))
		return 0;
	UINT result = EnumClipboardFormats(0);
	CloseClipboard();
	return result;
}

#endif // _WIN32
