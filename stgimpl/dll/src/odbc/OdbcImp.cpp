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

#include "ImplMain.h"
#pragma hdrstop

#include "OdbcImp.h"

#pragma warning( disable : 4200) // zero-sized array in struct, was set again somewhere

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "geo/Conversions.h"
#include "geo/Undefined.h"
#include "mci/Class.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"

#include <math.h>

/*****************************************************************************/
//										PROTOTYPES
/*****************************************************************************/

/*****************************************************************************/
//										DEFINES
/*****************************************************************************/

#define	BINARY_ELEMENT_SIZE	2

/*****************************************************************************/
//										FUNCTIONS
/*****************************************************************************/

char* DateToString(DATE_STRUCT *date, char *s)
{
	sprintf(s, "%hu-%hu-%hd", date->day, date->month, date->year);
	return	s;
} // DateToString

char* TimeToString(TIME_STRUCT *time, char *s)
{
	sprintf(s, "%hu:%hu:%hu", time->hour, time->minute, time->second);
	return	s;
} // TimeToString

bool TColumn::IsVarSized() const
{
	switch (CType())
	{
		case SQL_C_BINARY:
		case SQL_C_BIT:
		case SQL_C_CHAR:
			return true;
	}
	return false;
}

char* TimeStampToString(TIMESTAMP_STRUCT *timestamp, char *s)
{
	DATE_STRUCT date;
	TIME_STRUCT time;
	char        datestr[MAXBUFLEN], 
	            timestr[MAXBUFLEN];

	date.year	=	timestamp->year;
	date.month	=	timestamp->month;
	date.day		=	timestamp->day;
	time.hour	=	timestamp->hour;
	time.minute	=	timestamp->minute;
	time.second	=	timestamp->second;

	sprintf(s, "%s %s:%lu", DateToString(&date, datestr), TimeToString(&time, timestr), timestamp->fraction);
	return	s;
} // TimeStampToString

SharedStr TDatabase::DiagnosticString() const
{
	return mySSPrintF("\nDatabase : %s", DatasourceName());
}

SharedStr TRecordSet::DiagnosticString() const
{
	return mySSPrintF("\nRecordSet(%s): %s", 
			ExecTypeName(),
			SQL());
}

SharedStr TColumn::DiagnosticString() const
{
	return mySSPrintF("\nColumn(%s): %s", CTypeName(), Name());
}

SharedStr DiagnosticString(const TDatabase* db, const TRecordSet* rs, const TColumn* col)
{
	SharedStr inCol, inRS, inDB;
	if (col)
	{
		inCol = col->DiagnosticString();
		if (!rs) rs = col->RecordSet();
	}
	if (rs)
	{
		inRS = rs->DiagnosticString();
		if (!db) db = rs->DatabasePtr();
	}
	if (db) 
		inDB = db->DiagnosticString();

	return mySSPrintF("%s%s%s", inCol.c_str(), inRS.c_str(), inDB.c_str());
}

void Diagnostics(const SQLRETURN ret, const SQLSMALLINT handletype, const SQLHANDLE handle, TDatabase* db, TRecordSet* rs, TColumn* col)
{
	SQLCHAR     sqlstate[MAXBUFLEN],
	            message [MAXBUFLEN_EXT];
	SQLINTEGER  nativeerror;
	SQLSMALLINT messagelen; 

	sqlstate[0] = 0;
	message [0] = 0;
	SQLRETURN diagRet = SQLGetDiagRec(handletype, handle, 1, sqlstate, &nativeerror, message, MAXBUFLEN_EXT, &messagelen);

	CharPtr level = (ret == SQL_SUCCESS_WITH_INFO) ? "Diagnostic" : "Error";

	MG_CHECK(diagRet == SQL_SUCCESS || diagRet == SQL_SUCCESS_WITH_INFO);
	SharedStr msg = 
		mySSPrintF("ODBC %s: %s, %s%s", 
			level,
			sqlstate, 
			message, 
			DiagnosticString(db, rs, col).c_str()
		);
	if (ret != SQL_SUCCESS_WITH_INFO  || strcmp((char*)sqlstate, "IM006"))
		MGD_TRACE(("%s", msg.c_str()));
	if (ret != SQL_SUCCESS_WITH_INFO)
		::throwErrorD("ODBC", msg.c_str());
}

void Check(const SQLRETURN ret, const SQLSMALLINT handletype, const SQLHANDLE handle, TDatabase* db, TRecordSet* rs, TColumn* col)
{
	dms_assert(handletype == SQL_HANDLE_ENV || handletype == SQL_HANDLE_DBC || handletype == SQL_HANDLE_STMT || handletype == SQL_HANDLE_DESC);

	bool res
		=	ret == SQL_SUCCESS                                   // => return tue
		|| ret == SQL_SUCCESS_WITH_INFO                          // => return Diagnostics(SQL_SUCCESS_WITH_INFO, ...) -> Checck(SQLGetDiagRec()) only if handletpye any of the mentioned set
		|| (ret == SQL_NO_DATA && handletype != SQL_HANDLE_DBC); // => return tue

	// else: return Diagnostics(!WITH_INFO, ...)  only if handletype any of the mentioned set -> Throw

	if (! res || ret == SQL_SUCCESS_WITH_INFO) 
		Diagnostics(ret, handletype, handle, db, rs, col);
}

/*
	SQL types and character lengths 
	-------------------------------

	Type						length
	---------------------------
	SQL_BIGINT				19
	SQL_BINARY(n)			n 
	SQL_BIT 					1 
	SQL_CHAR(n)				n
	SQL_DECIMAL(n, m)		n			
	SQL_DOUBLE 				15 
	SQL_FLOAT				15 
	SQL_INTEGER				10 
	SQL_NUMERIC(n, m)		n			
	SQL_REAL 				7 
	SQL_SMALLINT 			5 
	SQL_TINYINT 			3 
	SQL_TYPE_DATE			10 (yyyy-mm-dd). 
	SQL_TYPE_TIME			8	(hh-mm-ss), 
								9 + s (hh:mm:ss[.fff…], s is the seconds precision). 
	SQL_TYPE_TIMESTAMP	16 (yyyy-mm-dd-hh:mm) 
								19 (yyyy-mm-dd hh:mm:ss)
								20 + s (yyyy-mm-dd hh:mm:ss[.fff…], where s is the seconds precision). 
	SQL_VARBINARY(n)		n 
	SQL_VARCHAR(n)			n
*/

