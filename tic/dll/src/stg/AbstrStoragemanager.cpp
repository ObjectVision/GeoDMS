// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <iostream> // DEBUG
#include <chrono>

#include "AbstrStorageManager.h"

#include "RtcInterface.h"

#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "ptr/InterestHolders.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
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
{
	if (m_StorageManager)
	{
//		MG_CHECK(! m_StorageManager->IsOpen());
		m_StorageManager->CloseStorage();
	}
}

auto StorageMetaInfo::CurrRD() const -> SharedPtr<const AbstrDataItem> { return AsDataItem(m_Curr); }
auto StorageMetaInfo::CurrRU() const -> SharedPtr<const AbstrUnit> { return AsUnit(m_Curr); }

void StorageMetaInfo::OnPreLock()
{
	if (IsDataItem(m_Curr.get()))
	{
		SharedPtr<const AbstrUnit> adu = { CurrRD()->GetAbstrDomainUnit(), existing_obj{} };
		adu->GetCount(); // Prepare for later DataWriteLock->DoCreateMemoryStorage
		SharedPtr<const AbstrUnit> avu = { CurrRD()->GetAbstrValuesUnit(), existing_obj{} };
		WaitForReadyOrSuspendTrigger(avu->GetCurrRangeItem());
	}
}

void StorageMetaInfo::OnOpenForRead(StorageReadHandle*)
{}

void StorageMetaInfo::OnClose(StorageCloseHandle*)
{}


TIC_CALL const TreeItem* GetExportSettingsContext(const TreeItem* context)
{
	const TreeItem* exportContext = context->FindItem(CharPtrRange("ExportSettings"));
	if (exportContext)
		return exportContext;
	return context;
}


