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

#include "TicPCH.h"
#pragma hdrstop

#include "SessionData.h"
#include "DataStoreManager.h"

#include "act/UpdateMark.h"
#include "dbg/DmsCatch.h"
#include "geo/PointOrder.h"
#include "utl/splitPath.h"

#include "stg/AbstrStreamManager.h"

#include "Param.h"

std::mutex s_SessionDataCS;
leveled_counted_section s_SessionUsageCounter(item_level_type(0), ord_level_type(1), "SessionUsageCounter");

//----------------------------------------------------------------------
// struct SessionData for near singleton management: implementation of member functions
//----------------------------------------------------------------------

SessionData::SessionData(CharPtr configLoadDir, CharPtr configSubDir)
	:	m_ConfigLoadDir(configLoadDir)
	,	m_ConfigSubDir(configSubDir)
	,	m_ConfigDir(  DelimitedConcat(configLoadDir, configSubDir) )
	,	m_ConfigLoadTS(-1)              // set by Open() 
	,	m_cfgColFirst( g_cfgColFirst )  // set by SetConfigPointColFirst
{
	ActivateThis();
}

SessionData::~SessionData() 
{
}

void SessionData::Release()
{
	leveled_counted_section::scoped_lock lock(s_SessionUsageCounter); // wait for readers

	assert(m_IsCancelling);

	DeactivateThis();
	delete this;
	s_CurrSD = nullptr;
}

void SessionData::DeactivateThis()
{
	auto sectionLock = std::scoped_lock(s_SessionDataCS);

	assert(Curr() == this);
	
	// save curr globals to the deactivating session
	assert(s_CurrSD->m_cfgColFirst == g_cfgColFirst);
	s_CurrSD->m_cfgColFirst = g_cfgColFirst; // ???

	g_cfgColFirst = false; // back to default value
}

void SessionData::ActivateThis()
{
	auto sectionLock = std::scoped_lock(s_SessionDataCS);

	if (s_CurrSD == this)
		return;

	if (s_CurrSD)
		s_CurrSD->DeactivateThis();

	s_CurrSD = this;
	dms_assert(s_CurrSD);

	// restore saved globals from the activating session
	g_cfgColFirst = m_cfgColFirst;
}

SharedStr SessionData::GetConfigDir() const
{
	auto sectionLock = std::scoped_lock(s_SessionDataCS);

#if defined(MG_DEBUG_DATA)
//	reportF(ST_MinorTrace, "m_ConfigDir: %s\n", m_ConfigDir.c_str());
	if (GetConfigRoot())
	{
//		reportF(ST_MinorTrace, "casedir: %s\n", GetCaseDir(GetConfigRoot()).c_str());
		dbg_assert(!stricmp(m_ConfigDir.c_str(), GetCaseDir(GetConfigRoot()).c_str()));
	}
	else
	{
//		reportF(ST_MinorTrace, "GetConfigLoadDir: %s\n", GetConfigLoadDir().c_str());
//		reportF(ST_MinorTrace, "casedir: %s\n", m_ConfigSubDir.c_str());
		dbg_assert(!stricmp(m_ConfigDir.c_str(), DelimitedConcat(GetConfigLoadDir().c_str(), m_ConfigSubDir.c_str()).c_str()));
	}
#endif
	return m_ConfigDir;
}

bool SessionData::IsConfigDirty() const
{
	auto sectionLock = std::scoped_lock(s_SessionDataCS);

	dms_assert(m_ConfigLoadTS != -1); // PRECONDITION: SessionData::Open was called after this was created
	return m_ConfigLoadTS < UpdateMarker::GetLastTS();
}

bool DMS_IsConfigDirty(const TreeItem* configRoot)
{
	DMS_CALL_BEGIN

		auto sectionLock = std::scoped_lock(s_SessionDataCS);

		SessionData::ActivateIt(configRoot);
		return SessionData::Curr()->IsConfigDirty();

	DMS_CALL_END
	return false;
}

SharedStr SessionData::GetConfigIniFile() const
{
	return DelimitedConcat(GetConfigDir().c_str(), "config.ini");
}

SharedStr SessionData::GetConfigIniFile(CharPtr configDir)
{
	return DelimitedConcat(configDir, "config.ini");
}

const TreeItem* SessionData::GetConfigSettings() const
{
	return m_ConfigSettings;
}