SQLSMALLINT	SQLToCType(const SQLINTEGER sqltype, const bool sign)
{
	switch (sqltype)
	{
		case SQL_TINYINT       : return	sign ? SQL_C_STINYINT : SQL_C_UTINYINT;
		case SQL_INTEGER       : return	sign ? SQL_C_SLONG   : SQL_C_ULONG;
		case SQL_SMALLINT      : return	sign ? SQL_C_SSHORT  : SQL_C_USHORT;
		case SQL_BIGINT        : return	sign ? SQL_C_SBIGINT : SQL_C_UBIGINT;

		case SQL_REAL          : return	SQL_C_FLOAT;
		case SQL_FLOAT         : 
		case SQL_DOUBLE        : return	SQL_C_DOUBLE;

		case SQL_BIT           : return	SQL_C_BIT;

		case SQL_LONGVARBINARY :
		case SQL_VARBINARY     :
		case SQL_BINARY        : return	SQL_C_BINARY;

		case SQL_NUMERIC       :
		case SQL_DECIMAL       :
		case SQL_CHAR          : 
		case SQL_LONGVARCHAR   :
		case SQL_VARCHAR       : return	SQL_C_CHAR;

		case SQL_DATETIME      : return	SQL_C_TYPE_TIMESTAMP; 
		case SQL_TIME          : return	SQL_C_TIME;
		case SQL_TIMESTAMP     : return	SQL_C_TIMESTAMP; 
		case SQL_TYPE_DATE     : return	SQL_C_TYPE_DATE;
		case SQL_TYPE_TIME     : return	SQL_C_TYPE_TIME;
		case SQL_TYPE_TIMESTAMP: return	SQL_C_TYPE_TIMESTAMP;	

		case SQL_GUID          : return	SQL_C_GUID;

		default                : return	SQL_C_SLONG;
	}	
}

CharPtr CTypeToString(const SQLSMALLINT ctype)
{
	switch (ctype)
	{
		case SQL_C_BINARY:         return "SQL_C_BINARY";
		case SQL_C_BIT:            return "SQL_C_BIT";
		case SQL_C_CHAR:           return "SQL_C_CHAR"; 
		case SQL_C_DOUBLE:         return "SQL_C_DOUBLE";
		case SQL_C_FLOAT:          return "SQL_C_FLOAT";
		case SQL_C_GUID:           return "SQL_C_GUID";
		case SQL_C_SBIGINT:        return "SQL_C_SBIGINT"; 
		case SQL_C_SLONG:          return "SQL_C_SLONG"; 
		case SQL_C_SSHORT:         return "SQL_C_SSHORT"; 
		case SQL_C_STINYINT:       return "SQL_C_STINYINT";
		case SQL_C_TIME:           return "SQL_C_TIME";
		case SQL_C_TIMESTAMP:      return "SQL_C_TIMESTAMP"; 
		case SQL_C_TYPE_DATE:      return "SQL_C_TYPE_DATE";
		case SQL_C_TYPE_TIME:      return "SQL_C_TYPE_TIME";
		case SQL_C_TYPE_TIMESTAMP: return "SQL_C_TYPE_TIMESTAMP";
		case SQL_C_UBIGINT:        return "SQL_C_UBIGINT";
		case SQL_C_ULONG:          return "SQL_C_ULONG";
		case SQL_C_USHORT:         return "SQL_C_USHORT";
		case SQL_C_UTINYINT:       return "SQL_C_UTINYINT";
		default:                   return "TYPE UNKNOWN";
	}
}

#if defined(MG_DEBUG)
CharPtr AttributeValueToString(const SQLINTEGER attribute, const SQLINTEGER attributevalue)
{
	switch (attribute)
	{
		case SQL_ATTR_CONCURRENCY	:	
			switch (attributevalue)
			{
				case SQL_CONCUR_READ_ONLY: return "SQL_ATTR_CONCURRENCY - SQL_CONCUR_READ_ONLY";	// default
				case SQL_CONCUR_LOCK     : return "SQL_ATTR_CONCURRENCY - SQL_CONCUR_LOCK";
				case SQL_CONCUR_ROWVER   : return "SQL_ATTR_CONCURRENCY - SQL_CONCUR_ROWVER";
				case SQL_CONCUR_VALUES   : return "SQL_ATTR_CONCURRENCY - SQL_CONCUR_VALUES";
				default                  : return "SQL_ATTR_CONCURRENCY - UNKNOWN VALUE";
			} // switch
			break;

		case SQL_ATTR_CURSOR_TYPE:
			switch (attributevalue)
			{
				case SQL_CURSOR_FORWARD_ONLY : return "SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_FORWARD_ONLY";	// default
				case SQL_CURSOR_STATIC       : return "SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_STATIC";
				case SQL_CURSOR_KEYSET_DRIVEN: return "SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_KEYSET_DRIVEN";
				case SQL_CURSOR_DYNAMIC      : return "SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_DYNAMIC";
				default                      : return "SQL_ATTR_CURSOR_TYPE - UNKNOWN VALUE";
			}
			break;

		case SQL_ATTR_CURSOR_SCROLLABLE:
			switch (attributevalue)
			{
				case SQL_NONSCROLLABLE : return "SQL_ATTR_CURSOR_SCROLLABLE - SQL_NONSCROLLABLE"; // default
				case SQL_SCROLLABLE    : return "SQL_ATTR_CURSOR_SCROLLABLE - SQL_SCROLLABLE";
				default                : return "SQL_ATTR_CURSOR_SCROLLABLE - UNKNOWN VALUE";
			}
			break;

		case SQL_ATTR_ROW_BIND_TYPE:
			switch (attributevalue)
			{
				case SQL_BIND_BY_COLUMN: return "SQL_ATTR_ROW_BIND_TYPE - SQL_BIND_BY_COLUMN";
				default                : return "SQL_ATTR_ROW_BIND_TYPE - SQL_BIND_BY_ROW";
			}
			break;

		case SQL_ATTR_ROW_ARRAY_SIZE		:	return	"SQL_ATTR_ROW_ARRAY_SIZE - %d"; 
			break;

		case SQL_ATTR_ROW_STATUS_PTR	:	
			return "SQL_ATTR_ROW_STATUS_PTR";
			break;

		case SQL_ATTR_USE_BOOKMARKS:
			switch (attributevalue)
			{
				case SQL_UB_OFF     : return "SQL_ATTR_USE_BOOKMARKS - SQL_UB_OFF";
				case SQL_UB_VARIABLE: return "SQL_ATTR_USE_BOOKMARKS - SQL_UB_VARIABLE";
				default             : return "SQL_ATTR_USE_BOOKMARKS - UNKNOWN VALUE";
			}
			break;

 		default: return "UNKNOWN ATTRIBUTE";
	}
}
#endif

