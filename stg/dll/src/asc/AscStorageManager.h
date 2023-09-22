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

// *****************************************************************************
// 
// DMS object to store and retrieve 'Ascii-grids'
//
// *****************************************************************************

#if !defined(__STG_ASC_STORAGEMANAGER_H)
#define __STG_ASC_STORAGEMANAGER_H

#include "RtcGeneratedVersion.h"

#include "StgBase.h"
#include "GridStorageManager.h"

// storagemanager for 'Ascii-grids'
class AsciiStorageManager : public AbstrGridStorageManager
{
public:
	AsciiStorageManager()
	{
		reportD(SeverityTypeID::ST_Warning, "AsciiStorageManager is depreciated and will be removed in GeoDms version 15.0.0");
		static_assert(DMS_VERSION_MAJOR != 15);
	}

//	implement AbstrStorageManager interface
	bool ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smi) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	STGDLL_CALL bool AllowRandomTileAccess() const override { return false; }

	DECL_RTTI(STGDLL_CALL, StorageClass)

private:
	void ReadGridCounts(const StgViewPortInfo& vpi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
	bool ReadGridData  (const StgViewPortInfo& vpi, AbstrDataItem* adi, AbstrDataObject* borrowedReadResultHolder, tile_id t);
};

#endif // _AsciiStorageManager_H_
