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

#if !defined(__STG_GDAL_BASE_H)
#define __STG_GDAL_BASE_H

#define CPL_DLL __declspec(dllimport)
#define CPL_INTERNAL

#include "StgBase.h"

#include <optional>

#include "ptr/WeakPtr.h"
#include "geo/Transform.h"

#include <gdal_priv.h>
#include <limits>
#include <format>
struct pj_ctx;

enum dms_CPLErr;
class GDALDataset;
class OGRLayer;

// *****************************************************************************
struct gdalDynamicLoader
{
	STGDLL_CALL gdalDynamicLoader();
};

struct gdalThread
{
	STGDLL_CALL gdalThread();
	STGDLL_CALL ~gdalThread();
};

struct gdalComponent : gdalDynamicLoader
{
	STGDLL_CALL gdalComponent();
	STGDLL_CALL ~gdalComponent();

	static bool isActive();

	STGDLL_CALL static void CreateMetaInfo(TreeItem* container, bool mustCalc);
	
	std::vector<std::pair<SharedStr, SharedStr>> m_test;
};

struct GDAL_ErrorFrame : gdalThread
{
	STGDLL_CALL GDAL_ErrorFrame();
	STGDLL_CALL ~GDAL_ErrorFrame() noexcept(false);

	void RegisterError(dms_CPLErr eErrClass, int err_no, const char *msg);
	STGDLL_CALL void ThrowUpWhateverCameUp();
	auto ReleaseError()
	{
		auto result = m_eErrClass;
		m_eErrClass = {};
		return result;
	}
	auto GetMsgAndReleaseError()
	{
		m_eErrClass = {};
		return std::move(m_msg);
	}
	bool HasError() const { return m_eErrClass >= CE_Failure; }

	dms_CPLErr m_eErrClass;
	int m_nr_uncaught_exceptions = 0;
	int m_err_no = 0, m_prev_proj_err_no = 0;
	SharedStr m_msg;

	GDAL_ErrorFrame* m_Prev;

	STGDLL_CALL static struct pj_ctx* GetProjectionContext();
	STGDLL_CALL static int GetProjectionContextErrNo();
	STGDLL_CALL static SharedStr GetProjectionContextErrorString();
};

//STGDLL_CALL void gdalComponent_CreateMetaInfo(TreeItem* container, bool mustCalc);

struct CplString
{
	CplString() : m_Text(nullptr) {}
	STGDLL_CALL ~CplString();

	char* m_Text;
};

struct FieldInfo
{
	int         field_index   = -1; // cache for usage in inner-loop
	bool	    isWritten :1  = false;   // don't write DataItems twice
	bool	    isGeometry:1  = false;  // DataItem is geometry field
	bool        doWrite   :1  = false;     // user issued interest for writing this Dataitem
	SharedStr   name;        // name of DataItem
	SharedStr launderedName; // gdal could launder the name of DataItem

	SharedDataItemInterestPtr m_DataHolder; // Ptr to keep data alive until all data for layer items of interest is present.
};

// REVIEW: TokenT vervangen door TokenID maakt code simpeler en encapsuleert implementatie van Tokens.

class DataItemsWriteStatusInfo
{
public:
#if defined(MG_DEBUG)
	static UInt32 sd_ObjCounter;
	DataItemsWriteStatusInfo();
#else
	DataItemsWriteStatusInfo() = default;
#endif //defined(MG_DEBUG)
	DataItemsWriteStatusInfo(const DataItemsWriteStatusInfo&) = delete;
	void operator =(const DataItemsWriteStatusInfo&) = delete;
	~DataItemsWriteStatusInfo();

	int  getNumberOfLayers();
	bool fieldIsWritten(TokenID layerID, TokenID  fieldID);
	void setFieldIsWritten(TokenID layerID, TokenID  fieldID, bool isWritten);
	void setIsGeometry(TokenID layerID, TokenID  geometryFieldID, bool isGeometry);
	void setInterest(TokenID layerID, TokenID  fieldID, bool hasInterest);
	void SetInterestForDataHolder(TokenID layerID, TokenID fieldID, const AbstrDataItem*);
	void SetLaunderedName(TokenID layerID, TokenID fieldID, SharedStr launderedName);
	void ReleaseAllLayerInterestPtrs(TokenID layerID);
	void RefreshInterest(const TreeItem* storageHolder);
	bool LayerIsReadyForWriting(TokenID layerID);
	bool LayerHasBeenWritten(TokenID layerID);