bool	CTypeHasVarSize(const SQLSMALLINT ctype)
{
	return	ctype == SQL_C_CHAR || ctype == SQL_C_BINARY;
} // CTypeHasVarSize

#if defined(MG_DEBUG)
const char* SQLRETURNToString(const SQLRETURN res)
{
	switch (res)
	{
		case SQL_SUCCESS          : return "SQL_SUCCESS";
		case SQL_SUCCESS_WITH_INFO: return "SQL_SUCCESS_WITH_INFO" ;
		case SQL_INVALID_HANDLE   : return "SQL_INVALID_HANDLE" ;
		case SQL_ERROR            : return "SQL_ERROR";
		case SQL_NEED_DATA        : return "SQL_NEED_DATA";
		case SQL_NO_DATA          : return "SQL_NO_DATA";
		case SQL_STILL_EXECUTING  : return "SQL_STILL_EXECUTING";
	
		default:
			return	"SQLRETURN undefined";
	}
}
#endif

ULONG GetRecordCount(TDatabase *db, CharPtr sql) 
{
	// XXX, TODO: some SELECT STATEMETNS REQUIRE going through the recordset anyway (such as WHEREm UNION and JOIN clauses) and SELECT COUNT(*) FROM XXX has issues
	// TODO: try UnbindAllInternal(); and or MoveLast to reduce overhead of the following counting
	ULONG result =0;
	TRecordSet recordCount(db);

	recordCount.CreateRecordSet(sql);
	recordCount.UnbindAllInternal();

	TRecordSetOpenLock rsOpenLokc(&recordCount); // Does Rewind
	recordCount.Next(); // combined with Rewind this constitutes a MoveFirst

	while (!recordCount.EndOfFile())
	{
		recordCount.Next();
		++result;
	}
	return result;
} // GetRecordCount

/*****************************************************************************/
//										TEnvironment
/*****************************************************************************/

TEnvironment	*g_DefaultEnvironment = NULL;

TEnvironment::TEnvironment()
{
	m_HENV	= SQL_NULL_HENV; 
	EnvironmentHandleDefine();
}

TEnvironment::~TEnvironment()
{
	try {
		EnvironmentHandleUndefine();
	}
	catch (...)
	{}

	if (this == g_DefaultEnvironment)
		g_DefaultEnvironment = NULL;
}

TEnvironment* TEnvironment::GetDefaultEnvironment()
{
	if (! g_DefaultEnvironment)
		g_DefaultEnvironment = new TEnvironment;

	return g_DefaultEnvironment;
}

void TEnvironment::EnvironmentHandleDefine()
{
	if (!EnvironmentHandleDefined())
	{
		Check(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_HENV));
		Check(SQLSetEnvAttr(EnvironmentHandle(), SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0));
	}
}

void TEnvironment::EnvironmentHandleUndefine()
{
	if (EnvironmentHandleDefined())
		Check(SQLFreeHandle(SQL_HANDLE_ENV, EnvironmentHandle()));

	m_HENV = SQL_NULL_HENV;
}

void TEnvironment::Check(const SQLRETURN ret)
{
	::Check(ret, SQL_HANDLE_ENV, EnvironmentHandle(), 0, 0, 0);
}

/*****************************************************************************/
//										TDatabase
/*****************************************************************************/

TRecordSet* TDatabase::s_MonadicRecordSet  = nullptr;

TDatabase::TDatabase(CharPtr configLoadDir, TEnvironment *env)
	:	m_ConfigLoadDir(configLoadDir)
	,	m_DriverType(dtUnknown)
{
	m_Environment       = (env != nullptr) ? env : TEnvironment::GetDefaultEnvironment();
	m_HDBC              = SQL_NULL_HDBC; 
	m_Active            = false;	
	m_ConnectionTimeOut = 5; 
	
	HandleDefine();
	Init();
}

TDatabase::~TDatabase()
{
	HandleUndefine();
}

void TDatabase::Init() // keep statement handle?
{
	Close();
	dms_assert(!Active()); 
	m_ConnectionString = ""; 
}

void TDatabase::SetDatasourceName(CharPtr name)
{ 
	if (m_DatasourceName == name)
		return;
	Init();
	m_DatasourceName = name; 
}

void TDatabase::SetUserName(CharPtr name)  
{ 
	if (m_UserName == name)
		return;
	Init();
	m_UserName = name; 
}

void TDatabase::SetPassword(CharPtr name)  
{ 
	if (m_Password == name)
		return;
	Init();
	m_Password = name; 
}


void TDatabase::HandleDefine()
{
	if (!HandleDefined())
	{
		Check(SQLAllocHandle(SQL_HANDLE_DBC, Environment().EnvironmentHandle(), &m_HDBC));
		Check(SQLSetConnectAttr(Handle(), SQL_ATTR_ACCESS_MODE,   (SQLPOINTER) SQL_MODE_READ_ONLY , 0));
		Check(SQLSetConnectAttr(Handle(), SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER) (UINT_PTR) m_ConnectionTimeOut, 0));
	}
	dms_assert(HandleDefined());
}

void TDatabase::HandleUndefine()
{
	if (HandleDefined())
	{
		Close();
		Check(SQLFreeHandle(SQL_HANDLE_DBC, Handle()));
		m_HDBC = SQL_NULL_HDBC;
	}
	dms_assert(!HandleDefined());
}

