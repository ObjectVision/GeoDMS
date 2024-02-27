// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <numbers>

// *****************************************************************************
// Implementations of GdalVectSM
// *****************************************************************************

#include "gdal_base.h"
#include "gdal_vect.h"

#include <gdal.h>
#include <proj.h>
#include <ogr_api.h>
#include <cpl_csv.h>
#include <ogrsf_frmts.h>
#include <boost/algorithm/string.hpp>

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "geo/PointOrder.h"
#include "geo/StringArray.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "LockLevels.h"

#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "LispTreeType.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include "stg/StorageClass.h"
#include <filesystem>
#include "Projection.h"

// include windows for LoadLibrary
#include <windows.h>

// declaration, should have been made in cpl_csv.h

// ------------------------------------------------------------------------
// Implementation of the abstact storagemanager interface
// ------------------------------------------------------------------------
#include "dbg/SeverityType.h"

SeverityTypeID gdalSeverity(CPLErr st)
{
	switch (st) {
	case CE_None:    return SeverityTypeID::ST_Nothing;
	case CE_Debug:   return SeverityTypeID::ST_MinorTrace;
	case CE_Warning:return SeverityTypeID::ST_Warning;
	case CE_Failure: return SeverityTypeID::ST_Error;
	case CE_Fatal:   return SeverityTypeID::ST_FatalError;
	default: throwIllegalAbstract(MG_POS, "gdalSeverity");
	}
}

CharPtr gdal2CharPtr(CPLErr st)
{
	switch (st) {
	case CE_Debug:   return "Debug";
	case CE_Warning:return "Warning";
	case CE_Failure: return "Failure";
	case CE_Fatal:   return "Fatal";
	default: throwIllegalAbstract(MG_POS, "gdalSeverity");
	}
}


// *****************************************************************************

namespace gdalComponentImpl
{
	UInt32          s_ComponentCount = 0;
	CPLErrorHandler s_OldErrorHandler = nullptr;

	THREAD_LOCAL UInt32 s_TlsCount = 0;

	leveled_critical_section gdalSection(item_level_type(0), ord_level_type::GDALComponent, "gdalComponent");

	std::map<SharedStr, SharedStr>* s_HookedFilesPtr = nullptr;

	CharPtr HookFilesToExeFolder(CharPtr fileName, CharPtr subFolder)
	{
		leveled_critical_section::scoped_lock lock(gdalSection);

		dms_assert(s_HookedFilesPtr != nullptr);
		SharedStr& fullFileName = (*s_HookedFilesPtr)[SharedStr(fileName)];
		if (fullFileName.empty())
		{
			fullFileName = ConvertDmsFileNameAlways(DelimitedConcat(GetExeDir().c_str(), DelimitedConcat(subFolder, fileName).c_str()));

			MG_DEBUGCODE(
				reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_MajorTrace, "Hook to GDAL file: %s", fullFileName.c_str());

			)
				if (!IsFileOrDirAccessible(fullFileName))
					reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_Warning, "Hook to unknown GDAL file: %s", fullFileName.c_str());
		}
		return fullFileName.c_str();
	}
	CharPtr HookFilesToExeFolder1(CharPtr pszBasename) {
		return HookFilesToExeFolder(pszBasename, "gdaldata");
	}

	CharPtr HookFilesToExeFolder2(CharPtr pszClass, CharPtr pszBasename) {
		return HookFilesToExeFolder(pszBasename, "gdaldata");
	}

	CharPtr proj_HookFilesToExeFolder(pj_ctx*, CharPtr pszBasename, void* user_data)
	{
		return HookFilesToExeFolder(pszBasename, "proj4data");
	}


	THREAD_LOCAL GDAL_ErrorFrame* s_ErrorFramePtr = nullptr;

	void ErrorHandlerImpl(CPLErr eErrClass, int err_no, const char* msg)
	{
		if (eErrClass >= CE_Failure)
			throwErrorF("gdal", "error(%d): %s", err_no, msg);
	}

	void __stdcall ErrorHandler(CPLErr eErrClass, int err_no, const char* msg)
	{
		//		AbstrContextHandle* ach = AbstrContextHandle::GetLast();
		SeverityTypeID st = gdalSeverity(eErrClass);
		MakeMin(st, SeverityTypeID::ST_Warning); // downplay gdal errors for now.

		reportF(st, "gdal %s(%d): %s "
			, gdal2CharPtr(eErrClass)
			, err_no
			, msg
		);

		if (s_ErrorFramePtr)
			s_ErrorFramePtr->RegisterError(dms_CPLErr(eErrClass), err_no, msg);
	}

}	// namespace gdalComponentImpl

GDAL_ErrorFrame::GDAL_ErrorFrame()
	: m_eErrClass(dms_CPLErr(CE_None))
	, m_Prev(gdalComponentImpl::s_ErrorFramePtr)
	, m_prev_proj_err_no(GetProjectionContextErrNo())

{
	m_nr_uncaught_exceptions = std::uncaught_exceptions();
	gdalComponentImpl::s_ErrorFramePtr = this;
}

void GDAL_ErrorFrame::ThrowUpWhateverCameUp()
{
	if (HasError())
	{
		auto errClass = ReleaseError();
		gdalComponentImpl::ErrorHandlerImpl(CPLErr(errClass), m_err_no, m_msg.c_str());
	}
	auto projErrNo = GetProjectionContextErrNo();
	if (!projErrNo)
		return;
	if (m_prev_proj_err_no == projErrNo)
		return;
	m_prev_proj_err_no = projErrNo; // avoid repeated calling from the nearing destructor.
	auto pjCtx = GetProjectionContext();
	auto projErrStr = SharedStr(proj_context_errno_string(pjCtx, projErrNo));
	throwErrorF("proj", "error(%d): %s", projErrNo, projErrStr.c_str());
}

GDAL_ErrorFrame::~GDAL_ErrorFrame()  noexcept(false)
{
	gdalComponentImpl::s_ErrorFramePtr = m_Prev;

	assert(m_nr_uncaught_exceptions <= std::uncaught_exceptions());
	if (m_nr_uncaught_exceptions == std::uncaught_exceptions())
		ThrowUpWhateverCameUp();
}

void GDAL_ErrorFrame::RegisterError(dms_CPLErr eErrClass, int err_no, const char* msg)
{
	if (eErrClass > m_eErrClass)
	{
		m_eErrClass = eErrClass;
		m_err_no = err_no;
		m_msg = msg;
	}
	auto projErrStr = GetProjectionContextErrorString();
	if (projErrStr.empty())
		return;

	m_msg += mySSPrintF("\n%s", projErrStr.c_str());
}

struct pj_ctx* GDAL_ErrorFrame::GetProjectionContext()
{
	//	return reinterpret_cast<PJ_CONTEXT*>(CPLGetTLS(CTLS_PROJCONTEXTHOLDER));
	//	return OSRGetProjTLSContext();
	return nullptr;
}

int GDAL_ErrorFrame::GetProjectionContextErrNo()
{
	auto pjCtx = GetProjectionContext();
	//	if (!pjCtx)
	//		return 0;
	return proj_context_errno(pjCtx);
}

SharedStr GDAL_ErrorFrame::GetProjectionContextErrorString()
{
	auto pjCtx = GetProjectionContext();
	//	if (!pjCtx)
	//		return {};
	auto pjErrno = GetProjectionContextErrNo();
	if (!pjErrno)
		return {};

	return mySSPrintF("Proj(%d): %s"
		, pjErrno
		, proj_context_errno_string(pjCtx, pjErrno)
	);
}

