// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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

leveled_counted_section s_SessionUsageCounter(item_level_type(0), ord_level_type(1), "SessionUsageCounter");

//----------------------------------------------------------------------
// struct SessionData for near singleton management: implementation of statics
//----------------------------------------------------------------------

std::mutex sd_SessionDataCriticalSection;

static std::shared_ptr<SessionData> s_CurrSD;

//----------------------------------------------------------------------
// struct SessionData for near singleton management: implementation of member functions
//----------------------------------------------------------------------

SessionData::SessionData(CharPtr configLoadDir, CharPtr configSubDir)
	:	m_ConfigLoadDir(configLoadDir)
	,	m_ConfigSubDir(configSubDir)
	,	m_ConfigDir(  DelimitedConcat(configLoadDir, configSubDir) )
	,	m_ConfigLoadTS(-1)              // set by Open() 
	,	m_cfgColFirst( g_cfgColFirst )  // set by SetConfigPointColFirst
{}

SessionData::~SessionData() 
{
}

void SessionData::release()
{
	if (s_CurrSD.get() != this)
		return;

	m_SFWA.Commit();
	deactivateThis();
}

void SessionData::deactivateThis()
{
	m_IsCancelling = true;

	assert(s_CurrSD.get() == this);
	
	// save curr globals to the deactivating session
	assert(s_CurrSD->m_cfgColFirst == g_cfgColFirst);
	s_CurrSD->m_cfgColFirst = g_cfgColFirst; // ???

	g_cfgColFirst = false; // back to default value
	s_CurrSD = nullptr;
}

void SessionData::ActivateThis()
{
	auto sectionLock = std::scoped_lock(sd_SessionDataCriticalSection);

	if (s_CurrSD.get() == this)
		return;

	if (s_CurrSD)
		s_CurrSD->deactivateThis();

	s_CurrSD = shared_from_this();
	assert(s_CurrSD);

	// restore saved globals from the activating session
	g_cfgColFirst = m_cfgColFirst;
}

SharedStr SessionData::GetConfigDir() const
{
	auto sectionLock = std::scoped_lock(sd_SessionDataCriticalSection);

	return m_ConfigDir;
}

bool SessionData::IsConfigDirty() const
{
	auto sectionLock = std::scoped_lock(sd_SessionDataCriticalSection);

	dms_assert(m_ConfigLoadTS != -1); // PRECONDITION: SessionData::Open was called after this was created
	return m_ConfigLoadTS < UpdateMarker::GetLastTS();
}

bool DMS_IsConfigDirty(const TreeItem* configRoot)
{
	DMS_CALL_BEGIN

		auto sectionLock = std::scoped_lock(sd_SessionDataCriticalSection);

		SessionData::activateIt(configRoot);
		return s_CurrSD->IsConfigDirty();

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

SharedTreeItem SessionData::GetConfigSettings() const
{
	return m_ConfigSettings.get();
}

SharedTreeItem SessionData::GetConfigSettings(CharPtr section, CharPtr key) const
{
	dms_assert(section && *section);
	auto result = GetConfigSettings();
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

std::shared_ptr<SessionData> SessionData::Create(CharPtr configLoadDir, CharPtr configSubDir)
{
	auto sectionLock = std::scoped_lock(sd_SessionDataCriticalSection);

	if (s_CurrSD)
		Curr()->deactivateThis();

	assert(!s_CurrSD);
	s_CurrSD = std::make_shared<SessionData>(MakeAbsolutePath(configLoadDir).c_str(), configSubDir );
	g_cfgColFirst = s_CurrSD->m_cfgColFirst;
	assert(s_CurrSD->m_ConfigRoot == nullptr); // POSTCONDITION of Created but not opend SessionData
	return s_CurrSD;
}

void SessionData::SetConfigPointColFirst(bool cfgColFirst)
{
	m_cfgColFirst = cfgColFirst;
	if (s_CurrSD.get() == this)
		g_cfgColFirst = cfgColFirst;
}

static TokenID t_ConfigSettings = GetTokenID_st("ConfigSettings");

void SessionData::Open(const TreeItem* configRoot)
{
	dms_assert(!m_ConfigRoot); // only open this once
	m_ConfigRoot = configRoot;
	dms_assert(this);
	m_ConfigLoadTS = UpdateMarker::GetLastTS();
	SharedPtr<TreeItem> configSettings = const_cast<TreeItem*>(configRoot)->CreateItem(t_ConfigSettings);
	configSettings->SetIsHidden(true);
	m_ConfigSettings = configSettings.get_ptr();
}

std::shared_ptr<SessionData> SessionData::Curr()
{
	std::lock_guard dcLock(sd_SessionDataCriticalSection);
	return s_CurrSD;
}

void SessionData::ReleaseCurr()
{
	std::lock_guard dcLock(sd_SessionDataCriticalSection);
	if (s_CurrSD)
		s_CurrSD->release();
	assert(s_CurrSD == nullptr);
}

void SessionData::activateIt(const TreeItem* configRoot) // for now, assume session to be a singleton
{
	assert(s_CurrSD && (s_CurrSD->GetConfigRoot() == configRoot || s_CurrSD->m_ConfigRoot.is_null()));
}

std::shared_ptr<SessionData> SessionData::GetIt(const TreeItem* configRoot)
{
	std::lock_guard dcLock(sd_SessionDataCriticalSection);
	return getIt(configRoot);
}

std::shared_ptr<SessionData> SessionData::getIt(const TreeItem* configRoot)
{
	if (s_CurrSD) // Assumes SessionData is a singleton
		activateIt(configRoot);
	return s_CurrSD;
}


void SessionData::ReleaseIt(const TreeItem* configRoot) // WARNING: this might point to a destroyed configRoot
{
	auto dcLock = std::scoped_lock(sd_SessionDataCriticalSection);

	auto sd = getIt(configRoot);
	if (sd)
		sd->release();
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
