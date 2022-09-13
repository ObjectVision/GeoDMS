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
// Project:     Storage
//
// Module/Unit: GeolibStorageManager.h
//
// Copyright (C)1998 YUSE GSO Object Vision B.V.
// Author:      M. Hilferink
// Created on:  15/09/98 14:35:04
// *****************************************************************************

#ifndef _GeolibStorageManager_H_
#define _GeolibStorageManager_H_

#include "DllMain.h"
#include "stg/AsmUtil.h"

class GeolibStorageManager : public AbstrStorageManager
{

public:
	GeolibStorageManager();
	virtual	~GeolibStorageManager();

//	implement AbstrStorageManager interface
	virtual void  DoCreateStorage(const TreeItem* storageHolder);
	virtual void DoOpenStorage  (bool intentionToWrite);
	virtual void DoCloseStorage (bool mustCommit);
	virtual bool  ReduceResources();

	virtual void DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr);
	virtual void DoWriteTree (const TreeItem* storageHolder);
	virtual bool ReadUnitRange(const TreeItem* storageHolder, AbstrUnit* u);

	virtual OutStreamBuff* DoOpenOutStream(const TreeItem* storageHolder, CharPtr, const AbstrDataItem* adi);
	virtual InpStreamBuff* DoOpenInpStream(const TreeItem* storageHolder, CharPtr, AbstrDataItem* adi);

private:
	Int32 GetTableHandle();
	Int32 GetColumnInfoAndIndex(const char*, struct GlColumninfoStruct&);
	void  FlushData();
	
	Int32 m_TableHandle;
	bool  m_bDidInit;

	friend class GeolibStorageOutStreamBuff; // can only be accessed by this class
	friend class GeolibStorageInpStreamBuff; // can only be accessed by this class

	DECL_RTTI(STGDLL_CALL ,StorageClass)
};


#endif // _GeolibStorageManager_H_
