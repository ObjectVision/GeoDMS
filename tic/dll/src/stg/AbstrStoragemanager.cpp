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

#include <iostream> // DEBUG

#include "AbstrStorageManager.h"

#include "RtcInterface.h"
#include "RtcVersion.h"

#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/UpdateMark.h"
#include "dbg/Debug.h"
#include "ptr/InterestHolders.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "DataLocks.h"
#include "DataArray.h"
#include "DataStoreManagerCaller.h"
#include "TreeItem.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "stg/StorageClass.h"


#if defined(MG_DEBUG)
#define MG_DEBUG_ASM 1
#else
#define MG_DEBUG_ASM 0
#endif

#if MG_DEBUG_ASM

static SizeT sd_AsmNr = 0;
static std::map<AbstrStorageManager*, SizeT> sd_ASM_set;
static std::mutex sd_asm;

#endif

StorageMetaInfo::~StorageMetaInfo()
{}

const AbstrDataItem* StorageMetaInfo::CurrRD() const { return AsDataItem(m_Curr.get_ptr()); }
const AbstrUnit*     StorageMetaInfo::CurrRU() const { return AsUnit(m_Curr.get_ptr()); }

void StorageMetaInfo::OnPreLock()
{
	if (IsDataItem(m_Curr.get_ptr()))
	{
		SharedPtr<const AbstrUnit> adu = CurrRD()->GetAbstrDomainUnit();
		adu->GetCount(); // Prepare for later DataWriteLock->DoCreateMemoryStorage
	}
}

void StorageMetaInfo::OnOpen()
{}

const TreeItem* GetExportSettings(const TreeItem* curr)
{
	if (curr) 
		curr = curr->GetTreeParent();
	if (!curr)
		return nullptr;
	return curr->FindItem("ExportSettings");
}

const TreeItem* GetExportMetaInfo(const TreeItem* curr)
{
	const TreeItem* exportSettings = GetExportSettings(curr);
	if (!exportSettings)
		return nullptr;
	return exportSettings->FindItem("MetaInfo");
}

// *****************************************************************************
// Section:     AbstractStorageManager counted wrapper
// *****************************************************************************

bool AbstrStorageManager::s_ReduceResources(ClientHandle clientHandle)
{
	AbstrStorageManager* self = reinterpret_cast<AbstrStorageManager*>(clientHandle);
	return self->ReduceResources();
}

AbstrStorageManager::AbstrStorageManager()
	:	m_Commit(true)
	,	m_IsOpen(false)
	,	m_IsReadOnly(false)
	,	m_FileTime(0)
	,	m_LastCheckTS(0)
	,	m_CriticalSection(item_level_type(0), ord_level_type::AbstrStorage, "AbstrStorageManager")
{
	DMS_RegisterReduceResourcesFunc(s_ReduceResources, typesafe_cast<ClientHandle>(this));

#if MG_DEBUG_ASM
	std::lock_guard lock(sd_asm);
	sd_ASM_set[this] = ++sd_AsmNr;
#endif // MG_DEBUG_ASM
}

bool AbstrStorageManager::ReduceResources()
{
	return true;
}

bool AbstrStorageManager::CanWriteTiles() const
{
	return false;
}

AbstrUnit* AbstrStorageManager::CreateGridDataDomain(const TreeItem* storageHolder)
{
	throwIllegalAbstract(MG_POS, this, "CreateGridDataDomain");
}

void AbstrStorageManager::InitStorageManager(CharPtr name, bool readOnly, item_level_type itemLevel)
{
	MGD_PRECONDITION(name != nullptr);

	m_ID = TokenID(name, multi_threading_tag_v);
	m_IsReadOnly              = readOnly;
	m_IsOpenedForWrite        = false;
#if defined(MG_DEBUG_LOCKLEVEL)
	m_CriticalSection.m_ItemLevel = itemLevel;
#endif
}

AbstrStorageManager::~AbstrStorageManager()
{
#if MG_DEBUG_ASM
	{
		std::lock_guard lock(sd_asm);
		sd_ASM_set.erase(this);
	}
#endif // MG_DEBUG_ASM

	DMS_ReleaseReduceResourcesFunc(s_ReduceResources, typesafe_cast<ClientHandle>(this));
}

// Static interface functions

#include "SessionData.h"

SharedStr GetConfigIniFileName()
{
	return SessionData::Curr()->GetConfigIniFile();
}

SharedStr GetConfigIniFileName(CharPtr configDir)
{
	return SessionData::GetConfigIniFile(configDir);
}