const TreeItem* GetExportSettings(const TreeItem* curr)
{
	if (curr) 
		curr = curr->GetTreeParent();
	if (!curr)
		return nullptr;
	return GetExportSettingsContext(curr);
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

AbstrStorageManager::AbstrStorageManager()
	:	m_Commit(true)
	,	m_IsOpen(false)
	,	m_IsReadOnly(false)
	,	m_FileTime(0)
	,	m_LastCheckTS(0)
	,   m_CriticalSection(1)
{
#if MG_DEBUG_ASM
	std::lock_guard guard(sd_asm);
	sd_ASM_set[this] = ++sd_AsmNr;
#endif // MG_DEBUG_ASM
}

void AbstrStorageManager::InitStorageManager(CharPtr name, bool readOnly)
{
	MGD_PRECONDITION(name != nullptr);

	m_ID = TokenID(name, multi_threading_tag_v);
	m_IsReadOnly = readOnly;
	m_IsOpenedForWrite = false;
}

AbstrStorageManager::~AbstrStorageManager()
{
#if MG_DEBUG_ASM
	{
		std::scoped_lock lock(sd_asm);
		sd_ASM_set.erase(this);
	}
#endif // MG_DEBUG_ASM
}

// *****************************************************************************
// Section:     NonmappableStorageManager impl
// *****************************************************************************

AbstrUnit* NonmappableStorageManager::CreateGridDataDomain(const TreeItem* storageHolder)
{
	throwIllegalAbstract(MG_POS, this, "CreateGridDataDomain");
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
	assert(configRoot);
	assert(IsMetaThread());

	// read value from environment setting GEODMS_Overridable_%key%
	SharedStr result;
	if (PlatformInfo::GetEnvString(OVERRIDABLE_NAME, key, result))
		return result;

	// read value from registry: Computer\HKEY_CURRENT_USER\Software\ObjectVision\%MACHINE%\GeoDMS\%key%
	result = GetGeoDmsRegKey(key);
	if (!result.empty())
		return result;

	// read from configuration as last resort
	const TreeItem* configSettings = SessionData::getIt(configRoot)->GetConfigSettings();
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
		{
			const_cast<TreeItem*>(keyTi)->SetKeepDataState(true);
			InterestPtr<const TreeItem*> haveInterest(keyTi);
			return AsDataItem(keyTi)->LockAndGetValue<SharedStr>(0);
		}
	}

	return SharedStr(defaultValue MG_DEBUG_ALLOCATOR_SRC("GetRegConfigSetting"));
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


SharedStr GetProjDir(CharPtr configDir)
{	
	assert(IsMainThread());
	assert(*configDir);
	assert(!HasDosDelimiters(configDir));
	assert(IsAbsolutePath(configDir));

	static SharedStr s_prevConfigDir;
	static SharedStr s_proj_dir;

	// memoization: only (re)calculate proj_dir when configDir is different than at last call
	if (s_prevConfigDir != configDir)
	{
		DBG_START("GetProjDir", configDir, true);

		s_proj_dir = GetConvertedConfigDirKeyString(configDir, "projDir", "..");
		if (!s_proj_dir.empty() && s_proj_dir[0] == '.')
		{
			SharedStr configLoadDir = splitFullPath(configDir);
			UInt32 c = 0;
			UInt32 cc = 0;
			UInt32 p = 1;
			do {
				while (s_proj_dir[p] == '.')
				{
					++c, ++p;
				}
				switch (s_proj_dir[p])
				{
					case '/':     ++p; [[fallthrough]];
					case char(0): cc += c; c = -1;
				}
			} while(s_proj_dir[p] == '.');

			DBG_TRACE(("GoUp %d times on %s", cc, configLoadDir.c_str()));

			while (cc)
			{
				configLoadDir = splitFullPath(configLoadDir.c_str());
				--cc;
			}

			DBG_TRACE(("GoUp %d times on %s", cc, configLoadDir.c_str()));

			s_proj_dir = DelimitedConcat(configLoadDir.c_str(), SharedStr(CharPtrRange(s_proj_dir.cbegin()+p, s_proj_dir.csend())).c_str());

			DBG_TRACE(("result after GoUp %s", s_proj_dir.c_str()));
		}
		s_prevConfigDir = configDir;
	}
	assert(!HasDosDelimiters(s_proj_dir.c_str()));
  	return s_proj_dir;
}

SharedStr GetLocalTime()
{
	std::time_t time_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string s(30, '\0');
	std::strftime(&s[0], s.size(), "%Y-%m-%d_%H-%M-%S", std::localtime(&time_now));

	return SharedStr(s.c_str() MG_DEBUG_ALLOCATOR_SRC("GetLocalTime"));
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

SharedStr GetPlaceholderValue(CharPtr subDirName, CharPtr placeHolder, bool mustThrow = true)
{
	assert(IsAbsolutePath(subDirName));

	if (!stricmp(placeHolder, "projDir"         )) return GetProjDir (subDirName);
	if (!stricmp(placeHolder, "projName"        )) return GetProjName(subDirName);
	if (!stricmp(placeHolder, "projBase"        )) return GetProjBase(subDirName);
	if (!stricmp(placeHolder, "localDataProjDir")) return GetLocalDataProjDir (subDirName);
	if (!stricmp(placeHolder, "calcCacheDir"    )) return GetCalcCacheDir     (subDirName);
	if (!stricmp(placeHolder, "exeDir"          )) return GetExeDir();
	if (!stricmp(placeHolder, "programFiles32"  )) return PlatformInfo::GetProgramFiles32();
	if (!stricmp(placeHolder, "localDataDir"    )) return GetLocalDataDir();
	if (!stricmp(placeHolder, "sourceDataDir"   )) return GetSourceDataDir();
	if (!stricmp(placeHolder, "configDir"       )) return SharedStr(subDirName MG_DEBUG_ALLOCATOR_SRC("GetPlaceholderValue.configDir"));
	if (!stricmp(placeHolder, "configName"      )) return GetConfigName(subDirName);
	if (!stricmp(placeHolder, "geodmsversion"   )) return SharedStr(DMS_GetVersion() MG_DEBUG_ALLOCATOR_SRC("GetPlaceholderValue.geodmsversion"));
	if (!stricmp(placeHolder, "platform"        )) return SharedStr(DMS_GetPlatform() MG_DEBUG_ALLOCATOR_SRC("GetPlaceholderValue.platform"));
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
	if (!result.empty())
		return result;

	result = GetConvertedRegConfigSetting(SessionData::Curr()->GetConfigRoot(), placeHolder, "");
	if (!result.empty())
		return result;

	reportF(MsgCategory::progress, SeverityTypeID::ST_Warning, "Unable to find placeholder: %%%s%%.", placeHolder);
	return SharedStr(placeHolder MG_DEBUG_ALLOCATOR_SRC("GetPlaceholderValue.placeHolder"));
}

SharedStr ExpandImpl(const auto* placeHolderRoot, SharedStr orgStorageName)
{
	const std::size_t
		maxSubstitutions = 1024,
		maxLength = 65000;

	auto storageName = orgStorageName;

	std::size_t
		nrSubstitutions = 0,
		lengthWithoutDelimiters = 0;

	while (true)
	{
		CharPtr pBegin = storageName.cbegin();
		CharPtr pEnd = storageName.csend();
		std::size_t pSize = pEnd - pBegin;

		if (pSize > maxLength)
		{
			if (nrSubstitutions == 0)
				throwDmsErrF("AbstrStorageManager::Expand(): length of storage name is %d; anything larger than %d characters is assumed to be faulty."
					"\nStorage name: '%s'"
					, pSize
					, maxLength
					, orgStorageName
				);
			else
				throwDmsErrF("AbstrStorageManager::Expand(): length of intermediate name during substitution is %d; anything larger than %d characters is assumed to be faulty."
					"\nNumber of completed substitutions: %d"
					"\nStorage name                     : '%s'"
					"\nCurent substitution result       : '%s'"
					, pSize
					, maxLength
					, nrSubstitutions
					, orgStorageName
					, storageName
				);

		}
		CharPtr p1 = std::find(pBegin + lengthWithoutDelimiters, pEnd, '%');
		if (p1 == pEnd)
			break;

		lengthWithoutDelimiters = p1 - pBegin;

		CharPtr p2 = std::find(p1 + 1, pEnd, '%');
		if (p2 == pEnd)
		{
			if (nrSubstitutions == 0)
				throwDmsErrF("AbstrStorageManager::Expand(): unbalanced placeholder delimiter (%%) at position %d."
					"\nStorage name                     : '%s'"
					, lengthWithoutDelimiters
					, orgStorageName
				);
			else
				throwDmsErrF("AbstrStorageManager::Expand(): unbalanced placeholder delimiter (%%) at position %d."
					"\nNumber of completed substitutions: %d"
					"\nStorage name                     : '%s'"
					"\nCurent substitution result       : '%s'"
					, lengthWithoutDelimiters
					, nrSubstitutions
					, orgStorageName
					, storageName
				);
		}

		if (++nrSubstitutions >= maxSubstitutions)
			throwDmsErrF("AbstrStorageManager::Expand(): substitution aborted after too many substitutions. Resursion suspected."
				"\nNumber of completed substitutions: %d"
				"\nStorage name                     : '%s'"
				"\nCurent substitution result       : '%s'"
				, nrSubstitutions
				, orgStorageName
				, storageName
			);

		storageName
			= SharedStr(CharPtrRange(pBegin, p1))
			+ GetPlaceholderValue(placeHolderRoot, SharedStr(CharPtrRange(p1 + 1, p2)).c_str())
			+ SharedStr(CharPtrRange(p2 + 1, pEnd));
	}
	return storageName;
}

SharedStr AbstrStorageManager::Expand(const TreeItem* configStore, SharedStr storageName)
{
	FencedInterestRetainContext irc("AbstrStorageManager::Expand");
	assert(configStore);

	return ExpandImpl(configStore, storageName);
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
		return SharedStr(CharPtrRange(storageName.cbegin()+1, storageName.csend()));
	return storageName;
}

SharedStr AbstrStorageManager::Expand(CharPtr subDirName, CharPtr storageNameCStr)
{
	SharedStr subDirNameStr;
	if (!IsAbsolutePath(subDirName))
	{
		subDirNameStr = MakeAbsolutePath(subDirName);
		subDirName = subDirNameStr.c_str();
	}
	return ExpandImpl(subDirName, SharedStr(storageNameCStr MG_DEBUG_ALLOCATOR_SRC("AbstrStorageManager::Expand")));
}

SharedStr AbstrStorageManager::GetFullStorageName(CharPtr subDirName, CharPtr storageNameCStr)
{
	auto storageName = Expand(subDirName, storageNameCStr);
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
			if (syncMode == st_AllTables) return SyncMode::AllTables;
			if (syncMode == st_AttrOnly)  return SyncMode::AttrsOfConfiguredTables;
			if (syncMode == st_None)      return SyncMode::None;
			break;
		}
		storageHolder = storageHolder->GetTreeParent();
	}
	while (storageHolder);
	return SyncMode::AllTables; // default
}

