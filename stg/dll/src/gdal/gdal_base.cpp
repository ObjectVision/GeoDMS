#include "StoragePCH.h"
#pragma hdrstop

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
	switch(st) {
		case CE_None:    return SeverityTypeID::ST_Nothing;
		case CE_Debug:   return SeverityTypeID::ST_MinorTrace;
		case CE_Warning :return SeverityTypeID::ST_Warning;
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
				reportF_without_cancellation_check(SeverityTypeID::ST_MajorTrace, "Hook to GDAL file: %s", fullFileName.c_str());
			
			)
			if (!IsFileOrDirAccessible(fullFileName))
				reportF_without_cancellation_check(SeverityTypeID::ST_Warning, "Hook to unknown GDAL file: %s", fullFileName.c_str());
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
 
	void ErrorHandlerImpl(CPLErr eErrClass, int err_no, const char *msg)
	{
		if (eErrClass >= CE_Failure)
			throwErrorF("gdal", "error(%d): %s", err_no, msg);
	}

	void __stdcall ErrorHandler(CPLErr eErrClass, int err_no, const char *msg)
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
	: m_eErrClass(dms_CPLErr(CE_None) )
	, m_Prev( gdalComponentImpl::s_ErrorFramePtr )
	, m_prev_proj_err_no( GetProjectionContextErrNo() )
{
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
	ThrowUpWhateverCameUp();
	MG_CHECK( !HasError() );
}

void GDAL_ErrorFrame::RegisterError(dms_CPLErr eErrClass, int err_no, const char *msg)
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
	SetCSVFilenameHook(nullptr);
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
	CPLCleanupTLS();
}

gdalDynamicLoader::gdalDynamicLoader()
{
}

auto ValidateSpatialReferenceFromWkt(OGRSpatialReference& ogrSR, SharedStr wkt_prj_str) -> void
{
	ogrSR.Validate();
	CplString pszEsriwkt;
	ogrSR.exportToWkt(&pszEsriwkt.m_Text);
	if (!(SharedStr(pszEsriwkt.m_Text) == wkt_prj_str))
		reportF(SeverityTypeID::ST_Warning, "PROJ interpreted spatial reference from user input %s as %s", wkt_prj_str, pszEsriwkt.m_Text);
}

void GDALDatasetHandle::UpdateBaseProjection(const AbstrUnit* uBase) const
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
	CheckSpatialReference(ogrSR, uBase); // update based on this external ogrSR, but use base's Format-specified EPGS when available
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

/* REMOVE
void sr_releaser::operator ()(OGRSpatialReference* p) const
{
	OSRRelease(p);
}
*/

auto GetOGRSpatialReferenceFromDataItems(const TreeItem* storageHolder) -> std::optional<OGRSpatialReference>
{
	for (auto subItem = storageHolder->WalkConstSubTree(nullptr); subItem; subItem = storageHolder->WalkConstSubTree(subItem))
	{
		if (not (IsDataItem(subItem) and subItem->IsStorable()))
			continue;

		auto subDI = AsDataItem(subItem);

		auto baseProjectionUnit = GetBaseProjectionUnitFromValuesUnit(subDI);
		auto wktPrjStr = SharedStr(baseProjectionUnit->GetSpatialReference());

		if (wktPrjStr.empty())
			continue;
		auto srOrErr = GetSpatialReferenceFromUserInput(wktPrjStr);
		if (srOrErr.second != OGRERR_NONE)
			return srOrErr.first;
	}
	return {};
}

void CheckCompatibility(OGRSpatialReference* fromGDAL, OGRSpatialReference* fromConfig)
{
	assert(fromGDAL);
	assert(fromConfig);
	if (GetAsWkt(fromGDAL) != GetAsWkt(fromConfig))
		throwErrorF("GDAL", "SpatialReferenceSystem that GDAL obtained from Dataset differs from baseProjectionUnit's SpatialReference."
			"\nDataset's SpatialReference:\n%s"
			"\nbaseProjectionUnit's SpatialReference:\n%s"
		, GetAsWkt(fromGDAL).c_str()
		, GetAsWkt(fromConfig).c_str()
		);
}

void CheckSpatialReference(std::optional<OGRSpatialReference>& ogrSR, const AbstrUnit* uBase)
{
	assert(IsMainThread());
	assert(uBase);
//	uBase->UpdateMetaInfo();

	SharedStr wktPrjStr(uBase->GetSpatialReference());
	if (wktPrjStr.empty())
	{
		auto fullName = SharedStr(uBase->GetFullName());
		reportF(SeverityTypeID::ST_Warning, "BaseProjection %s has no projection", fullName);
		return;
	}
	auto spOrErr = GetSpatialReferenceFromUserInput(wktPrjStr);
	if (spOrErr.second != OGRERR_NONE)
	{
		auto fullName = SharedStr(uBase->GetFullName());
		reportF(SeverityTypeID::ST_Warning, "BaseProjection %s has projection with error %d", fullName, spOrErr.second);
	}
	if (ogrSR)
		CheckCompatibility(&*ogrSR, &spOrErr.first);
	else
		ogrSR = spOrErr.first;
}


