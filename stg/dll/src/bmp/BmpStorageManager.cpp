// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// *****************************************************************************
//
// Implementations of - BmpStorageOutStreamBuff
//                    - BmpStorageInpStreamBuff
//                    - BmpPalStorageManager
//
// *****************************************************************************

#include "BmpStorageManager.h"
#include "bmp/BmpImp.h"

#include "act/InterestRetainContext.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "geo/color.h"
#include "mci/ValueClass.h"  
#include "ptr/InterestHolders.h"
#include "ser/BaseStreamBuff.h"  
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "TileLock.h"
#include "TreeItemContextHandle.h"
#include "Unit.h"

#include "stg/StorageClass.h"

#include <share.h>

// ------------------------------------------------------------------------
//
// Implementation of OutStreamBuff interface
//
// ------------------------------------------------------------------------

static DmsColor g_BmpDefaultPalette [ 259 ] =
{ 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
	0xFFFFFFFF, // Palette Index 255 (NO_DATA):  transparent  color
	0x00FFFFFF,  // Palette Index 256 (VIEWPORT BACKGROUND); white color
	0x00FF0000,  // Palette Index 257 (RAMP_START): clBlue
	0x000000FF   // Palette Index 258 (RAMP_END  ): clRed
};

struct InitBmpDefaultPalette
{
	InitBmpDefaultPalette()
	{
		for (PALETTE_SIZE c=1; c!=(CI_NODATA+1); c++)
			g_BmpDefaultPalette[ c-1 ] =
				CombineRGB(
					255-(c%3)*127, 
					255-(c%5)*63, 
					255-(c%17)*15
				);  // quasi 8 colors	
	}

} g_DoInit;

void CompleteColors(BmpImp& imp, PALETTE_SIZE nextColor)
{
	while (nextColor < CI_NODATA)
	{
		imp.SetCombinedColor(nextColor, g_BmpDefaultPalette[nextColor] );
		++nextColor;
	}
	imp.SetColorWithAlpha(CI_NODATA, 0xFF, 0xFF, 0xFF, 0xFF);
}

extern "C" STGDLL_CALL void DMS_CONV STG_Bmp_SetDefaultColor(PALETTE_SIZE i, DmsColor color)
{
	DMS_CALL_BEGIN

		assert(i <= CI_LAST);

		color &= MAX_COLOR;

		if (i != CI_NODATA)
			g_BmpDefaultPalette[i] = color;

	DMS_CALL_END
}


extern "C" STGDLL_CALL DmsColor DMS_CONV STG_Bmp_GetDefaultColor(PALETTE_SIZE i)
{
	DMS_CALL_BEGIN

		dms_assert(i <= CI_LAST);
		return g_BmpDefaultPalette[i];

	DMS_CALL_END
	return 0;
}


// ------------------------------------------------------------------------
//
// Helper classes
//
// ------------------------------------------------------------------------

inline UInt32 Mirror(UInt32 row, UInt32 height) { return height - 1 - row; };


namespace Bmp
{
	class GridDataHandler
	{
	public:
		void ReadData (BmpPalStorageManager* self, const BmpImp& Imp, AbstrDataObject* adi);
		void WriteData(BmpPalStorageManager* self, BmpImp& imp, const AbstrDataItem* adi);
	};

	class PaletteDataHandler
	{
	public:
		void ReadData (BmpPalStorageManager* self, const BmpImp& imp, AbstrDataObject* ado);
		void WriteData(BmpPalStorageManager* self, BmpImp& imp, const AbstrDataItem* adi);
	};

	void GridDataHandler::ReadData (BmpPalStorageManager* self, const BmpImp& imp, AbstrDataObject* gridData)
	{
		// note the width (needed to know when to cache incomplete rows) 

		UInt32 width = imp.GetWidth();
		UInt32 height= imp.GetHeight();

		auto nrBytes = Cardinality(UPoint(width, height));
		if (gridData->GetTiledRangeData()->GetRangeSize() != nrBytes)
			self->throwItemError("nr rows and colums are conflicting with configuration");

		std::vector<UInt8> buffer(width, 0);
		
		for (UInt32 r=height; r>0; --r)
		{
			imp.GetRow(Mirror(r, height), &*buffer.begin());
			gridData->SetValuesAsUInt8Array(tile_loc(no_tile, r*width), width, &*buffer.begin());
		}
	}

