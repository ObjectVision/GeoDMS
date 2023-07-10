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

#if !defined(__STG_TIF_STORAGEMANAGER_H)
#define __STG_TIF_STORAGEMANAGER_H

#include "StgBase.h"
#include "GridStorageManager.h"
#include "geo/Color.h"
#include "ptr/OwningPtr.h"

// hidden implementation
class TifImp;

// *****************************************************************************

// storagemanager for 'Tif-grids'
struct TiffSM : AbstrGridStorageManager
{
	TiffSM();
	~TiffSM();

    // Implement AbstrStorageManager interface
	void DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage(bool mustCommit) const override;
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	bool ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool CanWriteTiles() const override { return true; }

private:
	void ReadGridCounts(const TifImp& imp, const StgViewPortInfo& vpi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
	void ReadGridData  (const TifImp& imp, const StgViewPortInfo& vpi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
	bool ReadPalette   (const TifImp& imp, AbstrDataObject* ado);

	void WriteGridData(TifImp& imp, const StgViewPortInfo& vpi, const TreeItem* storageHolder, const AbstrDataItem* adi, const ValueClass* streamType);
	void WritePalette (TifImp& imp, const TreeItem* storageHolder, const AbstrDataItem* adi);

private:
	mutable OwningPtr<TifImp> m_pImp;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};


#endif // __STG_TIF_STORAGEMANAGER_H
