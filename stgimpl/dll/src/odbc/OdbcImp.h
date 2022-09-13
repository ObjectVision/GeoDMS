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

#if !defined(__STGIMPL_ODBC_IMP_H)
#define __STGIMPL_ODBC_IMP_H

#include "ImplMain.h"

#include "geo/BaseBounds.h"

#include <vector>
#include <set>
#include <map>
#include <windows.h>
#include <sqlext.h>

#include "geo/seqvector.h"
#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedStr.h"
#include "set/QuickContainers.h"

// somehow windows.h enabled warning 4200
#if defined(_MSC_VER)
#	pragma warning( disable : 4200) // nonstandard extension used : zero-sized array in struct/union
#endif

/*****************************************************************************/
//										DEFINES && TYPEDEFS
/*****************************************************************************/

#define	NOT_DEFINED			(-1)
#define	SQL_NOT_DEFINED	(-12345)
#define	MAXBUFLEN			256
#define	MAXBUFLEN_EXT		1024

/*****************************************************************************/
//										FUNCTIONS
/*****************************************************************************/

void  Check(const SQLRETURN ret, const SQLSMALLINT handletype, const SQLHANDLE handle, class TDatabase* db, class TRecordSet* rs, class TColumn* col);
char* DateToString(DATE_STRUCT *date, char* s);
char* TimeToString(TIME_STRUCT *time, char* s);
char* TimeStampToString(TIMESTAMP_STRUCT *timestamp, char* s);

SQLSMALLINT	SQLToCType(const SQLINTEGER sqltype, const bool sign);
CharPtr CTypeToString(const SQLSMALLINT ctype);
CharPtr AttributeValueToString(const SQLINTEGER attribute, const SQLINTEGER attributevalue);

/*****************************************************************************/
//										CLASSES
/*****************************************************************************/

enum   TExecType   { rseRecordSet, rseExecSQL, rseTableInfo, rseColumnInfo };
enum   TDriverType { dtUnknown, dtMsAccess, dtMySql };

class  TRecordSet;

class  TColumn;
class  TColumns;
struct TEnvironment;
class  TDatabase;

struct TExternalBuffer;

typedef std::vector<TExternalBuffer>	TExternalBuffers;


class TColumn
{
	friend	class	TColumns;
public:
	STGIMPL_CALL TColumn();
	STGIMPL_CALL ~TColumn();

	// structural
	bool              operator < (TColumn& c) { return Index() < c.Index(); }
//	TRecordSet*       RecordSet()             { return m_RecordSet; }
	const TRecordSet* RecordSet() const       { return m_RecordSet; }

	// status
	STGIMPL_CALL bool Define(SQLUSMALLINT number, TRecordSet *rs);
	             bool Defined()      const         { return m_Buffer != nullptr; }
	STGIMPL_CALL bool Redefine(SQLSMALLINT cType, SQLINTEGER elementSize);

	// meta
	
	SQLUSMALLINT      Index()     const { return m_Index; }
	CharPtr           Name()      const { return m_Name.c_str(); }
	SQLSMALLINT       CType()     const { return m_CType;	}
	CharPtr           CTypeName() const { return CTypeToString(CType()); }
	STGIMPL_CALL  bool IsVarSized() const;

	// data
	STGIMPL_CALL const SQLPOINTER  Buffer()       const;
	STGIMPL_CALL SQLINTEGER        BufferSize()   const;
	STGIMPL_CALL const SQLPOINTER  ElementBuffer()const;
	STGIMPL_CALL SQLINTEGER        ElementSize()  const;

	STGIMPL_CALL const SQLLEN*     ActualSizes()  const { return m_ActualSizes; }
	STGIMPL_CALL SQLLEN            ActualSize()   const;
	STGIMPL_CALL bool              IsNull()       const;

	STGIMPL_CALL SQLLEN            ActualSize(UInt32 frameElementNr)   const { dms_assert(m_ActualSizes); return m_ActualSizes[frameElementNr]; }
	STGIMPL_CALL bool              IsNull(UInt32 frameElementNr)       const { return ActualSize(frameElementNr) == SQL_NULL_DATA; }

	STGIMPL_CALL CharPtr           AsString()     const;

	SharedStr DiagnosticString() const;

private:
	TRecordSet*               m_RecordSet;
	SQLUSMALLINT              m_Index;
	SQLSMALLINT               m_CType;
	SQLINTEGER                m_ElementSize;
	SharedStr                 m_Name;

	// data
	SQLPOINTER                 m_Buffer;
	SQLLEN*                    m_ActualSizes;
	mutable std::vector<char>  m_AsString; 

	bool m_IsInternalBuffer;

	void Undefine();
	void FreeBuffers();
	bool BindNone();
	bool BindInternal();
	bool BindExternal(SQLPOINTER buf);
	bool Bind();
	void Check(const SQLRETURN ret);	
};