	void GridDataHandler::WriteData(BmpPalStorageManager* self, BmpImp& imp, const AbstrDataItem* gridData)
	{
		dms_assert(gridData);
		const AbstrUnit* gridUnit = gridData->GetAbstrDomainUnit();
		
		if (!gridUnit || gridUnit->GetNrDimensions() != 2)
			self->throwItemErrorF("%s has no grid domain ", gridData->GetFullName().c_str() );

		WriteGeoRefFile(gridData, replaceFileExtension(self->GetNameStr().c_str(), "bmpw") );
		
		// Set width and height
		UInt32 h = gridUnit->GetDimSize(0);
		UInt32 w = gridUnit->GetDimSize(1);
		UInt32 n = gridUnit->GetCount();
		dms_assert( w*h == n );

		imp.SetHeight(h); 
		imp.SetWidth (w); 

		const AbstrUnit* indexUnit = gridData->GetAbstrValuesUnit();

		imp.SetSubFormat(NO_COMPRESSION);
		imp.SetFalseColorImage();
		imp.SetClrImportant(gridData->GetRefObj()->GetValuesRangeCount());
		if (!imp.FileExisted())		// don't overwrite any existing palette with default colors
			CompleteColors(imp, 0);

		Bool result= imp.Open(self->GetNameStr(), BMP_WRITE);	// temp, write header
		if (!result)
			self->throwItemError("WriteData: cannot create or open file");

		std::vector<UInt8> buffer(w, 0);
		UInt8* buffPtr = begin_ptr(buffer);
		const AbstrDataObject* ado = gridData->GetCurrRefObj();

		if (ado->GetTiledRangeData()->GetNrTiles() > 1)
		{
//* DEBUG
			gridUnit = AsUnit(gridUnit->GetUltimateItem());
			SizeT rw = Cardinality(UPoint(w, h));
			for (UInt32 r=h; r>0;)
			{
				--r; rw -= w;
				for (UInt32 c=0; c!=w; ++c)
				{
					tile_loc tl = gridUnit->GetTiledRangeData()->GetTiledLocation(rw + c);
					if (tl.first == no_tile)
					{
						buffPtr[c] = 0; // conform TIFF
						continue;
					}
					ado->GetValuesAsUInt8Array(tl, 1, buffPtr+c);
				}
				imp.SetRow(Mirror(r, h), buffPtr);
			}
//*/
		}
		else
		{
			ReadableTileLock tileLock(ado, no_tile);
			for (UInt32 r=h, rw = h*w; r>0;)
			{
				--r; rw -= w;
				ado->GetValuesAsUInt8Array(tile_loc(no_tile, rw), w, buffPtr);
				imp.SetRow(Mirror(r, h), buffPtr);
			}
		}
	}

	void PaletteDataHandler::ReadData (BmpPalStorageManager* self, const BmpImp& imp, AbstrDataObject* ado)
	{
		dms_assert(ado);
//		const AbstrUnit* indexUnit = di->GetAbstrDomainUnit();

		UInt32 nrColors = imp.GetClrImportant();
		MakeMin(nrColors, UInt32(255));
		MakeMin(nrColors, ado->GetTiledRangeData()->GetRangeSize());

		while (nrColors--)
			ado->SetValueAsUInt32(
				nrColors
			,	imp.GetCombinedColor(nrColors)
			);
	}

	// If the bmp doesn't exist, a palette should be written
	// a 16x16 pixel placeholder is created
	void InitializePaletteFile(BmpImp& imp)
	{
		DBG_START("BmpPalStorageManager", "InitializePaletteFile", true);

		imp.SetSubFormat(NO_COMPRESSION);
		imp.SetFalseColorImage();
		imp.SetWidth (16);
		imp.SetHeight(16);
		for (long c=0; c<256; c++) imp.SetColor(c, 0, 0, 0);  // black	

		unsigned char buf[16];
		long row = 0;
		long col = 0;
		for (row = 0; row < 16; row++)
		{
			for (col = 0; col < 16; col++)
				buf[col] = 16*row + col;
			imp.SetRow(15-row, buf);
		}
	}


	void PaletteDataHandler::WriteData(BmpPalStorageManager* self, BmpImp& imp, const AbstrDataItem* di)
	{
		Bool result = imp.Open(self->GetNameStr(), BMP_EDITPALETTE);
		if (!result)
			self->throwItemError("WriteData: cannot create or open file");

		if (!imp.FileExisted())
			InitializePaletteFile(imp);

		UInt32 count = di->GetAbstrDomainUnit()->GetCount();
		MakeMin(count, UInt32(255));
		imp.SetClrImportant(count);

		for (UInt32 ci = 0; ci !=count; ++ci)
		{
			DmsColor color = di->GetRefObj()->GetValueAsUInt32(ci);
			if (!IsDefined(color))
				color = g_BmpDefaultPalette[255];
			imp.SetCombinedColor(ci,  color); 
		}
		CompleteColors(imp, count);
	}

} // namespace Bmp


