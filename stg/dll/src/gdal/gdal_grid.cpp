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
// Implementations of GdalGridSM
//
// *****************************************************************************

#include "gdal_base.h"
#include "gdal_grid.h"

#include <gdal_priv.h>

#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/PointOrder.h"
#include "geo/Round.h"
#include "xct/DmsException.h"
#include "cpc/EndianConversions.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "TreeItemContextHandle.h"
#include "Projection.h"
#include "ViewPortInfoEx.h"

#include "Unit.h"
#include "UnitClass.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "stg/StorageClass.h"

#include <sstream>

// ------------------------------------------------------------------------
// Implementation of the abstact storagemanager interface
// ------------------------------------------------------------------------
GdalGridSM::~GdalGridSM()
{
	CloseStorage();
	dms_assert(m_hDS == nullptr);
}

void GdalGridSM::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	DBG_START("GdalGridSM", "OpenStorage", true);
	dms_assert(m_hDS == nullptr);
	if (rwMode != dms_rw_mode::read_only && !IsWritableGDAL())
		throwErrorF("gdal.grid", "Cannot use storage manager %s with readonly type %s for writing data"
			, smi.StorageManager()->GetFullName().c_str()
			, smi.StorageManager()->GetClsName().c_str()
		);
	m_hDS = Gdal_DoOpenStorage(smi, rwMode, GDAL_OF_RASTER, false);
}

void GdalGridSM::DoCloseStorage(bool mustCommit) const
{
	DBG_START("GdalGridSM", "DoCloseStorage", true);
	dms_assert(m_hDS);

	m_hDS = nullptr; // calls GDALClose
}

bool GdalGridSM::ReadPalette(AbstrDataObject* ado)
{
	auto nrOfBands = this->m_hDS->GetRasterCount();
	auto rBand = this->m_hDS->GetRasterBand(1);

	auto nrElems = ado->GetTiledRangeData()->GetRangeSize();
	GDALColorTable* ct = rBand->GetColorTable();
	UInt32 nrColors = ct->GetColorEntryCount();

	UInt32 i;
	for (i = 0; i != nrColors; ++i)
	{
		auto ce = ct->GetColorEntry(i);
		ado->SetValueAsUInt32(i, CombineRGB(ce->c1, ce->c2, ce->c3));
	}

	return true;
}

bool GdalGridSM::ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	dms_assert(IsOpen());

	AbstrDataItem* adi = smi.CurrWD();

	if (adi->GetID() == PALETTE_DATA_ID)
		return ReadPalette(borrowedReadResultHolder);

	if (HasGridDomain(adi))
	{
		// Collect zoom info
		const GridStorageMetaInfo* gbr = debug_cast<const GridStorageMetaInfo*>(&smi);
		auto vpi = gbr->m_VPIP.value().GetViewportInfoEx(t);
		vpi.SetWritability(adi);
		auto test = vpi.GetCountColor();
		if (vpi.GetCountColor() != -1)
			ReadGridCounts(vpi, borrowedReadResultHolder, t, gbr->m_SqlString);
		else
			ReadGridData  (vpi, borrowedReadResultHolder, t, gbr->m_SqlString);
	}

	return true;
}

// ------------------------------------------------------------------------
// GDalGridImp
// ------------------------------------------------------------------------


GDalGridImp::GDalGridImp(GDALDataset* hDS, const AbstrDataObject* ado, UPoint viewPortSize, SharedStr sqlBandSpecification)
	: m_hDS(hDS)
	, poBand(GetRasterBand(sqlBandSpecification))
	//, poBands(GetRasterBands(sqlBandSpecification))
	, m_ValueClassID(ado->GetValueClass()->GetValueClassID())
	, m_ViewPortSize(viewPortSize)
{
	MG_CHECK(poBand);
}

std::vector<int> GDalGridImp::UnpackBandIndicesFromValue(std::string value)
{
	std::string bandIndiceString;
	std::vector<int> bandIndices;
	for (std::string::size_type i = 0; i < value.size(); i++) 
	{
		if (std::isdigit(value[i]))
			bandIndiceString+=value[i];
		else
		{
			if (!bandIndiceString.empty())
			{
				bandIndices.push_back(std::stoi(bandIndiceString.c_str()));
				bandIndiceString.clear();
			}
		}
	}
	int test = std::stoi(bandIndiceString);
	bandIndices.push_back(test);
	return bandIndices;
}

