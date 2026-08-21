// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
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
class StrStorageManager : public NonmappableStorageManager
{
	void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

protected:
	FileResult ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

	virtual SharedStr GetFileName(const TreeItem* storageHolder, const TreeItem* curr, SizeT recNo) const;
	virtual SizeT     GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const;

private:
	DECL_RTTI(,StorageClass)
};

class StrFilesStorageManager : public StrStorageManager
{
	typedef StrStorageManager base_type;
public:
	void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;

	FileResult ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	FileResult WriteDataItem(StorageMetaInfoPtr&& smiHolder) override;

protected:
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor, const TreeItem* storageHolder, const TreeItem* self) const override;

	SharedStr GetFileName(const TreeItem* storageHolder, const TreeItem* curr, SizeT recNo) const override;
	SizeT     GetNrFiles (const TreeItem* storageHolder, const TreeItem* curr) const override;

private:
	const AbstrDataItem* GetFileNameAttr(const TreeItem* storageHolder, const TreeItem* curr) const;
	mutable std::shared_ptr<const AbstrDataItem> m_FileNameAttr;

	DECL_RTTI(,StorageClass)
};


#endif // __STG_XDB_STORAGEMANAGER_H