void TDatabase::Open()
{
	if (Active())
		return;

	const int DEFAULT_BUFFER_SIZE = 1024;
	BYTE buffer[DEFAULT_BUFFER_SIZE];
	Int16 returnSize = 0;

	Check(SQLDriverConnect(
			Handle(), 
			NULL,                // SQLHWND WindowHandle
			const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>( GetConnectionStr() ) ), 
			SQL_NTS,             // length of connectionString
			buffer,              // Out ConntectionString
			DEFAULT_BUFFER_SIZE, // buffer length
			&returnSize,         // pointer to SQLSMALLINT for returned buffer size;
			// /*SQL_DRIVER_PROMPT SQL_DRIVER_NOPROMPT*/ SQL_DRIVER_COMPLETE_REQUIRED
			SQL_DRIVER_NOPROMPT));

	m_Active = true;
	if (returnSize == 0)
		m_ConnectionString = SharedStr();
	else
	{
		if (returnSize < DEFAULT_BUFFER_SIZE)
			m_ConnectionString = reinterpret_cast<char*>(buffer);
		else
		{
			++returnSize; // allow for string terminator
			m_ConnectionString.resize(returnSize);
			CharType* connectionStr = m_ConnectionString.begin();
			Check(SQLDriverConnect(
				Handle(), 
				NULL,                       // SQLHWND WindowHandle
				reinterpret_cast<SQLCHAR*>(connectionStr), // szConnStrIn
				SQL_NTS,                    // length of connectionString
				reinterpret_cast<SQLCHAR*>(connectionStr), // Out ConntectionString
				returnSize,                 // buffer length
				&returnSize,                // pointer to SQLSMALLINT for returned buffer size;
				/*SQL_DRIVER_PROMPT SQL_DRIVER_NOPROMPT*/ SQL_DRIVER_COMPLETE_REQUIRED)
			);
		}
	}
	reportD(SeverityTypeID::ST_MinorTrace, "Opened connection as ", m_ConnectionString.c_str());
}

CharPtr TDatabase::GetConnectionStr()
{
	if (m_ConnectionString.empty())
	{
		m_ConnectionString = DatasourceName();

		if (m_ConnectionString.find('=') == m_ConnectionString.send()) // no proper connection string, just a name, make it proper
		{
			if (m_ConnectionString.find(".mdb") !=  m_ConnectionString.send())
				m_ConnectionString = "DBQ=" + ConvertDmsFileName(m_ConnectionString);
			else
				m_ConnectionString = "DSN=" + m_ConnectionString;
		}

		if (m_ConnectionString.find("DBQ=") !=  m_ConnectionString.send())
		{
			if (m_ConnectionString.find(".mdb") != m_ConnectionString.send())
			{
				m_DriverType = dtMsAccess;
				if (m_ConnectionString.find("Driver=") ==  m_ConnectionString.send())
				{
#if defined(DMS_64)
					m_ConnectionString += ";Driver={Microsoft Access Driver (*.mdb, *.accdb)}";
#else
					m_ConnectionString += ";Driver={Microsoft Access Driver (*.mdb)}";
#endif
				}
				if (m_ConnectionString.find("DefaultDir=") ==  m_ConnectionString.send())
				{
					m_ConnectionString += ";DefaultDir=";
					m_ConnectionString += ConvertDmsFileName(m_ConfigLoadDir);
				}
			}
		}
		if (!m_UserName.empty())
			m_ConnectionString += SharedStr(";UID=") + m_UserName;
		if (!m_Password.empty())
			m_ConnectionString += SharedStr(";PWD=") + m_Password;
	}
	return m_ConnectionString.c_str();
}

TDriverType TDatabase::GetDriverType()
{
	GetConnectionStr();
	return m_DriverType;
}

void TDatabase::Close()
{
	if (! Active())
		return;

	CloseAllRecordSets();

	dms_assert(m_OpenRecordSets.size() == 0);

	Check(SQLDisconnect(Handle()));
	m_Active = false;
}

SQLUSMALLINT TDatabase::GetInfoUSInteger(SQLUSMALLINT infotype)
{
	SQLUSMALLINT infovalue;
	Check(SQLGetInfo(Handle(), infotype, (SQLPOINTER) &infovalue, sizeof(infovalue), NULL));
	return infovalue;
}

SQLUINTEGER TDatabase::GetInfoUInteger(SQLUSMALLINT infotype)
{
	SQLUINTEGER	infovalue;
	Check(SQLGetInfo(Handle(), infotype, (SQLPOINTER) &infovalue, sizeof(infovalue), NULL));
	return infovalue;
}

SharedStr TDatabase::GetInfoCharacter(SQLUSMALLINT infotype)
{
	char infoValue[MAXBUFLEN];
	SQLSMALLINT	infoValueLen;
	Check(SQLGetInfo(Handle(), infotype, infoValue, MAXBUFLEN, &infoValueLen));
	return SharedStr(infoValue, infoValue+infoValueLen);
}

void TDatabase::SetMonade(TRecordSet *recordset, const TExecType rse)
{
	if (rse != rseRecordSet ) // MTA: MS_ACCESS returns 0 but can only handle 1 rse ==rseExecSQL || MaxConcurrentActivities() == 0)
		return;

	if (s_MonadicRecordSet != recordset && s_MonadicRecordSet != nullptr)
	{
		if (s_MonadicRecordSet->IsLocked()) 
			throwErrorF("ODBC", "SetMonade failed for:%s\nBecause the following RecordSet is currently locking that database:%s"
			,	::DiagnosticString(0, recordset, 0).c_str()
			,	s_MonadicRecordSet->DiagnosticString().c_str()
			);
		s_MonadicRecordSet->Close();
	}

	s_MonadicRecordSet = recordset;
}

void TDatabase::ResetMonade(const TRecordSet* recordset)
{
	if (s_MonadicRecordSet == recordset)
		s_MonadicRecordSet = nullptr;
} // TDatabase::ResetMonade

void TDatabase::CloseAllRecordSets()
{
	TRecordSetCollection::iterator rsIter = m_OpenRecordSets.begin();
	while (rsIter != m_OpenRecordSets.end())
	{
		TRecordSet* rsCurr = *rsIter++; // iterator must be forwarded before ->Close() removes the pointee from the OpenRecordSets
		if (rsCurr->IsLocked()) 
			rsCurr->UnLockThis();
		rsCurr->Close();
	}
}

