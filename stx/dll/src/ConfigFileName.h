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

#if !defined( __STX_CONFIGFILENAME_H)
#define __STX_CONFIGFILENAME_H

#include "utl/SourceLocation.h" // for FileDescr

// *****************************************************************************
// class/module: ConfigurationFilenameContainer
// *****************************************************************************

struct ConfigurationFilenameContainer
{
	ConfigurationFilenameContainer(WeakStr configLoadDir);
	~ConfigurationFilenameContainer();
	FileDescrPtr GetFileRef(CharPtr name);
	static ConfigurationFilenameContainer* GetIt();
	static WeakStr GetConfigLoadDirFromCurrentDir() { return GetIt()->m_ConfigLoadDir; }

private:
	std::vector<FileDescrPtr>              m_FileRefs;
	SharedStr                              m_ConfigLoadDir;
	static ConfigurationFilenameContainer* s_Singleton;
};

// *****************************************************************************
// class/module: ConfigurationFilenameLock
// *****************************************************************************

struct ConfigurationFilenameLockBase
{
	ConfigurationFilenameLockBase(WeakStr sourceFileName, WeakStr currDirName);
	~ConfigurationFilenameLockBase();

	static FileDescr* GetCurrentFileDescrFromConfigLoadDir();
	static CharPtr GetCurrentDirNameFromConfigLoadDir();

	FileDescr* GetFileRef() const { return m_SourceFileRef; }

private:
	ConfigurationFilenameLockBase();
	ConfigurationFilenameLockBase(const ConfigurationFilenameLockBase&);

	FileDescrPtr m_SourceFileRef;
	SharedStr    m_CurrDirName;
	const ConfigurationFilenameLockBase* m_PrevFilenameLock;
	static const ConfigurationFilenameLockBase* s_LastFileNameLock;

	friend struct ConfigurationFilenameLock;
};

struct ConfigurationFilenameLock : ConfigurationFilenameLockBase
{
	ConfigurationFilenameLock(WeakStr sourceFileName, WeakStr currDirName);

private:
	ConfigurationFilenameLock();
	ConfigurationFilenameLock(const ConfigurationFilenameLock&);
};

// *****************************************************************************
// class/module: Implementation of global funcs.
// *****************************************************************************

bool GetConfigPointColFirst();

#endif //!defined( __STX_CONFIGFILENAME_H)