void AbstrStorageManager::UpdateTree(const TreeItem* storageHolder, TreeItem* curr) const
{
	auto nameStr = GetNameStr();
	CDebugContextHandle dch1("AbstrStorageManager::UpdateTree", nameStr.c_str(), false);
	auto syncMode = GetSyncMode(storageHolder);
	if (syncMode == SyncMode::None)
		return;
	DoUpdateTree(storageHolder, curr, syncMode);
}

bool AbstrStorageManager::DoesExistEx(CharPtr storageName, TokenID storageType, const TreeItem* storageHolder)
{
	DBG_START("AbstrStorageManager", "DoesExistEx", true);

	SharedPtr<AbstrStorageManager> sm = AbstrStorageManager::Construct(storageHolder, SharedStr(storageName MG_DEBUG_ALLOCATOR_SRC("AbstrStorageManager::DoesExistEx")), storageType, StorageReadOnlySetting::ReadOnly);

	if (!sm)
		return false;

	ObjectContextHandle dch2(sm);
	return sm->DoesExist(storageHolder);
}

AbstrStorageManagerRef
AbstrStorageManager::Construct(const TreeItem* holder, SharedStr relStorageName, TokenID typeID, StorageReadOnlySetting readOnly, bool throwOnFail)
{
	if (AbstrCalculator::MustEvaluate(relStorageName.c_str()))
		relStorageName = AbstrCalculator::EvaluatePossibleStringExpr(holder, relStorageName, CalcRole::Calculator);

	return Construct(AbstrStorageManager::GetFullStorageName(holder, relStorageName).c_str(), typeID, readOnly, throwOnFail);
}