	std::map<TokenID, std::map<TokenID, FieldInfo>> m_LayerAndFieldIDMapping;
	bool m_continueWrite = false;
};

// *****************************************************************************

GDALDataType gdalRasterDataType(ValueClassID tid);

// *****************************************************************************

auto DmsType2OGRFieldType(ValueClassID id) -> OGRFieldType; // TODO move OGR helper funcs to gdal_vect.cpp
auto DmsType2OGRSubFieldType(ValueClassID id) -> OGRFieldSubType;
auto DmsType2OGRGeometryType(ValueComposition vc) -> OGRwkbGeometryType;
auto GetWktProjectionFromValuesUnit(const AbstrDataItem* adi) -> SharedStr;
const TreeItem* GetLayerHolderFromDataItem(const TreeItem* storageHolder, const TreeItem* subItem);
auto GetOptionArray(const TreeItem* optionsItem) -> CPLStringList;
void SetFeatureDefnForOGRLayerFromLayerHolder(const TreeItem* subItem, OGRLayer* layerHandle);
STGDLL_CALL auto GetBaseProjectionUnitFromValuesUnit(const AbstrDataItem* adi) -> const AbstrUnit*;
auto GetGeometryTypeFromGeometryDataItem(const TreeItem* subItem) -> OGRwkbGeometryType;
auto GetAsWkt(const OGRSpatialReference* sr) -> SharedStr;
auto GetAffineTransformationFromDataItem(const TreeItem* storageHolder) -> std::vector<double>;
auto GetOGRSpatialReferenceFromDataItems(const TreeItem* storageHolder) -> std::optional<OGRSpatialReference>;
void CheckSpatialReference(std::optional<OGRSpatialReference>& ogrSR, const AbstrUnit* mutBase);
STGDLL_CALL auto GetUnitSizeInMeters(const AbstrUnit* projectionBaseUnit) -> Float64;
STGDLL_CALL void ValidateSpatialReferenceFromWkt(OGRSpatialReference* ogrSR, CharPtr wkt_prj_str);

struct GDALDatasetHandle
{
	GDALDatasetHandle(GDALDataset* ptr = nullptr) : dsh_(ptr) {}
	GDALDatasetHandle(GDALDatasetHandle&&) = default;
	GDALDatasetHandle(const GDALDatasetHandle&) = delete;

	GDALDatasetHandle& operator =(GDALDatasetHandle&& rhs) = default;
	void operator =(const GDALDatasetHandle& rhs) = delete;

	operator GDALDataset* () { return dsh_.get(); }
	GDALDataset* operator ->() { return dsh_.get(); }

	struct deleter {
		void operator ()(GDALDataset* dsh) {
			gdalThread cleanupProjAfterItDidItsPjThings; // see https://github.com/ObjectVision/GeoDMS/issues/11
			GDAL_ErrorFrame catchAllIssues;
			GDALClose(GDALDataset::ToHandle(dsh)); 
			catchAllIssues.ReleaseError();
		};
	};

	void UpdateBaseProjection(const AbstrUnit* uBase) const;

	std::unique_ptr < GDALDataset, deleter > dsh_;
};

struct GDAL_TransactionFrame 
{
	GDAL_TransactionFrame(GDALDataset *dsh)
	{
		dsh_ = dsh;
		dsh_->StartTransaction(true); // TODO: force flag specifically added to write using OpenFileGDB, sideeffects?
	}
	~GDAL_TransactionFrame()
	{
		dsh_->CommitTransaction();
	}

	GDALDataset *dsh_;
};

class GDAL_ConfigurationOptionsFrame
{
public:
	GDAL_ConfigurationOptionsFrame(const CPLStringList& pszConfigurationOptionsArray);
	~GDAL_ConfigurationOptionsFrame();
private:
	CPLStringList m_DefaultConfigurationOptions;
};

// *****************************************************************************
GDALDatasetHandle Gdal_DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode, UInt32 gdalOpenFlags, bool continueWrite);

using gdal_transform = double[6];

CrdTransformation GetTransformation(gdal_transform gdalTr);

#endif // __STG_GDAL_BASE_H
