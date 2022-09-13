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
// DMS object to store and retrieve ascii tables
//
// *****************************************************************************

#if !defined(__STG_XDB_STORAGEMANAGER_H)
#define __STG_XDB_STORAGEMANAGER_H

// for AbstactStorageManager interface
#include "StgBase.h"

// storagemanager for saving string info
class StrStorageManager : public AbstrStorageManager
{
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

protected:
	bool ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	virtual SharedStr GetFileName(const TreeItem* storageHolder, const TreeItem* curr, SizeT recNo) const;
	virtual SizeT     GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const;

private:
	DECL_RTTI(STGDLL_CALL,StorageClass)
};

class StrFilesStorageManager : public StrStorageManager
{
	typedef StrStorageManager base_type;
public:
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	StorageMetaInfoPtr GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction) const override;
	bool ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	bool WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

protected:
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const override;

	SharedStr GetFileName(const TreeItem* storageHolder, const TreeItem* curr, SizeT recNo) const override;
	SizeT     GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const override;

private:
	const AbstrDataItem* GetFileNameAttr(const TreeItem* storageHolder, const TreeItem* curr) const;
	mutable SharedPtr<const AbstrDataItem> m_FileNameAttr;

	DECL_RTTI(STGDLL_CALL,StorageClass)
};


#endif // __STG_XDB_STORAGEMANAGER_H