void TDatabase::Check(const SQLRETURN ret)	
{
	::Check(ret, SQL_HANDLE_DBC, Handle(), this, 0, 0);
}

/*****************************************************************************/
//										TColumn
/*****************************************************************************/

TColumn::TColumn()		
{ 
	m_RecordSet        = nullptr;
	m_Buffer           = nullptr; 
	m_ActualSizes      = nullptr;
	m_IsInternalBuffer = true;	
	Undefine();
}

TColumn::~TColumn()		
{ 
	BindNone();
}

bool TColumn::Define(const SQLUSMALLINT colindex, TRecordSet *rs)
{ 
	Undefine();

	m_Index       = colindex;
	m_RecordSet   = rs;
	m_Name        = rs->ColumnName(colindex).c_str();
	m_CType       = SQLToCType(rs->ColumnType(colindex), rs->ColumnSigned(colindex));
	m_ElementSize = rs->ColumnLength(colindex) + (m_CType == SQL_C_CHAR ? 1 : 0);
	MakeMin(m_ElementSize, 0x100000);
	return true;
}

bool CompatibleCTypes(SQLSMALLINT cTypeData, SQLSMALLINT cTypeRequested)
{
	return true;
}

bool TColumn::Redefine(SQLSMALLINT cType, SQLINTEGER elementSize)
{
	dms_assert(m_RecordSet); // Define must have been called
	if (m_CType == cType && m_ElementSize == elementSize)
		return true;
	MG_CHECK2(!m_RecordSet->Active() || m_RecordSet->m_FetchedCount == 0, 
		"Cannot reset column type after recordset activation (or after SQLFetch)" ); 

	if (!CompatibleCTypes(m_CType, cType))
		return false;

	m_CType       = cType;
	m_ElementSize = elementSize;
	return true;
}

void TColumn::Undefine()	
{ 
	m_Index       = 0;
	m_Name        = "";
	m_CType       = 0;
	m_ElementSize = 0;
	m_AsString.clear();
	BindNone();
} // TColumn::Undefine

void TColumn::FreeBuffers()
{ 
	if (m_IsInternalBuffer)
		delete [] m_Buffer;
	m_Buffer = nullptr;
	delete [] m_ActualSizes;
	m_ActualSizes = nullptr;
} // TColumn::FreeBuffers

bool TColumn::BindNone()
{
	FreeBuffers();
	return Bind();
} // TColumn::BindNone

SQLLEN* CreateActualSizeArray(UInt32 nrFrames)
{
	SQLLEN* result = new SQLLEN[nrFrames];
	fast_fill(result, result + nrFrames, SQL_NULL_DATA);
	return result;
}

bool TColumn::BindInternal()
{
	FreeBuffers();
	m_IsInternalBuffer = true;
	m_Buffer           = new char[BufferSize()];
	m_ActualSizes      = CreateActualSizeArray(m_RecordSet->FrameSize());
	return Bind();
} // TColumn::BindInternal

bool TColumn::BindExternal(SQLPOINTER buf)
{
	FreeBuffers();
	m_IsInternalBuffer = false;
	if (buf)
	{
		m_Buffer       = buf;
		m_ActualSizes  = CreateActualSizeArray(m_RecordSet->FrameSize());
	}
	return Bind();
}

bool TColumn::Bind()
{
	if (!m_RecordSet || ! m_RecordSet->Active())
		return false;
	Check(
		SQLBindCol(
			m_RecordSet->Handle(), 
			Index(), 
			CType(), 
			m_Buffer, 
			ElementSize(),
			m_ActualSizes
		)
	);
	return true;
}

CharPtr TColumn::AsString() const
{
	m_AsString.resize(0);

	dms_assert( Defined() );

	if (IsVarSized() && IsNull())
		return UNDEFINED_VALUE_STRING;

	dms_assert(!m_ActualSizes || (ActualSize() >= 0 && ActualSize() <= ElementSize()));

	switch (CType())
	{
		case SQL_C_BINARY:
		{
			m_AsString.resize(BINARY_ELEMENT_SIZE * ActualSize() + 1);
			char* tmp = begin_ptr(m_AsString);

			for (int i = 0; i < ActualSize(); ++i, tmp += BINARY_ELEMENT_SIZE)
				sprintf(tmp, "%02x", (UINT) ((UCHAR*) ElementBuffer())[i]);
			break;
		}
		case SQL_C_BIT:
			m_AsString.resize(ActualSize() + 1);
			sprintf(&*m_AsString.begin(), "%hu", (USHORT) (* ((UCHAR*) ElementBuffer())));
			break;

		case SQL_C_CHAR:
			m_AsString.resize(ElementSize());
			memcpy(&*m_AsString.begin(), ElementBuffer(), ElementSize());
			m_AsString[ActualSize()] = 0;
			break;	

		case SQL_C_DATE:
			m_AsString.resize(MAXBUFLEN);
			DateToString((DATE_STRUCT*) ElementBuffer(), &*m_AsString.begin());
			break;

		case SQL_C_DOUBLE:
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%E", * ((double*) ElementBuffer()));
			break;

		case SQL_C_FLOAT:
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%E", * ((float*) ElementBuffer()));
			break;	

		case SQL_C_GUID: 
			break;	

		case SQL_C_SLONG:	
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%ld", * ((long*) ElementBuffer()));
			break;

		case SQL_C_SSHORT:	
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%hd", * ((short*) ElementBuffer()));
			break;

		case SQL_C_TIME:
			m_AsString.resize(MAXBUFLEN);
			TimeToString((TIME_STRUCT*) ElementBuffer(), &*m_AsString.begin());
			break;

		case SQL_C_TIMESTAMP:
			m_AsString.resize(MAXBUFLEN);
			TimeStampToString((TIMESTAMP_STRUCT*) ElementBuffer(), &*m_AsString.begin());
			break;

		case SQL_C_STINYINT:
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%hd", (short) (* ((char*) ElementBuffer())));
			break;

		case SQL_C_TYPE_TIMESTAMP:
			m_AsString.resize(MAXBUFLEN);
			TimeStampToString((TIMESTAMP_STRUCT*) ElementBuffer(), &*m_AsString.begin());
			break;

		case SQL_C_ULONG:	
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%hd", * ((ULONG*) ElementBuffer()));
			break;

		case SQL_C_USHORT:	
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%hu", * ((USHORT*) ElementBuffer()));
			break;

		case SQL_C_UTINYINT:
			m_AsString.resize(MAXBUFLEN);
			sprintf(&*m_AsString.begin(), "%hu", (USHORT) (* ((UCHAR*) ElementBuffer())));
			break;
	} // switch

	dms_assert(begin_ptr(m_AsString));
	return begin_ptr(m_AsString);
}