const TreeItem* SessionData::GetConfigSettings(CharPtr section, CharPtr key) const
{
	dms_assert(section && *section);
	const TreeItem* result = GetConfigSettings();
	if (result)
	{
		result = result->GetConstSubTreeItemByID(GetTokenID_mt(section));
		if (result && key && *key)
			result = result->GetConstSubTreeItemByID(GetTokenID_mt(key));
	}
	return result;
}

SharedStr SessionData::ReadConfigString(CharPtr section, CharPtr key, CharPtr defaultValue) const
{
	dms_assert(this);

	SharedStr result;
	if (PlatformInfo::GetEnvString(section, key, result))
		return result;

	const AbstrDataItem* adi = AsDataItem(GetConfigSettings(section, key));
	if (adi)
		return adi->LockAndGetValue<SharedStr>(0);
	return GetConfigKeyString(GetConfigIniFile(), section, key, defaultValue ); // Backward compatibility, REMOVE
}

Int32 SessionData::ReadConfigValue(CharPtr section, CharPtr key, Int32 defaultValue) const
{
	dms_assert(this);
	const AbstrDataItem* adi = AsDataItem(GetConfigSettings(section, key));
	if (adi)
		return NumericParam_GetValueAsInt32(adi);
	return GetConfigKeyValue(GetConfigIniFile(), section, key, defaultValue ); // Backward compatibility, REMOVE
}

const TreeItem* SessionData::GetContainer(const TreeItem* context, CharPtr name) const
{
    SharedStr fullName = ReadConfigString("configuration", name, name);
	CharPtrRange searchKey = fullName.empty() ? CharPtrRange(name): fullName.AsRange();
    return context->FindItem(searchKey);
}

const TreeItem* SessionData::GetClassificationContainer(const TreeItem* context) const
{
	return GetContainer(context, "Classifications");
}

void SessionData::SetActiveDesktop(const TreeItem* tiActive)
{
	m_ActiveDesktop = tiActive;
}

const TreeItem* SessionData::GetActiveDesktop() const
{
	return m_ActiveDesktop;
}

//----------------------------------------------------------------------
// struct SessionData for near singleton management: implementation of statics
//----------------------------------------------------------------------

std::recursive_mutex sd_SessionDataCriticalSection;

WeakPtr<SessionData> SessionData::s_CurrSD;

void SessionData::Create(CharPtr configLoadDir, CharPtr configSubDir)
{
	dms_assert(! s_CurrSD);
	s_CurrSD = new SessionData(MakeAbsolutePath(configLoadDir).c_str(), configSubDir );
	dms_assert(s_CurrSD->m_ConfigRoot == 0); // POSTCONDITION of Created but not opend SessionData
}

void SessionData::SetConfigPointColFirst(bool cfgColFirst)
{
	if (Curr()->m_cfgColFirst == cfgColFirst)
		return;

	Curr()->m_cfgColFirst = cfgColFirst;
	g_cfgColFirst = cfgColFirst;
}

static TokenID t_ConfigSettings = GetTokenID_st("ConfigSettings");

void SessionData::Open(const TreeItem* configRoot)
{
	dms_assert(!m_ConfigRoot); // only open this once
	m_ConfigRoot = configRoot;
	dms_assert(this);
	m_ConfigLoadTS = UpdateMarker::GetLastTS();
	WeakPtr<TreeItem> configSettings = const_cast<TreeItem*>(configRoot)->CreateItem(t_ConfigSettings);
	configSettings->SetIsHidden(true);
	m_ConfigSettings = configSettings.get_ptr();
}


void SessionData::CancelDataStoreManager(const TreeItem* configRoot)
{
	WeakPtr<SessionData> sd = GetIt(configRoot);
	if (sd)
		sd->m_IsCancelling = true;
}

void SessionData::ReleaseIt(const TreeItem* configRoot) // WARNING: this might point to a destroyed configRoot
{
	auto dcLock = std::scoped_lock(sd_SessionDataCriticalSection);

	WeakPtr<SessionData> sd = GetIt(configRoot);
	if (sd)
		sd->Release();
}

//----------------------------------------------------------------------
// helper func
//----------------------------------------------------------------------

const TreeItem* GetCacheRoot(const TreeItem* subItem)
{
	if (subItem->IsCacheItem())
	{
		while (auto parent = subItem->GetTreeParent())
			subItem = parent;
		dms_assert(subItem->IsCacheRoot());
	}
	return subItem;
}

extern "C" TIC_CALL void DMS_CONV DMS_Config_SetActiveDesktop(TreeItem* tiActive)
{
	SessionData::Curr()->SetActiveDesktop(tiActive);
}
