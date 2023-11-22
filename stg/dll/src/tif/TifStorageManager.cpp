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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)


// *****************************************************************************
//
// Implementations of TiffSM
//
// *****************************************************************************

#include "TifStorageManager.h"

#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/BaseBounds.h"
#include "geo/color.h"
#include "geo/Round.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ser/BaseStreamBuff.h"  
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "DataStoreManagerCaller.h"

#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "UnitClass.h"
#include "ViewPortInfoEx.h"
#include "Projection.h"

#include "stg/StorageClass.h"

#include "tif/TifImp.h"

// ------------------------------------------------------------------------
// Implementation of the abstact storagemanager interface
// ------------------------------------------------------------------------

// Constructor for this implementation of the abstact storagemanager interface
TiffSM::TiffSM()
{
	DBG_START("TiffSM", "Create", false);
}

TiffSM::~TiffSM()
{ 
	CloseStorage(); 
}

// Open for read/write
void TiffSM::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	DBG_START("TiffSM", "OpenStorage", true);

	dms_assert(!IsOpen());
	assert(rwMode != dms_rw_mode::unspecified);

	DBG_TRACE(("storageName =  %s", GetNameStr().c_str()));

	assert(m_pImp.is_null());

	auto imp = std::make_unique<TifImp>();
	if (rwMode > dms_rw_mode::read_only)
	{
		if (!GetGridData(smi.StorageHolder(), false))
			if (!smi.CurrRD() || !GetGridData(smi.CurrRD(), false))
				smi.StorageHolder()->throwItemErrorF("TiffSM %s has no GridData sub item of the expected type and domain", GetNameStr().c_str());
		auto sfwa = DSM::GetSafeFileWriterArray();
		if (!sfwa|| ! imp->Open(GetNameStr(), TifFileMode::WRITE, sfwa.get()) )
			throwItemError("Unable to open for Write");
	}
	else
	{
		auto sfwa = DSM::GetSafeFileWriterArray();
		bool result = sfwa && imp->Open(GetNameStr(), TifFileMode::READ, sfwa.get());
		MG_CHECK(result); // false after TIFF open error
	}
	m_pImp = imp.release();
}


// Close any open file and forget about it
void TiffSM::DoCloseStorage(bool mustCommit) const
{
	DBG_START("TiffSM", "DoCloseStorage", true);

	dms_assert(IsOpen());
	assert(m_pImp.has_ptr());

	DBG_TRACE(("storageName=  %s", GetNameStr().c_str()));

	m_pImp.reset();
	assert(m_pImp.is_null());
}

bool TiffSM::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	assert(smi);
	assert(borrowedReadResultHolder);

	const TreeItem* storageHolder = smi->StorageHolder();
	AbstrDataItem*  adi = smi->CurrWD();

	assert(storageHolder);
	assert(adi);

	assert(IsOpen());
	assert(m_pImp->IsOpen());

	if (adi->GetID() == PALETTE_DATA_ID)
		return ReadPalette(borrowedReadResultHolder);

	// Collect zoom info
	const GridStorageMetaInfo* gbr = debug_cast<const GridStorageMetaInfo*>(smi.get());
	auto vpi = gbr->m_VPIP.value().GetViewportInfoEx(t);

	vpi.SetWritability(adi);

	if (vpi.GetCountColor() != -1)
		ReadGridCounts(vpi, adi, borrowedReadResultHolder, t);
	else
		ReadGridData  (vpi, adi, borrowedReadResultHolder, t);
	return true;
}

void TiffSM::ReadGridCounts(const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* ado, tile_id t)
{
	DBG_START("TiffSM", "ReadGridCounts", true);
	assert(m_pImp);
	assert(adi);
	assert(ado);

	Grid::ReadGridCounts(*m_pImp, vpi, ado, t, GetNameStr().c_str());
}

void TiffSM::ReadGridData(const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* ado, tile_id t)
{
	DBG_START("TiffSM", "ReadGridData", false);
	assert(m_pImp);
	assert(adi);
	assert(ado);

	// Check if values match with tiff raster value elements
	auto vc_ado = ado->GetValueClass();
	assert(vc_ado);

	auto vcid_ado = vc_ado->GetValueClassID();

	auto vcid_tiff = m_pImp->GetValueClassFromTiffDataTypeTag();
	auto vc_tiff = ValueClass::FindByValueClassID(vcid_tiff);
	if (!vc_tiff)
		adi->throwItemErrorF("Uknown tiff pixel value type");
	if (vc_ado->GetBitSize() != vc_tiff->GetBitSize())
		adi->throwItemErrorF("Mismatch in number of bits between user specified value type: '%s' and tiff pixel value type: '%s'."
			, AsString(vc_ado->GetID())
			, AsString(vc_tiff->GetID())
		);

	if (vcid_ado != vcid_tiff)
		reportF(MsgCategory::storage_read, SeverityTypeID::ST_Warning, "Mismatch between user specified value type: '%s' and tiff pixel value type: '%s' for item %s."
			, AsString(vc_ado->GetID())
			, AsString(vc_tiff->GetID())
			, adi->GetFullName()
		);


	Grid::ReadGridData(*m_pImp, vpi, ado, t, GetNameStr().c_str());
}