#define OVERRIDABLE_NAME "Overridable"
static TokenID t_Overridable = GetTokenID_st(OVERRIDABLE_NAME);

SharedStr GetRegConfigSetting(const TreeItem* configRoot, CharPtr key, CharPtr defaultValue)
{
//	static TokenID configSettingsID("ConfigSettings");
	dms_assert(configRoot);
	
	SharedStr result;
	if (PlatformInfo::GetEnvString(OVERRIDABLE_NAME, key, result))
		return result;

	result = GetGeoDmsRegKey(key);
	if (!result.empty())
		return result;

	const TreeItem* configSettings = SessionData::GetIt(configRoot)->GetConfigSettings();
	if (configSettings)
	{
		TokenID tidKey = GetTokenID_mt(key);
		const TreeItem* keyTi = nullptr;

		const TreeItem* overridable = configSettings->GetConstSubTreeItemByID(t_Overridable);
		if (overridable)
			keyTi = overridable->GetConstSubTreeItemByID(tidKey);
		if (!keyTi)
			keyTi = configSettings->GetConstSubTreeItemByID(tidKey);
		if (keyTi && IsDataItem( keyTi) )
			return AsDataItem(keyTi)->LockAndGetValue<SharedStr>(0);
	}

	return SharedStr(defaultValue);
}

SharedStr GetConvertedRegConfigSetting(const TreeItem* configRoot, CharPtr key, CharPtr defaultValue)
{
	return ConvertDosFileName(GetRegConfigSetting(configRoot, key, defaultValue));
}


SharedStr GetConvertedConfigDirKeyString(CharPtr configDir, CharPtr key, CharPtr defaultValue)
{
	SharedStr result;

	if (!PlatformInfo::GetEnvString("directories", key, result))
	{
		result = 
			GetConfigKeyString(
				GetConfigIniFileName(configDir),
				"directories", 
				key, defaultValue
			);
	}
	return ConvertDosFileName(result);
}

SharedStr GetCaseDir(const TreeItem* configStore)
{
	SharedStr caseDir;
	const TreeItem* configRoot = 0;
	while (configStore)
	{
		TokenID configStoreName = configStorePropDefPtr->GetValue(configStore);
		if (configStoreName)
		{
			caseDir = DelimitedConcat(getFileNameBase(configStoreName.GetStr().c_str()).c_str(), caseDir.c_str());
			if (IsAbsolutePath(AbstrStorageManager::Expand(configStore, caseDir).c_str()))
				return caseDir;
		}
		configRoot = configStore;
		configStore = configStore->GetTreeParent();
	}
	dms_assert(configRoot);
	dms_assert(SessionData::Curr() && SessionData::Curr()->GetConfigRoot() == configRoot);

	dms_assert(!IsAbsolutePath(caseDir.c_str()));
	return DelimitedConcat(SessionData::Curr()->GetConfigLoadDir().c_str(), caseDir.c_str());
}

SharedStr GetStorageBaseName(const TreeItem* configStore)
{
	if (configStore)
	{
		configStore = configStore->GetStorageParent(false);
		if (configStore)
			return getFileNameBase(TreeItemPropertyValue(configStore, storageNamePropDefPtr).c_str());
	}
	return SharedStr();
}

static SharedStr s_ProjDir;

SharedStr GetProjDir(CharPtr configDir)
{	
	dms_assert(*configDir);
	dms_assert(!HasDosDelimiters(configDir));
	dms_assert(IsAbsolutePath(configDir));

	DBG_START("GetProjDir", configDir, false);

	static SharedStr prevConfigDir;

	if (prevConfigDir != configDir)
	{
		s_ProjDir = GetConvertedConfigDirKeyString(configDir, "projDir", "..");
		if (!s_ProjDir.empty() && s_ProjDir[0] == '.')
		{
			SharedStr configLoadDir = splitFullPath(configDir);
			UInt32 c = 0;
			UInt32 cc = 0;
			UInt32 p = 1;
			do {
				while (s_ProjDir[p] == '.')
				{
					++c, ++p;
				}
				switch (s_ProjDir[p])
				{
					case '/':     ++p; [[fallthrough]];
					case char(0): cc += c; c = -1;
				}
			} while(s_ProjDir[p] == '.');
			DBG_TRACE(("GoUp %d times on %s", cc, configLoadDir.c_str()));
			while (cc)
			{
				configLoadDir = splitFullPath(configLoadDir.c_str());
				--cc;
			}
			DBG_TRACE(("GoUp %d times on %s", cc, configLoadDir.c_str()));
			s_ProjDir = DelimitedConcat(configLoadDir.c_str(), SharedStr(s_ProjDir.cbegin()+p, s_ProjDir.csend()).c_str());
			DBG_TRACE(("result after GoUp %s", s_ProjDir.c_str()));
		}
		prevConfigDir = configDir;
	}
	dms_assert(!HasDosDelimiters(s_ProjDir.c_str()));
	return s_ProjDir;
}