#include "proj.h"

gdalThread::gdalThread()
{
	if (!gdalComponentImpl::s_TlsCount)
	{
		DMS_SE_CALLBACK_BEGIN

			CPLPushFileFinder(gdalComponentImpl::HookFilesToExeFolder2); // can throw SE
			
		DMS_SE_CALLBACK_END // will throw a DmsException in case a SE was raised
	}
	++gdalComponentImpl::s_TlsCount;
}

gdalThread::~gdalThread()
{
	if (!--gdalComponentImpl::s_TlsCount)
	{
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
			dms_assert(gdalComponentImpl::s_OldErrorHandler == nullptr);
			gdalComponentImpl::s_OldErrorHandler = CPLSetErrorHandler(gdalComponentImpl::ErrorHandler); // can throw

			dms_assert(gdalComponentImpl::s_HookedFilesPtr == nullptr);
			gdalComponentImpl::s_HookedFilesPtr = new std::map<SharedStr, SharedStr>; // can throw

			SetCSVFilenameHook(gdalComponentImpl::HookFilesToExeFolder1);
			proj_context_set_file_finder(nullptr, gdalComponentImpl::proj_HookFilesToExeFolder, nullptr);

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

	if (!--gdalComponentImpl::s_ComponentCount)
	{
		proj_context_set_file_finder(nullptr, nullptr, nullptr);
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
	AbstrDataItem* adiLongName  = CreateDataItem(auTableGroups, GetTokenID_mt("LongName"),  auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiHelpUrl   = CreateDataItem(auTableGroups, GetTokenID_mt("HelpUrl"),   auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
	AbstrDataItem* adiOptions   = CreateDataItem(auTableGroups, GetTokenID_mt("CreationOptionList"),   auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());

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

		for (SizeT i=0; i!=nrDrivers; ++i)
		{
			GDALDriverH drH = GDALGetDriver(i);

			Assign(shortNameData[i], GDALGetDriverShortName(drH));
			Assign(longNameData[i],  GDALGetDriverLongName (drH));
			Assign(helpUrlData[i],   GDALGetDriverHelpTopic(drH));	
			Assign(optionsData[i],   GDALGetDriverCreationOptionList(drH));	
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
	AbstrDataItem* adiShortName = CreateDataItem(auTableGroups, GetTokenID_mt("Name"     ), auTableGroups, Unit<SharedStr>::GetStaticClass()->CreateDefault());
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

		for (SizeT i=0; i!=nrDrivers; ++i)
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
	auto shortName = SharedStr( GDALGetDriverShortName(h) );
	auto longName  = SharedStr( GDALGetDriverLongName(h) );
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
		for (int i=0; i!=dc; ++i)
			callBack(clientHandle, componentLevel, 
				GDALDriverDescr(GDALGetDriver(i)).c_str()
			);
#endif
	}
};

gdalVersionComponent s_gdalComponent;

// *****************************************************************************

GDALDataType gdalDataType(ValueClassID tid)
{
	switch (tid) {
		//		case Int8: 
	case VT_Bool:
	case VT_UInt2:
	case VT_UInt4:
	case VT_UInt8:   return GDT_Byte;
	case VT_UInt16:  return GDT_UInt16;
	case VT_Int16:   return GDT_Int16;

	case VT_UInt32:  return GDT_UInt32;
	case VT_Int32:   return GDT_Int32;
	case VT_Float32: return GDT_Float32;
	case VT_Float64: return GDT_Float64;

	case VT_SPoint:  return GDT_CInt16;   // Complex Int16
	case VT_IPoint:  return GDT_CInt32;   // Complex Int32
	case VT_FPoint:  return GDT_CFloat32; // Complex Float32
	case VT_DPoint:  return GDT_CFloat64; // Complex Float64
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

auto GetListOfRegsiteredGDALDriverShortNames(std::vector<GDALDriver*> &registered_drivers) -> std::vector<std::string>
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

void DataItemsWriteStatusInfo::SetInterestForDataHolder(TokenID layerID, TokenID fieldID, const AbstrDataItem *adi)
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
		MG_CHECK(AsDataItem(optionsItem)->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() == VT_String);

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
	if (spOrErr.second == OGRERR_NONE)
		return 1.0;
	return GetUnitSizeInMeters(&spOrErr.first);
}

OGRwkbGeometryType GetGeometryTypeFromGeometryDataItem(const TreeItem* subItem)
{
	auto geot   = OGRwkbGeometryType::wkbNone;
	auto vcprev = ValueComposition::Single;
	for (auto subDataItem = subItem; subDataItem; subDataItem = subItem->WalkConstSubTree(subDataItem))
	{
		if (not (IsDataItem(subDataItem) and subDataItem->IsStorable()))
			continue;

		auto subDI = AsDataItem(subDataItem);
		auto vc    = subDI->GetValueComposition();
		auto vci   = subDI->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
		auto id    = subDataItem->GetID();

		if (id == token::geometry)
			return DmsType2OGRGeometryType(vci, vc);

		if (vc >= vcprev && vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon))
		{
			geot = DmsType2OGRGeometryType(vci, vc);
			vcprev = vc;
		}
	}
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

auto GDALDriverSupportsUpdating(SharedStr datasourceName) -> bool
{
	if (std::string(CPLGetExtension(datasourceName.c_str())) == "gml")
		return false;

	return true;
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

	else if (knownDriverShortName == "OpenFileGDB")
		RegisterOGROpenFileGDB(); // RegisterOGROpenFileGDB();

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

GDALDatasetHandle Gdal_DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode, UInt32 gdalOpenFlags, bool continueWrite)
{
	dms_assert(rwMode != dms_rw_mode::unspecified);
	if (rwMode == dms_rw_mode::read_write)
		rwMode = dms_rw_mode::write_only_all;

	const TreeItem* storageHolder = smi.StorageHolder(); 

	SharedStr datasourceName = smi.StorageManager()->GetNameStr();

	int nXSize = 0, nYSize = 0, nBands = 0;
	GDALDataType eType = GDT_Unknown;
	auto optionArray = GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_OptionsItem);
	auto driverArray = GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_DriverItem);
	auto layerOptionArray = GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_LayerCreationOptions);
	//auto configurationOptionsArray = GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_ConfigurationOptions);

	GDAL_ErrorFrame gdal_error_frame; // catches errors and properly throws
	GDAL_ConfigurationOptionsFrame config_frame(GetOptionArray(dynamic_cast<const GdalMetaInfo&>(smi).m_ConfigurationOptions));

	// test configuration options array
	//const char* CPLParseNameValue(const char* pszNameValue, char** ppszKey)
	//CPLStringList pszConfigurationOptionsArray;
	//for (const auto& option : configurationOptionsArray)
	//	pszConfigurationOptionsArray.AddString(option.c_str());

	//if (pszConfigurationOptionsArray)
	//	CPLSetConfigOptions(pszConfigurationOptionsArray);

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
			auto valuesTypeID = smi.CurrRD()->GetAbstrValuesUnit()->GetValueType()->GetValueClassID();
			eType = gdalDataType(valuesTypeID);
			if (valuesTypeID == VT_Bool) optionArray.AddString("NBITS=1");
			if (valuesTypeID == VT_UInt2) optionArray.AddString("NBITS=2");
			if (valuesTypeID == VT_UInt4) optionArray.AddString("NBITS=3");
			optionArray.AddString("COMPRESS=LZW");
			optionArray.AddString("BIGTIFF=IF_SAFER");
			optionArray.AddString("TFW=YES");
			optionArray.AddString("TILED=YES");
			optionArray.AddString("BLOCKXSIZE=512");
			optionArray.AddString("BLOCKYSIZE=512");
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
			{
				//GDALAllRegister();
				throwErrorF("GDAL", "cannot register user specified gdal driver from GDAL_Driver array: %s", driverArray[i]);
			}
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
	if (!std::filesystem::is_directory(path.c_str()) &&	!std::filesystem::create_directories(path.c_str()))
		throwErrorF("GDAL", "Unable to create directories: %s", path);

	//if (driverArray.empty()) // need one driver, and one driver only
	//{
		auto driverShortName = FileExtensionToRegisteredGDALDriverShortName(ext);
		if (!driverShortName.empty())
			driverArray.AddString(driverShortName.c_str());
	//}

	//MG_CHECK(driverArray.size() == 1);
	auto driver = GetGDALDriverManager()->GetDriverByName(driverShortName.c_str());//driverArray[0]);
	if (!driver)
		throwErrorF("GDAL", "Cannot find driver for %s", driverArray[0]);

	GDALDatasetHandle result = nullptr;

 	if (not continueWrite || not GDALDriverSupportsUpdating(datasourceName))
	{
		driver->Delete(datasourceName.c_str()); gdal_error_frame.GetMsgAndReleaseError(); // start empty, release error in case of nonexistance.
		result = driver->Create(datasourceName.c_str(), nXSize, nYSize, nBands, eType, optionArray);
	}
	else
	{		
		result = reinterpret_cast<GDALDataset*>(GDALOpenEx(datasourceName.c_str(), GA_Update | GDAL_OF_VERBOSE_ERROR, nullptr, nullptr, nullptr));
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