std::vector<int> GDalGridImp::InterpretSqlBandSpecification(SharedStr sqlBandSpecification)
{
	std::vector<int> bandIndices;
	if (not sqlBandSpecification.empty()) 
	{
		if (std::string(sqlBandSpecification.c_str()).find('=') == std::string::npos)
			return bandIndices;

		std::string value = CPLParseNameValue(sqlBandSpecification.c_str(), nullptr);
		bandIndices = UnpackBandIndicesFromValue(value);
	}
	return bandIndices;
}

GDALRasterBand* GDalGridImp::GetRasterBand(SharedStr sqlBandSpecification)
{
	auto bandIndices = InterpretSqlBandSpecification(sqlBandSpecification);
	if (bandIndices.empty())
		return m_hDS->GetRasterBand(1); // default to first band
	
	return m_hDS->GetRasterBand(bandIndices[0]);
}

std::vector<GDALRasterBand*> GDalGridImp::GetRasterBands(SharedStr sqlBandSpecification)
{
	std::vector<GDALRasterBand*> poBands;
	/*auto bandIndices = InterpretSqlBandSpecification(sqlBandSpecification);
	if (bandIndices.empty())
		return poBands; // default to no bands

	for (auto ind : bandIndices)
		poBands.push_back(m_hDS->GetRasterBand(ind));*/

	return poBands;
}


UInt32 GDalGridImp::GetWidth() const { return poBand->GetXSize(); }
UInt32 GDalGridImp::GetHeight() const { return poBand->GetYSize(); }
UPoint GDalGridImp::GetTileSize() const {
	int x, y;
	poBand->GetBlockSize(&x, &y);
	return shp2dms_order(x, y);
}
UInt32 GDalGridImp::GetNrBitsPerPixel() const { return GDALGetDataTypeSize(gdalDataType(m_ValueClassID)); }
UInt32 GDalGridImp::GetTileByteWidth() const {
	return (GetTileSize().X() * GetNrBitsPerPixel() + 7) / 8;
}
SizeT GDalGridImp::GetTileByteSize() const {
	return Cardinality(UPoint(GetTileByteWidth(), GetTileSize().Y()));
}

/*template <typename T>
CPLErr ReadMultiBandTileNaive(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 sx, UInt32 sy, UInt32 nBandCount) const
{
	auto tilewh = GetTileSize();
	auto poBandsData = std::vector<std::vector<T>>();
	for (int i = 0; i < nBandCount; i++) {
		auto tempBand = m_hDS->GetRasterBand(i + 1); // starting at 1
		std::vector<T> tile_i(tilewh.X() * tilewh.Y());
		ReadSingleBandTile<T>(&tile_i[0], tile_x, tile_y, sx, sy, tempBand);
		poBandsData.push_back(std::move(tile_i));
	}

	char* b = (char*)stripBuff; // TODO: improve naive implementation below
	for (int yIndex = 0; yIndex < tilewh.Y(); yIndex++)
	{
		for (int xIndex = 0; xIndex < tilewh.X(); xIndex++)
		{
			for (int bandIndex = 0; bandIndex < nBandCount; bandIndex++)
			{
				std::memcpy(b, &poBandsData[bandIndex][xIndex + yIndex * sx], 1);
				b++;
			}
		}
	}
	return CE_None;
}*/

CPLErr GDalGridImp::ReadSingleBandTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 sx, UInt32 sy, GDALRasterBand* poBand) const
{
	GDAL_ErrorFrame x;
	auto resultCode = poBand->RasterIO(GF_Read,
		tile_x, tile_y,
		sx, sy,
		stripBuff,
		sx, sy,
		gdalDataType(m_ValueClassID), //poBand->GetRasterDataType(), //gdalDataType(m_ValueClassID),
		0,					//nPixelSpace,
		GetTileByteWidth()  //nLineSpace,
	);
	return resultCode;
}

CPLErr GDalGridImp::ReadInterleavedMultiBandTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 sx, UInt32 sy, UInt32 nBandCount) const
{
	GDAL_ErrorFrame x;
	auto tilewh = GetTileSize();
	auto resultCode = m_hDS->RasterIO(GF_Read,
		tile_x, tile_y,
		sx, sy,
		stripBuff,
		sx, sy,
		GDT_Byte,			
		nBandCount,
		NULL,						// panBandMap
		nBandCount,					// nPixelSpace,
		SizeT(nBandCount) * tilewh.X(),	// nLineSpace
		1,							// nBandSpace
		NULL						// psExtraArg
	);

	return resultCode;//resultCode;
}