void TColumn::Check(const SQLRETURN ret)	
{
	::Check(ret, SQL_HANDLE_STMT, m_RecordSet->Handle(), 0, 0, this);
}

/*****************************************************************************/
//										TColumns
/*****************************************************************************/

bool TColumns::Define(TRecordSet* rs)
{
	if (!Defined())
	{
		resize(Max<SQLSMALLINT>(rs->ColumnCount(), SQLSMALLINT(0)));

		m_Defined = true;

		UInt32 c = 0;
		for (iterator i = begin(), e = end(); i != e; ++i) 
			if (! (m_Defined = i->Define(++c, rs))) // one based column ids
				break;

		m_Defined = rs->IsExternalFrameSizeOK();
	}

	if (Defined() && !m_BuffersBound)
	{
		SizeT i = 0;
		for (; i < Min<SizeT>(rs->m_ExternalBuffers.size(), size()); ++i) 
			if (rs->m_ExternalBuffers[i].DefinedBind())
				if (! (m_Defined = operator[](i).BindExternal(rs->m_ExternalBuffers[i].m_Buffer)))
					break;

		iterator
			colPtr = begin(),
			colEnd = end();
		for (i = 0; colPtr != colEnd; ++i, ++colPtr) 
			if (colPtr->Buffer() == nullptr)
			{
				if	(	rs->m_UnbindAllInternal
					||	(	i < rs->m_ExternalBuffers.size() 
						&&	rs->m_ExternalBuffers[i].DefinedUnbind()
						)
					)
					colPtr->BindNone();
				else
				{
					m_Defined = colPtr->BindInternal();
					if (! m_Defined)
						break;
				}
			}
		m_BuffersBound = true;
	}

	if (! Defined())
		Undefine();

	return Defined();
}

bool TColumns::BindExternal(SQLUSMALLINT colindex, SQLPOINTER buf, SQLLEN buflen, TRecordSet *rs)
{
	dms_assert(Defined());
	if (colindex < 1 || size() < colindex)
		return false;

	SizeT framesize = 0;
	if (buf) framesize = ThrowingConvert<SizeT>( buflen / (begin() + (colindex - 1))->ElementSize() );
	if (buf && framesize != rs->FrameSize())
		return false;

	return operator[](colindex - 1).BindExternal(buf);
}

void  TColumns::UnBind()
{
	dms_assert(Defined());
	if (m_BuffersBound)
	{
		std::vector<TColumn>::iterator
			colIter = begin(),
			colEnd  = end();
		for (; colIter != colEnd; ++colIter)
			colIter->BindNone();
		m_BuffersBound = false;
	}
}

void	TColumns::Undefine()	
{ 
	clear(); // destroy each TColumn element
	m_Defined	=	false;
} // TColumns::Undefine

bool  TColumns::FindByName(CharPtr colName, UInt32* colIndex)
{
	dms_assert(Defined());
	std::vector<TColumn>::iterator
		colIter = begin(),
		colEnd  = end();
	for (*colIndex = 0; colIter != colEnd; ++colIter)
		if (!stricmp(colIter->Name(), colName))
		{
			*colIndex = (colIter - begin()) +1;
			return true;
		}
	return false;
}

/*****************************************************************************/
//										TRecordSet
/*****************************************************************************/

TRecordSet::TRecordSet(TDatabase *db)
{				
	m_Database     = db; 
	m_HSTMT        = SQL_NULL_HSTMT; 
	m_Active       = false;	
	m_UnbindAllInternal = false;
	m_IsLocked     = false;
	Init();
}

void TRecordSet::Init() // keep statement handle?
{
	Close();

	dms_assert(!Active());
	dms_assert(!IsLocked());

	m_BeginOfFile  = true;
	m_EndOfFile    = true;
	m_RecordNumber = NOT_DEFINED;	
	m_RecordCount  = NOT_DEFINED;	
	m_ColumnCount  = NOT_DEFINED;	
	m_CursorType   = SQL_CURSOR_FORWARD_ONLY;
	m_MaxRows      = 0;
	m_ExecType     = rseRecordSet;
	m_SQL          = "";
	m_SqlType      = NOT_DEFINED;

	m_Columns.Undefine();
	m_ExternalBuffers.clear();
	m_UnbindAllInternal = false;

	SetFrameSize();
}

TRecordSet::~TRecordSet()							
{	
	dms_assert(!GetRefCount()); // Only Release can destroy
	if (DatabasePtr() != nullptr) 
	{
		if (IsLocked())
			UnLockThis();
		Close(); // ->  ResetMonade()
		dms_assert(DatabasePtr()->m_OpenRecordSets.find(this) == DatabasePtr()->m_OpenRecordSets.end());
	}
}

void TRecordSet::CreateRecordSet(CharPtr sql)
{
	if (ExecType() != rseRecordSet || m_SQL != sql)
	{
		Init();
		m_ExecType = rseRecordSet;
		m_SQL = sql;
	}
}

void TRecordSet::CreateExecSQL  (CharPtr sql)
{
	if (ExecType() != rseExecSQL || m_SQL != sql)
	{
		Init();
		m_ExecType = rseExecSQL;
		m_SQL = sql;
	}
}

void TRecordSet::CreateTableInfo()
{
	if (ExecType() != rseTableInfo)
	{
		Init();
		m_ExecType = rseTableInfo;
	}
}

void TRecordSet::CreateColumnInfo(CharPtr tableName)
{
	if (ExecType() != rseColumnInfo || m_SQL != tableName)
	{
		Init();
		m_ExecType = rseColumnInfo;
		m_SQL = tableName;
	}
}

void TRecordSet::SetFrameSize(UInt32 size)
{ 
	dms_assert(!IsLocked() || m_FrameSize == size);
	if (m_FrameSize != size)
	{
		Close();
		m_FrameSize = size;
	}
}