static TokenID s_mdbToken = GetTokenID_st("mdb");
static TokenID s_odbcToken = GetTokenID_st("odbc");
static TokenID s_shpToken = GetTokenID_st("shp");
static TokenID s_tifToken = GetTokenID_st("tif");
static TokenID s_gdalVectToken = GetTokenID_st("gdal.vect");
static TokenID s_gdalGridToken = GetTokenID_st("gdal.grid");
static TokenID s_gdalWriteVectToken = GetTokenID_st("gdalwrite.vect");
static TokenID s_gdalWriteGridToken = GetTokenID_st("gdalwrite.grid");

AbstrStorageManagerRef AbstrStorageManager::Construct(CharPtr storageName, TokenID typeID, StorageReadOnlySetting readOnlySetting, bool throwOnFailure)
{
	CDebugContextHandle dc("AbstractStorageManager::Construct", storageName, false);

	if (!typeID)
	{
		typeID = GetTokenID_mt(getFileNameExtension(storageName));
		if (typeID == s_shpToken)
			typeID = s_gdalVectToken;
		else if (typeID == s_tifToken)
			typeID = s_gdalGridToken;
	}
	if (!typeID)
	{
		if (!throwOnFailure)
			return {};
		throwDmsErrF("Cannnot derive storage type for storage '%s'", storageName);
	}
	bool readOnly = false;
	if (typeID == s_mdbToken)
		typeID = s_odbcToken;
	if (typeID == s_odbcToken) // at this moment we can only read from ODBC
	{
		if (readOnlySetting == StorageReadOnlySetting::ReadWrite)
			throwDmsErrF("The odbc storage type does not allow for writing, yet StorageReadOnly is specified as false.\n"
				"Consider removing the StorageReadOnly property"
				, typeID == s_gdalGridToken ? "grid" : "vect"
			);
		readOnly = true;
	}
	else if (typeID == s_gdalGridToken || typeID == s_gdalVectToken)
	{
		if (readOnlySetting == StorageReadOnlySetting::ReadWrite)
			throwDmsErrF("The gdal.%s storage type does not allow for writing, yet StorageReadOnly is specified as false.\n"
				"Consider removing the StorageReadOnly property or set the StorageType to gdalwrite.%s"
				, typeID == s_gdalGridToken ? "grid" : "vect"
				, typeID == s_gdalGridToken ? "grid" : "vect"
			);
		readOnly = true;
	}
	else if (typeID == s_gdalWriteGridToken || typeID == s_gdalWriteVectToken)
	{
		if (readOnlySetting == StorageReadOnlySetting::ReadOnly)
			throwDmsErrF("The gdalwrite.%s storage type does not allow for writing, yet StorageReadOnly is specified as true.\n"
				"Consider removing the StorageReadOnly property or set the StorageType to gdal.%s"
				, typeID == s_gdalGridToken ? "grid" : "vect"
				, typeID == s_gdalGridToken ? "grid" : "vect"
			);
	}
	else if (readOnlySetting == StorageReadOnlySetting::ReadOnly)
		readOnly = true;

	auto sm = StorageClass::CreateStorageManager(storageName, typeID, readOnly, throwOnFailure);

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
bool AbstrStorageManager::DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const
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

void AbstrStorageManager::DoCreateStorage(const StorageMetaInfo& smi)
{
	assert(!m_IsReadOnly);
	DoOpenStorage(smi, dms_rw_mode::write_only_all);
}

void AbstrStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	assert(!m_CriticalSection.try_acquire()); // must already be locked
	assert(!IsOpen());
}