bool TiffSM::ReadPalette(AbstrDataObject* ado)
{
	DBG_START("TiffSM", "ReadPalette", true);
	assert(m_pImp);
	assert(ado);
	
	auto nrElems = ado->GetTiledRangeData()->GetRangeSize();
	auto nrColors = (m_pImp->HasColorTable())
		?	Min<SizeT>(m_pImp->GetClrImportant(), nrElems)
		:	0;

	SizeT i;
	for (i=0; i!=nrColors; ++i)
		ado->SetValueAsUInt32(i, m_pImp->GetColor( i ) );

	assert(i <= nrElems);
	// Beyond Eof?
	for (; i != nrElems; ++i)
		ado->SetValueAsUInt32(i, 0 ); 

	return true;
}

bool TiffSM::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	SharedPtr<const TreeItem> storageHolder = smi->StorageHolder();
	if (smi->CurrWD()->GetAbstrDomainUnit()->GetValueType()->GetNrDims() != 2)
		return true;

	SharedPtr<const AbstrDataItem> pd = GetPaletteData(storageHolder);
	if (pd)
	{
		pd->UpdateMetaInfo();
		ValueClassID streamTypeID = GetStreamType(pd)->GetValueClassID();
		if (streamTypeID == ValueClassID::VT_UInt32 || streamTypeID == ValueClassID::VT_Int32
			&& pd->GetID() == PALETTE_DATA_ID
			&& m_pImp->GetNrBitsPerPixel() <= MAX_BITS_PAL
			)
		{
			InterestRetainContextBase::Add(pd);
			pd->PrepareDataUsage(DrlType::CertainOrThrow);
		}
		else
			pd = nullptr;
	}

	StorageWriteHandle storageHandle(std::move(smiHolder));
	{
		dms_assert(m_pImp.has_ptr());

		dms_assert(smi->CurrRI()->GetInterestCount()); // Precondition that 
		const AbstrDataItem* adi = smi->CurrRD();
		const ValueClass* streamType = GetStreamType(adi);
		ViewPortInfoProvider vpip(smi->StorageHolder(), adi, false, true);
		WriteGridData(*m_pImp, vpip.GetViewportInfoEx(no_tile), storageHolder, adi, streamType);     // Byte stream, full image 
	}

	if (pd)
		if (auto nmsm = dynamic_cast<NonmappableStorageManager*>(storageHolder->GetStorageManager()))
		{
			auto paletteWriteMetaInfo = nmsm->GetMetaInfo(storageHolder, const_cast<AbstrDataItem*>(pd.get_ptr()), StorageAction::write);
			WritePalette(*m_pImp, storageHolder, pd); // Long stream, palette colors
		}

	return true;
}

void TiffSM::WriteGridData(TifImp& imp, const StgViewPortInfo& vpi, const TreeItem* storageHolder, const AbstrDataItem* adi, const ValueClass* streamType)
{
	DBG_START("TiffSM", "WriteGridData", true);

	WriteGeoRefFile(adi, replaceFileExtension(GetNameStr().c_str(), "tfw"));

	Grid::WriteGridData(imp, vpi, storageHolder, adi, streamType, GetNameStr().c_str());
}