typedef std::vector<TColumn> TColumnVector;

class	TColumns	: public TColumnVector
{
	friend class	TRecordSet;
	
public:
	TColumns()											{ m_Defined = false; m_BuffersBound = false; }

	// status
	             bool Defined()	{ return m_Defined; }
	STGIMPL_CALL bool FindByName(CharPtr colName, UInt32* colIndex);
	STGIMPL_CALL void  UnBind();

private:
	bool m_Defined, m_BuffersBound;

	bool Define(TRecordSet *rs);
	void Undefine();
	bool BindExternal(SQLUSMALLINT colindex, SQLPOINTER buf, SQLLEN buflen, TRecordSet *rs);
};

struct TEnvironment : SharedBase
{
	TEnvironment();
	~TEnvironment();
	
	void Release() const { if (!DecRef()) delete this; }
	static TEnvironment	*GetDefaultEnvironment();
	
	SQLHENV	EnvironmentHandle()        { return m_HENV; };
	bool    EnvironmentHandleDefined() { return m_HENV != SQL_NULL_HENV; }	

private:
	void EnvironmentHandleDefine();
	void EnvironmentHandleUndefine();
	void Check(const SQLRETURN ret);

	SQLHENV	m_HENV;
};

typedef SQLSMALLINT TSqlDataType;
typedef SQLUINTEGER TCursorType;

class	TDatabase
{
	friend class TRecordSet;
	typedef std::set<TRecordSet*> TRecordSetCollection;

public:
	STGIMPL_CALL TDatabase(CharPtr configLoadDir, TEnvironment *env = NULL);
	STGIMPL_CALL ~TDatabase();
	STGIMPL_CALL void Init();

	
	// structural
	TEnvironment&         Environment()            { return *m_Environment; };
	SQLHDBC               Handle()                 { return m_HDBC; };
	bool                  HandleDefined()          { return m_HDBC != SQL_NULL_HDBC; }	

	// these settings affect the reference to the data source
	STGIMPL_CALL void SetDatasourceName(CharPtr name);
	STGIMPL_CALL void SetUserName      (CharPtr name);
	STGIMPL_CALL void SetPassword      (CharPtr name);

	// these are called from RecordSet funcs
	void AddOpenRecordSet(TRecordSet* rs) { dms_assert(Active()); m_OpenRecordSets.insert(rs); }
	void DelOpenRecordSet(TRecordSet* rs) { dms_assert(Active()); m_OpenRecordSets.erase (rs); }

	// operational
	STGIMPL_CALL void Open();
	STGIMPL_CALL void Close();
	STGIMPL_CALL TDriverType   GetDriverType(); // calls GetConnectionStr that determines DriverType
	bool                    Active()                                 { return m_Active; }

	// info
	CharPtr                 DatasourceName() const                   { return m_DatasourceName.c_str(); }
	CharPtr                 UserName()       const                   { return m_UserName.c_str(); }
	CharPtr                 Password()       const                   { return m_Password.c_str(); }

	SQLUSMALLINT MaxConcurrentActivities()                { return GetInfoUSInteger(SQL_MAX_CONCURRENT_ACTIVITIES); }
	SQLUSMALLINT MaxAsyncConcurrentStatements()           { return GetInfoUSInteger(SQL_MAX_ASYNC_CONCURRENT_STATEMENTS); }
 	SharedStr    DriverName()                             { return GetInfoCharacter(SQL_DRIVER_NAME); }
 	SharedStr    DriverOBDCVersion()                      { return GetInfoCharacter(SQL_DRIVER_ODBC_VER); }
	SharedStr    Name()                                   { return GetInfoCharacter(SQL_DATABASE_NAME); }	

	bool         DriverODBCVersionAtLeast(const double v) { return ((double) DriverOBDCVersionX()) >= v; }
	double       DriverOBDCVersionX()                     { char *s; return strtod(DriverOBDCVersion().c_str(), &s); } 

	SharedStr               DiagnosticString() const;

private:
	CharPtr                 GetConnectionStr();

	void                    HandleDefine();
	void                    HandleUndefine();

	STGIMPL_CALL SQLUSMALLINT GetInfoUSInteger(SQLUSMALLINT infotype);
	STGIMPL_CALL SQLUINTEGER  GetInfoUInteger(SQLUSMALLINT infotype);
	STGIMPL_CALL SharedStr    GetInfoCharacter(SQLUSMALLINT infotype);

	void                    SetMonade(TRecordSet *recordset, TExecType rse);
	void                    ResetMonade(const TRecordSet *recordset);

	void                    Check(const SQLRETURN ret);

	void                    CloseAllRecordSets();