void gdalCleanup()
{
	//	SetCSVFilenameHook(nullptr);
	if (gdalComponentImpl::s_HookedFilesPtr != nullptr) {
		delete gdalComponentImpl::s_HookedFilesPtr;
		gdalComponentImpl::s_HookedFilesPtr = nullptr;
	}

	if (gdalComponentImpl::s_OldErrorHandler) {
		CPLSetErrorHandler(gdalComponentImpl::s_OldErrorHandler);
		gdalComponentImpl::s_OldErrorHandler = nullptr;
	}

	GDALDestroyDriverManager();
	OGRCleanupAll();
	//proj_cleanup();
	OSRCleanup();
	//	CPLCleanupTLS();
}

gdalDynamicLoader::gdalDynamicLoader()
{
}

bool AuthorityCodeIsValidCrs(std::string_view wkt)
{
	auto srs = OGRSpatialReference();
	srs.SetFromUserInput(wkt.data());

	auto is_geographic = srs.IsGeographic();
	auto is_derived_geographic = srs.IsDerivedGeographic();
	auto is_projected = srs.IsProjected();
	auto is_local = srs.IsLocal();
	auto is_dynamic = srs.IsDynamic();
	auto is_geocentric = srs.IsGeocentric();
	auto is_vertical = srs.IsVertical();
	auto is_compound = srs.IsCompound();
	return is_geographic || is_derived_geographic || is_projected || is_local || is_dynamic || is_geocentric || is_vertical || is_compound;
}

void ValidateSpatialReferenceFromWkt(OGRSpatialReference* ogrSR, CharPtr wkt_prj_str)
{
	assert(ogrSR);
	ogrSR->Validate();
	CplString pszEsriwkt;
	ogrSR->exportToWkt(&pszEsriwkt.m_Text);
	if (std::strlen(wkt_prj_str) > 20 && strcmp(pszEsriwkt.m_Text, wkt_prj_str)) // TODO: replace hardcoded 20 characters to get past strings that are ie. EPSG:XXXX
		reportF(SeverityTypeID::ST_MinorTrace, "PROJ reinterpreted user input wkt projection definition: %s", wkt_prj_str);
}

void GDALDatasetHandle::UpdateBaseProjection(const TreeItem* treeitem, const AbstrUnit* uBase) const
{
	assert(uBase);
	assert(dsh_);

	auto ogrSR_ptr = dsh_->GetSpatialRef();
	if (!ogrSR_ptr)
		ogrSR_ptr = dsh_->GetGCPSpatialRef();

	if (uBase->GetDescr().empty() && ogrSR_ptr)
	{
		CharPtr projName = nullptr;
		if (ogrSR_ptr)
			projName = ogrSR_ptr->GetName();
		if (projName == nullptr)
			projName = dsh_->GetProjectionRef();

		//		if (!uBase->IsCacheItem() && uBase->GetDescr().empty())
		//			const_cast<AbstrUnit*>(uBase)->SetDescr(SharedStr(projName));
	}

	std::optional<OGRSpatialReference> ogrSR;
	if (ogrSR_ptr)
		ogrSR = *ogrSR_ptr; // make a copy if necessary as UpdateBaseProjection may return another one that must be destructed
	CheckSpatialReference(ogrSR, treeitem, uBase); // update based on this external ogrSR, but use base's Format-specified EPGS when available
}

SharedStr GetAsWkt(const OGRSpatialReference* sr)
{
	assert(sr);
	CplString pszEsriwkt;
	auto err = sr->exportToWkt(&pszEsriwkt.m_Text);

	if (err != OGRERR_NONE)
		return {};
	return SharedStr(pszEsriwkt.m_Text);
}

auto GetSpatialReferenceFromUserInput(SharedStr wktPrjStr) -> std::pair<OGRSpatialReference, OGRErr>
{
	assert(!wktPrjStr.empty());

	gdalComponent lock_use_gdal;

	GDAL_ErrorFrame	error_frame;

	OGRSpatialReference	src;
	OGRErr err = src.SetFromUserInput(wktPrjStr.c_str());

	error_frame.ReleaseError();

	return { src, err };
}

auto GetUnitlabeledScalePair(TokenID wktPrjToken) -> UnitLabelScalePair
{
	if (!wktPrjToken)
		return {};

	auto wktPrjStr = AsString(wktPrjToken);
	auto srOrErr = GetSpatialReferenceFromUserInput(wktPrjStr);
	if (srOrErr.second != OGRERR_NONE)
		return {};

	auto sr = srOrErr.first;
	CharPtr metricUnitStr;

	auto unitScale = sr.GetLinearUnits(&metricUnitStr);
	MG_CHECK(metricUnitStr != nullptr);
	if (strcmp(metricUnitStr, "unknown") == 0)
	{
		unitScale = sr.GetAngularUnits(&metricUnitStr);
	}
	return { GetTokenID_mt(metricUnitStr), unitScale };
}

extern TIC_CALL GetUnitlabeledScalePairFuncType s_GetUnitlabeledScalePairFunc;

struct SetUnitlabeledScalePairFuncType
{
	SetUnitlabeledScalePairFuncType()
	{
		s_GetUnitlabeledScalePairFunc = GetUnitlabeledScalePair;
	}
	~SetUnitlabeledScalePairFuncType()
	{
		s_GetUnitlabeledScalePairFunc = nullptr;
	}
};

static SetUnitlabeledScalePairFuncType s_SetUnitlabeledScalePairFunc;


SharedStr GetWkt(const OGRSpatialReference* sr)
{
	assert(sr);

	GDAL_ErrorFrame	error_frame;

	auto wktPrjStr = GetAsWkt(sr);

	error_frame.ReleaseError();

	return wktPrjStr;
}

SharedStr GetWktProjectionFromBaseProjectionUnit(const AbstrUnit* base)
{
	gdalComponent lock_use_gdal;

	auto srStr = SharedStr(base->GetSpatialReference());

	if (srStr.empty())
		return {};

	auto srOrErr = GetSpatialReferenceFromUserInput(srStr);

	if (srOrErr.second != OGRERR_NONE)
		return {};

	return GetWkt(&srOrErr.first);
}

auto GetBaseProjectionUnitFromValuesUnit(const AbstrDataItem* adi) -> const AbstrUnit*
{
	const AbstrUnit* valuesUnit = adi->GetAbstrValuesUnit();
	auto curr_proj = valuesUnit->GetProjection();
	if (!curr_proj)
		return valuesUnit;
	auto result = curr_proj->GetCompositeBase();
	assert(result);
	return result;
}

SharedStr GetWktProjectionFromValuesUnit(const AbstrDataItem* adi)
{
	auto baseProjectionUnit = GetBaseProjectionUnitFromValuesUnit(adi);
	return GetWktProjectionFromBaseProjectionUnit(baseProjectionUnit);
}

void CheckCompatibility(const TreeItem* treeitem, OGRSpatialReference* fromGDAL, OGRSpatialReference* fromConfig)
{
	assert(fromGDAL);
	assert(fromConfig);

	if (fromGDAL->IsSame(fromConfig))
		return;

	SharedStr authority_code_from_gdal = SharedStr(fromGDAL->GetAuthorityName(NULL)) + ":" + fromGDAL->GetAuthorityCode(NULL);
	SharedStr authority_code_from_value_unit = SharedStr(fromConfig->GetAuthorityName(NULL)) + ":" + fromConfig->GetAuthorityCode(NULL);

	if (authority_code_from_gdal == authority_code_from_value_unit)
		return;

	reportF(SeverityTypeID::ST_Warning, "GDAL: item [[%s]] spatial reference (%s) differs from the spatial reference (%s) GDAL obtained from dataset"
		, treeitem->GetFullName().c_str()
		, authority_code_from_gdal.c_str()
		, authority_code_from_value_unit.c_str()
	);
}

