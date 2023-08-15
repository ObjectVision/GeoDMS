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

// *****************************************************************************
// Implementations of
//	- ODBCStorageReader
//	- ODBCStorageManager
// *****************************************************************************

#include "ODBCStorageManager.h"

#include "PropDefInterface.h"

#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "geo/GeoSequence.h"
#include "geo/StringBounds.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/PropDef.h"
#include "ser/BaseStreamBuff.h" 
#include "ser/SequenceArrayStream.h"
#include "ser/vectorstream.h"
#include "stg/StorageClass.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/splitpath.h"
#include "xct/DmsException.h"

#include "LockLevels.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "SessionData.h"
#include "TicPropDefConst.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include "dllimp/RunDllProc.h" 
#include "time.h" 

/*****************************************************************************/
//										DEFINES
/*****************************************************************************/

#define	DMSQUERY_NAME                     "DMSQUERY"

/*****************************************************************************/
//								GENERAL
/*****************************************************************************/

FileDateTime TIMESTAMP_STRUCT2FileDateTime(const TIMESTAMP_STRUCT ts)
{
	struct tm	tm_val;
	tm_val.tm_year		=	ts.year;
	tm_val.tm_mon		=	ts.month;
	tm_val.tm_mday		=	ts.day;
	tm_val.tm_hour		=	ts.hour;
	tm_val.tm_min		=	ts.minute;
	tm_val.tm_sec		=	ts.second;
	return AsFileDateTime(mktime(&tm_val), ts.fraction);
}

const ValueClass* CType2ValueClass(SQLSMALLINT ctype)
{
	switch (ctype)
	{
		case SQL_C_BINARY   :
		case SQL_C_CHAR     : return ValueWrap<SharedStr >::GetStaticClass(); 

		case SQL_C_DOUBLE   : return ValueWrap<Float64>::GetStaticClass();
		case SQL_C_FLOAT    : return ValueWrap<Float32>::GetStaticClass();

		case SQL_C_SLONG    : return ValueWrap< Int32 >::GetStaticClass(); 
		case SQL_C_ULONG    : return ValueWrap<UInt32 >::GetStaticClass();

		case SQL_C_SSHORT   : return ValueWrap< Int16 >::GetStaticClass(); 
		case SQL_C_USHORT   : return ValueWrap<UInt16 >::GetStaticClass();

		case SQL_C_STINYINT : return ValueWrap<  Int8 >::GetStaticClass();
		case SQL_C_UTINYINT : return ValueWrap< UInt8 >::GetStaticClass();

		case SQL_C_BIT      : return ValueWrap<  Bool >::GetStaticClass();

		default             : return NULL;
	}
}

SQLSMALLINT	ValueClassID2CType(ValueClassID vid)
{
	switch (vid)
	{
		case ValueClassID::VT_SharedStr:	return SQL_C_CHAR;
		case ValueClassID::VT_UInt32: return SQL_C_ULONG;
		case ValueClassID::VT_Int32:  return SQL_C_SLONG;
		case ValueClassID::VT_UInt16: return SQL_C_USHORT;
		case ValueClassID::VT_Int16:  return SQL_C_SSHORT;
		case ValueClassID::VT_UInt8:  return SQL_C_UTINYINT;
		case ValueClassID::VT_Int8:   return SQL_C_STINYINT;
		case ValueClassID::VT_Bool:   return SQL_C_BIT;
		case ValueClassID::VT_Float64:return SQL_C_DOUBLE;
		case ValueClassID::VT_Float32:return SQL_C_FLOAT;

	}
	throwErrorF("ODBC", "ValueType %s has no equivalent SQL_C type",
		ValueClass::FindByValueClassID(vid)->GetName());
}


/*****************************************************************************/
//							DATABASE OBJECT PROPERTIES
/*****************************************************************************/