	SharedPtr<TEnvironment> m_Environment;
	SharedStr               m_ConfigLoadDir,
	                        m_DatasourceName,
	                        m_UserName,
	                        m_Password;
	SharedStr               m_ConnectionString;
	TRecordSetCollection    m_OpenRecordSets;
	SQLHDBC                 m_HDBC;
	ULONG                   m_ConnectionTimeOut;
	bool                    m_Active;
	TDriverType             m_DriverType;

	static TRecordSet*      s_MonadicRecordSet;
};

struct	TExternalBuffer
{
	SQLUSMALLINT        m_ColumnIndex;
	SQLPOINTER          m_Buffer;
	UINT                m_BufferLength;

	TExternalBuffer()   { Init(0, NULL, 0); }

	bool Defined()      const { return m_ColumnIndex != 0; }
	bool DefinedBind()  const { return Defined() && m_Buffer != 0; }
	bool DefinedUnbind()const { return Defined() && m_Buffer == 0; }

	void Init(SQLUSMALLINT colindex, SQLPOINTER buf, UINT buflen)
	{ 
		m_ColumnIndex  = colindex;
		m_Buffer       = buf;
		m_BufferLength = buflen;
	}
};

class TRecordSet final : public SharedBase 
{
	friend TColumn;
	friend TColumns;
	friend TDatabase;

	typedef PersistentSharedObj base_type;

public:

	STGIMPL_CALL TRecordSet(TDatabase *db);
	STGIMPL_CALL ~TRecordSet();

	void Init();  // afer this, the column info, cursor, bindings, and abstr recordset are destroyed
	void Release() const { if (!DecRef()) delete this;	} // TRecordSet is a final class ?!


	// structural
	TDatabase&       Database()            { return *m_Database; }
	TDatabase*       DatabasePtr()         { return m_Database; }
	const TDatabase* DatabasePtr() const   { return m_Database; }
	SQLHSTMT         Handle()              { return m_HSTMT; }
	bool             HandleDefined() const { return m_HSTMT != SQL_NULL_HSTMT; }
	TColumns&        Columns()             { return m_Columns; }

	// set only before Navigating (or cause a Init()); it affects the contents of the abstr recordset
	STGIMPL_CALL void CreateRecordSet(CharPtr sql);
	STGIMPL_CALL void CreateExecSQL  (CharPtr sql);
	STGIMPL_CALL void CreateTableInfo();
	STGIMPL_CALL void CreateColumnInfo(CharPtr tableName);

	// these setting cause a close when open, the cursor and columns are affected, but not the abstract recordset contents
	STGIMPL_CALL void SetFrameSize(UInt32 size = 1);
	STGIMPL_CALL void SetMaxRows(UInt32 maxRows);

	// these settings can be done after Opening but before navigating, or after Rewind, ie cursor is affected, but not the abstract recordset contents or the columns
	STGIMPL_CALL bool BindExternal(SQLUSMALLINT colindex, SQLPOINTER buf, SQLLEN buflen);
	             bool Unbind(const SQLUSMALLINT colindex)    { return BindExternal(colindex, nullptr, 0); }
	STGIMPL_CALL bool UnbindAllInternal();


	// operational
	STGIMPL_CALL void OpenImpl  (); //bool throwError = true); // after this, ColumnInFo is available and a lock is obtained
	STGIMPL_CALL void OpenLocked(); //bool throwError = true); // after this, ColumnInFo is available and a lock is obtained
	STGIMPL_CALL void Close();                  
	STGIMPL_CALL bool LockThis();
	STGIMPL_CALL void UnLockThis();                 // after this, the database manager may close 
//	void Cancel()                               { if Active() Check(SQLCancel(StatementHandle()); }

	// recordset state-info
	bool          Active()       const          { return m_Active; }
	TExecType     ExecType()     const          { return m_ExecType; }
	CharPtr       ExecTypeName() const;
	CharPtr       SQL()          const          { return m_SQL.c_str(); }
	bool          IsLocked()     const          { return m_IsLocked; }
	STGIMPL_CALL SQLSMALLINT   ColumnCount();
	STGIMPL_CALL UInt32        RecordCount();

	// navigation
	STGIMPL_CALL void Next();
	STGIMPL_CALL bool Next(UInt32 forwardCount);

	STGIMPL_CALL void Rewind();

	// navigation-states
	bool          BeginOfFile()         const    { return m_BeginOfFile; }
	bool          EndOfFile()           const    { return m_EndOfFile; }
	bool          RecordDefined()       const    { return !(EndOfFile() || BeginOfFile()); }
	SharedStr     DiagnosticString()    const;
	
private:
	TRecordSet();
	TRecordSet(const TRecordSet&);

