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
#include "StxPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "StxInterface.h"

//#include "ser/AsString.h"
//#include "ser/MoreStreamBuff.h"
//#include "ser/FormattedStream.h"
#include "utl/Environment.h"
#include "xct/DmsException.h"

#include "stg/AbstrStorageManager.h"

#include "ConfigFileName.h"

// *****************************************************************************
// class/module: ConfigurationFilenameContainer
// *****************************************************************************

ConfigurationFilenameContainer::ConfigurationFilenameContainer(WeakStr configLoadDir, UInt32 loadNumber)
	:	m_ConfigLoadDir(configLoadDir)
	,	m_LoadNumber(loadNumber)
{
	dms_assert(!s_Singleton);
	s_Singleton = this;
}

ConfigurationFilenameContainer::~ConfigurationFilenameContainer()
{
	dms_assert(s_Singleton == this);
	s_Singleton = 0;
}

FileDescrPtr ConfigurationFilenameContainer::GetFileRef(CharPtr name)
{
	for (auto i = m_FileRefs.begin(), e = m_FileRefs.end(); i != e; ++i)
		if (!stricmp( (*i)->GetFileName().c_str(), name))
			return *i;

	m_FileRefs.push_back( new FileDescr( SharedStr(name), 0, m_LoadNumber ));
	return m_FileRefs.back();
}

ConfigurationFilenameContainer* ConfigurationFilenameContainer::GetIt()
{
	assert(s_Singleton);
	return s_Singleton;
}

ConfigurationFilenameContainer* ConfigurationFilenameContainer::s_Singleton = 0;

// *****************************************************************************
// class/module: ConfigurationFilenameLock
// *****************************************************************************

ConfigurationFilenameLockBase ::ConfigurationFilenameLockBase(WeakStr sourceFileName, WeakStr currDirName)
	:	m_SourceFileRef(ConfigurationFilenameContainer::GetIt()->GetFileRef(sourceFileName.c_str()) )
	,	m_CurrDirName(currDirName)
	,	m_PrevFilenameLock(s_LastFileNameLock)
{
	s_LastFileNameLock = this;
}

ConfigurationFilenameLock::ConfigurationFilenameLock(WeakStr sourceFileName, WeakStr currDirName)
	:	ConfigurationFilenameLockBase(sourceFileName, currDirName)
{
	const ConfigurationFilenameLockBase* i = s_LastFileNameLock->m_PrevFilenameLock;
	while (i)
	{
		if (!stricmp(i->m_SourceFileRef->GetFileName().c_str(), sourceFileName.c_str()))
			throwErrorF("CFG", "recursive inclusion of configuration file %s", sourceFileName.c_str());
		i = i->m_PrevFilenameLock;
	}
}

ConfigurationFilenameLockBase::~ConfigurationFilenameLockBase()
{
	dms_assert(s_LastFileNameLock == this);
	s_LastFileNameLock = m_PrevFilenameLock;
}

FileDescr* ConfigurationFilenameLockBase::GetCurrentFileDescrFromConfigLoadDir()
{
	if (!s_LastFileNameLock)
		return nullptr;
	return s_LastFileNameLock->m_SourceFileRef;
}

CharPtr ConfigurationFilenameLockBase::GetCurrentDirNameFromConfigLoadDir()
{
	return s_LastFileNameLock
		?	s_LastFileNameLock->m_CurrDirName.c_str()
		:	ConfigurationFilenameContainer::GetIt()->GetConfigLoadDirFromCurrentDir().c_str();
}

SharedStr ConfigurationFilenameLockBase::GetConfigDir()
{
	auto current_file = s_LastFileNameLock;
	if (!current_file)
		return ConfigurationFilenameContainer::GetIt()->GetConfigLoadDirFromCurrentDir();

	while (true)
	{
		auto prev = current_file->m_PrevFilenameLock;
		if (!prev)
			return current_file->m_CurrDirName;
		current_file = prev;
	}
}

const ConfigurationFilenameLockBase* ConfigurationFilenameLockBase::s_LastFileNameLock =0;

// *****************************************************************************
// class/module: Implementation of global funcs.
// *****************************************************************************

bool GetConfigPointColFirst()
{
	return
		GetConfigKeyValue(
			GetConfigIniFileName(),
			"general", 
			"ConfigPointColRow", 
			false
		);
}
