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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ser/FileStreamBuff.h"
#include "ser/SafeFileWriter.h"
#include "utl/Environment.h"

// *****************************************************************************
// Section:     Basic streambuffer Interface
// *****************************************************************************

SharedStr ConvertFileName(WeakStr fileName)
{
	return ConvertDmsFileName(fileName);
}

/********** FileOutStreamBuff Implementation **********/

FileOutStreamBuff::FileOutStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool isAsciiFile, bool mustAppend)
	:	m_FileName((GetWritePermission(fileName), fileName))
	,	m_ofstream(
			ConvertFileName(GetWorkingFileName(sfwa, fileName, mustAppend ? FCM_OpenRwGrowable : FCM_CreateAlways)).c_str()
		,	std::ios_base::openmode(std::ios::out | (isAsciiFile ? 0 : std::ios::binary) | (mustAppend ? std::ios::app : 0)
		))
{
}

FileOutStreamBuff::~FileOutStreamBuff()
{
}

void FileOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	if (!size) return;
	m_ByteCount += size;
	m_ofstream.write(data, size);
}

streamsize_t FileOutStreamBuff::CurrPos() const
{
	return m_ByteCount;
}

WeakStr FileOutStreamBuff::FileName()
{
	return m_FileName;
}

bool FileOutStreamBuff::IsOpen() const
{
	return m_ofstream.is_open();
}

/********** FileInpStreamBuff Implementation **********/

FileInpStreamBuff::FileInpStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool isAsciiFile)
	:	m_ByteCount(0)
	,	m_FileName(fileName)
	,	m_ifstream(
			ConvertFileName(GetWorkingFileName(sfwa, fileName, FCM_OpenReadOnly)).c_str(), 
			(isAsciiFile) 
				?	(std::ios::in) 
				:	(std::ios::in | std::ios::binary)
		)
{
}

FileInpStreamBuff::~FileInpStreamBuff()
{
}

void FileInpStreamBuff::ReadBytes(Byte* data, streamsize_t size) const
{
	m_ifstream.read(data, size);
	streamsize_t count = m_ifstream.gcount();
	assert(count <=size);
	m_ByteCount += count;
	if (count < size)
		fast_fill(data+count, data+size, EOF);
}

streamsize_t FileInpStreamBuff::CurrPos() const
{
	return m_ByteCount;
}

WeakStr FileInpStreamBuff::FileName()
{
	return m_FileName;
}

bool FileInpStreamBuff::IsOpen() const
{
	return m_ifstream.is_open();
}

/********** MappedFileInpStreamBuff Implementation **********/

MappedFileInpStreamBuff::MappedFileInpStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, bool throwOnOpenError, bool doRetry)
	:	m_FileName(fileName)
{
	auto cmfh = std::make_shared<ConstMappedFileHandle>(fileName, sfwa, throwOnOpenError, doRetry);
	m_FileView = ConstFileViewHandle(cmfh);
	m_FileView.Map();
	if (IsOpen())
		m_Curr = m_FileView.DataBegin();
}



MappedFileInpStreamBuff::~MappedFileInpStreamBuff()
{
	m_FileView.Unmap();
}

void MappedFileInpStreamBuff::ReadBytes(Byte* data, streamsize_t size) const
{
	SizeT restSize = m_FileView.DataEnd() - m_Curr;
	if (restSize < size)
	{
		fast_fill(data+restSize, data + size, EOF);
		size = restSize;
	}
	CharPtr next = m_Curr + size;
	fast_copy(m_Curr, next, data);
	m_Curr = next;
}

streamsize_t MappedFileInpStreamBuff::CurrPos() const
{
	return m_Curr - m_FileView.DataBegin();
}

WeakStr MappedFileInpStreamBuff::FileName()
{
	return m_FileName;
}

bool MappedFileInpStreamBuff::IsOpen() const
{
	return m_FileView.IsUsable();
}

/********** MappedFileOutStreamBuff Implementation **********/

MappedFileOutStreamBuff::MappedFileOutStreamBuff(WeakStr fileName, SafeFileWriterArray* sfwa, streamsize_t nrBytes)
	:	m_FileName(fileName)
{
	auto mfh = std::make_shared<MappedFileHandle>();
	mfh->OpenRw(fileName, sfwa, nrBytes, dms_rw_mode::write_only_all, false);
	m_FileView = FileViewHandle(mfh, 0, nrBytes);
	m_FileView.Map(true);

	dms_assert(nrBytes);
	m_Curr = m_FileView.DataBegin();
	if (!IsOpen())
		throwErrorF("MappedFileOutStream", "Cannot open file %s for writing", fileName);
}

MappedFileOutStreamBuff::~MappedFileOutStreamBuff()
{}

void MappedFileOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
	streamsize_t restSize = m_FileView.DataEnd() - m_Curr;
	if (restSize < size)
		throwErrorF("MappedFileOutStream", "Cannot write %s bytes to %s after pos %s since it has size %s",
			AsString(size).c_str(), m_FileName.c_str(), 
			AsString(m_Curr - m_FileView.DataBegin()).c_str(), 
			AsString(m_FileView.GetViewSize()).c_str()
		);
	memcpy(m_Curr, data, size);
	m_Curr += size;
}

streamsize_t MappedFileOutStreamBuff::CurrPos() const
{
	return m_Curr - m_FileView.DataBegin();
}

WeakStr MappedFileOutStreamBuff::FileName()
{
	return m_FileName;
}

bool MappedFileOutStreamBuff::IsOpen() const
{
	return m_FileView.DataEnd()- m_FileView.DataBegin();
}

//----------------------------------------------------------------------
// Creating and reading OutStreamBuff's Interface functions
//----------------------------------------------------------------------

#include "StreamBuffInterface.h"
#include "dbg/DmsCatch.h"

RTC_CALL FileOutStreamBuff*
DMS_CONV DMS_FileOutStreamBuff_Create(CharPtr fileName, bool isAsciiFile)
{
	DMS_CALL_BEGIN

		return new FileOutStreamBuff(ConvertDosFileName(SharedStr(fileName)), nullptr, isAsciiFile, false);

	DMS_CALL_END
	return nullptr;
}