auto ConvertProjectionStrToAuthorityIdentifierAndCode(const std::string projection) -> SharedStr
{
	return {};
}

void CheckSpatialReference(std::optional<OGRSpatialReference>& ogrSR, const TreeItem* treeitem, const AbstrUnit* uBase)
{
	if (!ogrSR) // dataset spatial reference does not exist, no check possible.
		return;

	assert(IsMainThread());
	assert(uBase);
	auto projection = uBase->GetProjectionStr(FormattingFlags::None);
	SharedStr wktPrjStr(uBase->GetSpatialReference());

	if (wktPrjStr.empty())
		return;

	auto spOrErr = GetSpatialReferenceFromUserInput(wktPrjStr);
	if (spOrErr.second != OGRERR_NONE)
	{
		auto fullName = SharedStr(uBase->GetFullName());
		reportF(SeverityTypeID::ST_Warning, "BaseProjection unit %s has projection with error %d", fullName, spOrErr.second);
	}
	if (ogrSR)
	{
		ValidateSpatialReferenceFromWkt(&spOrErr.first, wktPrjStr.c_str());
		CheckCompatibility(treeitem, &*ogrSR, &spOrErr.first);
	}
	else
		ogrSR = spOrErr.first;
}


#include "proj.h"

gdalThread::gdalThread()
{
	if (!gdalComponentImpl::s_TlsCount)
	{
		//		DMS_SE_CALLBACK_BEGIN

		CPLPushFileFinder(gdalComponentImpl::HookFilesToExeFolder2); // can throw SE
		//			proj_context_set_file_finder(nullptr, gdalComponentImpl::proj_HookFilesToExeFolder, nullptr);

		static auto projFolder = DelimitedConcat(GetExeDir(), "proj4data");
		CharPtr projFolderPtr[] = { projFolder.c_str(), nullptr };
		OSRSetPROJSearchPaths(projFolderPtr);

		//		DMS_SE_CALLBACK_END // will throw a DmsException in case a SE was raised
	}
	++gdalComponentImpl::s_TlsCount;
}

gdalThread::~gdalThread()
{
	if (!--gdalComponentImpl::s_TlsCount)
	{
		//	proj_context_set_file_finder(nullptr, nullptr, nullptr);
		//		OSRCleanup();
		CPLCleanupTLS();
		CPLPopFileFinder();
	}
}

gdalComponent::gdalComponent()
{
	leveled_critical_section::scoped_lock lock(gdalComponentImpl::gdalSection);

	// TODO: test, experimental
	//CPLSetConfigOption("OGR_SQLITE_CACHE", "512");
	//CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");

	if (!gdalComponentImpl::s_ComponentCount)
	{
		try {
			assert(gdalComponentImpl::s_OldErrorHandler == nullptr);
			gdalComponentImpl::s_OldErrorHandler = CPLSetErrorHandler(gdalComponentImpl::ErrorHandler); // can throw

			assert(gdalComponentImpl::s_HookedFilesPtr == nullptr);
			gdalComponentImpl::s_HookedFilesPtr = new std::map<SharedStr, SharedStr>; // can throw


			// Set the Proj context on the GDAL/OGR library
			CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION", "YES");
			CPLSetThreadLocalConfigOption("OGR_ENABLE_PARTIAL_REPROJECTION_THREADS", "YES");
			CPLSetThreadLocalConfigOption("PROJ_THREAD_SAFE", "YES");

			//			SetCSVFilenameHook(gdalComponentImpl::HookFilesToExeFolder1);
			//			proj_context_set_file_finder(nullptr, gdalComponentImpl::proj_HookFilesToExeFolder, nullptr);

						// Note: moved registering of drivers to Gdal_DoOpenStorage
						//GDALAllRegister(); // can throw
						//OGRRegisterAll(); // can throw
		}
		catch (...)
		{
			gdalCleanup();
			throw;
		}
	}
	++gdalComponentImpl::s_ComponentCount;
}

bool gdalComponent::isActive()
{
	return gdalComponentImpl::s_ComponentCount > 0;
}

gdalComponent::~gdalComponent()
{
	leveled_critical_section::scoped_lock lock(gdalComponentImpl::gdalSection);
	return; // MEMORY LEAK, prevent issue 169
	if (!--gdalComponentImpl::s_ComponentCount)
	{
		//		proj_context_set_file_finder(nullptr, nullptr, nullptr);
		gdalCleanup();
	}
}

CplString::~CplString()
{
	if (m_Text)
		CPLFree(m_Text);
}

typedef DataArray<SharedStr> StringDataItem;
typedef DataArray<Bool> BoolDataItem;

