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
#include "StoragePCH.h"
#pragma hdrstop


// *****************************************************************************
//
// Implementations of GdalVectSM
//
// *****************************************************************************
#include "gdal_base.h"
#include "gdal_vect.h"

#include <gdal.h>
#include <proj.h>
#include <ogr_api.h>
#include <cpl_csv.h>
#include <ogrsf_frmts.h>


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
		AbstrContextHandle* ach = AbstrContextHandle::GetLast();
		reportF(gdalSeverity(eErrClass), "gdal %s(%d): %s%s%s "
			, gdal2CharPtr(eErrClass)
			, err_no
			, msg, ach ? " while " : ""
			, ach ? ach->GetDescription() : ""
		);

		if (s_ErrorFramePtr)
			s_ErrorFramePtr->RegisterError(dms_CPLErr(eErrClass), err_no, msg);
//		else
//			ErrorHandlerImpl(eErrClass, err_no, msg);
	}

}	// namespace gdalComponentImpl

GDAL_ErrorFrame::GDAL_ErrorFrame()
	: m_eErrClass(dms_CPLErr(CE_None) )
	, m_Prev( gdalComponentImpl::s_ErrorFramePtr )
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
	OGRCleanupAll();
	GDALDestroyDriverManager();
}

gdalDynamicLoader::gdalDynamicLoader()
{
}


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
		CPLPopFileFinder();
	}
}

gdalComponent::gdalComponent()
{
	leveled_critical_section::scoped_lock lock(gdalComponentImpl::gdalSection);

	if (!gdalComponentImpl::s_ComponentCount)
	{
		try {
			dms_assert(gdalComponentImpl::s_OldErrorHandler == nullptr);
			gdalComponentImpl::s_OldErrorHandler = CPLSetErrorHandler(gdalComponentImpl::ErrorHandler); // can throw

			dms_assert(gdalComponentImpl::s_HookedFilesPtr == nullptr);
			gdalComponentImpl::s_HookedFilesPtr = new std::map<SharedStr, SharedStr>; // can throw

			SetCSVFilenameHook(gdalComponentImpl::HookFilesToExeFolder1);
			proj_context_set_file_finder(nullptr, gdalComponentImpl::proj_HookFilesToExeFolder, nullptr);

			GDALAllRegister(); // can throw
			OGRRegisterAll(); // can throw
		}
		catch (...)
		{
			gdalCleanup();
			throw;
		}
	}
	++gdalComponentImpl::s_ComponentCount;
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

SharedStr FileExtensionToGDALDriverShortName(SharedStr ext)
{
	auto driverManager = GetGDALDriverManager();
	SizeT driverCount = driverManager->GetDriverCount();
	auto DriverShortName = ext;

	for (SizeT i = 0; i != driverCount; i++)
	{
		auto driver = driverManager->GetDriver(i);
		CPLStringList driverExts(CSLTokenizeString(driver->GetMetadataItem(GDAL_DMD_EXTENSIONS)));
		auto extCount = CSLCount(driverExts);

		for (SizeT j = 0; j != extCount; j++) // loop over extensions of driver i
		{
			auto driverExt = SharedStr(CSLGetField(driverExts, j));
			if (ext == driverExt)
			{
				DriverShortName = GDALGetDriverShortName(driver); // found
				return DriverShortName;
			}
		}
	}

	return DriverShortName;
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

SharedStr GetWktProjectionFromValuesUnit(const AbstrDataItem* adi)
{
	const AbstrUnit* valuesUnit = adi->GetAbstrValuesUnit();
	auto curr_proj = valuesUnit->GetCurrProjection();

	auto base = curr_proj ? curr_proj->GetCompositeBase() : valuesUnit;

	gdalComponent lock_use_gdal;
	SharedStr wktPrjStr(base->GetFormat());

	if (wktPrjStr.empty()) return {};

	GDAL_ErrorFrame	error_frame;

	OGRSpatialReference	m_Src;
	OGRErr err = m_Src.SetFromUserInput(wktPrjStr.c_str());
	CplString pszEsriwkt;
	m_Src.exportToWkt(&pszEsriwkt.m_Text);

	if (err == OGRERR_NONE)
	{
		wktPrjStr = pszEsriwkt.m_Text;
	}
	else
	{
		wktPrjStr = "";
	}

	error_frame.ReleaseError();

	return wktPrjStr;
}

OGRSpatialReference* GetOGRSpatialReferenceFromDataItems(const TreeItem* storageHolder)
{
	OGRSpatialReference* srs = nullptr;
	for (auto subItem = storageHolder->WalkConstSubTree(nullptr); subItem; subItem = storageHolder->WalkConstSubTree(subItem))
	{
		if (not (IsDataItem(subItem) and subItem->IsStorable()))
			continue;

		auto subDI = AsDataItem(subItem);

		auto wktString = GetWktProjectionFromValuesUnit(subDI);
		if (not wktString.empty())
		{
			srs = new OGRSpatialReference(wktString.c_str());
			return srs;
		}
	}
	return srs;
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

bool GDALDriverSupportsUpdating(SharedStr datasourceName)
{
	if (std::string(CPLGetExtension(datasourceName.c_str())) == "gml")
		return false;

	return true;
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

	if (rwMode == dms_rw_mode::read_only)
	{
		GDALDatasetHandle result = GDALDataset::FromHandle( 
				GDALOpenEx(datasourceName.c_str()
					, (rwMode > dms_rw_mode::read_only) ? GA_Update : GA_ReadOnly | gdalOpenFlags | GDAL_OF_VERBOSE_ERROR
					, driverArray
					, optionArray
					, nullptr
				)
		);

		if (gdal_error_frame.HasError())
		{
			throwErrorF("GDAL", "cannot open dataset %s \n%s"
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

	if (driverArray.empty())
	{
		SharedStr ext = SharedStr(CPLGetExtension(datasourceName.c_str()));
		driverArray.AddString(FileExtensionToGDALDriverShortName(ext).c_str());
	}
	MG_CHECK(driverArray.size() == 1);
	auto driver = GetGDALDriverManager()->GetDriverByName(driverArray[0]);
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