SharedStr GetLocalTime()
{
	std::time_t time_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string s(30, '\0');
	std::strftime(&s[0], s.size(), "%Y-%m-%d_%H-%M-%S", std::localtime(&time_now));

	return SharedStr(s.c_str());
}

SharedStr GetProjBase(CharPtr configDir)
{
	return GetConvertedConfigDirKeyString(configDir, "projBase", splitFullPath(GetProjDir(configDir).c_str()).c_str());
}

SharedStr GetProjName(CharPtr configDir)
{
	return GetConvertedConfigDirKeyString(configDir, "projName", getFileName(GetProjDir(configDir).c_str()));
}

SharedStr GetConfigName(CharPtr configDir)
{
	return GetConvertedConfigDirKeyString(configDir, "configName", getFileName(configDir));
}

SharedStr GetCalcCacheDir(CharPtr configDir)
{
	return GetConvertedConfigDirKeyString(configDir, "calcCacheDir", 
		"%localDataProjDir%/CalcCache%platform%.v" 
		BOOST_STRINGIZE( DMS_VERSION_MAJOR) "." BOOST_STRINGIZE( DMS_VERSION_MINOR) 
	);
}

SharedStr GetLocalDataProjDir(CharPtr configDir)
{
	return GetConvertedConfigDirKeyString(configDir, "localDataProjDir", "%LocalDataDir%/%projName%");
}

static SharedStr s_SourceDataProjDir;

SharedStr GetSourceDataProjDir(const TreeItem* configRoot)
{
	s_SourceDataProjDir = GetConvertedRegConfigSetting(configRoot, "SourceDataProjDir", "%sourceDataDir%/%projName%"); // overridable in ConfigSettings
	return s_SourceDataProjDir;
}

SharedStr GetDataDir(const TreeItem* configRoot)
{
	return GetConvertedRegConfigSetting(configRoot, "dataDir", "%sourceDataProjDir%/data"); // overridable in ConfigSettings
}

SharedStr GetPlaceholderValue(CharPtr subDirName, CharPtr placeHolder, bool mustThrow)
{
	dms_assert(IsAbsolutePath(subDirName));

	if (!stricmp(placeHolder, "projDir"         )) return GetProjDir (subDirName);
	if (!stricmp(placeHolder, "projName"        )) return GetProjName(subDirName);
	if (!stricmp(placeHolder, "projBase"        )) return GetProjBase(subDirName);
	if (!stricmp(placeHolder, "localDataProjDir")) return GetLocalDataProjDir (subDirName);
	if (!stricmp(placeHolder, "calcCacheDir"    )) return GetCalcCacheDir     (subDirName);
	if (!stricmp(placeHolder, "exeDir"          )) return GetExeDir();
	if (!stricmp(placeHolder, "programFiles32"  )) return PlatformInfo::GetProgramFiles32();
	if (!stricmp(placeHolder, "localDataDir"    )) return GetLocalDataDir();
	if (!stricmp(placeHolder, "sourceDataDir"   )) return GetSourceDataDir();
	if (!stricmp(placeHolder, "configDir"       )) return SharedStr(subDirName);
	if (!stricmp(placeHolder, "configName"      )) return GetConfigName(subDirName);
	if (!stricmp(placeHolder, "geodmsversion"   )) return SharedStr(DMS_GetVersion());
	if (!stricmp(placeHolder, "platform"        )) return SharedStr(DMS_GetPlatform());
	if (!stricmp(placeHolder, "osversion"       )) return PlatformInfo::GetVersionStr();
	if (!stricmp(placeHolder, "username"        )) return PlatformInfo::GetUserNameA();
	if (!stricmp(placeHolder, "computername"    )) return PlatformInfo::GetComputerNameA();
	if (!stricmp(placeHolder, "DateTime"        )) return GetLocalTime();
	if ((StrLen(placeHolder) > 4) && !strnicmp(placeHolder, "env:", 4))
	{
		SharedStr result;
		if (PlatformInfo::GetEnv(placeHolder+4, result))
			return ConvertDosFileName( result );
	}
	// GetSystemInfo
	// GetVersion
	if (mustThrow)
		throwErrorD("Unknown placeholder", placeHolder);
	return SharedStr();
}

