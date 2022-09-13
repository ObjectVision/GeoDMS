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
// AStorageManager.h: interface for the AStorageManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__TIC_STG_ABSTRSTREAMMANAGER_H)
#define __TIC_STG_ABSTRSTREAMMANAGER_H

#include "AbstrStorageManager.h"

class InpStreamBuff;
class OutStreamBuff;


// *****************************************************************************
// 
// AbstrStreamManager
//
// *****************************************************************************

class AbstrStreamManager : public AbstrStorageManager
{
public:
	TIC_CALL AbstrStreamManager();

	TIC_CALL std::unique_ptr<OutStreamBuff> OpenOutStream(const StorageMetaInfo& smi, CharPtr path, tile_id t);
	TIC_CALL std::unique_ptr<InpStreamBuff> OpenInpStream(const StorageMetaInfo& smi, CharPtr path) const;

	TIC_CALL bool ReadDataItem(const StorageMetaInfo& smi, AbstrDataObject* borrowedReadResultHolder, tile_id t) override;
	TIC_CALL bool WriteDataItem(StorageMetaInfoPtr&& smi) override;

protected:
	TIC_CALL virtual std::unique_ptr<OutStreamBuff> DoOpenOutStream(const StorageMetaInfo& smi, CharPtr, tile_id t) = 0;
	TIC_CALL virtual std::unique_ptr<InpStreamBuff> DoOpenInpStream(const StorageMetaInfo& smi, CharPtr) const = 0;
	TIC_CALL bool ReadUnitRange(const StorageMetaInfo& smi) const override;
	TIC_CALL bool WriteUnitRange(StorageMetaInfoPtr&& smi) override;
	TIC_CALL void DoUpdateTree (const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const override;
};

#endif // __TIC_STG_ABSTRSTREAMMANAGER_H
