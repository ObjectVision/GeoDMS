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

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "RtcInterface.h"

#if defined(WIN32)

#include <windows.h>
#include "timezoneapi.h"

#endif //defined(WIN32)


//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "SourceLocation.h"

//*****************************************************************
//**********         FileDescr Interface                 **********
//*****************************************************************

using FileDescrSet = std::vector<FileDescr*>;
static FileDescrSet s_FDS;
std::mutex cs_FDS;

FileDescr::FileDescr(WeakStr str, FileDateTime fdt, UInt32 loadNumber)
	:	m_FileName(str)
	,	m_ReadFdt(fdt)
	,	m_LoadNumber(loadNumber)
{
	reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "load %s", str);

	auto lock = std::scoped_lock(cs_FDS);
	s_FDS.emplace_back(this);
}

FileDescr::~FileDescr()
{
	reportF(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "unload %s", GetFileName());

	auto lock = std::scoped_lock(cs_FDS);
	auto pos = std::find(s_FDS.begin(), s_FDS.end(), this);
	assert(pos != s_FDS.end());
	s_FDS.erase(pos);
}


// *****************************************************************************

#include "dbg/DmsCatch.h"
#include "ser/AsString.h"
#include "ser/MoreStreamBuff.h"
#include "ser/FormattedStream.h"
#include "utl/Environment.h"
#include "xml/XmlOut.h"

auto ReportChangedFiles() -> VectorOutStreamBuff
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
		if (fdt != fd->LastFdt())
		{
			fd->m_LaterFdt = fdt;
			fos << fd->GetFileName().c_str() << "\n";
		}
		++i;
	}
	
	return vos;
}

void ReportCurrentConfigFileList(OutStreamBase& os)
{
	XML_OutElement table(os, "TABLE");
	{
		XML_OutElement header_row(os, "TR");
		{
			XML_OutElement header_col1(os, "TH");
			os << "Configuration Files";
		}
		{
			XML_OutElement header_col2(os, "TH");
			os << "Load Number";
		}
		{
			XML_OutElement header_col3(os, "TH");
			os << "File date & time when read";
		}
		{
			XML_OutElement header_col4(os, "TH");
			os << "... when ignored";
		}
	}
	auto
		i = s_FDS.begin(),
		e = s_FDS.end();
	for (; i != e; ++i)
	{
		FileDescr* fd = *i;
		XML_OutElement row(os, "TR");
		{
			XML_OutElement col(os, "TD");
			os << fd->GetFileName().c_str();
		}
		{
			XML_OutElement col(os, "TD");
			os << AsString(fd->m_LoadNumber).c_str();
		}
		{
			XML_OutElement col(os, "TD");
			os << AsDateTimeString(fd->m_ReadFdt).c_str();
		}
		if (fd->m_LaterFdt) // this files change is only to be notified once in current configuration session
		{
			XML_OutElement col(os, "TD");
			os << AsDateTimeString(fd->m_LaterFdt).c_str();
		}
	}
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