SharedStr GetPlaceholderValue(const TreeItem* configStore, CharPtr placeHolder)
{
	if (!stricmp(placeHolder, "caseDir"))           return GetCaseDir        (configStore);
	if (!stricmp(placeHolder, "storageBaseName"  )) return GetStorageBaseName(configStore);
	if (!stricmp(placeHolder, "currDir"))           return SharedStr( SessionData::Curr()->GetConfigLoadDir() );

	if (!stricmp(placeHolder, "sourceDataProjDir")) return GetSourceDataProjDir(SessionData::Curr()->GetConfigRoot());
	if (!stricmp(placeHolder, "dataDir"          )) return GetDataDir          (SessionData::Curr()->GetConfigRoot());

	SharedStr result = GetPlaceholderValue(SessionData::Curr()->GetConfigDir().c_str(), placeHolder, false);
	if (result.empty())
	{
		result = GetConvertedRegConfigSetting(SessionData::Curr()->GetConfigRoot(), placeHolder, "");
		if (result.empty())
			return SharedStr(placeHolder);
	}
	return result;
}

SharedStr AbstrStorageManager::Expand(const TreeItem* configStore, SharedStr storageName)
{
	FencedInterestRetainContext irc;

	dms_assert(configStore);
	while (true)
	{
		CharPtr p1 = storageName.find('%');
		if (p1 == storageName.csend())
			break;
		CharPtr p2 = std::find(p1+1, storageName.csend(), '%');
		if (p2 == storageName.csend())
			configStore->throwItemErrorF("GetFullStorageName('%s'): unbalanced placeholder delimiters (%%).", storageName.c_str());
		storageName
				=	SharedStr(storageName.cbegin(), p1)
				+	GetPlaceholderValue(configStore, SharedStr(p1+1, p2).c_str())
				+	SharedStr(p2+1, storageName.csend());
	}
	return storageName;
}

SharedStr AbstrStorageManager::GetFullStorageName(const TreeItem* configStore, SharedStr storageName)
{
	dms_assert(configStore);
	storageName = AbstrCalculator::EvaluatePossibleStringExpr(configStore, storageName, CalcRole::Other);
	if (storageName.empty() || !strncmp(storageName.begin(), "http:", 5) || !strncmp(storageName.begin(), "https:", 6))
		return storageName;

	storageName = ConvertDosFileName(storageName);
	storageName = Expand(configStore, storageName);

	while (configStore && !IsAbsolutePath(storageName.c_str()))
	{
		dms_assert(*storageName.c_str() != ':');
		TokenID configStoreName = configStorePropDefPtr->GetValue(configStore);
		if (configStoreName)
			storageName = DelimitedConcat(getFileNameBase(configStoreName.GetStr().c_str()).c_str(), storageName.c_str());
		configStore = configStore->GetTreeParent();
	}
	if (!IsAbsolutePath(storageName.c_str()))
		storageName = MakeAbsolutePath(storageName.c_str());
	if (*storageName.c_str() == ':')
		return SharedStr(storageName.cbegin()+1, storageName.csend());
	return storageName;
}

SharedStr AbstrStorageManager::GetFullStorageName(CharPtr subDirName, CharPtr storageNameCStr)
{
	SharedStr storageName = SharedStr(storageNameCStr);
	SharedStr subDirNameStr;
	if (!IsAbsolutePath(subDirName))
	{
		subDirNameStr = MakeAbsolutePath(subDirName);
		subDirName = subDirNameStr.c_str();
	}
	while (true)
	{
		CharPtr p1 = storageName.find('%');
		if (p1 == storageName.csend())
			break;
		CharPtr p2 = std::find(p1+1, storageName.csend(), '%');
		if (p2 == storageName.csend())
			throwDmsErrF("GetFullStorageName('%s'): unbalanced placeholder delimiters (%%).", storageName.c_str());
		storageName 
				= SharedStr(storageName.cbegin(), p1) 
				+ GetPlaceholderValue(subDirName,  SharedStr(p1+1, p2).c_str(), true)
				+ SharedStr(p2+1, storageName.csend());
	}

	return IsAbsolutePath(storageName.c_str())
		?	storageName
		:	DelimitedConcat(subDirName, storageName.c_str());
}

static TokenID st_AllTables = GetTokenID_st("AllTables");
static TokenID st_AttrOnly = GetTokenID_st("AttrOnly");
static TokenID st_None = GetTokenID_st("None");