SizeT GDalGridImp::ReadTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y, UInt32 strip_y, SizeT tileByteSize) const
{
	dms_assert(tile_x < GetWidth()); UInt32 sx = Min<UInt32>(GetTileSize().X(), GetWidth() - tile_x);
	dms_assert(tile_y < GetHeight()); UInt32 sy = Min<UInt32>(GetTileSize().Y(), GetHeight() - tile_y);
	dms_assert(GetTileByteSize() <= tileByteSize);

	UPoint tileSize = GetTileSize();
	auto resultCode = CE_None;
	auto nBandCount = m_hDS->GetRasterCount();
	auto bandType = poBand->GetRasterDataType();

	if (bandType == GDT_Byte && nBandCount == 4 && m_ValueClassID == VT_UInt32) // interleaved UInt32 four bands of type GDT_Byte
		resultCode = ReadInterleavedMultiBandTile(stripBuff, tile_x, tile_y, sx, sy, nBandCount);
	else // single band
		resultCode = ReadSingleBandTile(stripBuff, tile_x, tile_y, sx, sy, poBand);

	dms_assert(resultCode == CE_None);
	return GetTileByteSize();
}

Int32 GDalGridImp::WriteTile(void* stripBuff, UInt32 tile_x, UInt32 tile_y) // REMOVE, UInt32 strip_y, SizeT tileByteSize) const
{
	dms_assert(tile_x < GetWidth()); UInt32 sx = Min<UInt32>(GetTileSize().X(), GetWidth() - tile_x);
	dms_assert(tile_y < GetHeight()); UInt32 sy = Min<UInt32>(GetTileSize().Y(), GetHeight() - tile_y);

	auto tileByteSize = GetTileByteSize();
	dms_assert(GetTileByteSize() <= tileByteSize);

	GDAL_ErrorFrame x;
	auto resultCode = poBand->RasterIO(GF_Write,
		tile_x, tile_y,
		sx, sy,
		stripBuff,
		sx, sy,
		gdalDataType(m_ValueClassID),
		0, //nPixelSpace,
		GetTileByteWidth()  //nLineSpace,
	);
	dms_assert(resultCode == CE_None);
	return tileByteSize;
}

void GDalGridImp::UnpackCheck(UInt32 nrDmsBitsPerPixel, UInt32 nrRasterBitsPerPixel, CharPtr functionName) const
{
	if (nrRasterBitsPerPixel == nrDmsBitsPerPixel)
		return;
	if (nrDmsBitsPerPixel >= 8)
		return;
	if (nrDmsBitsPerPixel == 1 && nrRasterBitsPerPixel == 8)
		return;
	if (nrDmsBitsPerPixel == 4 && nrRasterBitsPerPixel == 8)
		return;
	throwErrorF(functionName, "Cannot convert %d bits DMS data from/to %d bits Raster file data", nrDmsBitsPerPixel, nrRasterBitsPerPixel);
}

template <int N>
static void GDalGridImp::UnpackStrip(bit_iterator<N, bit_block_t> pixelData, void* stripBuff, UInt32 nrBitsPerPixel, Int32& currNrProcesedBytes, UInt32 nrBytesPerRow, UInt32 tw, UInt32 th)
{

	if (nrBitsPerPixel == 8)
	{
		char* byteBuff = reinterpret_cast<char*>(stripBuff);
		for (; th; --th, byteBuff += nrBytesPerRow)
			for (Int32 i = 0; i != tw; ++i)
				*pixelData++ = byteBuff[i] & ((1 << N) - 1);

		currNrProcesedBytes = (N * currNrProcesedBytes + 7) / 8;
	}
}

void GDalGridImp::PackStrip(void* stripBuf, Int32 currDataSize, UInt32 nrBitsPerPixel) const {}
void GDalGridImp::UnpackStrip(void* stripBuff, Int32 currDataSize, UInt32 nrBitsPerPixel) const {}

