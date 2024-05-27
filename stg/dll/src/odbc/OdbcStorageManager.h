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

// storagemanager for 'ODBC storages'

#if !defined(__STG_ODBC_STORAGEMANAGER_H)
#define __STG_ODBC_STORAGEMANAGER_H

// for AbstactStorageManager interface
#include "StgBase.h"
#include "odbc/ODBCImp.h"
#include "ptr/OwningPtr.h"

#include <map>

// *****************************************************************************
//		DMS object to store and retrieve ODBC data
// *****************************************************************************

class ODBCStorageManager : public NonmappableStorageManager
{
	typedef AbstrStorageManager base_type;
	friend class ODBCStorageOutStreamBuff; 
	friend class ODBCStorageInpStreamBuff; 

public:
	ODBCStorageManager();

	//	implement AbstrStorageManager interface
	bool DoCheckExistence(const TreeItem* storageHolder, const TreeItem* storageItem) const override;
	bool DoCheckWritability() const override;
	
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	bool ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool ReadUnitRange(const StorageMetaInfo& smi) const override;

	STGDLL_CALL SharedStr GetDatabaseFilename(const TreeItem* storageHolder);

	TDatabase*         DatabaseInstance    (const TreeItem* storageHolder) const;
	TDatabase*         OpenDatabaseInstance(const TreeItem* storageHolder) const;
	TRecordSet*        GetRecordSet(const TreeItem* storageHolder, TreeItem* tableHolder, SharedStr sqlString) const;

	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;

private:
	typedef std::map<SharedStr, TIMESTAMP_STRUCT> TTableTimestampCacheType;
	typedef SharedPtr<TRecordSet>               TRecordSetRef;

//	TIMESTAMP_STRUCT AccessTableLastUpdate(const TreeItem* storageHolder, const TreeItem* tableHolder);

	void ResetAccessSysObjectsCopy() // Public soon? Triggered by Invalidate or something?
	{
		m_HasAccessSysObjectsCopy = false;
		m_TableTimestampCache.clear();
	}

	mutable OwningPtr<TDatabase>        m_Database;
	mutable const TreeItem*             m_TiDatabase;
	mutable std::map<TreeItem*, TRecordSetRef> m_RecordSets;
	bool                                m_HasAccessSysObjectsCopy;
	TTableTimestampCacheType            m_TableTimestampCache;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

#endif // __STG_ODBC_STORAGEMANAGER_H

/*****************************************************************************/
//										END OF FILE
/*****************************************************************************/
