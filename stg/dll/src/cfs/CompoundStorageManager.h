// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once
// CompoundStorageManager.h: interface for the CompoundStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__STG_COMPOUND_STORAGEMANAGER_H)
#define __STG_COMPOUND_STORAGEMANAGER_H


#include "StgBase.h"
#include "stg/AsmUtil.h"
#include "AbstrStreamManager.h"
#include "ptr/OLEPtr.h"

//class CMemFile;
struct IStream;
struct IStorage;
class CompoundStorageOutStreamBuff;
template <class T> struct cfsptr;
/*
 *	CompoundStorageManager
 *
 *	Implementation over the OLE structured-storage (compound file) API, IStorage/IStream.
 *	Full support of the AbstrStorageManager contract.
 */

class CompoundStorageManager : public AbstrStreamManager
{
	friend class CompoundStorageOutStreamBuff; // can only be accessed by this class

	~CompoundStorageManager();

//	implement AbstrStorageManager interface
	void DoOpenStorage  (const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage (bool mustCommit) const override;

//	implement AbstrStreamManager interface
	std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr, tile_id t) override;
	std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr) const override;

private:
	void CheckResult(UInt32 result, CharPtr func, CharPtr path) const;
	bool IsCompoundStorageFile() const;
	void CreateNewFile(WeakStr workingFileName);

	cfsptr<IStream>* GetStream(CharPtr path, bool mayCreate);
	IStorage*	OpenSubStorage(IStorage* p_parent, CharPtr name, bool mayCreate);
	IStream*	OpenDataStream(IStorage* p_parent, bool mayCreate);

	mutable OlePtr<IStorage> m_Root; // Storage pointer to the root of the compound file tree

	friend class CompoundStorageOutStreamBuff;
	friend class CompoundStorageInpStreamBuff;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

#endif // !defined(__STG_COMPOUND_STORAGEMANAGER_H)
