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
// Project:     Storage
//
// Module/Unit: GeolibStorageManager.cpp
//
// Copyright (C)1998 YUSE GSO Object Vision B.V.
// Author:      M. Hilferink
// Created on:  15/09/98 14:43:03
// *****************************************************************************

#include "stg/StorageClass.h"
#include "gst/GeolibStorageManager.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "ser/BaseStreamBuff.h"
#include "ser/VectorStream.h"
#include "utl/mySPrintF.h"
#include "utl/Environment.h"
#include "dbg/debug.h"


#include "TicInterface.h"
#include "UnitClass.h"
#include "TreeItemClass.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataItemClass.h"

// for LoadLibrary & GetProcAddress
#include <Windows.h>

#include "GeolibInterface.h"

namespace Geolib {

//PFUNC_TableExists          pfuncTableExists = 0;
PFUNC_TableOpen              pfuncTableOpen = 0;
PFUNC_TableClose             pfuncTableClose = 0;
PFUNC_TableClose             pfuncTableHandleRemove = 0;
PFUNC_TableRowCount          pfuncTableRowCount = 0;
PFUNC_TableRowAppend         pfuncTableRowAppend = 0;
PFUNC_TableFloatSetByIndex   pfuncTableFloatSetByIndex = 0;
PFUNC_TableIntegerSetByIndex pfuncTableIntegerSetByIndex = 0;
PFUNC_TableFloatColumnGet    pfuncTableFloatColumnGet = 0;
PFUNC_TableIntegerColumnGet  pfuncTableIntegerColumnGet = 0;
PFUNC_TableFloatColumnSet    pfuncTableFloatColumnSet = 0;
PFUNC_TableIntegerColumnSet  pfuncTableIntegerColumnSet = 0;
PFUNC_TableColumnInfoGet     pfuncTableColumnInfoGet = 0;
PFUNC_TableColumnCount       pfuncTableColumnCount = 0;
PFUNC_TableCreateByStruct    pfuncTableCreateByStruct = 0;
PFUNC_TableStringGetByIndex  pfuncTableStringGetByIndex = 0;
PFUNC_TableStringSetByIndex  pfuncTableStringSetByIndex = 0;

Int32 s_InstanceCount = 0;
void* s_hGgl32Dll     =0;

void Init()
{
	if (!s_InstanceCount)
	{
		s_hGgl32Dll = LoadLibrary("ggl32.dll");
		if (!s_hGgl32Dll)
			throwStorageError(ASM_E_FILENOTFOUND, "cannot load ggl32.dll");

		pfuncTableOpen             =(PFUNC_TableOpen)             GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableOpen");
//		pfuncTableExists           =(PFUNC_TableExists)           GetProcAddress(s_hGgl32Dll, "GL_TableExists");
		pfuncTableClose            =(PFUNC_TableClose)            GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableClose");
		pfuncTableHandleRemove     =(PFUNC_TableClose)            GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableHandleRemove");
		pfuncTableRowCount         =(PFUNC_TableRowCount)         GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableRowCount");
		pfuncTableRowAppend        =(PFUNC_TableRowAppend)        GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableRowAppend");
		pfuncTableFloatSetByIndex  =(PFUNC_TableFloatSetByIndex)  GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableFloatSetByIndex");
		pfuncTableIntegerSetByIndex=(PFUNC_TableIntegerSetByIndex)GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableIntegerSetByIndex");
		pfuncTableFloatColumnGet   =(PFUNC_TableFloatColumnGet)   GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableFloatColumnGet");
		pfuncTableIntegerColumnGet =(PFUNC_TableIntegerColumnGet) GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableIntegerColumnGet");
		pfuncTableFloatColumnSet   =(PFUNC_TableFloatColumnSet)   GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableFloatColumnSet");
		pfuncTableIntegerColumnSet =(PFUNC_TableIntegerColumnSet) GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableIntegerColumnSet");
		pfuncTableColumnInfoGet    =(PFUNC_TableColumnInfoGet)    GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableColumnInfoGet");
		pfuncTableColumnCount      =(PFUNC_TableColumnCount)      GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableColumnCount");
		pfuncTableCreateByStruct   =(PFUNC_TableCreateByStruct)   GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableCreateByStruct");
		pfuncTableStringGetByIndex =(PFUNC_TableStringGetByIndex) GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableStringGetByIndex");
		pfuncTableStringSetByIndex =(PFUNC_TableStringSetByIndex) GetProcAddress((HINSTANCE)s_hGgl32Dll, "GL_TableStringSetByIndex");

	}
	s_InstanceCount++;
}

void Exit()
{
	dms_assert(s_InstanceCount > 0);
	if (--s_InstanceCount == 0)
	{
		FreeLibrary((HINSTANCE)s_hGgl32Dll);
		s_hGgl32Dll = 0;
	}
}

struct Lock
{
	Lock(): m_DidInit(false)  { Init(); m_DidInit = true; }
	~Lock() { if (m_DidInit) Exit(); }
private:
	Bool m_DidInit;
};

}

