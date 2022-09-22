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
#include "StoragePCH.h"
#pragma hdrstop


// FileSystemStorageManager.cpp: implementation of the FileSystemStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#include "fss/FileSystemStorageManager.h"

#include "dbg/Debug.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/SplitPath.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataStoreManagerCaller.h"

#include "stg/StorageClass.h"

//////////////////////////////////////////////////////////////////////
// FileSystemStorageManager implementation
//////////////////////////////////////////////////////////////////////

FileSystemStorageManager::~FileSystemStorageManager()
{
	CloseStorage();
	dms_assert(!m_FssLockFile.IsOpen());
}

void FileSystemStorageManager::DropStream(const TreeItem* item, CharPtr path)
{
	dms_assert(item);

	reportF(SeverityTypeID::ST_MajorTrace, "Drop  fss(%s,%s)", GetNameStr().c_str(), path);

	DSM::GetSafeFileWriterArray(item)->OpenOrCreate(
		GetFullFileName(path).c_str(), 
		FCM_Delete
	);
}

SharedStr FileSystemStorageManager::GetFullFileName(CharPtr name) const
{
	return DelimitedConcat(GetNameStr().c_str(), MakeDataFileName(name).c_str());
}

FileDateTime FileSystemStorageManager::GetLastChangeDateTime(const TreeItem* storageHolder, CharPtr path) const
{
	if (DoesExist(storageHolder)) // TODO: lock deze file vanaf hier.
	{
		m_FileTime = GetFileOrDirDateTime(
			DSM::GetSafeFileWriterArray(storageHolder)->GetWorkingFileName(GetFullFileName(path), FCM_OpenReadOnly)
		);
	}
	return m_FileTime; 
}

std::unique_ptr<OutStreamBuff> FileSystemStorageManager::DoOpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t)
{
	const AbstrDataItem* adi = AsDynamicDataItem(smi.CurrRI());

	dms_assert(!m_IsReadOnly);

	reportF(SeverityTypeID::ST_MajorTrace, "Write fss(%s,%s)", GetNameStr().c_str(), path);

	SharedStr fullName = GetFullFileName(path); 
	SafeFileWriterArray* sfwa = DSM::GetSafeFileWriterArray(smi.StorageHolder());
	if (adi)
	{
		dms_assert(t != no_tile);
		const AbstrDataObject* ado = adi->GetRefObj();

		return std::make_unique<MappedFileOutStreamBuff>(
			fullName
		,	sfwa
		,	ado->GetNrTileBytesNow(t, true)
		);
	}
	dms_assert(t == no_tile);
	return std::make_unique<FileOutStreamBuff>( fullName, sfwa, false );
}

std::unique_ptr<InpStreamBuff> FileSystemStorageManager::DoOpenInpStream(const StorageMetaInfo& smi, CharPtr path) const
{
	reportF(SeverityTypeID::ST_MajorTrace, "Read  fss(%s,%s)", GetNameStr().c_str(), path);

	dms_assert(IsOpen());

	auto result = std::make_unique<MappedFileInpStreamBuff>(
		GetFullFileName(path)
	,	DSM::GetSafeFileWriterArray(smi.StorageHolder())
	,	false
	,	false);

	if (!result->IsOpen())
		return {};
	return result;
}

void FileSystemStorageManager::DoOpenStorage(const StorageMetaInfo& smi, dms_rw_mode rwMode) const
{
	dms_assert(!IsOpen());
	dms_assert(rwMode != dms_rw_mode::unspecified);

	SharedStr fileName = DelimitedConcat(GetNameStr().c_str(), "LockFile.fss");
	if (rwMode > dms_rw_mode::read_only)
		m_FssLockFile.OpenRw(fileName, nullptr, 0, rwMode, true, false, true);
	else
		m_FssLockFile.OpenForRead(fileName, nullptr, true, false, true);
}

void FileSystemStorageManager::DoCloseStorage (bool mustCommit) const
{
	m_FssLockFile.CloseFile();
}

//----------------------------------------------------------------------
// instantiation and registration
//----------------------------------------------------------------------

IMPL_DYNC_STORAGECLASS(FileSystemStorageManager, "FSS")
