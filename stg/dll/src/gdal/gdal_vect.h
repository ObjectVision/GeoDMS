// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STG_GDAL_VECT_H)
#define __STG_GDAL_VECT_H

#include "gdal/gdal_base.h"
#include "ptr/Resource.h"

#include <filesystem>
#include <ranges>

class GDALDataset;
class OGRLayer;

struct GdalVectSM;

// ------------------------------------------------------------------------
// installation of gdalVect component
// ------------------------------------------------------------------------

struct gdalVectComponent : gdalComponent
{
	gdalVectComponent();
	~gdalVectComponent();
};

// *****************************************************************************

struct GdalVectlMetaInfo : GdalMetaInfo
{
	GdalVectlMetaInfo(const GdalVectSM*, const TreeItem* storageHolder, const TreeItem* adiParent);
	~GdalVectlMetaInfo();
	void OnOpen() override;
	void SetCurrFeatureIndex(SizeT firstFeatureIndex) const;

	OGRLayer* Layer() const;

	WeakPtr<const GdalVectSM> m_GdalVectSM;
	SharedStr                 m_SqlString;
	TokenID                   m_NameID;
	WeakPtr<OGRLayer>         m_Layer;
	bool                      m_IsOwner = false;

	mutable ResourceHandle  m_ReadBuffer;
	mutable SizeT           m_CurrFeatureIndex = 0;
	mutable SizeT           m_CurrFieldIndex = -1;
};

// *****************************************************************************



// storagemanager for GDal vector data
struct GdalVectSM : NonmappableStorageManager, gdalVectComponent
{
#if defined(MG_DEBUG)
	GdalVectSM();
#endif
	~GdalVectSM() override;
	virtual bool IsWritableGDAL() const { return false;  }

    // Implement AbstrStorageManager interface
	bool DoCheckExistence(const TreeItem* storageHolder) const override;
	void DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage(bool mustCommit) const override;
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void OnTerminalDataItem(const AbstrDataItem* adi) const override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;

	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;
	bool ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;
	void WriteLayer(TokenID layer_id, const GdalMetaInfo& gmi);
	void DoUpdateTable(const TreeItem* storageHolder, AbstrUnit* curr, OGRLayer* layer) const;
	prop_tables GetPropTables(const TreeItem* storageHolder = nullptr, TreeItem* curr = nullptr) const override;

	mutable DataItemsWriteStatusInfo m_DataItemsStatusInfo;
	mutable std::recursive_mutex m_xSectionDataItemsStatusInfo;

private:
	bool ReadLayerData(const GdalVectlMetaInfo* br, AbstrDataObject* ado, tile_id t);
	bool ReadGeometry (const GdalVectlMetaInfo* br, AbstrDataObject* ado, tile_id t, SizeT firstIndex, SizeT size);
	bool ReadAttrData (const GdalVectlMetaInfo* br, AbstrDataObject* ado, tile_id t, SizeT firstIndex, SizeT size);
	bool WriteGeometryElement(const AbstrDataItem* adi, OGRFeature* feature, tile_id t, SizeT featureIndex);
	bool WriteFieldElement   (const AbstrDataItem* adi, int field_index, OGRFeature* feature, tile_id t, SizeT featureIndex);

	mutable GDALDatasetHandle m_hDS;


	DECL_RTTI(STGDLL_CALL, StorageClass)
	friend GdalVectlMetaInfo;
};

struct GdalWritableVectSM : GdalVectSM
{
	bool IsWritableGDAL() const override { return true; }

	DECL_RTTI(STGDLL_CALL, StorageClass)
};


#endif // __STG_GDAL_VECT_H