void TRecordSet::SetMaxRows(UInt32 maxRows)
{ 
	dms_assert(!IsLocked() || m_MaxRows == maxRows);
	if (m_MaxRows != maxRows)
	{
		Close();
		m_MaxRows = maxRows;
	}
}

void TRecordSet::SetAttributes()
{
	m_FetchedCount = 0;
	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_CONCURRENCY,      (SQLPOINTER) SQL_CONCUR_READ_ONLY, 0));
	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_CURSOR_TYPE,      (SQLPOINTER)(UINT_PTR) m_CursorType, 0));
	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_MAX_ROWS,         (SQLPOINTER)(UINT_PTR) m_MaxRows,    0));
	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_ROWS_FETCHED_PTR, &m_FetchedCount,           0));
	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_ROW_ARRAY_SIZE,   (SQLPOINTER)(UINT_PTR) FrameSize(),  0));
}

void TRecordSet::HandleDefine()
{
	dms_assert(DatabasePtr());
	if (HandleDefined())
		return;

	dms_assert(!m_Active);

	Database().Open();
	Check(SQLAllocHandle(SQL_HANDLE_STMT, Database().Handle(), &m_HSTMT));
	Database().AddOpenRecordSet(this);
}

void TRecordSet::HandleUndefine()
{
	if (!HandleDefined())
		return;

	dms_assert(DatabasePtr());
	Check(SQLFreeHandle(SQL_HANDLE_STMT, Handle()));
	Database().DelOpenRecordSet(this);
	m_HSTMT = SQL_NULL_HSTMT;
}

void TRecordSet::OpenImpl()
{
	dms_assert(!m_Active && !m_IsLocked);

	m_RecordNumber = NOT_DEFINED;	

	HandleDefine();
	SetAttributes();
	SetMonade()	;
	dms_assert(ExecType() >= rseRecordSet && ExecType() <= rseColumnInfo);

	switch (ExecType())
	{
		case rseRecordSet:
		case rseExecSQL:
			Check(SQLExecDirect(Handle(), (SQLCHAR*) (m_SQL.c_str()), m_SQL.ssize())); 
			break;

		case rseTableInfo:
			Check(SQLTables(Handle(), nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0)); 
			break;

		case rseColumnInfo:
			Check(SQLColumns(Handle(), nullptr, 0, nullptr, 0, (SQLCHAR*) (m_SQL.c_str()), SQL_NTS, nullptr, 0)); 
			break;
	}
	m_Active = true;
}

void TRecordSet::OpenLocked()
{
	if (!Active())
	{
		OpenImpl();
		// stuff that was done again after each LockThis if it wasn't active
		Columns().Define(this);
		if (ExecType() != rseExecSQL)
			InitBEOF();
	}
	LockThis();
}

bool TRecordSet::LockThis()
{
	dms_assert(m_Active || !m_IsLocked);
	if (!Active())
		return false;
	m_IsLocked = true;
	return true;
}

void TRecordSet::UnLockThis()
{
	dms_assert(m_Active || m_IsLocked);
	m_IsLocked = false;
}

void TRecordSet::Close()
{ 
	dms_assert( !IsLocked() );

	ResetMonade();
	if (Active())
	{

		if ( m_ExecType != rseExecSQL)
			Check(SQLCloseCursor(Handle()));

		m_Active = false;
		Columns().UnBind();

	}
	HandleUndefine(); // Calls DelOpenRecordSet
}

/*************************************************************

  Description of finite state machine of recordset navigation
	-----------------------------------------------------------

	legend          result
	--------        ------
	p	prior        true = m_FetchedCount > 0
	n	next
	f	first
	l	last

	state	EOF	BOF		state transitions   at result
	---------------		-----------------   ---------
	0		0		0			0 -> 0	p, n, f, l true
	1		0		1			0 -> 1	p, l       false
	2		1		0			0 -> 2	n, f       false
	3		1		1             
   		             	1 -> 0	n, f, l    true 
								1 -> 1   p, l       false
								1 -> 2   n, f       false
								1 -> ERR	p          true

   		             	2 -> 0	p, f, l    true 
								2 -> 1   p, l       false
								2 -> 2   n, f       false
								2 -> ERR	n          true

                        3 -> ERR: no state transistion can lead to this state.
To summarize (Colmogoff):
  newState := (oldState == 3)
     ? ERR
	  : result 
			? (old, trans) in ((1, p), (2,n)) ? ERR : 0
			: trans in 
				?(p, l): 1
				?(n, f): 2
Without error trapping:
  newState := 
	   result 
			? 0
			: trans in 
				?(p, l): 1
				?(n, f): 2

Separating states:
	BOF := lastReadError && trans = (p,l)
	EOF := lastReadError && trans = (n,f)

Stating state before first next: 1: BOF, RecordNumber = -1

*************************************************************/

void	TRecordSet::InitBEOF()
{
	m_BeginOfFile	=	true;
	m_EndOfFile		=	false;
}

void	TRecordSet::SetEOF()
{
	dms_assert(!(m_BeginOfFile && m_EndOfFile));
	bool lastFetchError = FrameElementNumber() >= SQLINTEGER( m_FetchedCount ); // FrameElementNumber can be negative
	m_BeginOfFile = false;
	m_EndOfFile   = lastFetchError;
}

void TRecordSet::Rewind()
{
	if (Active())
	{
		if (m_BeginOfFile)
			return;
		if (m_CursorType != SQL_CURSOR_FORWARD_ONLY)
		{
			Check(SQLFetchScroll(Handle(), SQL_FETCH_ABSOLUTE, 0) );
			LockThis();
			return;
		}
		if (IsLocked())
			UnLockThis();
		Close();
	}
	OpenLocked();
}

void TRecordSet::Next()
{ 
	dms_assert(Active());

	if (m_EndOfFile)
		return;

	SQLRETURN ret         = SQL_SUCCESS;
	long      framenumber = FrameNumber();
	
	m_RecordNumber++;
	if (FrameNumber() != framenumber)
	{
		m_FetchedCount = 0; // written by SQLFetch
		ret            = SQLFetch(Handle()); 
	}
	SetEOF();

	Check(ret);	
}