void GDalGridImp::SetDataMode(UInt32 bitsPerSample, UInt32 samplesPerPixel, bool hasPalette, SAMPLEFORMAT sampleFormat)
{
//		TIFFSetField(m_TiffHandle, TIFFTAG_BITSPERSAMPLE, UInt16(bitsPerSample));
//		TIFFSetField(m_TiffHandle, TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel);
		MG_CHECK(bitsPerSample * samplesPerPixel == GetNrBitsPerPixel());
//		MG_CHECK(sampleFormat.m_Value == gdalDataType(m_ValueClassID))
//		TIFFSetField(m_TiffHandle, TIFFTAG_PHOTOMETRIC, hasPalette ? PHOTOMETRIC_PALETTE : PHOTOMETRIC_RGB);
//		TIFFSetField(m_TiffHandle, TIFFTAG_SAMPLEFORMAT, sampleFormat.m_Value);

//		if (samplesPerPixel > 3)
//		{
//			unsigned short s = EXTRASAMPLE_ASSOCALPHA;
//			TIFFSetField(m_TiffHandle, TIFFTAG_EXTRASAMPLES, 1, &s); // RGB and Alpha
//		}
}

void GDalGridImp::SetWidth(auto width)
{
	MG_CHECK(width == GetWidth());
}
void GDalGridImp::SetHeight(auto height)
{
	MG_CHECK(height == GetWidth());
}
void GDalGridImp::SetTiled()
{
//		m_hDS->SetMetadataItem()
//		TIFFSetFieldBit(m_TiffHandle, FIELD_TILEDIMENSIONS);
//		TIFFSetField(m_TiffHandle, TIFFTAG_TILEWIDTH, UInt32(256));
//		TIFFSetField(m_TiffHandle, TIFFTAG_TILELENGTH, UInt32(256));
}

void GdalGridSM::ReadGridData(StgViewPortInfo& vpi, AbstrDataObject* ado, tile_id t, SharedStr sqlBandSpecification)
{
	MG_CHECK(m_hDS->GetRasterCount() >=  1 );

	GDalGridImp imp(m_hDS, ado, Size(vpi.GetViewPortExtents()), sqlBandSpecification);
	Grid::ReadGridData(imp, vpi, ado, t);
}

void GdalGridSM::ReadGridCounts(StgViewPortInfo& vpi, AbstrDataObject* ado, tile_id t, SharedStr sqlBandSpecification)
{
	MG_CHECK(m_hDS->GetRasterCount() >= 1);

	GDalGridImp imp(m_hDS, ado, Size(vpi.GetViewPortExtents()), sqlBandSpecification);
	Grid::ReadGridCounts(imp, vpi, ado, t);
}

bool GdalGridSM::WriteDataItem(StorageMetaInfoPtr&& smi)
{
	auto adi = smi->CurrRD();
	if (!HasGridDomain(adi))
		return false;
	auto gridStorageDomain = AsDynamicUnit(smi->StorageHolder());
	if (gridStorageDomain && !adi->GetAbstrDomainUnit()->UnifyDomain(gridStorageDomain))
		return false;

	auto storageHolder = smi->StorageHolder();
	StorageWriteHandle storageHandle(std::move(smi));
	dms_assert(IsOpen());

	MG_CHECK(m_hDS->GetRasterCount() >= 1);

	auto x = m_hDS->GetRasterXSize();
	auto y = m_hDS->GetRasterYSize();

	GDalGridImp imp(m_hDS, adi->GetCurrRefObj(), shp2dms_order(x, y), SharedStr(""));
	ViewPortInfoProvider vpip(storageHolder, adi, false, true);
	Grid::WriteGridData(imp, vpip.GetViewportInfoEx(no_tile), storageHolder, adi, adi->GetCurrRefObj()->GetValuesType());
	return true;
}


bool GdalGridSM::ReadUnitRange(const StorageMetaInfo& smi) const
{
	dms_assert(IsOpen());

	int x = m_hDS->GetRasterXSize();
	int y = m_hDS->GetRasterYSize();

	FixedContextHandle exceptionContext("while reading the extents of raster data; consider another values type for this grid-domain such as IPoint");

	smi.CurrWU()->SetRangeAsIPoint(0, 0, y, x);

	return true;
}

bool GdalGridSM::WriteUnitRange(StorageMetaInfoPtr&& smi)
{
	return smi->CurrRU() == smi->StorageHolder();
}

struct netCDFSubdatasetInfo
{
	UInt32 nx = 0;
	UInt32 ny = 0;
	UInt32 nz = 0;
	ValueClassID vc = VT_Unknown;
};

std::string GetNetCDFItemName(std::string rawItemName)
{
	return rawItemName.substr(rawItemName.find_last_of(":") + 1);
}