SyncMode AbstrStorageManager::GetSyncMode(const TreeItem* storageHolder)
{
	dms_assert(storageHolder);
	TokenID syncMode;
	do
	{
		syncMode = syncModePropDefPtr->GetValue(storageHolder);
		if (syncMode)
		{
			if (syncMode == st_AllTables) return SM_AllTables;
			if (syncMode == st_AttrOnly)  return SM_AttrsOfConfiguredTables;
			if (syncMode == st_None)      return SM_None;
			break;
		}
		storageHolder = storageHolder->GetTreeParent();
	}
	while (storageHolder);
	return SM_AttrsOfConfiguredTables; // default
}

bool AbstrStorageManager::DoesExistEx(CharPtr storageName, TokenID storageType, const TreeItem* storageHolder)
{
	DBG_START("AbstrStorageManager", "DoesExistEx", true);

	SharedPtr<AbstrStorageManager> sm = AbstrStorageManager::Construct(storageHolder, SharedStr(storageName), storageType, true);

	if (!sm) 
		return false;

	ObjectContextHandle dch2(sm);
	return sm->DoesExist(storageHolder);
}

AbstrStorageManagerRef
AbstrStorageManager::Construct(const TreeItem* holder, SharedStr relStorageName, TokenID typeID, bool readOnly, bool throwOnFail)
{	
	if (AbstrCalculator::MustEvaluate(relStorageName.c_str()))
		relStorageName = AbstrCalculator::EvaluatePossibleStringExpr(holder, relStorageName, CalcRole::Calculator);

	return Construct(AbstrStorageManager::GetFullStorageName(holder, relStorageName).c_str(), typeID
	,	readOnly, throwOnFail
	,	GetItemLevel(holder)
	);
}

static TokenID s_mdbToken = GetTokenID_st("mdb");
static TokenID s_odbcToken = GetTokenID_st("odbc");

AbstrStorageManagerRef AbstrStorageManager::Construct(CharPtr storageName, TokenID typeID, bool readOnly, bool throwOnFailure, item_level_type itemLevel)
{
	CDebugContextHandle dc("AbstractStorageManager::Construct", storageName, false);

	if (!typeID)
		typeID = GetTokenID_mt(getFileNameExtension(storageName));
	if (!typeID)
	{
		if (!throwOnFailure)
			return {};
		throwDmsErrF("Cannnot derive storage type for storage '%s'", storageName);
	}
	if (typeID == s_mdbToken)
		typeID = s_odbcToken;
	if (typeID == s_odbcToken) // at this moment we can only read from ODBC
		readOnly = true;

	auto sm = StorageClass::CreateStorageManager(storageName, typeID, readOnly, throwOnFailure, itemLevel);

#if MG_DEBUG_ASM
#endif
	dms_assert(sm || !throwOnFailure);
	if (sm && (readOnly || !sm->IsWritable()))
		sm->m_IsReadOnly = true;

	return sm;
}

SharedStr AbstrStorageManager::GetNameStr() const 
{ 
	return SharedStr(m_ID); 
}

SharedStr AbstrStorageManager::GetUrl() const 
{ 
	return ConvertDmsFileName(splitFullPath(GetNameStr().c_str()));
}

bool AbstrStorageManager::DoesExist(const TreeItem* storageHolder) const
{
	if (IsOpen()) // was only opended if it was created or DoesExist returned true
		return true;
	return DoCheckExistence(storageHolder);
}

bool AbstrStorageManager::IsWritable() const
{
	if (IsOpen() && !IsReadOnly()) // was only opended if it was created or DoesExist returned true
		return true;
	return DoCheckWritability();
}

// Default implementation now checks existence of m_Name as a file
bool AbstrStorageManager::DoCheckExistence(const TreeItem* storageHolder) const
{
	return IsFileOrDirAccessible(GetNameStr());
}

bool AbstrStorageManager::DoCheckWritability() const
{
	SharedStr name(GetNameStr());
	return IsFileOrDirWritable(name) || !IsFileOrDirAccessible(name);
}

FileDateTime AbstrStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const
{
	if (DoesExist(storageHolder))
		m_FileTime = GetFileOrDirDateTime(GetNameStr());
	return m_FileTime; 
}

FileDateTime AbstrStorageManager::GetCachedChangeDateTime(const TreeItem* storageHolder, CharPtr relativePath) const
{
	TimeStamp lastTS = UpdateMarker::GetLastTS();
	if (m_FileTime == FileDateTime() || m_LastCheckTS < lastTS)
	{
		GetLastChangeDateTime(storageHolder, relativePath);
		m_LastCheckTS = lastTS;
	}
	return m_FileTime; 
}