bool	TRecordSet::Next(UInt32 forwardCount)
{ 
	dms_assert(FrameSize() > 0);
	while (forwardCount && !m_EndOfFile)
	{
		Int32 firstRecNrOutFrame = Max<Int32>(Int32(FrameNumber()) * Int32(FrameSize()) + Int32(m_FetchedCount), Int32(0));
		dms_assert(firstRecNrOutFrame > m_RecordNumber); // or else it would have been EOF

		UInt32 nrHarmlessSkips = firstRecNrOutFrame - m_RecordNumber - 1;
		m_RecordNumber += nrHarmlessSkips;
		forwardCount   -= nrHarmlessSkips;
		if (forwardCount)
		{
			Next();
			forwardCount--;
		}
	}
	return true;
}

SQLSMALLINT TRecordSet::ColumnCount()
{ 
	if (m_ColumnCount != NOT_DEFINED)
		return m_ColumnCount;
	if (ExecType() == rseExecSQL)
		return (m_ColumnCount = 0);

	dms_assert(Active());

	SQLSMALLINT count; 
	Check(SQLNumResultCols(Handle(), &count));
	m_ColumnCount = count;
	return m_ColumnCount;
}

UInt32 TRecordSet::RecordCount()
{
	if (m_RecordCount != NOT_DEFINED)
		return m_RecordCount;

	if (m_ExecType == rseExecSQL)
		return (m_RecordCount = 0);

	if (m_ExecType == rseRecordSet)
		return (m_RecordCount = GetRecordCount(m_Database, m_SQL.c_str()));

	dms_assert( m_ExecType == rseTableInfo || m_ExecType == rseColumnInfo);
	dms_assert( m_ExternalBuffers.size() == 0 );
	dms_assert( m_FrameSize == 1 );

	TRecordSetOpenLock openLock(this);

	SQLINTEGER recnum =	RecordNumber();
	unsigned long
		minimum	=	0,
		maximum	=  16; // skips 4 silly seeks when bigger 

	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_RETRIEVE_DATA, (SQLPOINTER) SQL_RD_OFF, 0));

	while (maximum != (1 << (8 * sizeof(maximum)-1)) && SQLFetchScroll(Handle(), SQL_FETCH_ABSOLUTE, maximum) != SQL_NO_DATA)
		maximum *= 2;

	while (minimum + 1 < maximum)
	{
		unsigned long	half	=	(minimum + maximum) / 2;
		
		if (SQLFetchScroll(Handle(), SQL_FETCH_ABSOLUTE, half) == SQL_NO_DATA)
			maximum	=	half;
		else
			minimum	=	half;
	}

	Check(SQLSetStmtAttr(Handle(), SQL_ATTR_RETRIEVE_DATA, (SQLPOINTER) SQL_RD_ON, 0));

	SQLFetchScroll(Handle(), SQL_FETCH_ABSOLUTE, recnum + 1);

	return (m_RecordCount = minimum);
}

bool TRecordSet::UnbindAllInternal()
{
	if (Active() && ! m_UnbindAllInternal)
	{
		if(ColumnCount() >= 0)
		for (UInt32 i = ColumnCount(); i; --i)
		{
			if (i >= m_ExternalBuffers.size() || !m_ExternalBuffers[i].Defined())
				if (!Unbind(i))
					return false;
		}
	}
	m_UnbindAllInternal = true;
	return true;
}

bool TRecordSet::BindExternal(SQLUSMALLINT colindex, SQLPOINTER buf, SQLLEN buflen)
{
	if	(Active() && !Columns().BindExternal(colindex, buf, buflen, this))
		return false;

	m_ExternalBuffers.resize(Max<SQLSMALLINT>(colindex, SQLUSMALLINT(m_ExternalBuffers.size())));
	m_ExternalBuffers[colindex - 1].Init(colindex, buf, buflen);
	dms_assert(IsExternalFrameSizeOK());
	return true;
}

bool	TRecordSet::IsExternalFrameSizeOK() const
{
	for (UInt32 i = 0, n = Min<UInt32>(m_ExternalBuffers.size(), m_Columns.size()); i!=n; ++i) 
	{
		if (m_ExternalBuffers[i].DefinedBind())
		{
			int	framesize =	m_ExternalBuffers[i].m_BufferLength / m_Columns[i].ElementSize();

			if (framesize != FrameSize())
				return	false;
		}
	}
	return true;
}

CharPtr TRecordSet::ExecTypeName() const
{
	switch (m_ExecType) {
		case rseRecordSet:  return "OpenSQL";
		case rseExecSQL:    return "ExecSQL";
		case rseTableInfo:  return "TableInfo";
		case rseColumnInfo: return "ColumnInfo";
	}
	return "Unknown RecordSetType";
}

SQLINTEGER TRecordSet::StatementAttributeValue(const SQLINTEGER attribute)
{
	SQLINTEGER attributevalue, attributevaluelen;
	Check(SQLGetStmtAttr(Handle(), attribute, (SQLPOINTER) &attributevalue, sizeof(attributevalue), &attributevaluelen));
	return attributevalue;
}

SQLLEN TRecordSet::ColumnAttrValueNumeric(const SQLUSMALLINT col, const SQLUSMALLINT attribute)
{
	SQLLEN result = 0;
	Check(SQLColAttributes(Handle(), col, attribute, nullptr, 0, nullptr, &result));
	return result;
}

SharedStr TRecordSet::ColumnAttrValueString(const SQLUSMALLINT col, const SQLUSMALLINT attribute)
{
	char result[MAXBUFLEN];
	SQLSMALLINT	attrValueLen;
	Check(SQLColAttributes(Handle(), col, attribute, result, MAXBUFLEN, &attrValueLen, nullptr));
	return SharedStr(result, result + attrValueLen);
}

void TRecordSet::Check(const SQLRETURN ret)	
{
	::Check(ret, SQL_HANDLE_STMT, Handle(), 0, this, 0);
}

/*****************************************************************************/
//								TRecordSetOpenLock
/*****************************************************************************/

TRecordSetOpenLock::TRecordSetOpenLock(TRecordSet* rs)
	:	m_RS(rs)
{
	dms_assert(rs);
	dms_assert(!rs->IsLocked()); // no refernece counting, so this will unlock anyway
	rs->Rewind();
}

TRecordSetOpenLock::~TRecordSetOpenLock()
{
	m_RS->UnLockThis();
}