void gdalRaster_CreateMetaInfo(TreeItem* container, bool mustCalc)
{
	AbstrUnit* auTableGroups = Unit<UInt32>::GetStaticClass()->CreateUnit(container, GetTokenID_mt("gdal_grid"));
	AbstrDataItem* adiShortName = CreateDataItem(auTableGroups, GetTokenID_mt("ShortName"), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiLongName = CreateDataItem(auTableGroups, GetTokenID_mt("LongName"), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiHelpUrl = CreateDataItem(auTableGroups, GetTokenID_mt("HelpUrl"), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiOptions = CreateDataItem(auTableGroups, GetTokenID_mt("CreationOptionList"), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());

	if (mustCalc)
	{
		gdalComponent lockGDALRegister;
		SizeT nrDrivers = GDALGetDriverCount();
		auTableGroups->SetCount(nrDrivers);

		DataWriteLock res1Lock(adiShortName, dms_rw_mode::write_only_mustzero);
		DataWriteLock res2Lock(adiLongName, dms_rw_mode::write_only_mustzero);
		DataWriteLock res3Lock(adiHelpUrl, dms_rw_mode::write_only_mustzero);
		DataWriteLock res4Lock(adiOptions, dms_rw_mode::write_only_mustzero);

		auto shortNameData = mutable_array_cast<SharedStr>(res1Lock)->GetDataWrite();
		auto longNameData = mutable_array_cast<SharedStr>(res2Lock)->GetDataWrite();
		auto helpUrlData = mutable_array_cast<SharedStr>(res3Lock)->GetDataWrite();
		auto optionsData = mutable_array_cast<SharedStr>(res4Lock)->GetDataWrite();

		for (SizeT i = 0; i != nrDrivers; ++i)
		{
			GDALDriverH drH = GDALGetDriver(i);

			Assign(shortNameData[i], GDALGetDriverShortName(drH));
			Assign(longNameData[i], GDALGetDriverLongName(drH));
			Assign(helpUrlData[i], GDALGetDriverHelpTopic(drH));
			Assign(optionsData[i], GDALGetDriverCreationOptionList(drH));
		}

		res1Lock.Commit();
		res2Lock.Commit();
		res3Lock.Commit();
		res4Lock.Commit();
	}
}

void gdalVector_CreateMetaInfo(TreeItem* container, bool mustCalc)
{
	AbstrUnit* auTableGroups = Unit<UInt32>::GetStaticClass()->CreateUnit(container, GetTokenID_mt("gdal_vect"));
	AbstrDataItem* adiShortName = CreateDataItem(auTableGroups, GetTokenID_mt("Name"), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiCanCreate = CreateDataItem(auTableGroups, GetTokenID_mt("CanCreate"), auTableGroups, Unit<Bool     >::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiCanDelete = CreateDataItem(auTableGroups, GetTokenID_mt("CanDelete"), auTableGroups, Unit<Bool     >::GetStaticClass()->CreateDefault());

	if (mustCalc)
	{
		gdalVectComponent lockOGRRegister;
		SizeT nrDrivers = OGRGetDriverCount();
		auTableGroups->SetCount(nrDrivers);
		DataWriteLock res1Lock(adiShortName, dms_rw_mode::write_only_mustzero);
		DataWriteLock res2Lock(adiCanCreate, dms_rw_mode::write_only_mustzero);
		DataWriteLock res3Lock(adiCanDelete, dms_rw_mode::write_only_mustzero);

		auto diShortName = mutable_array_cast<SharedStr>(res1Lock);
		auto diCanCreate = mutable_array_cast<Bool>(res2Lock);
		auto diCanDelete = mutable_array_cast<Bool>(res3Lock);

		auto shortNameData = diShortName->GetLockedDataWrite();
		auto canCreateData = diCanCreate->GetLockedDataWrite();
		auto canDeleteData = diCanDelete->GetLockedDataWrite();

		for (SizeT i = 0; i != nrDrivers; ++i)
		{
			OGRSFDriverH drH = OGRGetDriver(i);

			Assign(shortNameData[i], OGR_Dr_GetName(drH));
			canCreateData[i] = OGR_Dr_TestCapability(drH, ODrCCreateDataSource);
			canDeleteData[i] = OGR_Dr_TestCapability(drH, ODrCDeleteDataSource);
		}

		res1Lock.Commit();
		res2Lock.Commit();
		res3Lock.Commit();
	}
}

void gdalComponent::CreateMetaInfo(TreeItem* container, bool mustCalc)
{
	gdalRaster_CreateMetaInfo(container, mustCalc);
	gdalVector_CreateMetaInfo(container, mustCalc);
}

// ****************************  Version info

SharedStr GDALDriverDescr(GDALDriverH h)
{
	auto shortName = SharedStr(GDALGetDriverShortName(h));
	auto longName = SharedStr(GDALGetDriverLongName(h));
	if (shortName == longName || longName.empty())
		return shortName;
	return mySSPrintF("%s: %s", shortName, longName);
}

#include "VersionComponent.h"
struct gdalVersionComponent : AbstrVersionComponent
{
	void Visit(ClientHandle clientHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const override
	{
		callBack(clientHandle, componentLevel, GDALVersionInfo("--version"));
#if defined(MG_DEBUG)
		++componentLevel;
		gdalComponent lockGDALRegister;
		int dc = GDALGetDriverCount();
		for (int i = 0; i != dc; ++i)
			callBack(clientHandle, componentLevel,
				GDALDriverDescr(GDALGetDriver(i)).c_str()
			);
#endif
	}
};

gdalVersionComponent s_gdalComponent;

#include "proj.h"

#define PROJ_VERSION_STRING "Proj " BOOST_STRINGIZE(PROJ_VERSION_MAJOR) "." BOOST_STRINGIZE(PROJ_VERSION_MINOR) "." BOOST_STRINGIZE(PROJ_VERSION_PATCH)
VersionComponent s_ProjComponent(PROJ_VERSION_STRING);

// *****************************************************************************

GDALDataType gdalRasterDataType(ValueClassID tid, bool write)
{
	switch (tid) {
	case ValueClassID::VT_Bool:	   return write ? GDT_Unknown : GDT_Byte;
	case ValueClassID::VT_UInt2:   return write ? GDT_Unknown : GDT_Byte;
	case ValueClassID::VT_UInt4:   return write ? GDT_Unknown : GDT_Byte;

	case ValueClassID::VT_UInt8:   return GDT_Byte;
	case ValueClassID::VT_UInt16:  return GDT_UInt16;
	case ValueClassID::VT_UInt32:  return GDT_UInt32;
	case ValueClassID::VT_UInt64:  return GDT_UInt64;

	case ValueClassID::VT_Int8:   return GDT_Int8;
	case ValueClassID::VT_Int16:   return GDT_Int16;
	case ValueClassID::VT_Int32:   return GDT_Int32;
	case ValueClassID::VT_Int64:   return GDT_Int64;

	case ValueClassID::VT_Float32: return GDT_Float32;
	case ValueClassID::VT_Float64: return GDT_Float64;

	case ValueClassID::VT_SPoint:  return GDT_CInt16;   // Complex Int16
	case ValueClassID::VT_IPoint:  return GDT_CInt32;   // Complex Int32
	case ValueClassID::VT_FPoint:  return GDT_CFloat32; // Complex Float32
	case ValueClassID::VT_DPoint:  return GDT_CFloat64; // Complex Float64
	}
	return GDT_Unknown;
}

auto GetListOfDriverFileExts(GDALDriver* driver) -> std::vector<std::string>
{
	std::vector<std::string> driver_exts;
	CPLStringList driver_cpl_exts(CSLTokenizeString(driver->GetMetadataItem(GDAL_DMD_EXTENSIONS)));
	auto ext_count = CSLCount(driver_cpl_exts);

	for (SizeT i = 0; i != ext_count; i++) // loop over extensions of driver i
		driver_exts.push_back(CSLGetField(driver_cpl_exts, i));

	return driver_exts;
}

auto GetListOfRegsiteredGDALDriverShortNames(std::vector<GDALDriver*>& registered_drivers) -> std::vector<std::string>
{
	std::vector<std::string> registered_drivers_shortnames;
	for (auto driver : registered_drivers)
		registered_drivers_shortnames.push_back(GDALGetDriverShortName(driver));

	return registered_drivers_shortnames;
}

auto GetListOfRegisteredGDALDrivers() -> std::vector<GDALDriver*>
{
	std::vector<GDALDriver*> registered_drivers;
	auto driver_manager = GetGDALDriverManager();
	auto driver_count = driver_manager->GetDriverCount();

	for (int i = 0; i != driver_count; i++)
		registered_drivers.push_back(driver_manager->GetDriver(i));

	return registered_drivers;
}

auto FileExtensionToRegisteredGDALDriverShortName(std::string_view ext) -> std::string
{
	std::string driver_short_name = {};
	auto registered_drivers = GetListOfRegisteredGDALDrivers();
	for (auto driver : registered_drivers)
	{
		auto driver_exts = GetListOfDriverFileExts(driver);
		for (auto driver_ext : driver_exts)
		{
			if (driver_ext == ext)
				driver_short_name = GDALGetDriverShortName(driver); // found
		}
	}

	return driver_short_name;
}

int DataItemsWriteStatusInfo::getNumberOfLayers()
{
	return m_LayerAndFieldIDMapping.size();
}

bool DataItemsWriteStatusInfo::fieldIsWritten(TokenID layerID, TokenID fieldID)
{
	return m_LayerAndFieldIDMapping[layerID][fieldID].isWritten;
}

void DataItemsWriteStatusInfo::setFieldIsWritten(TokenID layerID, TokenID fieldID, bool isWritten)
{
	m_LayerAndFieldIDMapping[layerID][fieldID].isWritten = isWritten;
}

void DataItemsWriteStatusInfo::setIsGeometry(TokenID layerID, TokenID geometryFieldID, bool isGeometry)
{
	m_LayerAndFieldIDMapping[layerID][geometryFieldID].isGeometry = isGeometry;
}

void DataItemsWriteStatusInfo::setInterest(TokenID layerID, TokenID fieldID, bool hasInterest)
{
	m_LayerAndFieldIDMapping[layerID][fieldID].doWrite = hasInterest;
}

void DataItemsWriteStatusInfo::SetInterestForDataHolder(TokenID layerID, TokenID fieldID, const AbstrDataItem* adi)
{
	m_LayerAndFieldIDMapping[layerID][fieldID].m_DataHolder = adi;
	m_LayerAndFieldIDMapping[layerID][fieldID].name = SharedStr(fieldID);
	m_LayerAndFieldIDMapping[layerID][fieldID].doWrite = true;
}

void DataItemsWriteStatusInfo::ReleaseAllLayerInterestPtrs(TokenID layerID)
{
	for (auto& fieldInfo : m_LayerAndFieldIDMapping[layerID])
	{
		fieldInfo.second.m_DataHolder = {};
		fieldInfo.second.isWritten = true;
	}
}

void DataItemsWriteStatusInfo::SetLaunderedName(TokenID layerID, TokenID fieldID, SharedStr launderedName)
{
	m_LayerAndFieldIDMapping[layerID][fieldID].launderedName = launderedName;
}

void DataItemsWriteStatusInfo::RefreshInterest(const TreeItem* storageHolder)
{
	for (auto subItem = storageHolder->WalkConstSubTree(nullptr); subItem; subItem = storageHolder->WalkConstSubTree(subItem))
	{
		if (not (IsDataItem(subItem) and subItem->IsStorable()))
			continue;

		auto unitItem = GetLayerHolderFromDataItem(storageHolder, subItem);
		SharedStr layerName(unitItem->GetName().c_str());
		SharedStr fieldName(subItem->GetName().c_str());
		if (subItem->GetInterestCount() && !m_LayerAndFieldIDMapping[GetTokenID_mt(layerName)][GetTokenID_mt(fieldName)].isWritten)
			setInterest(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), true);
		else
			setInterest(GetTokenID_mt(layerName), GetTokenID_mt(fieldName), false);
	}
}

#if defined(MG_DEBUG)
UInt32 DataItemsWriteStatusInfo::sd_ObjCounter = 0;

DataItemsWriteStatusInfo::DataItemsWriteStatusInfo()
{
	++sd_ObjCounter;
}

#endif //defined(MG_DEBUG)

DataItemsWriteStatusInfo::~DataItemsWriteStatusInfo()
{
	for (auto& layerID : m_LayerAndFieldIDMapping)
		ReleaseAllLayerInterestPtrs(layerID.first);
#if defined(MG_DEBUG)
	--sd_ObjCounter;
#endif
}

bool DataItemsWriteStatusInfo::LayerIsReadyForWriting(TokenID layerID)
{
	for (auto& writableField : m_LayerAndFieldIDMapping[layerID])
	{
		if (not writableField.second.doWrite)
			continue;

		auto fieldname_n = writableField.second.name;
		if (not writableField.second.m_DataHolder) // field is not on the writables list.
			return false;
	}
	return true;
}

bool DataItemsWriteStatusInfo::LayerHasBeenWritten(TokenID layerID)
{
	for (auto& fieldInfo : m_LayerAndFieldIDMapping[layerID])
	{
		if (!fieldInfo.second.isWritten)
			return false;
	}
	return true;
}

CPLStringList GetOptionArray(const TreeItem* optionsItem)
{
	CPLStringList result;
	if (optionsItem)
	{
		MG_CHECK(IsDataItem(optionsItem));
		MG_CHECK(AsDataItem(optionsItem)->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == ValueClassID::VT_String);

		DataReadLock lock(AsDataItem(optionsItem));
		auto data = const_array_cast<SharedStr>(optionsItem)->GetLockedDataRead();
		std::vector<char> zeroTerminatedCurrOption;
		for (const auto& value : data)
		{
			zeroTerminatedCurrOption.clear();
			zeroTerminatedCurrOption.reserve(value.size() + 1);
			zeroTerminatedCurrOption.insert(zeroTerminatedCurrOption.end(), value.begin(), value.end());
			zeroTerminatedCurrOption.emplace_back(0x00);
			result.AddString(begin_ptr(zeroTerminatedCurrOption));
		}
	}
	return result;
}

GDAL_ConfigurationOptionsFrame::GDAL_ConfigurationOptionsFrame(const CPLStringList& pszConfigurationOptionsArray)
{
	m_DefaultConfigurationOptions = CPLGetThreadLocalConfigOptions();
	CPLSetThreadLocalConfigOptions(pszConfigurationOptionsArray);
}

GDAL_ConfigurationOptionsFrame::~GDAL_ConfigurationOptionsFrame()
{
	CPLSetThreadLocalConfigOptions(m_DefaultConfigurationOptions);
}

auto GetUnitSizeInMetersFromAngularProjection(std::pair<CharPtr, Float64>& angular_unit) -> Float64
{
	if (boost::iequals(angular_unit.first, "degree"))
	{
		if (angular_unit.second > 0.017 && angular_unit.second < 0.018)
			// wierdly, the size of degree is given in radians an not the number of degrees per unit, which should be 1.0 in case of normal lat-long
			angular_unit.first = "radian";
		else
			return angular_unit.second *= (40000.0 / 360.0) * 1000.0;
	}
	if (boost::iequals(angular_unit.first, "radian")) {
		return angular_unit.second *= (40000.0 / (2.0 * std::numbers::pi_v<Float64>)) * 1000.0;
	}
	throwErrorF("GetUnitSizeInMetersFromAngularProjection", "unknown OGRSpatialReference unitName: '%s'", angular_unit.first);
}

auto GetUnitSizeInMetersFromLinearProjection(std::pair<CharPtr, Float64>& linear_unit) -> Float64
{
	if (!strcmp(linear_unit.first, "km"))
		return linear_unit.second *= 1000.0;
	if (!strcmp(linear_unit.first, "m") || boost::iequals(linear_unit.first, "meter") || boost::iequals(linear_unit.first, "metre"))
		return linear_unit.second;
	throwErrorF("GetUnitSizeInMetersFromLinearProjection", "unknown OGRSpatialReference unitName: '%s'", linear_unit.first);
}

auto GetUnitSizeInMeters(const OGRSpatialReference* sr) -> Float64
{
	std::pair<CharPtr, Float64> linear_unit;
	std::pair<CharPtr, Float64> angular_unit;
	linear_unit.second = sr->GetLinearUnits(&linear_unit.first);
	angular_unit.second = sr->GetAngularUnits(&angular_unit.first);

	if (!strcmp(linear_unit.first, "unknown"))
		return GetUnitSizeInMetersFromAngularProjection(angular_unit);
	else
		return GetUnitSizeInMetersFromLinearProjection(linear_unit);
}

auto GetUnitSizeInMeters(const AbstrUnit* projectionBaseUnit) -> Float64
{
	assert(projectionBaseUnit);
	auto projStr = SharedStr(projectionBaseUnit->GetSpatialReference());
	if (projStr.empty())
		return 1.0;
	auto spOrErr = GetSpatialReferenceFromUserInput(projStr);
	if (spOrErr.second != OGRERR_NONE)
		return 1.0;
	auto result = GetUnitSizeInMeters(&spOrErr.first);
	return result;
}

auto GetAffineTransformationFromDataItem(const TreeItem* storageHolder) -> std::vector<double>
{
	auto affine_transformation = std::vector<double>();

	if (!IsDataItem(storageHolder))
		return {};

	auto adi = AsDataItem(storageHolder);
	const AbstrUnit* colDomain = adi->GetAbstrDomainUnit();
	auto unit_projection = colDomain->GetProjection();
	auto [gridBegin, gridEnd] = colDomain->GetRangeAsDRect();

	DPoint factor = (unit_projection) ? unit_projection->Factor() : DPoint(1.0, 1.0), f2 = factor;
	if (factor.X() < 0) { f2.X() = -factor.X(); gridBegin.Col() = gridEnd.Col(); }
	if (factor.Y() > 0) { f2.Y() = -factor.Y(); gridBegin.Row() = gridEnd.Row(); }
	DPoint offset = ((unit_projection) ? unit_projection->Offset() : DPoint()) + gridBegin * factor + 0.5 * f2;

	affine_transformation.push_back(offset.X());   // x-coordinate of the upper-left corner of the upper-left pixel.
	affine_transformation.push_back(f2.X());       // w-e pixel resolution / pixel width.
	affine_transformation.push_back(Float64(0.0)); // row rotation (typically zero).
	affine_transformation.push_back(offset.Y());   // y-coordinate of the upper-left corner of the upper-left pixel.
	affine_transformation.push_back(Float64(0.0)); // column rotation (typically zero).
	affine_transformation.push_back(f2.Y()); 	   // n-s pixel resolution / pixel height (negative value for a north-up image).
	return affine_transformation;
}

auto GetOGRSpatialReferenceFromDataItems(const TreeItem* storageHolder) -> std::optional<OGRSpatialReference>
{
	if (IsDataItem(storageHolder))
	{
		auto adi = AsDataItem(storageHolder);
		auto wktString = GetWktProjectionFromValuesUnit(adi);

		if (wktString.empty())
		{
			const AbstrUnit* colDomain = adi->GetAbstrDomainUnit();
			auto unit_projection = colDomain->GetProjection();
			if (unit_projection)
				wktString = unit_projection->GetBaseUnit()->GetNameOrCurrMetric(FormattingFlags::None);
		}

		if (!wktString.empty())
		{
			auto srOrErr = GetSpatialReferenceFromUserInput(wktString);
			if (srOrErr.second == OGRERR_NONE)
				return srOrErr.first;
		}
	}

	for (auto subItem = storageHolder->WalkConstSubTree(nullptr); subItem; subItem = storageHolder->WalkConstSubTree(subItem))
	{
		if (not (IsDataItem(subItem) and subItem->IsStorable()))
			continue;

		auto subDI = AsDataItem(subItem);

		auto wktString = GetWktProjectionFromValuesUnit(subDI);

		if (wktString.empty())
			continue;
		auto srOrErr = GetSpatialReferenceFromUserInput(wktString);
		if (srOrErr.second == OGRERR_NONE)
			return srOrErr.first;
	}
	return {};
}

#include "TreeItemUtils.h"
OGRwkbGeometryType GetGeometryTypeFromLayerHolder(const TreeItem* subItem)
{
	auto geot = OGRwkbGeometryType::wkbNone;
	auto vcprev = ValueComposition::Single;

	// get mapping item from unit subitem
	const TreeItem* mapping_item = GetMappingItem(subItem);
	if (!mapping_item)
	{
		auto adu = AsUnit(subItem);
		if (adu)
			mapping_item = GetMappingItem(adu);
	}

	// get geometry item from mapping item
	if (mapping_item)
	{
		auto geometry_item = GeometrySubItem(mapping_item);
		auto adi = AsDataItem(geometry_item);
		auto vc = adi->GetValueComposition();
		auto vci = adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		geot = DmsType2OGRGeometryType(vc);
	}

	/*auto adi = AsDataItem(subItem);
	auto mapping_item_2 = GetMappingItem(adi->GetAbstrDomainUnit());
	auto geometry_item = GeometrySubItem(mapping_item_2);

	for (auto subDataItem = subItem; subDataItem; subDataItem = subItem->WalkConstSubTree(subDataItem))
	{
		if (not (IsDataItem(subDataItem) and subDataItem->IsStorable()))
			continue;

		auto subDI = AsDataItem(subDataItem);
		auto vc = subDI->GetValueComposition();
		auto vci = subDI->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		auto id = subDataItem->GetID();

		if (id == token::geometry)
			return DmsType2OGRGeometryType(vc);

		if (vc >= vcprev && vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon))
		{
			geot = DmsType2OGRGeometryType(vc);
			vcprev = vc;
		}
	}*/



	return geot;
}

const TreeItem* GetLayerHolderFromDataItem(const TreeItem* storageHolder, const TreeItem* subItem)
{
	dms_assert(storageHolder && subItem && storageHolder->DoesContain(subItem)); // PRECONDITION

	const TreeItem* unitItem = subItem;
	while (unitItem && unitItem != storageHolder && not IsUnit(unitItem))
	{
		unitItem = unitItem->GetTreeParent();

		if (unitItem->GetCurrSourceItem() == unitItem->GetTreeParent()) // case of nested unit.
		{
			if (unitItem == storageHolder)
				break;
			unitItem = unitItem->GetTreeParent();
		}
	}
	dms_assert(!unitItem || storageHolder->DoesContain(unitItem)); // INVARIANT ?
	if (!unitItem || !IsUnit(unitItem))
	{
		unitItem = subItem;
		if (subItem != storageHolder)
			unitItem = subItem->GetTreeParent();
	}
	dms_assert(unitItem && storageHolder->DoesContain(unitItem)); // POSTCONDITION ?
	return unitItem;
}

#include <boost/algorithm/string.hpp>

auto FileExtensionToKnownGDALDriverShortName(std::string_view ext) -> std::string
{
	if (ext.size() == 3) // reduce the number of actually evaluated iffs.
	{
		if (boost::iequals(ext, "shp") || boost::iequals(ext, "dbf") || boost::iequals(ext, "shx"))
			return "ESRI Shapefile";

		else if (boost::iequals(ext, "csv"))
			return "CSV";

		else if (boost::iequals(ext, "gml") || boost::iequals(ext, "xml"))
			return "GML";

		else if (boost::iequals(ext, "gdb"))
			return "OpenFileGDB";

		else if (boost::iequals(ext, "tif"))
			return "GTiff";

		else if (boost::iequals(ext, "hdf") || boost::iequals(ext, "he2") || boost::iequals(ext, "he5"))
			return "HDF5";

		else if (boost::iequals(ext, "png"))
			return "PNG";

		else if (boost::iequals(ext, "jpg") || boost::iequals(ext, "jfi") || boost::iequals(ext, "jif"))
			return "JPEG";

		else if (boost::iequals(ext, "bmp"))
			return "BMP";
	}
	else
	{
		if (boost::iequals(ext, "gpkg"))
			return "GPKG";

		else if (boost::iequals(ext, "json") || boost::iequals(ext, "geojson"))
			return "GeoJSON";

		else if (boost::iequals(ext, "tiff"))
			return "GTiff";

		else if (boost::iequals(ext, "nc"))
			return "netCDF";

		else if (boost::iequals(ext, "h4") || boost::iequals(ext, "hdf4") || boost::iequals(ext, "h5") || boost::iequals(ext, "hdf5"))
			return "HDF5";

		else if (boost::iequals(ext, "png"))
			return "PNG";

		else if (boost::iequals(ext, "jpeg") || boost::iequals(ext, "jfif"))
			return "JPEG";

	}
	return {};
}

auto TryRegisterVectorDriverFromKnownDriverShortName(std::string_view knownDriverShortName) -> void
{
	if (knownDriverShortName == "ESRI Shapefile")
		RegisterOGRShape();

	else if (knownDriverShortName == "GPKG")
		RegisterOGRGeoPackage();

	else if (knownDriverShortName == "CSV")
		RegisterOGRCSV();

	else if (knownDriverShortName == "GML")
		RegisterOGRGML();
	
	else if (knownDriverShortName == "MVT")
		RegisterOGRMVT();

	else if (knownDriverShortName == "OpenFileGDB")
		RegisterOGROpenFileGDB();

	else if (knownDriverShortName == "GeoJSON")
	{
		RegisterOGRGeoJSON();
		RegisterOGRGeoJSONSeq();
		RegisterOGRESRIJSON();
		RegisterOGRTopoJSON();
	}
}

auto TryRegisterRasterDriverFromKnownDriverShortName(std::string_view knownDriverShortName) -> void
{
	if (knownDriverShortName == "GTiff")
		GDALRegister_GTiff();

	else if (knownDriverShortName == "netCDF")
		GDALRegister_netCDF();

	else if (knownDriverShortName == "HDF5")
	{
		GDALRegister_BAG();
		GDALRegister_HDF5();
		GDALRegister_HDF5Image();
	}
	else if (knownDriverShortName == "PNG")
		GDALRegister_PNG();

	else if (knownDriverShortName == "JPEG")
		GDALRegister_JPEG();

	else if (knownDriverShortName == "BMP")
		GDALRegister_BMP();

	else if (knownDriverShortName == "MBTiles")
		GDALRegister_MBTiles();
}

auto GDALRegisterTrustedDriverFromKnownDriverShortName(std::string_view knownDriverShortName) -> std::string
{
	if (knownDriverShortName.empty())
		return {};

	TryRegisterVectorDriverFromKnownDriverShortName(knownDriverShortName);
	TryRegisterRasterDriverFromKnownDriverShortName(knownDriverShortName);

	return GetGDALDriverManager()->GetDriverByName(&knownDriverShortName.front()) ? std::string(knownDriverShortName) : "";
}

auto GDALRegisterTrustedDriverFromFileExtension(std::string_view ext) -> std::string
{
	auto knownDriverShortName = FileExtensionToKnownGDALDriverShortName(ext);
	return GDALRegisterTrustedDriverFromKnownDriverShortName(knownDriverShortName);
}

bool Gdal_DetermineIfDriverHasVectorOrRasterCapability(UInt32 gdalOpenFlags, GDALDriver* driver)
{
	if (gdalOpenFlags & GDAL_OF_RASTER)
	{
		if (GDALGetMetadataItem(driver, GDAL_DCAP_RASTER, nullptr) == nullptr) // this driver does not support raster
			return false;
	}
	else if (gdalOpenFlags & GDAL_OF_VECTOR)
	{
		if (GDALGetMetadataItem(driver, GDAL_DCAP_VECTOR, nullptr) == nullptr) // this driver does not support vector
			return false;
	}
	return true;
}

bool dmsAndDriverTypeAreCompatible(std::string_view target_type, std::string_view supported_types_sequence)
{
	int word_begin = 0;
	int word_end = 0;
	for (char const& c : supported_types_sequence)
	{
		if (c == ' ')
		{
			auto supported_type = supported_types_sequence.substr(word_begin, word_end - word_begin);
			if (supported_type.compare(target_type) == 0) // driver supports this value type!
				return true;

			word_begin = word_end + 1;
		}

		word_end++;

		if (c == '\n')
			break;
	}
	return false;
}

bool Gdal_DriverSupportsDmsValueType(UInt32 gdalOpenFlags, ValueClassID dms_value_class_id, ValueComposition dms_value_composition, GDALDriver* driver)
{
	if (gdalOpenFlags & GDAL_OF_RASTER)
	{
		auto raster_driver_supported_value_types = std::string(driver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES));
		auto target_gdal_raster_type = gdalRasterDataType(dms_value_class_id, true);
		return dmsAndDriverTypeAreCompatible(GDALGetDataTypeName(target_gdal_raster_type), raster_driver_supported_value_types);
	}
	else if (gdalOpenFlags & GDAL_OF_VECTOR)
	{
		auto vector_driver_supported_field_data_types = std::string(driver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES));// DmsType2OGRFieldType(dms_value_class_id);
		auto vector_driver_supported_field_data_subtypes = driver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES) ? std::string(driver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES)) : ""; //DmsType2OGRSubFieldType(dms_value_class_id);
		auto target_gdal_vector_type = DmsType2OGRFieldType(dms_value_class_id);
		auto target_gdal_vector_subtype = DmsType2OGRSubFieldType(dms_value_class_id);
		return dmsAndDriverTypeAreCompatible(OGR_GetFieldSubTypeName(target_gdal_vector_subtype), vector_driver_supported_field_data_subtypes) || dmsAndDriverTypeAreCompatible(OGR_GetFieldTypeName(target_gdal_vector_type), vector_driver_supported_field_data_types);
	}

	return true;
}

GDALDatasetHandle Gdal_DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode, UInt32 gdalOpenFlags, bool continueWrite)
{
	dms_assert(rwMode != dms_rw_mode::unspecified);
	if (rwMode == dms_rw_mode::read_write)
		rwMode = dms_rw_mode::write_only_all;

	const TreeItem* storageHolder = smi.StorageHolder();

	SharedStr datasourceName = smi.StorageManager()->GetNameStr();

	auto& gmi = dynamic_cast<const GdalMetaInfo&>(smi);
	int nXSize = 0, nYSize = 0, nBands = 0;
	GDALDataType eType = GDT_Unknown;
	auto optionArray = GetOptionArray(gmi.m_OptionsItem);
	auto driverArray = GetOptionArray(gmi.m_DriverItem);

	if (!gmi.m_Options.empty())
		optionArray.AddString(gmi.m_Options.c_str());

	if (!gmi.m_Driver.empty())
		driverArray.AddString(gmi.m_Driver.c_str());

	GDAL_ErrorFrame gdal_error_frame; // catches errors and properly throws
	GDAL_ConfigurationOptionsFrame config_frame(GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_ConfigurationOptions));

	auto valuesTypeID = ValueClassID::VT_Unknown;
	auto value_composition = ValueComposition::Unknown;
	if (IsDataItem(smi.CurrRI()))
	{
		valuesTypeID = smi.CurrRD()->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		value_composition = smi.CurrRD()->GetValueComposition();
	}
	if (rwMode != dms_rw_mode::read_only && (gdalOpenFlags & GDAL_OF_RASTER) && IsDataItem(smi.CurrRI())) // not a container without a domain
	{

		auto domainUnit = AsDynamicUnit(smi.StorageHolder());
		if (domainUnit == nullptr)
		{
			auto gridData = AsDynamicDataItem(smi.CurrRD());
			if (gridData)
				domainUnit = gridData->GetAbstrDomainUnit();
		}
		if (!domainUnit || !domainUnit->UnifyDomain(smi.CurrRD()->GetAbstrDomainUnit()))
			throwErrorF("GDAL", "Cannot determine domain %s", datasourceName.c_str());

		if (domainUnit->GetNrDimensions() == 2)
		{
			auto range = domainUnit->GetRangeAsIRect();
			auto size = Size(range);
			nXSize = size.Col();
			nYSize = size.Row();
			nBands = 1;

			eType = gdalRasterDataType(valuesTypeID);
			if (valuesTypeID == ValueClassID::VT_Bool) optionArray.AddString("NBITS=1"); // overruling of gdal options
			if (valuesTypeID == ValueClassID::VT_UInt2) optionArray.AddString("NBITS=2");
			if (valuesTypeID == ValueClassID::VT_UInt4) optionArray.AddString("NBITS=3");
			optionArray.AddString("COMPRESS=LZW");
			optionArray.AddString("BIGTIFF=IF_SAFER");
			optionArray.AddString("TFW=YES");
			optionArray.AddString("TILED=YES");
			optionArray.AddString("BLOCKXSIZE=256");
			optionArray.AddString("BLOCKYSIZE=256");
		}
	}

	std::string ext = CPLGetExtension(datasourceName.c_str()); // TLS !
	if (!driverArray.empty()) // user specified list of drivers
	{
		auto n = driverArray.size();

		std::string driverShortName = {};
		for (auto i = 0; i != n; ++i)
		{
			driverShortName = GDALRegisterTrustedDriverFromKnownDriverShortName(driverArray[i]);
			if (driverShortName.empty())
				throwErrorF("GDAL", "cannot register user specified gdal driver from GDAL_Driver array: %s", driverArray[i]);
		}

	}
	else // try to register driver based on file extension and known file formats
	{
		std::string driverShortName = GDALRegisterTrustedDriverFromFileExtension(ext);
		if (!driverShortName.empty())
			driverArray.AddString(driverShortName.c_str());
		else
		{
			GDALAllRegister(); // cannot open file based on trusted drivers
		}
	}

	if (rwMode == dms_rw_mode::read_only)
	{
		GDALDatasetHandle result = GDALDataset::FromHandle(
			GDALOpenEx(datasourceName.c_str()
				, (rwMode > dms_rw_mode::read_only) ? GA_Update : GA_ReadOnly | gdalOpenFlags | GDAL_OF_VERBOSE_ERROR
				, driverArray
				, optionArray
				, nullptr // papszSiblingFiles
			)
		);

		if (gdal_error_frame.HasError())
		{
			throwErrorF("GDAL", "cannot open dataset %s \n\t%s"
				, datasourceName.c_str()
				, gdal_error_frame.GetMsgAndReleaseError().c_str()
			);
		}
		if (result == nullptr)
			throwErrorF("GDAL", "Failed to open %s for reading", datasourceName.c_str());
		return result;
	}

	if (rwMode <= dms_rw_mode::read_write || rwMode == dms_rw_mode::unspecified)
		throwErrorF("GDAL", "Unsupported rwMode %d for %s", int(rwMode), datasourceName.c_str());

	auto path = SharedStr(CPLGetPath(datasourceName.c_str())); // some GDAL drivers cannot create when there is no folder present (ie GPKG)
	if (!std::filesystem::is_directory(path.c_str()) && !std::filesystem::create_directories(path.c_str()))
		throwErrorF("GDAL", "Unable to create directories: %s", path);

	auto driverShortName = GDALRegisterTrustedDriverFromFileExtension(ext); // first option, get from filename ext
	if (driverShortName.empty())
		driverShortName = driverArray.size() ? driverArray[0] : ""; // secondary option, get from driverArray

	auto driver = GetGDALDriverManager()->GetDriverByName(driverShortName.c_str());
	if (!driver)
		throwErrorF("GDAL", "Cannot find driver for %s", datasourceName);

	GDALDatasetHandle result = nullptr;

	if (!continueWrite || driverShortName == "GML" || (gdalOpenFlags & GDAL_OF_RASTER))
	{
		if (std::filesystem::exists(datasourceName.c_str()))
			driver->Delete(datasourceName.c_str()); gdal_error_frame.GetMsgAndReleaseError(); // start empty, release error in case of nonexistance.

		// check for values unit support in driver
		if (!(smi.CurrRI()->GetID() == token::geometry) && !Gdal_DriverSupportsDmsValueType(gdalOpenFlags, valuesTypeID, value_composition, driver))
		{
			auto dms_value_type_token_str = smi.CurrRD()->GetAbstrValuesUnit()->GetValueType()->GetID().GetStr();
			throwErrorF("GDAL", "driver %s does not support writing of values type %s", driverShortName.c_str(), dms_value_type_token_str.c_str());
		}

		result = driver->Create(datasourceName.c_str(), nXSize, nYSize, nBands, eType, optionArray);

		if (gdalOpenFlags & GDAL_OF_RASTER) // set projection if available
		{
			// spatial reference system
			auto spatial_reference_system = GetOGRSpatialReferenceFromDataItems(storageHolder);
			if (spatial_reference_system)
			{
				char* pszSRS_WKT = NULL;
				spatial_reference_system->exportToWkt(&pszSRS_WKT);
				result->SetProjection(pszSRS_WKT);
			}

			// affine transformation
			auto affine_transformation = GetAffineTransformationFromDataItem(storageHolder);
			if (!affine_transformation.empty())
			{
				result->SetGeoTransform(&affine_transformation[0]);
			}
		}

		if (!result && !Gdal_DetermineIfDriverHasVectorOrRasterCapability(gdalOpenFlags, driver))
		{
			if (gdal_error_frame.HasError())
			{
				gdal_error_frame.GetMsgAndReleaseError();
			}

			if (gdalOpenFlags & GDAL_OF_VECTOR)
				throwErrorF("GDAL", "driver %s does not have vector capabilities did you use gdalwrite.vect instead of gdalwrite.grid?", driverShortName.c_str());
			else
				throwErrorF("GDAL", "driver %s does not have raster capabilities did you use gdalwrite.grid instead of gdalwrite.vect?", driverShortName.c_str());

		}

	}
	else
	{
		result = reinterpret_cast<GDALDataset*>(GDALOpenEx(datasourceName.c_str(), GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR, driverArray, nullptr, nullptr));
	}

	if (gdal_error_frame.HasError())
	{
		throwErrorF("GDAL", "cannot open dataset %s\n %s"
			, datasourceName.c_str()
			, gdal_error_frame.GetMsgAndReleaseError().c_str()
		);
	}
	if (result == nullptr)
		throwErrorF("GDAL", "Failed to open %s for writing", datasourceName.c_str());

	return result;
}

// *****************************************************************************

CrdTransformation GetTransformation(gdal_transform gdalTr)
{
	//	Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
	//	Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];

	MG_CHECK(gdalTr[2] == 0);
	MG_CHECK(gdalTr[4] == 0);
	return CrdTransformation(
		shp2dms_order(gdalTr[0], gdalTr[3]),
		shp2dms_order(gdalTr[1], gdalTr[5])
	);
}

// *****************************************************************************

#include "gdal_grid.h"

GDAL_SimpleReader::GDAL_SimpleReader()
{
	GDALRegisterTrustedDriverFromKnownDriverShortName("JPEG");
	GDALRegisterTrustedDriverFromKnownDriverShortName("PNG");
	GDALRegisterTrustedDriverFromKnownDriverShortName("GTiff");
	GDALRegisterTrustedDriverFromKnownDriverShortName("BMP");
}