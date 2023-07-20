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
#include "stg/StoragePCH.h"
#pragma hdrstop

// *****************************************************************************
//
// Implementations of - E00StorageOutStreamBuff
//                    - E00StorageInpStreamBuff
//                    - E00StorageManager
//
// *****************************************************************************

#include "E00StorageManager.h"
#include "e00/E00Imp.h"

#include "stg/StorageClass.h"
#include "utl/mySPrintF.h"
#include "e00/E00Imp.h"
#include "dbg/debug.h"

#include "TicInterface.h"
#include "AbstrDataItem.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "Param.h"  // 'Hidden DMS-interface functions'

// Implementation of OutStreamBuff interface
class E00StorageOutStreamBuff : public OutStreamBuff  
{

public:

	E00StorageOutStreamBuff(E00StorageManager* e_sm, CharPtr name) 
		: m_CurrPos(0), m_ESM(e_sm), m_Name(name)
	{
		DBG_START("E00StorageOutStreamBuff", "Constructor", true);
		dms_assert(e_sm);
		DBG_TRACE(("manager = %s", e_sm->GetName()));
		DBG_TRACE(("subname = %s", name));
	}

	// Write shorts to grid
	virtual void WriteBytes(const Byte* data, UInt32 size)
	{
		DBG_START("E00StorageOutStreamBuff", "WriteBytes", true);

		// Skip first 4 bytes (= length info)
		if (m_CurrPos == 0)
		{
			dms_assert(size >= 4);
			UInt32 nrRows = *(const UInt32*)data;
			size -= 4;
			data += 4;
			m_CurrPos += 4;

		}

		// write bytes
		if (size)
		{
			m_ESM->pImp->WriteCells((char *)data, size );
			m_CurrPos += size;
		}
	}

	virtual UInt32 CurrPos() const { return m_CurrPos; }

	~E00StorageOutStreamBuff()
	{
		DBG_START("E00StorageOutStreamBuff", "Destructor", true);
	}

private:

	UInt32                m_CurrPos;
	std::string            m_Name;
	E00StorageManager*	  m_ESM;
};



// Implementation of InpStreamBuff interface
class E00StorageInpStreamBuff : public InpStreamBuff  
{
public:

	E00StorageInpStreamBuff(E00StorageManager* e_sm, CharPtr name) 
		: m_CurrPos(0), m_ESM(e_sm), m_Name(name)
	{
		DBG_START("E00StorageInpStreamBuff", "Constructor", true);
		dms_assert(e_sm);
		DBG_TRACE(("manager = %s", e_sm->GetName()));
		DBG_TRACE(("subname = %s", name));

	}

	// Read from grid. 
	// The expected output consists of a long giving the number of bytes to come,
	// and a bunch of shorts. 
	virtual void ReadBytes(Byte* data, UInt32 size)
	{
		DBG_START("E00StorageInpStreamBuff", "ReadBytes", true);
		
		// Put nr of bytes in first 4 bytes of outputstream
		if (m_CurrPos < 4)
		{
			Int32 nrBytes = m_ESM->pImp->NrOfRows() * m_ESM->pImp->NrOfCols();
			while (m_CurrPos < 4 && size > 0)
			{
				*data++ = *((const Byte*)&nrBytes + m_CurrPos++);
				--size;
			}
		}

		// Let implementation object read data
		if (size > 0)
		{
			m_ESM->pImp->ReadCells(data, size);
			m_CurrPos += size;
		}
	}

	virtual UInt32 CurrPos() const { return m_CurrPos; }

	~E00StorageInpStreamBuff()
	{
		DBG_START("E00StorageInpStreamBuff", "Destructor", true);

		m_ESM->CloseStorage(); // MTA: Dat doet de deur weer dicht.
	}


private:
	UInt32                m_CurrPos;
	std::string            m_Name;
	E00StorageManager*	  m_ESM;
};



