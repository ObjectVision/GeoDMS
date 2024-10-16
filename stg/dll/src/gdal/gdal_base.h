// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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

struct gdalThread
{
	STGDLL_CALL gdalThread();
	STGDLL_CALL ~gdalThread();
};

struct gdalComponent
{
	STGDLL_CALL gdalComponent();
	STGDLL_CALL ~gdalComponent();

	static bool isActive();

	STGDLL_CALL static void CreateMetaInfo(TreeItem* container, bool mustCalc);
	
	std::vector<std::pair<SharedStr, SharedStr>> m_test;
};

struct pj_ctx;

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
	pj_ctx* m_ctx = nullptr;

	STGDLL_CALL pj_ctx* GetProjectionContext();
	STGDLL_CALL int GetProjectionContextErrNo();
	STGDLL_CALL static SharedStr GetProjectionContextErrorString();
};

//STGDLL_CALL void gdalComponent_CreateMetaInfo(TreeItem* container, bool mustCalc);

struct CplString
{
	CplString() : m_Text(nullptr) {}
	STGDLL_CALL ~CplString();

	char* m_Text;
};

using WeakDataItemInterestPtr = InterestPtr<WeakPtr<const AbstrDataItem>>;

struct FieldInfo
{
	int         field_index   = -1; // cache for usage in inner-loop
	bool	    isWritten :1  = false;   // don't write DataItems twice
	bool	    isGeometry:1  = false;  // DataItem is geometry field
	bool        doWrite   :1  = false;     // user issued interest for writing this Dataitem
	TokenID     nameID;      // name of DataItem
	TokenID     launderedNameID; // gdal could launder the name of DataItem

	WeakDataItemInterestPtr m_DataHolder; // Ptr to keep data alive until all data for layer items of interest is present.
};

struct affine_transformation
{
	Float64 x_offset = 0.0;
	Float64 x_scale = 1.0;
	Float64 x_rotation = 0.0;
	Float64 y_offset = 0.0;
	Float64 y_rotation = 0.0;
	Float64 y_scale = 1.0;
};

// REVIEW: TokenT vervangen door TokenID maakt code simpeler en encapsuleert implementatie van Tokens.

class DataItemsWriteStatusInfo
{
	using layer_id = TokenID;
	using field_id = TokenID;

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
	bool hasGeometry(TokenID layerID);
	void setFieldIsWritten(TokenID layerID, TokenID  fieldID, bool isWritten);
	void setIsGeometry(TokenID layerID, TokenID  geometryFieldID, bool isGeometry);
	void setInterest(TokenID layerID, TokenID  fieldID, bool hasInterest);
	void SetInterestForDataHolder(TokenID layerID, TokenID fieldID, const AbstrDataItem*);
	void SetLaunderedName(TokenID layerID, TokenID fieldID, TokenID launderedNameID);
	void ReleaseAllLayerInterestPtrs(TokenID layerID);
	void RefreshInterest(const TreeItem* storageHolder);
	bool DatasetIsReadyForWriting();
	bool LayerIsReadyForWriting(TokenID layerID);
	bool LayerHasBeenWritten(TokenID layerID);
	auto GetExampleAdiFromLayerID(TokenID layerID) -> SharedDataItem;

	std::map<layer_id, std::map<field_id, FieldInfo>> m_LayerAndFieldIDMapping;
	std::map<layer_id, SharedDataItemInterestPtr> m_orphan_geometry_items;
	bool m_continueWrite = false;
	bool m_initialized = false;
};

// *****************************************************************************

GDALDataType gdalRasterDataType(ValueClassID tid, bool write = false);

// *****************************************************************************

// Proj specific tests
STGDLL_CALL bool AuthorityCodeIsValidCrs(std::string_view wkt);

auto DmsType2OGRFieldType(ValueClassID id) -> OGRFieldType; // TODO move OGR helper funcs to gdal_vect.cpp
auto DmsType2OGRSubFieldType(ValueClassID id) -> OGRFieldSubType;
auto DmsType2OGRGeometryType(ValueComposition vc) -> OGRwkbGeometryType;
auto GetWktProjectionFromValuesUnit(const AbstrDataItem* adi) -> SharedStr;
const TreeItem* GetLayerHolderFromDataItem(const TreeItem* storageHolder, const TreeItem* subItem);
auto GetOptionArray(const TreeItem* optionsItem) -> CPLStringList;
void SetFeatureDefnForOGRLayerFromLayerHolder(const TreeItem* subItem, OGRLayer* layerHandle);
STGDLL_CALL auto GetBaseProjectionUnitFromValuesUnit(const AbstrDataItem* adi) -> const AbstrUnit*;
auto GetGeometryItemFromLayerHolder(const TreeItem* subItem) -> const TreeItem*;
auto GetGeometryTypeFromLayerHolder(const TreeItem* subItem) -> OGRwkbGeometryType;
auto GetAsWkt(const OGRSpatialReference* sr) -> SharedStr;
auto GetOGRSpatialReferenceFromDataItems(const TreeItem* storageHolder) -> std::optional<OGRSpatialReference>;
void CheckSpatialReference(std::optional<OGRSpatialReference>& ogrSR, const TreeItem* treeitem, const AbstrUnit* mutBase);
STGDLL_CALL auto GetUnitSizeInMeters(const AbstrUnit* projectionBaseUnit) -> Float64;
STGDLL_CALL void ValidateSpatialReferenceFromWkt(OGRSpatialReference* ogrSR, CharPtr wkt_prj_str);
bool DriverSupportsUpdate(std::string_view dataset_file_name, const CPLStringList driver_array);

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

	void UpdateBaseProjection(const TreeItem* treeitem, const AbstrUnit* uBase) const;

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