/*
 *	GeolibStorageOutStreamBuff
 *
 *	Private class for the GeolibStorageManager class.
 */

class GeolibStorageOutStreamBuff : public OutStreamBuff  
{
public:
	/*
	 * construction can only be done with a valid pointer to a 
	 * GeolibStorageManager and with a path.
	 */
	GeolibStorageOutStreamBuff(GeolibStorageManager* gsm, CharPtr name, ValueClassID valueTypeID)
		: m_CurrPos(0), m_GSM(gsm), m_Name(name), m_ValueTypeID(valueTypeID)
	{
		dms_assert(gsm);
		DBG_START("GeolibStorageOutStreamBuff", "Constructor", true);
		DBG_TRACE(("manager = %s", gsm->GetName()));
		DBG_TRACE(("subname = %s", name));

		m_ColIndex = gsm->GetColumnInfoAndIndex(name, m_ColInfo);
		if (m_ColIndex == -1)
			throwStorageError(ASM_E_FILENOTFOUND, name);
		m_ElemSize = (m_ValueTypeID == VT_String)
			? (m_ColInfo.Length + 4)
			: ValueClass::FindByValueClassID(m_ValueTypeID)->GetSize();
	}

	virtual void WriteBytes(const Byte* data, UInt32 size)
	{
		Int32 th = m_GSM->GetTableHandle();
		if (m_CurrPos == 0)
		{
			dms_assert(size >= 4);
			UInt32 nrRows = *(const UInt32*)data;
			size -= 4;
			data += 4;
			m_CurrPos += 4;
			dms_assert(nrRows == Geolib::pfuncTableRowCount(th));
		}
		if (size)
		{
			UInt32 rowNr = (m_CurrPos -4) / m_ElemSize;
			UInt32 nrRows = size / m_ElemSize;
			switch(m_ColInfo.ColumnType)
			{
				case GL_TYPE_FLOAT:
				{ 
					switch (m_ValueTypeID)
					{
						case VT_Float64:
						{
							Int32 result = Geolib::pfuncTableFloatColumnSet(th, m_Name.c_str(), rowNr, nrRows, const_cast<Float64*>(reinterpret_cast<const Float64*>(data)), 0);
							CheckWrite(result, nrRows);
							break;
						}
						case VT_Float32:
						{
							const Float32* floatData = reinterpret_cast<const Float32*>( data );
							std::vector<double> buffer(nrRows);
							std::copy(floatData, floatData+nrRows, buffer.begin());
							Int32 result = Geolib::pfuncTableFloatColumnSet(th, m_Name.c_str(), rowNr, nrRows, &*buffer.begin(), 0);
							CheckWrite(result, nrRows);
							break;
						}
					}
					break;
				}
				case GL_TYPE_UINT32:
//				case GL_TYPE_INTEGER:
					Int32 result = Geolib::pfuncTableIntegerColumnSet(th, m_Name.c_str(), rowNr, nrRows, (Int32*)data, 0);
					CheckWrite(result, nrRows);
					break;
			}
			m_CurrPos += nrRows * m_ElemSize;
			size -= nrRows * m_ElemSize;
			dms_assert(size == 0);
		}
	}
	~GeolibStorageOutStreamBuff()
	{
		DBG_START("GeolibStorageOutStreamBuff", "Destructor", true);
		m_GSM->FlushData();
	}