void AbstrStorageManager::DoCloseStorage(bool mustCommit) const
{}

void AbstrStorageManager::DoWriteTree(const TreeItem* storageHolder)
{}

void AbstrStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	if (curr != storageHolder)
		return;

	if (IsDataItem(curr))
		if (!curr->IsStorable())
			if (curr->HasCalculator())
				curr->Fail("Item has both a Calculation Rule and a read-only storage spec", FailType::MetaInfo);
}

// ===========================================================================
// Section:     NonmappableStorageManager impl
// ===========================================================================

NonmappableStorageManager::NonmappableStorageManager()
{}

NonmappableStorageManager::~NonmappableStorageManager()
{}

auto NonmappableStorageManager::ReaderClone(StorageMetaInfoPtr smi) const -> std::unique_ptr<StorageReadHandle>
{
	auto cls = dynamic_cast<const StorageClass*>(GetDynamicClass());
	MG_CHECK(cls);
	NonmappableStorageManagerRef result = debug_cast<NonmappableStorageManager*>(cls->CreateObj());
	assert(result);

	result->InitStorageManager(GetNameStr().c_str(), true);
	return std::make_unique<StorageReadHandle>(result, std::move(smi));
}

StorageMetaInfoPtr NonmappableStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* focusItem, StorageAction) const
{
	return std::make_unique<StorageMetaInfo>(storageHolder, focusItem);
}

