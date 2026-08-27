// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_SER_FILESTREAMBUFF_H
#define __RTC_SER_FILESTREAMBUFF_H

#include "dbg/Diagnostics.h"
#include "ser/BaseStreamBuff.h"
#include "ptr/SharedStr.h"

#include <fstream>

/********** FileOutStreamBuff Interface **********/

class FileOutStreamBuff : public OutStreamBuff
{
public:
	RTC_CALL FileOutStreamBuff(WeakStr fileName, bool isAsciiFile, bool mustAppend = false);
	RTC_CALL virtual ~FileOutStreamBuff();

	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
	streamsize_t CurrPos() const override;
	bool AtEnd() const override { return false; }
	WeakStr FileName() override;

	// Push what std::ofstream still holds in its own buffer to the OS. Only the destructor did that
	// until now, which is nothing when the process fail-fasts instead of unwinding; see DBG_FlushLogs.
	RTC_CALL void Flush();

	bool IsOpen() const;

private:
	SharedStr      m_FileName;
	std::ofstream  m_ofstream;
	streamsize_t   m_ByteCount = 0;
};

/********** FileInpStreamBuff Interface **********/

class FileInpStreamBuff : public InpStreamBuff
{
public:
	RTC_CALL FileInpStreamBuff(WeakStr fileName, bool isAsciiFile);
	RTC_CALL virtual ~FileInpStreamBuff();

	void ReadBytes (Byte* data, streamsize_t size) const override;
	streamsize_t CurrPos() const override;
	WeakStr FileName() override;
	bool AtEnd() const override { return !m_ifstream; }

	bool IsOpen() const;

private:
	SharedStr             m_FileName;
	// non const access required for read
	mutable std::ifstream m_ifstream;  
	mutable streamsize_t  m_ByteCount;
};

/********** MappedFileInpStreamBuff Interface **********/

#include "set/FileView.h"

class MappedFileInpStreamBuff : public InpStreamBuff
{
public:
	RTC_CALL MappedFileInpStreamBuff(WeakStr fileName, bool throwOnError, bool doRetry);
	virtual ~MappedFileInpStreamBuff();

	void ReadBytes (Byte* data, streamsize_t size) const override;
	streamsize_t CurrPos() const override;
	WeakStr FileName() override;
	bool AtEnd() const override { return m_Curr == m_FileView.DataEnd(); }

	RTC_CALL bool IsOpen() const;

protected:
	SharedStr           m_FileName;

private:
	ConstFileViewHandle m_FileView;
	mutable CharPtr     m_Curr = nullptr;
};

/********** MappedFileOutStreamBuff Interface **********/

class MappedFileOutStreamBuff : public OutStreamBuff
{
public:
	RTC_CALL MappedFileOutStreamBuff(WeakStr fileName, streamsize_t nrBytes);
	virtual ~MappedFileOutStreamBuff();

	void WriteBytes(const Byte* data, streamsize_t size) override;
	streamsize_t CurrPos() const override;
	bool AtEnd() const override { return m_Curr >= m_FileView.DataEnd(); }
	WeakStr FileName() override;

	bool IsOpen() const;

private:
	SharedStr                 m_FileName;
	char*                     m_Curr;
	FileViewHandle            m_FileView;
};

#endif // __RTC_SER_FILESTREAMBUFF_H