	virtual UInt32 CurrPos() const
	{
		return m_CurrPos;
	}

private:
	void CheckWrite(UInt32 result, UInt32 nrRows)
	{
		if (result != nrRows)
			ColumnError(mySPrintF("only %d rows where written",  result).c_str());
	}
	[[noreturn]] void ColumnError(CharPtr problem)
	{
		throwItemError("Error while writing column %s to table %s: %s",
			m_Name.c_str(), m_GSM->GetName(), problem);
	}

	UInt32                m_CurrPos;
	std::string            m_Name;
	GeolibStorageManager* m_GSM;
	UInt32                m_ColIndex;
	GlColumninfoStruct    m_ColInfo;
	ValueClassID          m_ValueTypeID;
	UInt32                 m_ElemSize;
};

/*
 *	GeolibStorageInpStreamBuff
 *
 *	Private class for the CompoundStorageManager class. 
 */

class GeolibStorageInpStreamBuff : public InpStreamBuff  
{
public:
	/*
	 * construction can only be done with a valid pointer to a 
	 * CompoundStorageManager and with a path.
	 */
	GeolibStorageInpStreamBuff(GeolibStorageManager* gsm, CharPtr name, ValueClassID valueTypeID)
		: m_CurrPos(0), m_GSM(gsm), m_Name(name), m_ValueTypeID(valueTypeID)
	{
		dms_assert(gsm);
		DBG_START("GeolibStorageInpStreamBuff", "Constructor", true);
		DBG_TRACE(("manager = %s", gsm->GetName()));
		DBG_TRACE(("subname = %s", name));

		m_ColIndex = gsm->GetColumnInfoAndIndex(name, m_ColInfo);
		if (m_ColIndex == -1)
			throwStorageError(ASM_E_FILENOTFOUND, name);

		m_ElemSize = (m_ValueTypeID == VT_String)
			? (m_ColInfo.Length + 4)
			: ValueClass::FindByValueClassID(m_ValueTypeID)->GetSize();
	}

	virtual void ReadBytes(Byte* data, UInt32 size)
	{
		Int32 th = m_GSM->GetTableHandle();
		if (m_CurrPos < 4)
		{
			Int32 nrRows = Geolib::pfuncTableRowCount(th);
			while (m_CurrPos < 4 && size > 0)
			{
				*data++ = *((const Byte*)&nrRows + m_CurrPos++);
				--size;
			}
		}
		if (size > 0)
		{
			UInt32 rowNr = (m_CurrPos-4) / m_ElemSize;
			UInt32 nrRows = size / m_ElemSize;
			switch(m_ColInfo.ColumnType)
			{
				case GL_TYPE_FLOAT:
				{ 
					switch (m_ValueTypeID)
					{
						case VT_Float64:
						{
							Int32 result = Geolib::pfuncTableFloatColumnGet(th, m_Name.c_str(), rowNr, nrRows, reinterpret_cast<Float64*>(data), 0);
							CheckRead(result, nrRows);
							break;
						}
						case VT_Float32:
						{
							std::vector<double> buffer(nrRows);
							Int32 result = Geolib::pfuncTableFloatColumnGet(th, m_Name.c_str(), rowNr, nrRows, &*buffer.begin(), 0);
							CheckRead(result, nrRows);
							std::copy(buffer.begin(), buffer.end(), (Float32*)data);
							break;
						}
						default:
							ColumnError(mySPrintF("Conversion from GL_TYPE_FLOAT to DMS type %s  is not implemented", 
								ValueClass::FindByValueClassID(m_ValueTypeID)->GetName()).c_str());
					}
					m_CurrPos += nrRows * m_ElemSize;
					size -= nrRows * m_ElemSize;
					break;
				}
//				case GL_TYPE_UINT32:
				case GL_TYPE_INTEGER:
					if (m_ValueTypeID != VT_UInt32 && m_ValueTypeID != VT_Int32)
						throwDmsError(MG_NIL, "GeolibStorageManager: reading column %s of type GL_TYPE_INTEGER: conversion to DMS type %s  is not implemented", 
							m_Name.c_str(), 
							ValueClass::FindByValueClassID(m_ValueTypeID)->GetName());
					Geolib::pfuncTableIntegerColumnGet(th, m_Name.c_str(), rowNr, nrRows, (Int32*)data, 0);
					m_CurrPos += nrRows * m_ElemSize;
					size -= nrRows * m_ElemSize;
					break;
				case GL_TYPE_STRING:
				{
					UInt32 rowOffset= (m_CurrPos-4) - rowNr * m_ElemSize;
					if (rowOffset < 4)
					{
						dms_assert(size == 4 && rowOffset == 0);
						*((UInt32*)data) = m_ElemSize - 4;
						m_CurrPos += 4;
						size -= 4;
					}
					else
					{
						dms_assert(rowOffset == 4 && size == m_ElemSize -4);
						Geolib::pfuncTableStringGetByIndex(th, rowNr, m_ColIndex, data);
						m_CurrPos += (m_ElemSize -4);
						size -= (m_ElemSize -4);
					}
					break;
				}
				default:
					ColumnError(mySPrintF("Geolib type %d is not supported", m_ColInfo.ColumnType).c_str());
			}
			dms_assert(size == 0);
		}
	}