void TiffSM::WritePalette(TifImp& imp, const TreeItem* storageHolder, const AbstrDataItem* adi)
{
	DBG_START("TiffSM", "WritePalette", true);

	dms_assert(adi);

	dms_assert( imp.GetNrBitsPerPixel() <= MAX_BITS_PAL );
	if (! imp.IsPalettedImage())
		adi->throwItemError("Tiff file was not opened as PalettedImage");

	PreparedDataReadLock lock(adi);

	auto dataHandle = adi->GetRefObj()->GetDataReadBegin();
	const UInt32* data = reinterpret_cast<const UInt32*> (dataHandle.get_ptr());
	SizeT         size = adi->GetAbstrDomainUnit()->GetCount();

	UInt32 nrColorsRepr   = (1 << imp.GetNrBitsPerPixel());
	UInt32 nrColorsDomain = adi->GetAbstrDomainUnit()->GetCount();

	if (nrColorsDomain > MAX_COLORS_PAL)
		adi->throwItemError("Cannnot write palette to TIFF, too many colors");
	MakeMin(nrColorsDomain, nrColorsRepr);

	UInt16 red[MAX_COLORS_PAL], green[MAX_COLORS_PAL], blue[MAX_COLORS_PAL];

	UInt32 i;
	for (i = 0; i != nrColorsDomain; ++i, ++data)
	{
		red  [i] = GetRed  (*data)*257;
		green[i] = GetGreen(*data)*257;
		blue [i] = GetBlue (*data)*257;
	}

	dms_assert(i <= nrColorsRepr);
	dms_assert(nrColorsRepr <= CI_NRCOLORS);
	MakeMin(nrColorsRepr, CI_NRCOLORS-1);
	if (i < nrColorsRepr)
		for (; i != nrColorsRepr; ++i)
		{
			DmsColor clr = STG_Bmp_GetDefaultColor(i);
			red  [i] = GetRed  (clr)*257;
			green[i] = GetGreen(clr)*257;
			blue [i] = GetBlue (clr)*257;
		}

	dms_assert(i <= MAX_COLORS_PAL);
	for (; i != MAX_COLORS_PAL; ++i)
	{
		red  [i] = 0; 
		green[i] = 0;
		blue [i] = 0;
	}

	if (!imp.SetColorMap(red, green, blue))
		adi->throwItemError("Cannnot write palette to TIFF, SetColorMap failed");
}


bool TiffSM::ReadUnitRange(const StorageMetaInfo& smi) const
{
	dms_assert(IsOpen());

	dms_assert(IsOpen());
	MG_CHECK(m_pImp->IsOpen());

	UpdateMarker::ChangeSourceLock changeStamp( smi.CurrWU(), "ReadUnitRange");
	if (smi.CurrWU()->GetNrDimensions() == 2)
		smi.CurrWU()->SetRangeAsIPoint(0, 0, m_pImp->GetHeight(), m_pImp->GetWidth() );
	else
	{
		if (!m_pImp->HasColorTable())
			return false; // cannot return Range of ValuesUnit if Tiff has no palette (assumed when more bits per pixel)
		smi.CurrWU()->SetRangeAsUInt64(0, m_pImp->GetClrImportant());
	}
	return true;
}

// Property description for Tif
void TiffSM::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
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

	SharedStr projectionFileName = replaceFileExtension(GetNameStr().c_str(), "tfw");
	bool tfw_file_exists = IsFileOrDirAccessible(projectionFileName);

	std::vector<Float64> pixel_to_world_transform = {};
	auto storage_manager = storageHolder->GetStorageManager();
	auto nmsm = dynamic_cast<NonmappableStorageManager*>(storage_manager);
	if (nmsm)
	{
		auto smi = nmsm->GetMetaInfo(storageHolder, curr, StorageAction::read);
		DoOpenStorage(*smi, dms_rw_mode::read_only);
		pixel_to_world_transform = m_pImp->GetAffineTransformation();
		m_IsOpen = true;
		//DoCloseStorage(false);
	}

	// GridData item && GridPalette item
	const AbstrDataItem* gridData  = GetGridData(storageHolder, tfw_file_exists || !pixel_to_world_transform.empty());
	const AbstrDataItem* paletteData = GetPaletteData(storageHolder);

	//if (!gridData || !paletteData)
	//	storageHolder->throwItemErrorF("No user defined GridData or PaletteData attribute found for storage item %s.", storageHolder->GetFullName().c_str());
	MG_CHECK( !gridData || !paletteData || gridData->GetAbstrValuesUnit()->UnifyDomain(paletteData->GetAbstrDomainUnit()) );

	// Compare value type of tiff with value type of griddata / palettedata
	//DoOpenStorage(const StorageMetaInfo & smi, dms_rw_mode rwMode) const

	// Use pixel to world transformation obtained form GeoTiff tags
	if (!pixel_to_world_transform.empty())
	{
		AbstrUnit* gridDataDomainRW = GetGridDataDomainRW(const_cast<TreeItem*>(storageHolder));
		if (!gridDataDomainRW)
			return;

		const AbstrUnit* uBase = FindProjectionBase(storageHolder, gridDataDomainRW);
		if (!uBase)
			return;
		uBase->UpdateMetaInfo();
		DPoint factor(pixel_to_world_transform[3], pixel_to_world_transform[0]);
	    DPoint offset(pixel_to_world_transform[5], pixel_to_world_transform[4]);
		gridDataDomainRW->SetProjection(new UnitProjection(AsUnit(uBase->GetCurrUltimateItem()), offset - 0.5 * factor, factor));
		return;
	}

	// Final straw, get pixel to world transformation from .tfw file
	ReadProjection(curr, projectionFileName);
}

// Register
IMPL_DYNC_STORAGECLASS(TiffSM, "tif")