// Constructor for this implementation of the abstact storagemanager interface
E00StorageManager::E00StorageManager()
{
	DBG_START("E00StorageManager", "Create", true);

	// External implementation
	pImp = new E00Imp;

	// Convenient header values
	pImp->SetNrOfRows(0);
	pImp->SetNrOfCols(0);

}

// Destructor. Close open files, and get rid of implementation object
E00StorageManager::~E00StorageManager()
{
	DBG_START("E00StorageManager", "Destroy", true);
	DBG_TRACE(("storageName =  %s", GetName()));
	CloseStorage();

	// Get rid of external implementation
    delete pImp;
	pImp = 0;

}

// Open for read
void E00StorageManager::DoOpenStorage(bool intentionToWrite)
{
	DBG_START("E00StorageManager", "OpenStorage", true);

	dms_assert(!IsOpen());

	DBG_TRACE(("storageName =  %s", GetName()));

	dms_assert(DoesExist(0));

	// Retrieve header info
	Bool result = pImp->OpenForRead(GetName());
	MG_CHECK2(result, "Cannot open E00 for read");
}


// Close any open file and forget about it
void E00StorageManager::DoCloseStorage(bool mustCommit)
{
	DBG_START("E00StorageManager", "CloseStorage", true);

	dms_assert(IsOpen());

	DBG_TRACE(("storageName=  %s", GetName()));

	pImp->Close();
}


// Open for writing
void E00StorageManager::DoCreateStorage(const TreeItem* storageHolder)
{
	DBG_START("E00StorageManager", "DoCreateStorage", true);
	dms_assert(!IsOpen());

	DBG_TRACE(("storageName =  %s", GetName()));


	// the real open is triggered by OpenOutStream 
	// (the implementation object writes the header when a file is created)


	// MTA: Now use range values of DomainUnit of subitem("griddata") for 
	// setting dimensions
	static TokenID t_GridData = GetTokenID("GridData");
	const AbstrDataItem* gridData
		=	debug_cast<const AbstrDataItem*>(
				storageHolder->GetConstSubTreeItemByID(t_GridData)
			->	CheckObjCls(
					DMS_DataItemClass_Find
					(
						DMS_UInt8Unit_GetStaticClass()
					,	VC_Single
					)
				)
			);
	checked_domain<SPoint>(gridData, "GridData");
	MG_CHECK2
	(
		gridData, 
		mySPrintF
		(
			"E00StorageManager %s for %s has no GridData sub item of the correct type ", 
			GetName(), 
			storageHolder->GetFullName().c_str()
		).c_str()
	);

	// Set width and height
	const AbstrUnit* domainUnit = DMS_DataItem_GetDomainUnit(gridData);
	pImp->SetNrOfRows(domainUnit->GetDimSize(0));
	pImp->SetNrOfCols(domainUnit->GetDimSize(1));
}


// Open a stream for writing
// The parameter is obligatory in the abstract storagemanager interface
// but is in this case not applicable
OutStreamBuff* E00StorageManager::DoOpenOutStream(const TreeItem* storageHolder, CharPtr colName, const AbstrDataItem* adi)
{
	DBG_START("E00StorageManager", "DoOpenOutStream", true);

	report(ST_MajorTrace, "Write asc(%s,%s)\n", GetName(), colName);

	MG_CHECK( ! m_IsReadOnly );

	pImp->OpenForWrite(GetName());	// temp, write header
	
	return new E00StorageOutStreamBuff(this, colName);
}


// Open a stream for reading
// The parameter is obligatory in the abstract storagemanager interface
// but is in this case not applicable
InpStreamBuff* E00StorageManager::DoOpenInpStream(const TreeItem* storageHolder, CharPtr colName, AbstrDataItem* adi)
{
	DBG_START("E00StorageManager", "OpenInStream", true);

	report(ST_MajorTrace, "Read E00 (%s,%s)\n", GetName(), colName);

	dms_assert(IsOpen());

	// Pass a stream object
	return new E00StorageInpStreamBuff(this, colName);
}


