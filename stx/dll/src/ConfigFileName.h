// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#pragma once

#if !defined( __STX_CONFIGFILENAME_H)
#define __STX_CONFIGFILENAME_H

#include "utl/SourceLocation.h" // for FileDescr

// *****************************************************************************
// class/module: ConfigurationFilenameContainer
// *****************************************************************************

struct ConfigurationFilenameContainer
{
	ConfigurationFilenameContainer(WeakStr configLoadDir, UInt32 loadNumber);
	~ConfigurationFilenameContainer();
	FileDescrPtr GetFileRef(CharPtr name);
	static ConfigurationFilenameContainer* GetIt();
	static WeakStr GetConfigLoadDirFromCurrentDir() { return GetIt()->m_ConfigLoadDir; }

private:
	std::vector<FileDescrPtr>              m_FileRefs;
	SharedStr                              m_ConfigLoadDir;
	UInt32                                 m_LoadNumber;
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
	static SharedStr GetConfigDir();

	FileDescr* GetFileRef() const { return m_SourceFileRef.get(); }

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


#endif //!defined( __STX_CONFIGFILENAME_H)