	virtual UInt32 CurrPos() const
	{
		return m_CurrPos;
	}

	~GeolibStorageInpStreamBuff()
	{
		DBG_START("GeolibStorageInpStreamBuff", "Destructor", true);
		m_GSM->FlushData();
	}
private:
	void CheckRead(UInt32 result, UInt32 nrRows)
	{
		if (result != nrRows)
			ColumnError(mySPrintF("only %d rows where read",  result).c_str());
	}
	[[noreturn]] void ColumnError(CharPtr problem)
	{
		throwDmsError(MG_NIL, "Error while reading column %s from table %s: %s",
			m_Name.c_str(), m_GSM->GetName(), problem);
	}

	UInt32                m_CurrPos;
	std::string            m_Name;
	GeolibStorageManager* m_GSM;
	UInt32                m_ColIndex;
	GlColumninfoStruct    m_ColInfo;
	ValueClassID          m_ValueTypeID;
	UInt32                m_ElemSize;
};

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:GeolibStorageManager
// Description:       constructor
// Parameters:        
// Returns:           
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:45:07, M. Hilferink, Created
// *****************************************************************************

const int NO_TABLEHANDLE = -1;
GeolibStorageManager::GeolibStorageManager()
{
	DBG_START("GeolibStorageManager", "Create", true);
	m_TableHandle = NO_TABLEHANDLE;
	m_bDidInit = false;
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:~GeolibStorageManager
// Description:       destructor
// Parameters:        
// Returns:           
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:45:47, M. Hilferink, Created
// *****************************************************************************

GeolibStorageManager::~GeolibStorageManager()
{
	DBG_START("GeolibStorageManager", "Destroy", true);
	DBG_TRACE(("storageName =  %s", GetName()));
	CloseStorage();
	if (m_bDidInit)
	{
		Geolib::Exit();
		m_bDidInit = false;
	}
}

void GeolibStorageManager::FlushData()
{
	if (m_TableHandle != NO_TABLEHANDLE)
	{
		dms_assert(Geolib::s_hGgl32Dll != 0);
		Geolib::pfuncTableClose(m_TableHandle);
	}
	m_TableHandle = NO_TABLEHANDLE;
}

bool GeolibStorageManager::ReduceResources()
{
	if (Geolib::s_hGgl32Dll != 0)
	{
		FlushData();
		if (m_bDidInit)
		{
			Geolib::Exit();
			m_bDidInit = false;
		}
	}
	return true;
}

Int32 GeolibStorageManager::GetTableHandle()
{
	if (!m_bDidInit) 
	{
		Geolib::Init();
		m_bDidInit = true;
	}
	if (m_TableHandle == NO_TABLEHANDLE)
	{
		m_TableHandle = Geolib::pfuncTableOpen(GetName(), 0);
	}
	return m_TableHandle;
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:OpenStorage
// Description:       opens a geolib table
// Parameters:        
//     CharPtr p_pTblname: name of the table
// Returns:           
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:54:02, M. Hilferink, Created
// *****************************************************************************

void GeolibStorageManager::DoOpenStorage(bool intentionToWrite)
{
	dms_assert(!IsOpen());
	dms_assert(DoesExist(0));
	dms_assert(! ( m_IsReadOnly && intentionToWrite) );

	DBG_START("GeolibStorageManager", "OpenStorage", true);
	DBG_TRACE(("storageName =  %s", GetName()));

	GetTableHandle();
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:CloseStorage
// Description:       
// Parameters:        
// Returns:           
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:54:21, M. Hilferink, Created
// *****************************************************************************

void GeolibStorageManager::DoCloseStorage(bool mustCommit)
{
	dms_assert(IsOpen());

	DBG_START("GeolibStorageManager", "CloseStorage", true);
	DBG_TRACE(("storageName=  %s", GetName()));

	FlushData();
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:OpenForWrite
// Description:       opens a column in the opened table for writing
// Parameters:        
//     CharPtr p_pColname: name of the column (attribute)
// Returns:           CFile*: pointer to a memory blok where to write data to
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:56:10, M. Hilferink, Created
// *****************************************************************************
OutStreamBuff* GeolibStorageManager::DoOpenOutStream(const TreeItem* storageHolder, CharPtr colName, const AbstrDataItem* adi)
{
	report(ST_MajorTrace, "Write gtf(%s,%s)\n", GetName(), colName);

	MG_CHECK( ! m_IsReadOnly );
	MG_CHECK(DoesExist(0));
	if (!adi || adi->HasVoidDomainGuarantee())
		return 0;
	return new GeolibStorageOutStreamBuff(this, colName, adi->GetDataObj()->GetValuesType()->GetValueClassID());
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:DoOpenInpStream
// Description:       Opens a column in the table for reading
// Parameters:        
//     CharPtr p_pColname: name of the column (attribute)
// Returns:           CFile*: memory block with read data
// *****************************************************************************
// History:                                                                    
// 15/09/98 14:57:53, M. Hilferink, Created
// *****************************************************************************

InpStreamBuff* GeolibStorageManager::DoOpenInpStream(const TreeItem* storageHolder, CharPtr colName, AbstrDataItem* adi)
{
	report(ST_MajorTrace, "Read  gtf(%s,%s)\n", GetName(), colName);

	dms_assert(IsOpen());
	dms_assert(adi);

	GlColumninfoStruct colInfo;
	if (GetColumnInfoAndIndex(colName, colInfo) == -1)
		return 0;

	const AbstrDataObject* adObj = adi->GetDataObj();
	if (!adObj || adi->HasVoidDomainGuarantee())
		return 0;
	return new GeolibStorageInpStreamBuff(this, colName, adObj->GetValuesType()->GetValueClassID());
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:ReadMetaInfo
// Description:       reads meta info from file
// Parameters:        
// Returns:           TreeItem*: pointer to root of build tree
// *****************************************************************************
// History:                                                                    
// 29/10/98 10:53:50, M. Hilferink, Created
// *****************************************************************************

bool GeolibCompatibleTypes(const ValueClass* diClass, const ValueClass* glClass)
{
	return OverlappingTypes(diClass, glClass) 
		|| ( diClass == ValueWrap<Float64>::GetStaticClass() && glClass == ValueWrap<Float32>::GetStaticClass());
}


void GeolibStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr)
{
	if (!storageHolder || storageHolder != curr)
		return;

	DBG_START("GeolibStorageManager::DoUpdateTree", storageHolder->GetName(), false);

	SyncMode syncMode = GetSyncMode(storageHolder);

	const AbstrUnit* domain = StorageHolder_GetTableDomain(storageHolder);
	if (!domain)
		storageHolder->ThrowFail("GTF storage expected a domain");

	GlColumninfoStruct colInfo;
	Int32 colcnt = Geolib::pfuncTableColumnCount(GetTableHandle());
	for (Int32 i =0; i<colcnt; i++)
	{
		Geolib::pfuncTableColumnInfoGet(GetTableHandle(), i, NULL, &colInfo);
		const ValueClass* glClass = 0;
		switch (colInfo.ColumnType) 
		{
			case GL_TYPE_INTEGER:
				glClass = ValueWrap<Int32>::GetStaticClass(); break;
			case GL_TYPE_FLOAT:
				glClass = ValueWrap<Float32>::GetStaticClass(); break;
//				case GL_TYPE_UINT32: made equal to GL_TYPE_INTEGER
//					glClass = ValueWrap<UInt32>::GetStaticClass(); break;
			case GL_TYPE_BYTE:
				glClass = ValueWrap<UInt8>::GetStaticClass(); break;
			case GL_TYPE_CHAR:
				glClass = ValueWrap<Int8>::GetStaticClass(); break;
			case GL_TYPE_STRING:
				glClass = ValueWrap<String>::GetStaticClass(); break;
		}
		MG_CHECK(glClass != 0);
		TreeItem* item = curr->GetSubTreeItemByID(GetTokenID(colInfo.Name));
		if (!item)
		{
			if (syncMode != SM_None)
			{
				const AbstrUnit* valuesUnit = UnitClass::Find(glClass)->CreateDefault();
				item = CreateDataItem(curr, GetTokenID(colInfo.Name), domain, valuesUnit);
			}
		}
		else
		{
			if (!item->IsKindOf(AbstrDataItem::GetStaticClass()))
				item->Fail("GTF storageManager expected this item to be an DataItem");

			AbstrDataItem* dataItem = debug_cast<AbstrDataItem*>(item);
			const ValueClass* diClass = dataItem->GetDataObj()->GetValuesType();
			if (!GeolibCompatibleTypes(diClass, glClass))
				item->Fail(
					mySPrintF("GTF: inconsistent value types; table: %s, column: %s, configured type: %s, database type: %s", 
						storageHolder->GetFullName().c_str(), colInfo.Name,
						diClass->GetName(),
						glClass->GetName()
					).c_str()
				);					
		}
	}
	FlushData();
}

bool GeolibStorageManager::ReadUnitRange(const TreeItem* storageHolder, AbstrUnit* u)
{
	if (u != StorageHolder_GetTableDomain(storageHolder))
		return false;
	UInt32 count = Geolib::pfuncTableRowCount(GetTableHandle());
	FlushData();
	u->SetCount(count);
	return true;
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:WriteMetaInfo
// Description:       writes meta info to file
// Parameters:        
//     const TreeItem* storageHolder: item to subtree to write meta info from
// Returns:           
// *****************************************************************************
// History:                                                                    
// 02/11/98           M. Hilferink
// 29/10/98 10:56:15, M. Hilferink, Created
// *****************************************************************************

const AbstrUnit* GetEntityAndSubAttributes(
		std::vector<const AbstrDataItem*>& attributes, 
		const TreeItem* storageHolder, CharPtr storageName)
{	
	const AbstrUnit* e = 0;
	for (const TreeItem* subItem = storageHolder->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
	{
		if ( IsDataItem(subItem) )
		{
			AbstrDataItem* subAttr = (AbstrDataItem*)subItem;
			if (!subAttr->HasVoidDomainGuarantee())
			{
				if (!e)
					e = DMS_DataItem_GetDomainUnit(subAttr);
				if (e != DMS_DataItem_GetDomainUnit(subAttr))
					storageHolder->ThrowFail(
						mySPrintF(
							"Cannot write to gtf(%s) since attributes have different entities", 
							storageName
						).c_str()
					);
				attributes.push_back(subAttr);
			}
		}
	}

	if (attributes.size()==0)
	{
		storageHolder->ThrowFail(
			mySPrintF(
				"Cannot write to gtf(%s) since item has no attribute subitems", 
				storageName
			).c_str()
		);
	}

	dms_assert(e);
	return e;
}

void GeolibStorageManager::DoCreateStorage(const TreeItem* storageHolder)
{
	MG_CHECK(!m_IsReadOnly);
	MG_CHECK(!IsOpen());
	dms_assert(storageHolder);
	dms_assert(GetName());

	report(ST_MajorTrace, "Create new gtf(%s) as storage for %s\n", GetName(), storageHolder->GetFullName().c_str());

	std::vector<const AbstrDataItem*> attributes;
	const AbstrUnit* e = GetEntityAndSubAttributes(attributes, storageHolder, GetName());
	UInt32 nrAttrs = attributes.size();
	dms_assert(nrAttrs != 0);
	UInt32 nrRows = e->GetCount();
	if (!nrRows)
		storageHolder->ThrowFail(
			mySPrintF(
				"Cannot write to gtf(%s) since nr_rows == 0", 
				GetName()
			).c_str()
		);
	
	std::vector<GlColumninfoStruct> colinfoVec(nrAttrs);
	GlColumninfoStruct* colinfo = &*colinfoVec.begin();
	memset(colinfo, 0, sizeof(GlColumninfoStruct)*nrAttrs); // DIT MOET MEN ALTIJD DOEN
	for(UInt32 i=0; i!=nrAttrs; ++i)
	{
		const AbstrDataItem* attr = attributes[i];
		CharPtr name = DMS_TreeItem_GetName((TreeItem*)attr);
		strncpy(colinfo[i].Name,       name, GL_NAME_LEN);
		strncpy(colinfo[i].Description,name, GL_MAX_STRING);
//		strcpy(colinfo[i].Description,DMS_TreeItem_GetDescr((TreeItem*)attr));
		colinfo[i].ColumnType = 0;
		ValueClassID vcID = attr->GetDataObj()->GetValuesType()->GetValueClassID();

		colinfo[i].Length = 12; //Int32
		colinfo[i].Decimals= 0; //Int32 
		switch (vcID) {
			case VT_Int32:
				colinfo[i].ColumnType = GL_TYPE_INTEGER; break;
			case VT_Float32:
				colinfo[i].ColumnType = GL_TYPE_FLOAT; 
				colinfo[i].Decimals= 5; //Float32
				break;
			case VT_Float64:
				colinfo[i].ColumnType = GL_TYPE_FLOAT; 
				colinfo[i].Length = 19;
				colinfo[i].Decimals= 9; //Float64
				break;
			case VT_UInt32:
				colinfo[i].ColumnType = GL_TYPE_UINT32;
				break;
			case VT_UInt8:
				colinfo[i].ColumnType = GL_TYPE_BYTE; 
				colinfo[i].Length = 3; // 255
				break;
			case VT_Int8:
				colinfo[i].ColumnType = GL_TYPE_CHAR; 
				colinfo[i].Length = 4; // -128
				break;
			default:
				throwDmsError(MG_NIL, "Cannot create Geolib storage becase DataItem %s has a values unit of type %s",
					name, ValueClass::FindByValueClassID(vcID)->GetName());
				colinfo[i].ColumnType = GL_TYPE_UNKNOWN; break;
		}

		//Int32 Offset;
		//Int32 AsString;
		//Int32 UseNodata;
		//double Nodata;
	}
	Geolib::Lock geolibLock;
	UInt32 ret = Geolib::pfuncTableCreateByStruct(GetName(), colinfo, nrAttrs, nrRows, 0, 0);

	DoOpenStorage( true );
	UInt32 rows = Geolib::pfuncTableRowCount(GetTableHandle());
	if (rows < nrRows)
	{
		Geolib::pfuncTableRowAppend(m_TableHandle, nrRows - rows);
		rows = Geolib::pfuncTableRowCount(m_TableHandle);
	}
	FlushData();
	dms_assert(rows == nrRows);
}

void GeolibStorageManager::DoWriteTree(const TreeItem* storageHolder)
{
//	throwIllegalAbstract(MG_POS, "GeolibStorageManager","WriteTree");
}

// *****************************************************************************
// class/module:      GeolibStorageManager
// Function/Procedure:GetColumnInfoAndIndex
// Description:       retrieves the index and column info by searching on name
// Parameters:        
//     CharPtr p_pColname: the column name
//     GlColumninfoStruct& p_pColInfo: the column info struct
//     Int32& p_pIndex: the column index
// Returns:           :
// *****************************************************************************
// History:                                                                    
// 16/09/98 15:05:19, M. Hilferink, Created
// 21/09/98 10:56:12, M. Hilferink, changed signature to retrieve also column 
//						info struct
// *****************************************************************************

Int32 GeolibStorageManager::GetColumnInfoAndIndex(CharPtr p_pColname, GlColumninfoStruct& p_pColInfo)
{
	Int32 colcnt = Geolib::pfuncTableColumnCount(GetTableHandle());
	for (Int32 i =0; i<colcnt; i++)
	{
		Geolib::pfuncTableColumnInfoGet(m_TableHandle, i, NULL, &p_pColInfo);
		if (! stricmp(p_pColInfo.Name, p_pColname))
			return i;
	}
	return -1;
}

IMPL_DYNC_STORAGECLASS(GeolibStorageManager, "gtf")
