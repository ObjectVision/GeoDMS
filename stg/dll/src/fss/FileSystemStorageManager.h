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

//////////////////////////////////////////////////////////////////////

#if !defined(__STG_FILESYSTEM_STORAGEMANAGER_H)
#define __STG_FILESYSTEM_STORAGEMANAGER_H


#include "StgBase.h"
#include "ptr/OlePtr.h"
#include "ser/SafeFileWriter.h"
#include "ser/FileMapHandle.h"

#include "stg/AsmUtil.h"
#include "stg/AbstrStreamManager.h"
/*
 *	FileSystemStorageManager
 *
 */

class FileSystemStorageManager : public AbstrStreamManager
{
	friend class CompoundStorageOutStreamBuff; // can only be accessed by this class

public:

	STGDLL_CALL ~FileSystemStorageManager();

	STGDLL_CALL SharedStr GetFullFileName(CharPtr name) const;

protected:
//	implement AbstrStorageManager interface
	void DropStream(const TreeItem* item, CharPtr path) override;
	FileDateTime GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const override;

	std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t) override;
	std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const override;

	void DoOpenStorage  (const StorageMetaInfo& smi, dms_rw_mode rwMode) const override;
	void DoCloseStorage (bool mustCommit) const override;

	mutable FileHandle m_FssLockFile;

	friend class FileSystemStorageOutStreamBuff;
	friend class FileSystemStorageInpStreamBuff;

	DECL_RTTI(STGDLL_CALL, StorageClass)
};

#endif // !defined(__STG_FILESYSTEM_STORAGEMANAGER_H)