// ------------------------------------------------------------------------
//
// Implementation of the abstact storagemanager interface
//
// ------------------------------------------------------------------------

bool BmpPalStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	dms_assert(t == no_tile);

	// Pass a stream object
	BmpImp imp;
	if (! imp.Open(GetNameStr(), BMP_READ) )
		throwItemError("cannot open file for reading");

//	OwningPtr<Bmp::AbstrDataHandler>  f(Bmp::AbstrDataHandler::Create(adi));
//	dms_assert(f);
//	f->ReadData(this, &imp, adi);
	AbstrDataItem* adi = smi->CurrWD();
	switch (adi->GetAbstrDomainUnit()->GetValueType()->GetNrDims())
	{
	case 2: 
		{
			Bmp::GridDataHandler().ReadData(this, imp, borrowedReadResultHolder);
			return true;
		}
	case 1: 
		{
			Bmp::PaletteDataHandler().ReadData(this, imp, borrowedReadResultHolder);
			return true;
		}
	}
	return false;
}

bool BmpPalStorageManager::WriteDataItem(StorageMetaInfoPtr&& smiHolder)
{
	auto smi = smiHolder.get();
	StorageWriteHandle hnd(std::move(smiHolder));

	BmpImp imp;
	const AbstrDataItem* adi = smi->CurrRD();
	switch (adi->GetAbstrDomainUnit()->GetValueType()->GetNrDims())
	{
		case 2:
		{
			Bmp::GridDataHandler().WriteData(this, imp, adi);
			adi = GetPaletteData(smi->StorageHolder());
			if (!adi)
				return true;
		}
		[[fallthrough]];
		case 1:
		{
			InterestRetainContextBase::Add(adi); // TODO: make this interest dependent on GridData, as in TifStorageManager::VisitSuppliers; maybe even move that to the common base class.
			PreparedDataReadLock paletteLock(adi);

			Bmp::PaletteDataHandler().WriteData(this, imp, adi);
			return true;
		}
	}
	throwItemErrorF("WriteDataItem(): Cannot create DataHandler for %s",	adi->GetFullName().c_str() );
}

bool BmpPalStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	if (smi.CurrRU()->GetNrDimensions() != 2)
		return false;

	BmpImp imp; 
	if (!imp.Open(GetNameStr(), BMP_READ))
		return false;

	AbstrUnit* gridDomain = smi.CurrWU();
	UpdateMarker::ChangeSourceLock changeStamp(gridDomain, "ReadUnitRange");
	gridDomain->SetRangeAsIPoint(0, 0, imp.GetHeight(), imp.GetWidth());
	return true;
}

// Property description for Bmp
void BmpPalStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrGridStorageManager::DoUpdateTree(storageHolder, curr, sm);

	dms_assert(storageHolder);
	if (storageHolder != curr)
		return;
	if (curr->IsStorable() && curr->HasCalculator())
		return;
	if (!DoesExist(storageHolder))
		return;

	UpdateMarker::ChangeSourceLock changeStamp( storageHolder, "DoUpdateTree");
	curr->SetFreeDataState(true);

	SharedStr projectionFileName = replaceFileExtension(storageHolder->GetStorageManager()->GetNameStr().c_str(), "bmpw");
	// GridData item && GridPalette item 
	const AbstrDataItem* gridData  = GetGridData(storageHolder, IsFileOrDirAccessible(projectionFileName));
	const AbstrDataItem* paletteData = GetPaletteData(storageHolder);

	MG_CHECK(!gridData || !paletteData || gridData->GetAbstrValuesUnit() == paletteData->GetAbstrDomainUnit());

	GetImageToWorldTransformFromFile(curr, projectionFileName);
}


class PalStorageManager : public BmpPalStorageManager
{
	bool HasGridData() override { return false; }

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

class BmpStorageManager : public BmpPalStorageManager
{
	bool HasGridData() override { return true; }

	DECL_RTTI(STGDLL_CALL, StorageClass)
};


// Register

IMPL_DYNC_STORAGECLASS(BmpStorageManager, "bmp")
IMPL_DYNC_STORAGECLASS(PalStorageManager, "pal")