SizeT stripDimFromStr(std::string subDatasetItem, int dim)
{
	std::string word = "";
	int cur_dim = 0;
	auto value = std::string(CPLParseNameValue(subDatasetItem.c_str(), nullptr));
	for (auto it = value.cbegin(); it != value.cend(); ++it) {
		if (*it >= '0' && *it <= '9') // to confirm it's a digit 
			word += *it;
		else
		{
			if (cur_dim == dim && not word.empty())
				return std::stoi(word);

			if (not word.empty())
			cur_dim++;
			word = "";
		}
	}
	throwErrorF("GDAL", "Unable to find size indication for dimension %d in dimension string '%s' in identification '%s'", dim, value, subDatasetItem);
}

netCDFSubdatasetInfo GetNetCDFSubdatasetInfo(std::string subDatasetItem)
{
	netCDFSubdatasetInfo netcdf_info;
	netcdf_info.nx = stripDimFromStr(subDatasetItem, 2);
	netcdf_info.ny = stripDimFromStr(subDatasetItem, 1);
	netcdf_info.nz = stripDimFromStr(subDatasetItem, 0);

	return netcdf_info;
}

// Property description for Tif
void GdalGridSM::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrGridStorageManager::DoUpdateTree(storageHolder, curr, sm);

	if (sm == SM_None)
		return;
	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;
	if (curr->IsStorable() && curr->HasCalculator())
		return;

	UpdateMarker::ChangeSourceLock changeStamp( storageHolder, "DoUpdateTree");
	curr->SetFreeDataState(true);

	AbstrUnit* gridDataDomain = GetGridDataDomainRW(curr);
	const AbstrUnit* uBase = FindProjectionBase(storageHolder, gridDataDomain);

	if (!uBase)
	{
		uBase = FindProjectionBase(curr, gridDataDomain);
	}

	StorageReadHandle storageHandle(this, storageHolder, curr, StorageAction::updatetree);
	
	if (!uBase && sm != SM_AllTables)
		return;

	else if (uBase)
	{
		if (IsOpen())
		{
			if (m_hDS->GetRasterCount())
			{
				gdal_transform gdalTr;
				m_hDS->GetGeoTransform(gdalTr);
				gridDataDomain->SetProjection(new UnitProjection(uBase, GetTransformation(gdalTr)));
				gridDataDomain->SetDescr(SharedStr(m_hDS->GetProjectionRef()));
			}
		}
	}

	if (!sm == SM_AllTables)
		return;

	auto subDatasetList = m_hDS->GetMetadata("SUBDATASETS");
	if (subDatasetList)
	{
		std::string itemName, subDatasetName;
		netCDFSubdatasetInfo netCDFSubInfo;

		auto count = CSLCount(subDatasetList);
		for (int i = 0; i < CSLCount(subDatasetList); i++)
		{
			auto subDatasetItem = std::string(CSLGetField(subDatasetList, i));

			if (subDatasetItem.contains("NAME"))
			{
				itemName = GetNetCDFItemName(subDatasetItem);
				subDatasetName = CPLParseNameValue(subDatasetItem.c_str(), nullptr);
				continue;
			}
			else if (subDatasetItem.contains("DESC"))
			{
				netCDFSubInfo = GetNetCDFSubdatasetInfo(subDatasetItem);
			}

			TokenID itemID = GetTokenID_mt(itemName.c_str()); // GDAL
			const UnitClass* uc = nullptr;
			if (!curr->GetSubTreeItemByID(itemID))
			{
				if (netCDFSubInfo.nx > 1 && netCDFSubInfo.ny > 1)
				{
					if (netCDFSubInfo.nx < std::numeric_limits<UInt16>::max() and netCDFSubInfo.ny < (std::numeric_limits<UInt16>::max()))
						uc = Unit<WPoint>::GetStaticClass();
					else
					{
						dms_assert(netCDFSubInfo.nx < std::numeric_limits<UInt32>::max() && netCDFSubInfo.ny < std::numeric_limits<UInt32>::max());
						uc = Unit<UPoint>::GetStaticClass();
					}
				}
				else
				{
					if (netCDFSubInfo.nx*netCDFSubInfo.ny < (std::numeric_limits<UInt8>::max()))
						uc = Unit<UInt8>::GetStaticClass();
					else if (netCDFSubInfo.nx * netCDFSubInfo.ny < (std::numeric_limits<UInt16>::max()))
						uc = Unit<UInt16>::GetStaticClass();
					else if (netCDFSubInfo.nx * netCDFSubInfo.ny < (std::numeric_limits<UInt32>::max()))
						uc = Unit<UInt32>::GetStaticClass();
					else
						uc = Unit<UInt64>::GetStaticClass();
				}
			}
			if (uc)
				uc->CreateUnit(curr, itemID);
			auto dataItem = curr->GetSubTreeItemByID(itemID);
			if (dataItem)
				dataItem->SetStorageManager(subDatasetName.c_str(), "gdal.grid", true);
		}
	}




}