StorageMetaInfoPtr AbstrStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* focusItem, StorageAction) const
{
	return std::make_unique<StorageMetaInfo>(storageHolder, focusItem);
}

bool AbstrStorageManager::ReadDataItem  (const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) { return false; }
bool AbstrStorageManager::WriteDataItem (StorageMetaInfoPtr&& smi) { return false; }
bool AbstrStorageManager::ReadUnitRange (const StorageMetaInfo& smi) const       { return false; }
bool AbstrStorageManager::WriteUnitRange(StorageMetaInfoPtr&& smi)       { return false; }

ActorVisitState AbstrStorageManager::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const
{
	if (self->IsStorable() && (self->HasCalculator() || self->HasConfigData()))
	{
		const TreeItem* metaInfo = GetExportMetaInfo(self);
		if (metaInfo && metaInfo->VisitConstVisibleSubTree(visitor)== AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}
	return AVS_Ready;
}

void AbstrStorageManager::DropStream(const TreeItem* item, CharPtr path)
{
	throwIllegalAbstract(MG_POS, this, "DropStream");
}

void AbstrStorageManager::UpdateTree(const TreeItem* storageHolder, TreeItem* curr) const
{ 
	auto nameStr = GetNameStr();
	CDebugContextHandle dch1("AbstrStorageManager::UpdateTree", nameStr.c_str(), false);
	auto syncMode = GetSyncMode(storageHolder);
	if (syncMode == SM_None)
		return;
	DoUpdateTree(storageHolder, curr, syncMode);
}

void AbstrStorageManager::DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const 
{
	if (curr != storageHolder)
		return;

	if (IsDataItem(curr))
		if (!curr->IsStorable())
			if (curr->HasCalculator())
				curr->Fail("Item has both a Calculation Rule and a read-only storage spec", FR_MetaInfo);
}

void AbstrStorageManager::StartInterest(const TreeItem* storageHolder, const TreeItem* self) const
{
	interest_holders_container interestHolders;
	auto visitor = MakeDerivedProcVistor([&interestHolders](const Actor* item) { if (!item->IsPassorOrChecked()) interestHolders.emplace_back(item); });

	VisitSuppliers(SupplierVisitFlag::StartSupplInterest, visitor, storageHolder, self);

	if (interestHolders.size())
		m_InterestHolders[interest_holders_key(storageHolder, self)].swap(interestHolders);
	else
		StopInterest(storageHolder, self);
}

void AbstrStorageManager::StopInterest(const TreeItem* storageHolder, const TreeItem* self) const noexcept
{
	m_InterestHolders.erase(interest_holders_key(storageHolder, self));
}

void AbstrStorageManager::DoWriteTree(const TreeItem* storageHolder)
{}

// Wrapper functions for consistent calls to specific StorageManager overrides

bool AbstrStorageManager::OpenForRead(const StorageMetaInfo& smi) const
{
	dms_assert(smi.StorageHolder());

	const TreeItem* storageHolder = smi.StorageHolder();
	if (m_IsOpen) 
		return true;
	try {
		DoOpenStorage(smi, dms_rw_mode::read_only);
		m_IsOpen = true;
	}
	catch (...)
	{
		storageHolder->CatchFail(FR_MetaInfo);
		storageHolder->ThrowFail();
	}
	return true;
}

void AbstrStorageManager::OpenForWrite(const StorageMetaInfo& smi) // PRECONDITION !m_IsReadOnly
{
	if (m_IsReadOnly)
		throwItemError("Storage is read only - write request is denied");
	if (m_IsOpen && m_IsOpenedForWrite) 
		return;
	if (m_IsOpen) // dus niet m_IsOpenedForWrite
	{
		DoCloseStorage(false);
		m_IsOpen = false;
	}

	if (DoesExist(smi.StorageHolder()))
		DoOpenStorage(smi, dms_rw_mode::read_write);
	else
		DoCreateStorage(smi);

	m_IsOpenedForWrite = m_IsOpen = true;
	DoWriteTree(smi.StorageHolder());
}

void AbstrStorageManager::CloseStorage() const
{
	if (!m_IsOpen) 
		return;
	DoCloseStorage(m_Commit && m_IsOpenedForWrite);
	m_IsOpen = false;
}


void AbstrStorageManager::DoCreateStorage(const StorageMetaInfo& smi)
{
	dms_assert(!m_IsReadOnly);
	DoOpenStorage(smi, dms_rw_mode::write_only_all);
}

void AbstrStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	dms_assert(!IsOpen());
}

void AbstrStorageManager::DoCloseStorage (bool mustCommit) const
{}