// Non obligatory in abstract storagemanager
// Gives a value signifying no data available
void E00StorageManager::GetNoDataVal(long & val)
{
//	val = pImp->NoDataVal();
	val = -1234567;			// interface should be bool
}


// Non obligatory in abstract storagemanager
// Sets which value signifies no data available
void E00StorageManager::SetNoDataVal(long val)
{
	// not supported by implementation
	// pImp->SetNoDataVal(val);	
}


bool E00StorageManager::ReadUnitRange(const TreeItem* storageHolder, AbstrUnit* u_size)
{
	RetrieveHeaderInfoOnly();
	u_size->SetRangeAsIPoint(0, 0, pImp->NrOfRows(), pImp->NrOfRows());
	return true;
}

void E00StorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr)
{
	DBG_START("E00StorageManager::DoUpdateTree", storageHolder->GetName(), false);

	if (storageHolder && storageHolder == curr)
	{
		AbstrUnit* u_content = DMS_CreateUnit(curr, "GridUnit", DMS_UnitClass_Find(ValueWrap< UInt8>::GetStaticClass()));
		AbstrUnit* u_size    = DMS_CreateUnit(curr, "GridSize", DMS_UnitClass_Find(ValueWrap<SPoint>::GetStaticClass()));
		CreateDataItem(curr, GetTokenID("GridData"), u_size, u_content);

		// parameters from fileheader
		AbstrDataItem* par_Precision = CreateParam<Int32>  (curr, "Precision");
		AbstrDataItem* par_PixelX    = CreateParam<Float32>(curr, "PixelX"  );
		AbstrDataItem* par_PixelY    = CreateParam<Float32>(curr, "PixelY"  );
		AbstrDataItem* par_Ulx       = CreateParam<Float32>(curr, "Ulx"     );
		AbstrDataItem* par_Lry       = CreateParam<Float32>(curr, "Lry"     );
		AbstrDataItem* par_Lrx       = CreateParam<Float32>(curr, "Lrx"     );
		AbstrDataItem* par_Uly       = CreateParam<Float32>(curr, "Uly"     );

	    // Retrieve real gridsize
		RetrieveHeaderInfoOnly();

		SetTheValue<Int32  >(par_Precision, pImp->Precision);   
		SetTheValue<Float32>(par_PixelX, pImp->PixelX);   
		SetTheValue<Float32>(par_PixelY, pImp->PixelY);   
		SetTheValue<Float32>(par_Ulx, pImp->Ulx);   
		SetTheValue<Float32>(par_Lry, pImp->Lry);   
		SetTheValue<Float32>(par_Lrx, pImp->Lrx);   
		SetTheValue<Float32>(par_Uly, pImp->Uly);   
	}
}



// Retrieves header info by inspecting the file locally
bool E00StorageManager::RetrieveHeaderInfoOnly()
{
	DBG_START("E00StorageManager", "RetrieveHeaderInfoOnly", true);
	
	// Action only necessary if the file isn't open yet
	if (!IsOpen())
	{
		E00Imp local_imp;
	   if (true == local_imp.OpenForRead(GetName()))
		{
			long h = local_imp.NrOfRows();
			long w = local_imp.NrOfCols();
			pImp->SetNrOfRows(h);
			pImp->SetNrOfCols(w);

			pImp->PixelX = local_imp.PixelX;			
			pImp->PixelY = local_imp.PixelY;			
			pImp->Ulx = local_imp.Ulx;			
			pImp->Lry = local_imp.Lry;			
			pImp->Lrx = local_imp.Lrx;			
			pImp->Uly = local_imp.Uly;			
		}
		local_imp.Close();
	}
	return true;
}

void E00StorageManager::DoWriteTree(const TreeItem* storageHolder)
{
	DBG_START("E00StorageManager", "DoWriteTree", true);

	// flat file format
	throwIllegalAbstract(MG_POS, this, "DoWriteTree");
}

IMPL_DYNC_STORAGECLASS(E00StorageManager, "e00");