long TreeItemColumnCount(TreeItem *ti)
{
	long count = 0;

	for (TreeItem* subItem = ti->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		if (TreeItemIsColumn(subItem))
			count++;
	return count;
}

bool UnitIsTable(AbstrUnit* domainUnit)
{	
	if (domainUnit->IsDisabledStorage())
		return false;
	if (TreeItemColumnCount(domainUnit) == 0)
		return true;

	for (TreeItem* subItem = domainUnit->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
	{
		if (TreeItemIsColumn(subItem))
		{
			const AbstrUnit* thisUnit = debug_cast<AbstrDataItem*>(subItem)->GetAbstrDomainUnit();
			if (domainUnit != thisUnit)
				return false;
		}
	}
	return	true;
}

bool TreeItemIsTable(TreeItem *ti, const AbstrUnit*& domainUnit)
{	
	if (ti->IsDisabledStorage())
		return false;
	domainUnit = AsDynamicUnit(ti);
	if (TreeItemColumnCount(ti) == 0)
		return	domainUnit != NULL;

	for (TreeItem* subItem = ti->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
	{
		if (TreeItemIsColumn(subItem))
		{
			const AbstrUnit* thisUnit = debug_cast<AbstrDataItem*>(subItem)->GetAbstrDomainUnit();
			if (domainUnit == NULL)
				domainUnit	= thisUnit;
			else if (domainUnit != thisUnit)
				return false;
		}
	}
	dms_assert(domainUnit); // TreeItemColumnCount guarantees that for some subItems TreeItemIsColumn(subItem) is true
	return	true;
}

bool TreeItemIsDatabaseTable(TreeItem *ti, const AbstrUnit*& domainUnit)
{
	if (TreeItemHasPropertyValue(ti, sqlStringPropDefPtr))
		return false;
	return TreeItemIsTable(ti, domainUnit);
}

bool TreeItemIsQuery(TreeItem *ti, const AbstrUnit*&	domainUnit)
{
	return
		TreeItemHasPropertyValue(ti, sqlStringPropDefPtr) 
	&&	TreeItemIsTable(ti, domainUnit);
}

SharedStr GetSqlString(const TreeItem* item) // can be tableHolder or attr
{
	return TreeItemPropertyValue(item, sqlStringPropDefPtr);
}

SharedStr GetOrCreateSqlString(const TreeItem* tableHolder)
{
	SharedStr result = GetSqlString(tableHolder);
	if (result.empty())
		result = mySSPrintF("SELECT * FROM %s", tableHolder->GetName());
	return result;
}

/*****************************************************************************/
//		OdbcTableContextHandle to provide context in error messages
/*****************************************************************************/

struct OdbcTableContextHandle : public ContextHandle
{
	OdbcTableContextHandle(TreeItem* tableItem) 
		: m_TableItem(tableItem) 
	
	{};

	void GenerateDescription() override
	{
		SharedStr descr("with TableItem [");
		descr += m_TableItem->GetFullName().c_str();
		descr += ']';
		const AbstrUnit* domainUnit = 0;
		if (TreeItemIsQuery(m_TableItem, domainUnit))
		{
			descr += "\n" SQLSTRING_NAME " = ";
			descr += GetOrCreateSqlString(m_TableItem).c_str();
		}
		SetText(descr);
	}

	TreeItem* m_TableItem;
};


/*****************************************************************************/
//							DATABASE CONDFIGURATION CREATE/UPDATE FUNCTIONS
/*****************************************************************************/
leveled_critical_section s_OdbcSection(item_level_type(0), ord_level_type::Storage, "Odbc");

void CreateAllColumnInfo(
	const ODBCStorageManager* self, 
	const TreeItem* storageHolder,
	TreeItem* tiRecordSet, 
	const AbstrUnit* domainUnit,
	CharPtr tabletypename
)
{
	try {

		OdbcTableContextHandle och(tiRecordSet);

		TRecordSet* recordSet = self->GetRecordSet(storageHolder, tiRecordSet, GetOrCreateSqlString(tiRecordSet));
		dms_assert(recordSet);

		recordSet->UnbindAllInternal();
		recordSet->SetMaxRows(1); // limit work

		TRecordSetOpenLock rsLock(recordSet);

		for (auto columnIter= recordSet->Columns().begin(), columnEnd = recordSet->Columns().end(); columnIter != columnEnd; ++columnIter)
			CreateTreeItemColumnInfo(
				tiRecordSet, 
				columnIter->Name(),
				domainUnit,
				CType2ValueClass(columnIter->CType())
			);
	}
	catch (...)
	{
		auto err = catchException(true);
		tiRecordSet->DoFail(err, FR_MetaInfo);
	}
}

void CreateQueryColumnInfo(const ODBCStorageManager* self, const TreeItem* storageHolder, TreeItem* tiQuery)
{
	if (tiQuery->GetTSF(TSF_DisabledStorage|TSF_IsTemplate) )
		return;

	const AbstrUnit* domainUnit = nullptr;
	if ( TreeItemIsQuery(tiQuery, domainUnit) )
		CreateAllColumnInfo(self, storageHolder, tiQuery, domainUnit, DMSQUERY_NAME);
}

void CreateDatabaseTableColumnInfo(const ODBCStorageManager* self, const TreeItem* storageHolder, CharPtr tableName, CharPtr tableTypeName)
{
	TreeItem* tableHolder = const_cast<TreeItem*>(storageHolder)->GetSubTreeItemByID(GetTokenID_mt(tableName));

	const AbstrUnit *domainUnit;

	if (tableHolder)
	{
		if (!TreeItemIsDatabaseTable(tableHolder, domainUnit))
			return;
	}
	else
	{
		AbstrUnit* newTable = Unit<UInt32>::GetStaticClass()->CreateUnit(const_cast<TreeItem*>(storageHolder), GetTokenID_mt(tableName));
		tableHolder = newTable;
		domainUnit = newTable;
	}
	dms_assert(tableHolder);
	CreateAllColumnInfo(self, storageHolder, tableHolder, domainUnit, tableTypeName);
}


void CreateDatabaseTableInfo(const ODBCStorageManager* self, const TreeItem* storageHolder, SyncMode syncMode)
{
	dms_assert(syncMode != SM_None);

	TDatabase* database = self->OpenDatabaseInstance(storageHolder);
	if (! database)
		return;

	TRecordSet tableInfo(database);

	tableInfo.CreateTableInfo();

	TRecordSetOpenLock tableInfoLock(&tableInfo); // Does Rewind
	tableInfo.Next(); // combined with Rewind this constitutes a MoveFirst

	while	(! tableInfo.EndOfFile())
	{
		if (SharedStr(tableInfo.Columns()[3].AsString()) != "SYSTEM TABLE")
		{
			CharPtr tableName = tableInfo.Columns()[2].AsString();
			if (syncMode == SM_AllTables || const_cast<TreeItem*>(storageHolder)->GetSubTreeItemByID(GetTokenID_mt(tableName)))
				CreateDatabaseTableColumnInfo(self, storageHolder, tableName, tableInfo.Columns()[3].AsString());
		}
		tableInfo.Next();
	}
}

// *****************************************************************************

struct OdbcMetaInfo : StorageMetaInfo
{
	OdbcMetaInfo(const ODBCStorageManager* sm, const TreeItem* storageHolder, const TreeItem* curr)
		: StorageMetaInfo(storageHolder, curr)
		, m_TableHolder(IsDataItem(curr) ? curr->GetTreeParent() : curr)
	{
		if (m_TableHolder)
			m_SqlString = GetOrCreateSqlString(m_TableHolder);
	}

	SharedPtr<const TreeItem> m_TableHolder;
	SharedStr m_SqlString;
};

/*****************************************************************************/
//								ODBCStorageReader
/*****************************************************************************/


class ODBCStorageReader
{
public:
	ODBCStorageReader(ODBCStorageManager* odbcstoragemanager, const OdbcMetaInfo* smi, TreeItem* tableHolder, CharPtr columnName, AbstrDataItem* colItem)
		:	m_ODBCStorageManager(odbcstoragemanager)
		,	m_OdbcInfo(smi)
		,	m_TableHolder(tableHolder)
		,	m_Name(columnName)
		,	m_ColItem(colItem)
		,	m_RecordCount(-1)
		,	m_ColIndex(0)
		,	m_RecordSet(0)
		,	m_InternalValueClass(0)
		,	m_Column(0)
	{
		DBG_START("ODBCStorageReader", "Constructor", false);
		dms_assert(odbcstoragemanager);
		DBG_TRACE(("manager = %s", odbcstoragemanager->GetNameStr().c_str()));
		DBG_TRACE(("table   = %s", tableHolder->GetName().c_str()));
		DBG_TRACE(("column  = %s", columnName));

		SizeT fileNamePos = getFileName(m_Name.begin(), m_Name.send()) - m_Name.begin();
		if (fileNamePos)
			m_Name.GetAsMutableCharArray()->erase(SizeT(0), fileNamePos);
	}

	~ODBCStorageReader()
	{ 
		if (m_RecordSet)
		{
			m_RecordSet->UnLockThis();
			if (m_ColIndex)
				m_RecordSet->BindExternal(m_ColIndex, 0, 0);
			m_RecordSet->UnbindAllInternal();
		}
	}

	TRecordSet* GetRecordSet()
	{
		if (!m_RecordSet)
		{
			m_RecordSet = m_ODBCStorageManager->GetRecordSet(m_OdbcInfo->StorageHolder(), m_TableHolder, m_OdbcInfo->m_SqlString);
			dms_assert(m_RecordSet);
			dms_assert(!m_RecordSet->IsLocked());
			m_RecordSet->SetMaxRows(0);
		}
		dms_assert(m_RecordSet);
		return m_RecordSet;
	}

	TRecordSet* GetOpenRecordSet()
	{
		GetRecordSet()->OpenLocked();
		return m_RecordSet;
	}

	const ValueClass* GetInternalValueClass()
	{
		if (!m_InternalValueClass)
			m_InternalValueClass = m_ColItem->GetDataObj()->GetValuesType();
		dms_assert(m_InternalValueClass);
		return m_InternalValueClass;
	}

	UInt32 GetColIndex()
	{
		if (m_ColIndex == 0)
		{
			if ( !GetOpenRecordSet()->Columns().FindByName(m_Name.c_str(), &m_ColIndex) )
				throwErrorF("ODBC", "Cannot Find Column with Name = '%s' in table '%s'", m_Name.c_str(), m_ColItem->GetParent()->GetFullName().c_str());

			const ValueClass* dc = CType2ValueClass(m_RecordSet->Columns()[m_ColIndex-1].CType() );
			const ValueClass* ic = GetInternalValueClass();
			if (!CompatibleTypes(ic, dc))
				throwErrorF(
					"ODBC","inconsistent ValueTypes; table: %s, column: %s has configured type '%s', but odbc type '%s'", 
						m_ColItem->GetParent()->GetFullName().c_str(), m_Name.c_str(),
						ic->GetName().c_str(),
						dc->GetName().c_str()
					);
		}
		dms_assert(m_ColIndex != 0);
		return m_ColIndex;		
	}

	TColumn* GetColumn()
	{
		if (m_Column == nullptr)
			m_Column = &(GetOpenRecordSet()->Columns()[GetColIndex()-1]);
		dms_assert(m_Column != nullptr);
		return m_Column;
	}

	UInt32 GetRecordCount()
	{
		if (m_RecordCount == -1)
			m_RecordCount = GetRecordSet()->RecordCount();
		dms_assert(m_RecordCount != -1);
		return m_RecordCount;
	}

	SQLLEN GetActualSizeEstimate()
	{
		SQLLEN c = 0;
		for (const SQLLEN *b = GetColumn()->ActualSizes(), *e = b + GetRecordCount(); b!=e; ++b)
		{
			SQLLEN s = *b;
			if (s != SQL_NULL_DATA) 
				c += s;
		}
		return c;
	}
	UInt32 ReadNrRecs()
	{
		OdbcTableContextHandle odbcTCG(m_TableHolder);
		dms_assert(!IsDefined(UInt32(SQL_NULL_DATA))); // we assume SQL_NULL_DATA to be equal to the dms null data value

		// ============== Get and check valid recordset, do not yet open.
		TRecordSet* recordSet = GetRecordSet(); // does not open the recordset
		dms_assert(recordSet);

		if (!recordSet->UnbindAllInternal())   // only possible when recordset is not yet opened
			m_ODBCStorageManager->throwItemError("UnbindAllInternal Failed");

		// ============== SetFrameSize: applied once only on closed rs
		UInt32 recordCount = GetRecordCount(); // not yet opened?
		recordSet->SetFrameSize(recordCount);  // only possible when rs is not yet opened

		return recordCount;
	}
	void BindData(Byte* data, UInt32 size)
	{
		OdbcTableContextHandle odbcTCG(m_TableHolder);
		MG_CHECK(GetColumn()->CType() != SQL_C_CHAR);

		TRecordSet* recordSet = GetRecordSet();
		recordSet->Rewind(); // does open recordset and locks it

		const ValueClass* vc = GetInternalValueClass();
		GetColumn()->Redefine(
			ValueClassID2CType(vc->GetValueClassID()), 
			vc->GetSize());

		dms_assert(size == GetRecordCount() * GetColumn()->ElementSize());
		if ( !recordSet->BindExternal(GetColIndex(), data, SizeT(m_RecordCount) * vc->GetSize()))
			m_ODBCStorageManager->throwItemError("BindExternal Failed");

	}
	void ReadData()
	{
		OdbcTableContextHandle odbcTCG(m_TableHolder);
		// get the stuff
		GetOpenRecordSet()->Next(GetRecordCount());
		dms_assert(!m_RecordSet->EndOfFile());
	}

	void ConvertNullData(Byte* data, UInt32 size)
	{
		// ============== SetFrameSize: applied once only on closed rs, then open in case of chars
		OdbcTableContextHandle odbcTCG(m_TableHolder);
		dms_assert(GetColumn()->CType() != SQL_C_CHAR);
		UInt32       elemSize          = GetInternalValueClass()->GetSize();
		const Byte*  undefinedValuePtr = GetInternalValueClass()->GetUndefinedValuePtr();
		UInt32       recordCount       = GetRecordCount(); // not yet opened?

		dms_assert(size == recordCount * elemSize);
		dms_assert((size % elemSize) == 0);

		// set elems to UNDEFINED_VALUE if they are NULL according to ODBC
		Byte *dataEnd = data + size, *dataBase = data;
		UInt32 currRec = 0, lastRec = currRec + size / elemSize;
		while (currRec < lastRec)
		{
			if (GetColumn()->IsNull(currRec))
				memcpy(dataBase + SizeT(currRec) * elemSize, undefinedValuePtr, elemSize);
			++currRec;
		}
	}

	void ReadStrings(sequence_traits<SharedStr>::seq_t data)
	{
		OdbcTableContextHandle odbcTCG(m_TableHolder);

		// ============== Get and check valid recordset, do not yet open.
		TRecordSet* recordSet = GetRecordSet(); // does not open the recordset
		dms_assert(recordSet);

		// ============== SetFrameSize: applied once only on closed rs, then open in case of chars

		UInt32 recordCount = GetRecordCount(); // not yet opened?
		UInt32 recordsRead = 0;
		const UInt32 recordsPerFrame  = 1;
		if (!recordSet->UnbindAllInternal())   // only possible when recordset is not yet opened
			m_ODBCStorageManager->throwItemError("UnbindAllInternal Failed");
		recordSet->SetFrameSize(recordsPerFrame);  // only possible when rs is not yet opened

		TRecordSetOpenLock rsOpenLock(recordSet);
		TColumn* column = GetColumn();
		dms_assert(column->IsVarSized());

		m_CharBuffer.resize(column->BufferSize()); // NYI: lees in begrensde blokken; NYI: read directly into data.get_sa()
		recordSet->BindExternal(GetColIndex(), &*m_CharBuffer.begin(), m_CharBuffer.size());

		// get the stuff already in order to get size estimate
		for (; recordsRead < recordCount; recordsRead += recordsPerFrame)
		{
			recordSet->Next(recordsPerFrame);
			
			dms_assert(!recordSet->EndOfFile()); 

			// ============== provide total size estimate from the actual size array
			// certainly opens recordset

			dms_assert(data.size() == recordCount);
			if (recordsPerFrame == recordCount)
				data.get_sa().data_reserve(GetActualSizeEstimate() MG_DEBUG_ALLOCATOR_SRC("ODBC"));

			sequence_array<char>::iterator stringPtr = data.begin() + recordsRead;

			UInt32  buffElemSize = column->ElementSize();
			CharPtr 
				buffPtr = CharPtr(column->Buffer()),
				buffEnd = buffPtr + buffElemSize * recordsPerFrame;

			const SQLLEN* actualSizePtr = GetColumn()->ActualSizes();
			dms_assert(actualSizePtr);
			while (buffPtr != buffEnd)
			{
				SQLLEN actualSize = *actualSizePtr++;

				if (actualSize == SQL_NULL_DATA)
					(*stringPtr).assign( Undefined() );
				else
				{
					if (ThrowingConvert<unsigned_type<SQLLEN>::type>(actualSize) > buffElemSize)
						m_ODBCStorageManager->throwItemErrorF("ReadStrings cannot read %d chars for row %d with a buffersize of only %d bytes", actualSize, recordsRead, buffElemSize);
					(*stringPtr).assign( buffPtr, buffPtr+actualSize);
				}
				++stringPtr;
				buffPtr += buffElemSize;
			}
		}
	}
private:
	TRecordSet*          m_RecordSet;
	TColumn*             m_Column;
	UInt32               m_RecordCount;
	UInt32               m_ColIndex;    // 1-based
	SharedStr            m_Name;
	AbstrDataItem*       m_ColItem;
	const ValueClass*    m_InternalValueClass;
	ODBCStorageManager*  m_ODBCStorageManager;
	const OdbcMetaInfo*  m_OdbcInfo;
	TreeItem*            m_TableHolder; // Maybe we read all columns at once
	std::vector<BYTE>    m_CharBuffer;
};

/*****************************************************************************/
//								ODBCStorageManager
/*****************************************************************************/

StorageMetaInfoPtr ODBCStorageManager::GetMetaInfo(const TreeItem* storageHolder, TreeItem* adi, StorageAction sa) const
{
	return std::make_unique<OdbcMetaInfo>(this, storageHolder, adi);
}

ODBCStorageManager::ODBCStorageManager()
{
	m_TiDatabase              = NULL;
	m_HasAccessSysObjectsCopy = false;
}

void GetDatabaseLocationArguments(const ODBCStorageManager* osm, TDatabase& database)
{
	dms_assert(osm); // PRECONDITION

	SharedStr datasourceName ( osm->GetNameStr().c_str());

	database.SetDatasourceName( datasourceName.c_str() );
}

TDatabase* ODBCStorageManager::DatabaseInstance(const TreeItem* storageHolder) const
{
	dms_assert(storageHolder);
	if (m_Database.is_null())
	{
		m_Database.assign( new TDatabase(SessionData::Curr()->GetConfigLoadDir().c_str() ) );
		m_TiDatabase= NULL;
	}
	if(m_TiDatabase != storageHolder)
	{
		m_TiDatabase = nullptr;
		GetDatabaseLocationArguments(this, *m_Database);
		m_TiDatabase = storageHolder;
	}
	return m_Database;
}

TDatabase* ODBCStorageManager::OpenDatabaseInstance(const TreeItem* storageHolder) const
{
	TDatabase* db= DatabaseInstance(storageHolder);
	if (! db )
		return nullptr;

	db->Open();
	dms_assert(db->Active());

	return db;
}

TRecordSet* ODBCStorageManager::GetRecordSet(const TreeItem* storageHolder, TreeItem* tableHolder, SharedStr sqlString) const
{
	TRecordSetRef& rsRef = m_RecordSets[tableHolder];
	if (rsRef.is_null())
	{
		rsRef = new TRecordSet(DatabaseInstance(storageHolder));
		rsRef->CreateRecordSet(sqlString.c_str());
	}
	return rsRef;
}

bool ODBCStorageManager::DoCheckExistence(const TreeItem* storageHolder) const
{
	TDatabase* db = DatabaseInstance(storageHolder);
	if (! db )
		return false;
	if (db->GetDriverType() == dtMsAccess)
		return base_type::DoCheckExistence(storageHolder);
	db->Open(); // open to test existence
	return true;
}

bool ODBCStorageManager::DoCheckWritability() const
{
	return false;
}

template <typename T>
void ReadData(ODBCStorageReader& ir, IterRange<T*> data)
{
	UInt32 nrRecs = ir.ReadNrRecs();
	dms_assert(data.size() == nrRecs);
	if (!nrRecs) return;

	SizeT byteSize = nrRecs * sizeof(T);
	ir.BindData(reinterpret_cast<Byte*>(data.begin()), byteSize);
	ir.ReadData();
	ir.ConvertNullData(reinterpret_cast<Byte*>(data.begin()), byteSize);
}

void ReadData(ODBCStorageReader& ir, sequence_traits<Bool>::seq_t data)
{
	std::vector<BYTE> tmp(data.size());
	ReadData(ir, IterRange<BYTE*>(&tmp));
	std::copy(tmp.begin(), tmp.end(), data.begin());
}

void ReadData(ODBCStorageReader& isb, sequence_traits<SharedStr>::seq_t data)
{
	isb.ReadStrings(data);
}


typedef SharedStr String;

bool ODBCStorageManager::ReadDataItem(StorageMetaInfoPtr smi, AbstrDataObject* borrowedReadResultHolder, tile_id t)
{
	dms_assert(t == 0);

	AbstrDataItem* adi = smi->CurrWD();
	dms_assert(adi->GetDataObjLockCount() < 0); // DataWriteLock is already set

	TreeItem* tableHolder = const_cast<TreeItem*>(adi->GetTreeParent());

	leveled_critical_section::scoped_lock lock(s_OdbcSection);
	ODBCStorageReader ir(this, debug_cast<const OdbcMetaInfo*>(smi.get()), tableHolder, adi->GetName().c_str(), adi);

	adi->GetAbstrDomainUnit()->ValidateCount(ir.GetRecordCount());

	ValueClassID  vc = borrowedReadResultHolder->GetValuesType()->GetValueClassID();

	switch (vc)
	{
#define INSTANTIATE(T) case ValueClassID::VT_##T: ::ReadData(ir, mutable_array_cast<T>(borrowedReadResultHolder)->GetDataWrite().get_view()); break;
		INSTANTIATE_NUM_ORG
		INSTANTIATE_OTHER
#undef INSTANTIATE

		default	:
			adi->throwItemErrorF("OdbcStorageManager::ReadDataItem not implemented for odbc data with ValuesUnitType: %s"
			,	borrowedReadResultHolder->GetValuesType()->GetName()
			);
	} // switch
	return true;
}

bool ODBCStorageManager::ReadUnitRange(const StorageMetaInfo& smi) const
{
	leveled_critical_section::scoped_lock lock(s_OdbcSection);
	UInt32 count = const_cast<ODBCStorageManager*>(this)->GetRecordSet(smi.StorageHolder(), smi.CurrWU(), debug_cast<const OdbcMetaInfo*>(&smi)->m_SqlString)->RecordCount();
	smi.CurrWU()->SetCount(count);
	return true;
}

void ODBCStorageManager::DoUpdateTree(const TreeItem* storageHolder, TreeItem* curr, SyncMode sm) const
{
	AbstrStorageManager::DoUpdateTree(storageHolder, curr, sm);

	dms_assert(sm != SM_None);

	dms_assert(storageHolder);

	leveled_critical_section::scoped_lock lock(s_OdbcSection);
	CreateQueryColumnInfo(this, storageHolder, curr);

	if (storageHolder == curr)
		CreateDatabaseTableInfo(this, storageHolder, sm);
}

SharedStr ODBCStorageManager::GetDatabaseFilename(const TreeItem* storageHolder)
{
	leveled_critical_section::scoped_lock lock(s_OdbcSection);
	TDatabase* db = DatabaseInstance(storageHolder);

	if (!db || db->GetDriverType() != dtMsAccess)
		return SharedStr();

	db->Open(); // required to get DataBase().Name() 
	return db->Name() + ".mdb";
}

/*
TIMESTAMP_STRUCT ODBCStorageManager::AccessTableLastUpdate(const TreeItem* storageHolder, const TreeItem* tableHolder)
{
	DBG_START("ODBCStorageManager", "AccessTableLastUpdate", true);

	TIMESTAMP_STRUCT				ts;
	memset(&ts, 0, sizeof(TIMESTAMP_STRUCT));

// ODBCTIME, TODO: ODBCStorageManager::AccessTableLastUpdate
//	updateinfo.SQL	=	"SELECT DATEUPDATE FROM MSysObjects WHERE TYPE IN (1, 5, 6) AND NAME = '"; 
//	updateinfo.SQL	+=	DMS_TreeItem_GetName(tiTable); 
//	updateinfo.SQL	+=	"'"; 

//	if (updateinfo.Open())
//		ts	=	* ((TIMESTAMP_STRUCT*) updateinfo.Columns()[0].ElementBuffer());
//	else
//	{
//		updateinfo.Close();
		// 1. create copy of relevant MSYSOBJECTS data (neccesary because MSYSOBJECTS data can't be retrieved via ODBC)
//		if (!m_HasAccessSysObjectsCopy) // prevent making copy too often.
//		{
			// 3. drop MSYSOBJECTS_COPY
//ODBCTIME	updateinfo.SQL	=	"DROP TABLE MSYSOBJECTS_COPY";
//			updateinfo.ExecSQL(false); // dont worry if table does not exist
//			updateinfo.Close();
//
			// VRAAG: wat is het type van een VIEW ? Ligt dit in de geselcteerde range 1-6 ?
			// ANTWOORD: begluur access
			RunAccessSql(GetDatabaseFilename(storageHolder).c_str(), 
							"SELECT NAME, DATEUPDATE INTO MSYSOBJECTS_COPY FROM MSYSOBJECTS WHERE TYPE IN (1, 5, 6)",
							"MYSYSOBJECTS_COPY");
			m_HasAccessSysObjectsCopy = true; 
		}

		// 2. retrieve DATEUPDATE from MSYSOBJECTS_COPY via ODBC for the current tiTable
		// PAS OP DUBBELE Namen!
		TRecordSet updateinfo(OpenDatabaseInstance(storageHolder));
		updateinfo.CreateRecordSet(mySSPrintF("SELECT DATEUPDATE FROM MSYSOBJECTS_COPY WHERE NAME = '%s'", tableHolder->GetName()).c_str());
		
//ODBCTIME	}
	TRecordSetOpenLock uiLock(&updateinfo);
	if (!updateinfo.EndOfFile())
		ts = * ((TIMESTAMP_STRUCT*) updateinfo.Columns()[0].ElementBuffer());

	return ts;
}
*/

/* ODBCTIME
FileDateTime ODBCStorageManager::GetLastChangeDateTime((const TreeItem* storageHolder, CharPtr columnName)
{
	return TimeStamp(0, 0);
	// NYI
	CDebugContextHandle debugContext("ODBCStorageManager::GetLastChangeDataTime", columnName, true);
	ObjectContextHandle och(storageHolder, "StorageHolder");

	const TreeItem *tiTable	= storageHolder->GetItem(GetTokenID_mt(columnName));
	MG_CHECK(tiTable);

	const AbstrUnit *domainUnit;
	if	(	TreeItemIsColumn(const_cast<TreeItem*>(tiTable)) 
		||	!TreeItemIsTable(const_cast<TreeItem*>(tiTable), domainUnit))
		return TimeStamp(0, 0);
	

	TIMESTAMP_STRUCT ts;

	CharPtr                            tablename = tiTable->GetName();
	TTableTimestampCacheType::iterator i         = m_TableTimestampCache.find(tablename);
	
	if (i != m_TableTimestampCache.end())
		ts = i->second;
	else
	{
		if (TreeItemIsQuery(const_cast<TreeItem*>(tiTable), domainUnit))  
			memset(&ts, 0, sizeof(TIMESTAMP_STRUCT));
		else
		{
			DMS_CALL_BEGIN
				ts = AccessTableLastUpdate(storageHolder, tiTable);
			DMS_CALL_END
		}
		m_TableTimestampCache[tablename] = ts;
	}
	
	return TIMESTAMP_STRUCT2FileDateTime(ts);
} // ODBCStorageManager::GetLastChangeDateTime
ODBCTIME */

// Register

IMPL_DYNC_STORAGECLASS(ODBCStorageManager, "odbc")