IMPL_CLASS(AbstrStorageManager, 0)

TIC_CALL const Class*     DMS_CONV DMS_AbstrStorageManager_GetStaticClass()
{
	return AbstrStorageManager::GetStaticClass();
}

// *****************************************************************************
// 
// GdalMetaInfo
//
// *****************************************************************************

GdalMetaInfo::GdalMetaInfo(const TreeItem* storageHolder, const TreeItem* curr)
	: StorageMetaInfo(storageHolder, curr)
	, m_OptionsItem(storageHolder->FindItem("GDAL_Options"))
	, m_DriverItem(storageHolder->FindItem("GDAL_Driver"))
	, m_LayerCreationOptions(storageHolder->FindItem("GDAL_LayerCreationOptions"))
	, m_ConfigurationOptions(storageHolder->FindItem("GDAL_ConfigurationOptions"))
{
	if (m_OptionsItem)
		m_OptionsItem->PrepareDataUsage(DrlType::Certain);
	if (m_DriverItem)
		m_DriverItem->PrepareDataUsage(DrlType::Certain);
	if (m_LayerCreationOptions)
		m_LayerCreationOptions->PrepareDataUsage(DrlType::Certain);
	if (m_ConfigurationOptions)
		m_ConfigurationOptions->PrepareDataUsage(DrlType::Certain);
}

// *****************************************************************************
// 
// scoped StorageHandles
//
// *****************************************************************************

StorageCloseHandle::StorageCloseHandle(const AbstrStorageManager* storageManager, const TreeItem* storageHolder, const TreeItem* focusItem, StorageAction sa)
	: m_MetaInfo(storageManager->GetMetaInfo(storageHolder, const_cast<TreeItem*>(focusItem), sa))
	, m_TimeStampBefore()
	, m_StorageManager(storageManager)
	, m_StorageHolder(storageHolder)
	, m_FocusItem(focusItem)
	, m_StorageLock(storageManager->m_CriticalSection)
{
	Init(storageManager, storageHolder, focusItem);
}

StorageCloseHandle::StorageCloseHandle(StorageMetaInfoPtr&& smi)
	: m_MetaInfo(std::move(smi))
	, m_TimeStampBefore()
	, m_StorageManager(m_MetaInfo->StorageManager())
	, m_StorageHolder(m_MetaInfo->StorageHolder())
	, m_FocusItem(m_MetaInfo->CurrRI())
	, m_StorageLock(m_StorageManager->m_CriticalSection)
{
	Init(m_StorageManager, m_StorageHolder, m_FocusItem);
}

void StorageCloseHandle::Init(const AbstrStorageManager* storageManager, const TreeItem* storageHolder, const TreeItem* focusItem)
{
	dms_assert(m_MetaInfo);
	if (storageHolder->DoesContain(focusItem))
	{
		dms_assert(focusItem);
		// enable Registration of external DateTime stamp changes during the lifetime of this StorageHandle
		// Registers LastChange with the latest Filesystem TimeStamp of the storage to prevent data consumers from external 
		// TODO: Also update FileSystem timestamp for other items with the same storageManager
		// ISSUE: mapping between item hierarchy and hierarchy of external TimeStamps and external change detection
		// ISSUE: different sources with the same external Filesystem Timestamp may be registered at different internal times
		FileDateTime fdt = storageManager->GetCachedChangeDateTime(storageHolder, m_MetaInfo->m_RelativeName.c_str());
		if (fdt)
		{
			m_TimeStampBefore = DataStoreManager::Curr()->DetermineExternalChange(fdt);
			dms_assert(m_TimeStampBefore); // POSTCONDITION
		}
		else
			m_TimeStampBefore = focusItem->GetLastChangeTS();
	}
};

StorageCloseHandle::~StorageCloseHandle()
{
	dms_assert(m_StorageManager);
	m_MetaInfo.reset();
	m_StorageManager->CloseStorage();
	if (m_StorageHolder && m_StorageHolder->DoesContain(m_FocusItem) && !m_StorageManager->IsReadOnly())
	{
		dms_assert(m_FocusItem);
		FileDateTime fdt = m_StorageManager->GetLastChangeDateTime(m_StorageHolder, m_FocusItem->GetRelativeName(m_StorageHolder).c_str());
		if (fdt)
			DataStoreManager::Curr()->RegisterExternalTS(fdt, m_TimeStampBefore);
	}
}

StorageReadHandle::StorageReadHandle(StorageMetaInfoPtr&& smi)
	: StorageCloseHandle(std::move(smi))
{
	Init();
}

