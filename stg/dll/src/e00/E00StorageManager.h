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
// DMS object to retrieve 'E00-grids' (Esri)
//
// *****************************************************************************

#if !defined(__STG_E00_STORAGEMANAGER_H)
#define __STG_E00_STORAGEMANAGER_H

// for AbstactStorageManager interface
#include "DllMain.h"

// hidden implementation
class E00Imp;

// storagemanager for 'E00-grids' (Esri)
class E00StorageManager : public AbstrStorageManager
{

public:

	E00StorageManager();
	virtual	~E00StorageManager();

//	implement AbstrStorageManager interface
	virtual void DoCreateStorage(const TreeItem* storageHolder) override;
	virtual void DoOpenStorage  (bool intentionToWrite) override ;
	virtual void DoCloseStorage (bool mustCommit) override;

	virtual bool ReadUnitRange(const TreeItem* storageHolder, AbstrUnit* u_size) override;
	virtual void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr) override;
	virtual void DoWriteTree (const TreeItem* storageHolder) override;

	virtual OutStreamBuff* DoOpenOutStream(const TreeItem* storageHolder, CharPtr, const AbstrDataItem* adi);
	virtual InpStreamBuff* DoOpenInpStream(const TreeItem* storageHolder, CharPtr, AbstrDataItem* adi);

// new
	virtual void GetNoDataVal(long & val);
	virtual void SetNoDataVal(long val);

private:

	// Implementations of InpStreamBuf & OutStreamBuf interfaces
	friend class E00StorageOutStreamBuff; 
	friend class E00StorageInpStreamBuff; 

	// hidden implementation which doesn't know about DMS structure
	E00Imp * pImp;	

	// Helper functions
	bool RetrieveHeaderInfoOnly();		

	DECL_RTTI(STGDLL_CALL,StorageClass)
};


#endif // __STG_E00_STORAGEMANAGER_H
