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

/*****************************************************************************/
// DMS object to store and retrieve ascii tables
/*****************************************************************************/

#if !defined(__STG_DBF_STORAGEMANAGER_H)
#define __STG_DBF_STORAGEMANAGER_H

//#include "StgImpl.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"

/*****************************************************************************/
//									CLASSES
/*****************************************************************************/

// hidden implementation
class DbfImpl;
struct TNameSet;

// storagemanager for 'Dbf-grids'
class DbfStorageManager : public AbstrStorageManager
{
	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
	void DoWriteTree  (const TreeItem* storageHolder) override;

	bool ReadDataItem (StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;

private:
	TNameSet*     BuildNameSet(const TreeItem* storageHolder) const;
	void          TestDomain(const AbstrDataItem* adi) const;

//	hidden implementation which doesn't know about DMS structure
	mutable SharedPtr<TNameSet>        m_NameSet;          // nameset cache
	mutable const TreeItem*            m_NameSetStorageHolder = nullptr; // used to check for which storageHolder the nameset cache was made
	mutable SharedPtr<const AbstrUnit> m_TableDomain;

	friend struct DbfMetaInfo;
	DECL_RTTI(STGDLL_CALL,StorageClass)
};

#endif // __STG_DBF_STORAGEMANAGER_H