// *****************************************************************************

void ReadBand(GDALRasterBand* poBand, GDAL_SimpleReader::band_data& buffer)
{
	auto width = poBand->GetXSize();
	auto height = poBand->GetYSize();

	typedef UInt32 color_type;
	vector_resize(buffer, Cardinality(IPoint(width, height) ));

	GDAL_ErrorFrame x;
	auto resultCode = poBand->RasterIO(GF_Read,
		0, 0, // roi offset
		width, height, // roi size
		&buffer[0],
		width, height, // buffer size
		gdalDataType(VT_UInt8),
		0, //nPixelSpace,
		0 //nLineSpace
	);
	dms_assert(resultCode == CE_None);
}

WPoint GDAL_SimpleReader::ReadGridData(CharPtr fileName, buffer_type& buffer)
{
	GDAL_ErrorFrame frame;
	GDALDatasetHandle dsHnd = (GDALDataset*)GDALOpen(fileName, GA_ReadOnly);
	if (!dsHnd)
	{
		reportD(SeverityTypeID::ST_Warning, "Failed to open wmts tile, likely due to a corrupted download, delete the file for redownload.");
		return WPoint();
	}

		//throwDmsErrF("GDAL: cannot open %s", fileName);

	auto rBand = dsHnd->GetRasterBand(1);
	MG_CHECK(rBand); 
	ReadBand(rBand, buffer.redBand);
	auto size = buffer.redBand.size();
	if (dsHnd->GetRasterCount() > 2) // Red Green Blue, at least.
	{
		auto gBand = dsHnd->GetRasterBand(2);
		auto bBand = dsHnd->GetRasterBand(3);
		ReadBand(gBand, buffer.greenBand);
		ReadBand(bBand, buffer.blueBand);

		MakeMin(size, buffer.greenBand.size());
		MakeMin(size, buffer.blueBand.size());

		vector_resize(buffer.combinedBands, size);
		SizeT i = 0;
		if (dsHnd->GetRasterCount() > 3) // also a transparency band ?
		{
			auto tBand = dsHnd->GetRasterBand(4);
			ReadBand(tBand, buffer.transBand);
			auto alphaSize = Min<SizeT>(size, buffer.transBand.size());
			for (i = 0; i != alphaSize; ++i)
			{
				UInt8 opacity = buffer.transBand[i];
				if (opacity == 255)
					buffer.combinedBands[i] = (buffer.redBand[i] << 16) | (buffer.greenBand[i] << 8) | buffer.blueBand[i];
				else
				{
					if (!opacity)
						buffer.combinedBands[i] = 0xFFFFFF;
					else
					{
						UInt8 transValue = 255 - opacity;
						buffer.combinedBands[i] =
							((((opacity * buffer.redBand[i]) / 255) << 16) |
								(((opacity * buffer.greenBand[i]) / 255) << 8) |
								(((opacity * buffer.blueBand[i]) / 255)))
							+ transValue * 0x010101;
					}
				}
			}
		}
		// finish with non-alpha combination
		for (; i != size; ++i)
			buffer.combinedBands[i] = (buffer.redBand[i] << 16) | (buffer.greenBand[i] << 8) | buffer.blueBand[i];
	}
	else
	{
		vector_resize(buffer.combinedBands, size);
		for (SizeT i = 0; i != size; ++i)
			buffer.combinedBands[i] = buffer.redBand[i] * 0x010101;
	}

	auto width = rBand->GetXSize();
	auto height = rBand->GetYSize();
	return shp2dms_order(width, height);
}


// Register
IMPL_DYNC_STORAGECLASS(GdalGridSM, "gdal.grid")
IMPL_DYNC_STORAGECLASS(GdalWritableGridSM, "gdalwrite.grid")

struct GdalGridSM2 : GdalGridSM
{
	DECL_RTTI(STGDLL_CALL, StorageClass)
};

IMPL_DYNC_STORAGECLASS(GdalGridSM2, "gdal2.grid")

// type aliasses
namespace {
	StorageClass ilwisStorageManagers(CreateFunc<GdalGridSM>, GetTokenID_st("ilwis"));
}
