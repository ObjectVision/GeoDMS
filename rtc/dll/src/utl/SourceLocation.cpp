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

#include "RtcInterface.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "SourceLocation.h"

//*****************************************************************
//**********         FileDescr Interface                 **********
//*****************************************************************
#include "set/QuickContainers.h"

struct CompareFD
{
	bool operator()(const FileDescr* a, FileDescr* b) const
	{
		dms_assert(a);
		dms_assert(b);
		return a->GetFileName() < b->GetFileName();
	}
};

typedef std::set<FileDescr*, CompareFD> FileDescrSet;

static FileDescrSet s_FDS;

FileDescr::FileDescr(WeakStr str, FileDateTime fdt)
	:	m_FileName(str)
	,	m_Fdt(fdt)
{
	s_FDS.insert(this);
}

FileDescr::~FileDescr()
{
	s_FDS.erase(this);
}


// *****************************************************************************

#include "dbg/DmsCatch.h"
#include "ser/AsString.h"
#include "ser/MoreStreamBuff.h"
#include "ser/FormattedStream.h"
#include "utl/Environment.h"

auto ReportChangedFiles(bool updateFileTimes) -> VectorOutStreamBuff
{
	VectorOutStreamBuff vos;
	FormattedOutStream fos(&vos, FormattingFlags::None);

	FileDateTime fdt;
	auto
		i = s_FDS.begin(),
		e = s_FDS.end();
	while (i != e)
	{
		FileDescr* fd = *i;
		fdt = GetFileOrDirDateTime(fd->GetFileName());
		if (fdt != fd->m_Fdt)
		{
			if (updateFileTimes)
				fd->m_Fdt = fdt;
			fos << fd->GetFileName().c_str() << "\n";
		}
		++i;
	}
	
	return vos;
}

IStringHandle DMS_ReportChangedFiles(bool updateFileTimes) //TODO: remove IStringHandle
{
	DMS_CALL_BEGIN
		auto vos = ReportChangedFiles(updateFileTimes);
		if (vos.CurrPos() == 0)
			return nullptr;
		return IString::Create(vos.GetData(), vos.GetDataEnd());

	DMS_CALL_END
	return nullptr;
}

//----------------------------------------------------------------------
// struct SourceLocation
//----------------------------------------------------------------------

#include "mci/Class.h"

SourceLocation::SourceLocation(FileDescrPtr configFileDescr, UInt32 lineNr, UInt32 colNr)
	: m_ConfigFileDescr(configFileDescr)
	, m_ConfigFileLineNr(lineNr)
	, m_ConfigFileColNr(colNr)
{}

void SourceLocation::Release() const //most descendant dtor visible from here
{
	if (!DecRef())
		delete this;
} 

SharedStr SourceLocation::AsText() const
{
	return mgFormat2SharedStr("%s(%u,%u)"
		, ConvertDmsFileNameAlways(m_ConfigFileDescr->GetFileName())
		, m_ConfigFileLineNr
		, m_ConfigFileColNr
	);
}

SharedStr SourceLocation::GetSourceName(WeakStr fullName, const Class* cls) const
{
	dms_assert(cls);
	return mgFormat2SharedStr("%s(%u,%u): %s: %s"
		, ConvertDmsFileNameAlways(m_ConfigFileDescr->GetFileName())
		, m_ConfigFileLineNr
		, m_ConfigFileColNr
		, fullName
		, cls->GetName()
	);
}

