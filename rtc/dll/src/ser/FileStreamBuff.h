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

#ifndef __RTC_SER_FILESTREAMBUFF_H
#define __RTC_SER_FILESTREAMBUFF_H

#include "dbg/Check.h"
#include "ser/BaseStreamBuff.h"
#include "ptr/SharedStr.h"
struct SafeFileWriterArray;

#include <fstream>

/********** FileOutStreamBuff Interface **********/
#include <iostream>

class FileOutStreamBuff : public OutStreamBuff
{
public:
	RTC_CALL FileOutStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool isAsciiFile, bool mustAppend = false);
	RTC_CALL virtual ~FileOutStreamBuff();

	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { return false; }
	RTC_CALL WeakStr FileName() override;

	RTC_CALL bool IsOpen() const;

private:
	SharedStr      m_FileName;
	std::ofstream  m_ofstream;
	streamsize_t   m_ByteCount;
};

/********** FileInpStreamBuff Interface **********/

class FileInpStreamBuff : public InpStreamBuff
{
public:
	RTC_CALL FileInpStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool isAsciiFile);
	RTC_CALL virtual ~FileInpStreamBuff();

	RTC_CALL void ReadBytes (Byte* data, streamsize_t size) const override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL WeakStr FileName() override;
	RTC_CALL bool AtEnd() const override { return !m_ifstream; }

	RTC_CALL bool IsOpen() const;

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
	RTC_CALL MappedFileInpStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnError, bool doRetry);
	RTC_CALL virtual ~MappedFileInpStreamBuff();

	RTC_CALL void ReadBytes (Byte* data, streamsize_t size) const override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL WeakStr FileName() override;
	RTC_CALL bool AtEnd() const override { return m_Curr == m_FileView.DataEnd(); }

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
	RTC_CALL MappedFileOutStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, streamsize_t nrBytes);
	RTC_CALL virtual ~MappedFileOutStreamBuff();

	RTC_CALL void WriteBytes(const Byte* data, streamsize_t size) override;
	RTC_CALL streamsize_t CurrPos() const override;
	RTC_CALL bool AtEnd() const override { return m_Curr >= m_FileView.DataEnd(); }
	RTC_CALL WeakStr FileName() override;

	RTC_CALL bool IsOpen() const;

private:
	SharedStr                 m_FileName;
	char*                     m_Curr;
	FileViewHandle            m_FileView;
};

#endif // __RTC_SER_FILESTREAMBUFF_H
