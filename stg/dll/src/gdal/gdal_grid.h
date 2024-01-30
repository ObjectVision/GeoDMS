// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STG_GDAL_GRID_H)
#define __STG_GDAL_GRID_H


#include "gdal/gdal_base.h"
#include "GridStorageManager.h"

class  GDALDataset;

// *****************************************************************************

// storagemanager for GDal grids
struct GdalGridSM : AbstrGridStorageManager, gdalComponent
{
	GdalGridSM() = default;
	~GdalGridSM() override;

	virtual bool IsWritableGDAL() const { return false; }

    // Implement AbstrStorageManager interface
	void DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage(bool mustCommit) const override;

	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;

	bool ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;
	prop_tables GetPropTables(const TreeItem* storageHolder=nullptr, TreeItem* curr=nullptr) const override;

private:
	void ReadGridData  (StgViewPortInfo& vip, AbstrDataObject* ado, tile_id t, SharedStr sqlBandSpecification);
	void ReadGridCounts(StgViewPortInfo& vip, AbstrDataObject* ado, tile_id t, SharedStr sqlBandSpecification);
	bool ReadPalette(AbstrDataObject* adi);
//	void WriteGridData(const TreeItem* storageHolder, const AbstrDataItem* adi);

	mutable GDALDatasetHandle m_hDS;
	//mutable DataItemsWriteStatusInfo m_DataItemsWriteStatus;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

struct GdalWritableGridSM : GdalGridSM
{
	bool IsWritableGDAL() const override { return true; }

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

// *****************************************************************************

struct GDAL_SimpleReader : gdalComponent
{
	typedef UInt32 color_type;
	typedef std::vector<UInt8> band_data;
	STGDLL_CALL GDAL_SimpleReader();

	struct buffer_type
	{
		band_data redBand, greenBand, blueBand, transBand;
		std::vector<color_type> combinedBands;
	};

	STGDLL_CALL WPoint ReadGridData(CharPtr fileName, buffer_type& buffer);
};

class GDalGridImp {
public:
	GDalGridImp(GDALDataset* hDS, const AbstrDataObject* ado, UPoint viewPortSize, SharedStr sqlBandSpecification);
	SizeT ReadTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 strip_y, SizeT tileByteSize) const;
	Int32 WriteTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y);
	void UnpackCheck(UInt32 nrDmsBitsPerPixel, UInt32 nrRasterBitsPerPixel, CharPtr functionName, CharPtr direction, CharPtr dataSourceName) const;
	
	template <typename ...Args> static void UnpackStrip(Args...)  {};
	void PackStrip(void* stripBuf, Int32 currDataSize, UInt32 nrBitsPerPixel) const;
	void UnpackStrip(void* stripBuff, Int32 currDataSize, UInt32 nrBitsPerPixel) const;

	template <int N>
	static void UnpackStrip(bit_iterator<N, bit_block_t> pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th);

	UInt32 GetWidth() const;
	UInt32 GetHeight() const;
	UPoint GetTileSize() const;
	void SetWidth(auto width);
	void SetHeight(auto height);
	void SetTiled();
	void SetDataMode(UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat);
	UInt32 GetNrBitsPerPixel() const;
	UInt32 GetTileByteWidth() const;
	SizeT GetTileByteSize() const;

private:
	std::vector<int> UnpackBandIndicesFromValue(std::string value);
	std::vector<int> InterpretSqlBandSpecification(SharedStr sqlBandSpecification);
	GDALRasterBand* GetRasterBand(SharedStr sqlBandSpecification);
	std::vector<GDALRasterBand*> GetRasterBands(SharedStr sqlBandSpecification);
	CPLErr ReadSingleBandTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 sx, UInt32 sy, GDALRasterBand* poBand) const;
	CPLErr ReadInterleavedMultiBandTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 sx, UInt32 sy, UInt32 nBandCount) const;

	GDALDataset*    m_hDS;
	GDALRasterBand* m_RasterBand;
	UPoint          m_ViewPortSize;
	ValueClassID    m_ValueClassID;
};

#endif // __STG_GDAL_GRID_H