StorageReadHandle::StorageReadHandle(const AbstrStorageManager* sm, const TreeItem* storageHolder, TreeItem* focusItem, StorageAction sa)
	: StorageCloseHandle(sm, storageHolder, focusItem, sa)
{
	Init();
}

void StorageReadHandle::Init()
{
	dms_assert(m_StorageManager);
	dms_assert(m_StorageHolder);
	dms_assert(MetaInfo());
	m_StorageManager->OpenForRead(*MetaInfo());
	if (m_StorageManager->IsOpen())
	{
		MetaInfo()->OnOpen();
	}
}

bool StorageReadHandle::Read() const
{
	dms_assert(m_FocusItem); // note that storageHolder may be nullptr
	dms_assert(m_StorageManager);
	if (!m_StorageManager->IsOpen())
		return false;

	return FocusItem()->DoReadItem(MetaInfo());
}


StorageWriteHandle::StorageWriteHandle(StorageMetaInfoPtr&& smi)
	: StorageCloseHandle(std::move(smi))
{
	dms_assert(MetaInfo());
	dms_assert(!MetaInfo()->StorageManager()->IsOpen());

	//	dms_assert(storageHolder);
	MetaInfo()->StorageManager()->OpenForWrite(*MetaInfo());
}

// *****************************************************************************
// 
// AbstrStorageManager::ExportMetaInfo
//
// *****************************************************************************

#include "Xml/PropWriter.h"

void GenerateMetaInfo(AbstrPropWriter& apw, const TreeItem* curr, const TreeItem* contents)
{
	dms_assert(contents);
	dms_assert(IsMainThread());

	GenerateSystemInfo(apw, curr);

	for (const TreeItem* section = contents->GetFirstVisibleSubItem(); section; section = section->GetNextVisibleItem())
	{
		const_cast<TreeItem*>(section)->SetKeepDataState(true);
		InterestRetainContextBase::Add(section);

		SharedStr sectionName( section->GetID().AsStrRange() );
		apw.OpenSection(sectionName.c_str());
		for (const TreeItem* key = section->GetFirstVisibleSubItem(); key; key = key->GetNextVisibleItem())
		{
			const_cast<TreeItem*>(key)->SetKeepDataState(true);
			InterestRetainContextBase::Add(key);

			key->CertainUpdate(PS_Committed);

			if (!IsDataItem(key))
				continue;
			SharedStr keyValue;
			try {
				SharedStr keyValueExp = GetTheValue<SharedStr>(key);
				keyValue = AbstrCalculator::EvaluatePossibleStringExpr(curr, keyValueExp, CalcRole::Other);
				apw.WriteKey(key->GetName().c_str(), keyValue);
			}
			catch (...) {}
		}
		apw.CloseSection();
	}
}

static TokenID contentsID = GetTokenID_st("Contents");
static TokenID fileNameID = GetTokenID_st("FileName");
static TokenID fileTypeID = GetTokenID_st("FileType");

void ExportMetaInfoToFileImpl(const TreeItem* curr)
{
	FencedInterestRetainContext holdCalcResultsHere;

	const TreeItem* metaInfo = GetExportMetaInfo(curr);
	if (!metaInfo)
		return;

	const_cast<TreeItem*>(metaInfo)->SetKeepDataState(true);
	const TreeItem* contents = metaInfo->GetConstSubTreeItemByID(contentsID);
	if (!contents)
		return;

	const TreeItem* fileNameItem = metaInfo->GetConstSubTreeItemByID(fileNameID);
	if (!fileNameItem)
		return;

	const TreeItem* fileTypeItem = metaInfo->GetConstSubTreeItemByID(fileTypeID);

	bool isXml = (fileTypeItem != nullptr) && (stricmp(GetTheValue<SharedStr>(fileTypeItem).c_str(), "xml") == 0);

	SharedStr fileNamePattern = GetTheValue<SharedStr>(fileNameItem);
	if (fileNamePattern.empty())
		return;
	SharedStr fileName = AbstrStorageManager::Expand(curr, fileNamePattern);
	fileName = DSM::GetSafeFileWriterArray(curr)->GetWorkingFileName(fileName, FCM_CreateAlways);
	if (isXml)
	{
		XmlPropWriter writer(fileName);
		GenerateMetaInfo(writer, curr, contents);
	}
	else
	{
		IniPropWriter writer(fileName);
		GenerateMetaInfo(writer, curr, contents);
	}
}

void AbstrStorageManager::ExportMetaInfo(const TreeItem* storageHolder, const TreeItem* curr)
{
	ExportMetaInfoToFileImpl(curr);
}