	// cursor state-info
	long          RecordNumber()        const    { return m_RecordNumber; }
	long          FrameNumber()         const    { return (RecordNumber() - (RecordNumber() < 0 ? FrameSize() - 1 : 0)) / FrameSize(); }
	long          FrameElementNumber()  const    { return (RecordNumber() - (RecordNumber() < 0 ? FrameSize() - 1 : 0)) % FrameSize(); }
	SQLINTEGER    FrameSize()           const    { return m_FrameSize; }


	//MTA DISCARDABLE: make sure that RecordSetManager reinitialises after GetStorageLocationArguments moved to a new table
	// info

	// odbc version 2.0
//	SQLINTEGER    ColumnDisplaySize(SQLUSMALLINT col) { return ColumnAttrValueNumeric2(col, SQL_COLUMN_DISPLAY_SIZE); }
//	SharedStr     ColumnLabel      (SQLUSMALLINT col) { return ColumnAttrValueCharacter2(col, SQL_COLUMN_LABEL); } 
	SQLINTEGER    ColumnLength     (SQLUSMALLINT col) { return ColumnAttrValueNumeric(col, SQL_COLUMN_LENGTH); }
	SharedStr     ColumnName       (SQLUSMALLINT col) { return ColumnAttrValueString(col, SQL_COLUMN_NAME); } 
	bool          ColumnNullable   (SQLUSMALLINT col) { SQLLEN res = ColumnAttrValueNumeric(col, SQL_COLUMN_NULLABLE); return res != SQL_NOT_DEFINED && res; }
	SQLINTEGER    ColumnPrecision  (SQLUSMALLINT col) { return ColumnAttrValueNumeric(col, SQL_COLUMN_PRECISION); }
	SQLINTEGER    ColumnScale      (SQLUSMALLINT col) { return ColumnAttrValueNumeric(col, SQL_COLUMN_SCALE); }
	bool          ColumnSigned     (SQLUSMALLINT col) { SQLLEN res = ColumnAttrValueNumeric(col, SQL_COLUMN_UNSIGNED); return res == SQL_NOT_DEFINED || ! res; }
	SQLINTEGER    ColumnType       (SQLUSMALLINT col) { return ColumnAttrValueNumeric(col, SQL_COLUMN_TYPE); }

protected:
	void SetAttributes();	
	void HandleDefine();	
	void HandleUndefine();

	SQLINTEGER StatementAttributeValue(const SQLINTEGER attribute);
	SQLLEN     ColumnAttrValueNumeric(const SQLUSMALLINT col, const SQLUSMALLINT attribute);
	SharedStr  ColumnAttrValueString (const SQLUSMALLINT col, const SQLUSMALLINT attribute);

	void       SetEOF();
	void       Check(const SQLRETURN ret);

	bool       IsExternalFrameSizeOK() const;

	void       SetMonade()   { dms_assert(HandleDefined()); Database().SetMonade(this, m_ExecType); }
	void       ResetMonade() { if(HandleDefined()) Database().ResetMonade(this); }
	void       InitBEOF();

private:
	TDatabase*       m_Database;
	SQLHSTMT         m_HSTMT;

	TExecType        m_ExecType;
	SharedStr        m_SQL;
	TSqlDataType     m_SqlType;

	TCursorType      m_CursorType;
	UInt32           m_FrameSize;
	UInt32           m_MaxRows;

	TColumns         m_Columns;
	TExternalBuffers m_ExternalBuffers;

	bool             m_Active, 
	                 m_BeginOfFile, 
	                 m_EndOfFile,
	                 m_UnbindAllInternal,
	                 m_IsLocked;
	SQLUINTEGER      m_FetchedCount;
	long             m_RecordNumber;
	long             m_RecordCount;
	long             m_ColumnCount;
};

struct TRecordSetOpenLock
{
	STGIMPL_CALL TRecordSetOpenLock(TRecordSet* rs);
	STGIMPL_CALL ~TRecordSetOpenLock();

	WeakPtr<TRecordSet> m_RS;
};

// out of class inline member function definitions
// inline TColumn member functions

inline const SQLPOINTER TColumn::Buffer()       const { return m_Buffer; }
inline SQLINTEGER       TColumn::BufferSize()   const { return m_RecordSet->FrameSize() * ElementSize(); }
inline SQLINTEGER       TColumn::ElementSize()  const { return m_ElementSize; }	
inline const SQLPOINTER TColumn::ElementBuffer()const { return((char*) Buffer()) + m_RecordSet->FrameElementNumber() * ElementSize(); }
inline SQLLEN           TColumn::ActualSize()   const { dms_assert(m_ActualSizes); return m_ActualSizes[m_RecordSet->FrameElementNumber()]; }
inline bool             TColumn::IsNull()       const { return ActualSize() == SQL_NULL_DATA; }

#endif //!defined(__STGIMPL_ODBC_IMP_H)
