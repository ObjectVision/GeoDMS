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
// CompoundStorageManager.h: interface for the CompoundStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__STG_COMPOUND_STORAGEMANAGER_H)
#define __STG_COMPOUND_STORAGEMANAGER_H


#include "StgBase.h"
#include "stg/AsmUtil.h"
#include "stg/AbstrStreamManager.h"
#include "ptr/OlePtr.h"

//class CMemFile;
struct IStream;
struct IStorage;
class CompoundStorageOutStreamBuff;
template <class T> struct cfsptr;
/*
 *	CompoundStorageManager
 *
 *	Implementation for the compound file structured of MFC. Full support of the 
 *	CAbstractStorageManager contract.
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