FileResult AbstrStorageManager::ReadDataItem  (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) 
{ 
	throwIllegalAbstract(MG_POS, "AbstrStorageManager::ReadDataItem");
}

FileResult AbstrStorageManager::WriteDataItem(StorageMetaInfoPtr&& smi)
{
	throwIllegalAbstract(MG_POS, "AbstrStorageManager::WriteDataItem");
}

bool AbstrStorageManager::ReadUnitRange (const StorageMetaInfo& smi) const
{
	throwIllegalAbstract(MG_POS, "AbstrStorageManager::ReadUnitRange");
}

bool AbstrStorageManager::WriteUnitRange(StorageMetaInfoPtr&& smi)  
{
	throwIllegalAbstract(MG_POS, "AbstrStorageManager::WriteUnitRange");
}


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

void NonmappableStorageManager::DropStream(const TreeItem* item, CharPtr path)
{
	throwIllegalAbstract(MG_POS, this, "DropStream");
}

void NonmappableStorageManager::StartInterest(const TreeItem* storageHolder, const TreeItem* self) const
{
	interest_holders_container interestHolders;

	auto visitorImpl = [&interestHolders](const Actor* item) 
		{ 
			if (!item->IsPassor()) 
				if (auto sa = dynamic_cast<const SharedActor*>(item))
					interestHolders.emplace_back(sa); 
		};
	auto visitor = MakeDerivedProcVisitor(std::move(visitorImpl));

	VisitSuppliers(SupplierVisitFlag::StartSupplInterest, visitor, storageHolder, self);

	if (interestHolders.size())
		m_InterestHolders[interest_holders_key(storageHolder, self)].swap(interestHolders);
	else
		StopInterest(storageHolder, self);
}

void NonmappableStorageManager::StopInterest(const TreeItem* storageHolder, const TreeItem* self) const noexcept
{
	m_InterestHolders.erase(interest_holders_key(storageHolder, self));
}

// Wrapper functions for consistent calls to specific StorageManager overrides

bool AbstrStorageManager::OpenForRead(const StorageMetaInfo& smi) const
{
	assert(smi.StorageHolder());

	const TreeItem* storageHolder = smi.StorageHolder();
	assert(!m_IsOpen);
	assert(!m_CriticalSection.try_acquire()); // check that CS was already taken
	if (m_IsOpen) 
		return true;
	try {
		DoOpenStorage(smi, dms_rw_mode::read_only);
		m_IsOpen = true;
	}
	catch (...)
	{
		if (!smi.m_MustRememberFailure)
			throw;
		storageHolder->CatchFail(FailType::MetaInfo);
		storageHolder->ThrowFail();
	}
	return true;
}

void AbstrStorageManager::OpenForWrite(const StorageMetaInfo& smi) // PRECONDITION !m_IsReadOnly
{
	if (m_IsReadOnly)
		throwItemError("Storage is read only - write request is denied");
	assert(!m_CriticalSection.try_acquire());  // check that CS was already taken

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

	assert(!m_CriticalSection.try_acquire()); // must already be locked by caller

	DoCloseStorage(m_Commit && m_IsOpenedForWrite);
	m_IsOpen = false;
}

IMPL_CLASS(AbstrStorageManager, nullptr)
IMPL_CLASS(NonmappableStorageManager, nullptr)

TIC_CALL const Class* DMS_CONV DMS_AbstrStorageManager_GetStaticClass()
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
	, m_OptionsItem(storageOptionsPropDefPtr->HasNonDefaultValue(storageHolder) ? nullptr : storageHolder->FindItem("GDAL_Options"))
	, m_DriverItem (storageDriverPropDefPtr ->HasNonDefaultValue(storageHolder) ? nullptr : storageHolder->FindItem("GDAL_Driver"))
	, m_LayerCreationOptions(storageHolder->FindItem("GDAL_LayerCreationOptions"))
	, m_ConfigurationOptions(storageHolder->FindItem("GDAL_ConfigurationOptions"))
{
	if (storageOptionsPropDefPtr->HasNonDefaultValue(storageHolder))
		m_Options = storageOptionsPropDefPtr->GetValue(storageHolder).c_str();

	if (storageDriverPropDefPtr->HasNonDefaultValue(storageHolder))
		m_Driver = storageDriverPropDefPtr->GetValue(storageHolder).c_str();

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

StorageCloseHandle::StorageCloseHandle(NonmappableStorageManager* storageManager, const TreeItem* storageHolder, const TreeItem* focusItem, StorageAction sa)
	: m_StorageManager(storageManager)
	, m_StorageLock(storageManager->m_CriticalSection)
	, m_MetaInfo(storageManager->GetMetaInfo(storageHolder, const_cast<TreeItem*>(focusItem), sa))
{}

StorageCloseHandle::StorageCloseHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi)
	: m_StorageManager(storageManager)
	, m_StorageLock(storageManager->m_CriticalSection)
	, m_MetaInfo(std::move(smi))
{}

StorageCloseHandle::~StorageCloseHandle()
{
	assert(StorageManager());
	if (StorageManager()->IsOpen())
		MetaInfo()->OnClose(this);
	StorageManager()->CloseStorage();
	m_MetaInfo.reset();
}

StorageReadHandle::StorageReadHandle(NonmappableStorageManager* sm, const TreeItem* storageHolder, TreeItem* focusItem, StorageAction sa, bool mustRememberFailure)
	: StorageCloseHandle(sm, storageHolder, focusItem, sa)
{
	m_MetaInfo->m_MustRememberFailure = mustRememberFailure;
	Init();
}

StorageReadHandle::StorageReadHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi)
: StorageCloseHandle(storageManager, std::move(smi))
{
	Init();
}

void StorageReadHandle::Init()
{
	assert(StorageManager());
	assert(StorageHolder ());
	assert(MetaInfo());
	StorageManager()->OpenForRead(*MetaInfo());
	if (StorageManager()->IsOpen())
		MetaInfo()->OnOpenForRead(this);
}

bool StorageReadHandle::Read() const
{
	assert(FocusItem     ()); // note that storageHolder may be nullptr
	assert(StorageManager());
	if (!StorageManager()->IsOpen())
		return false;

	return FocusItem()->DoReadItem(MetaInfo());
}


StorageWriteHandle::StorageWriteHandle(NonmappableStorageManager* storageManager, StorageMetaInfoPtr&& smi)
	:	StorageCloseHandle(storageManager, std::move(smi))
{
	assert(MetaInfo());
	assert(!StorageManager()->IsOpen());

	//	dms_assert(storageHolder);
	StorageManager()->OpenForWrite(*MetaInfo());
}

// *****************************************************************************
// 
// AbstrStorageManager::ExportMetaInfo
//
// *****************************************************************************

#include "xml/PropWriter.h"

void GenerateMetaInfo(AbstrPropWriter& apw, const TreeItem* curr, const TreeItem* contents)
{
	assert(contents);
	assert(IsMainThread());

	GenerateSystemInfo(apw, curr);

	InterestRetainContextBase irc;

	for (const TreeItem* section = contents->GetFirstVisibleSubItem(); section; section = section->GetNextVisibleItem())
	{
		const_cast<TreeItem*>(section)->SetKeepDataState(true);
		irc.Add(section);

		SharedStr sectionName( section->GetID().AsStrRange() );
		apw.OpenSection(sectionName.c_str());
		for (const TreeItem* key = section->GetFirstVisibleSubItem(); key; key = key->GetNextVisibleItem())
		{
			const_cast<TreeItem*>(key)->SetKeepDataState(true);
			irc.Add(key);

			key->CertainUpdate(ProgressState::Committed, "GenerateMetaInfo");

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
	FencedInterestRetainContext holdCalcResultsHere("ExportMetaInfoToFile");

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
