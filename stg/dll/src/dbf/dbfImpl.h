// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __STGIMPL_DBFIMPL_H
#define __STGIMPL_DBFIMPL_H

#include "ImplMain.h"

#include <vector>	// ColDescriptions
#include "stdio.h"	// m_FP

#include "FileResult.h"
#include "geo/BaseBounds.h"
#include "geo/iterrange.h"
#include "ptr/SharedStr.h"
#include "utl/Instantiate.h"

#include "FilePtrHandle.h"

/*****************************************************************************/
//									DEFINES
/*****************************************************************************/

const UInt32 DBF_COLNAME_SIZE = 10;
#define	INSTANTIATE_DATATYPES	INSTANTIATE_NUM_ELEM INSTANTIATE_OTHER

/*****************************************************************************/
//									TYPES
/*****************************************************************************/

// next type is enum of the DB III PLUS types
enum TDbfType { dtNotDefined, dtCharacter, dtDate, dtNumeric, dtLogical, dtMemo };

// wrapper for columnparameters
struct DbfColDescription
{
	DbfColDescription()
		:	m_DbfType( dtNotDefined)
		,	m_Offset(0)
		,	m_Length(0)
		,	m_DecimalCount(0)
	{}

	SharedStr m_Name;
	TDbfType  m_DbfType; 
	UInt32    m_Offset;
	UInt8     m_Length,
	          m_DecimalCount;
};

template<class T> class	DbfImplStub;

typedef SharedStr String;

class DbfImpl : private FilePtrHandle
{
#define INSTANTIATE(T) 	friend class	 DbfImplStub<T>;
	INSTANTIATE_DATATYPES
#undef INSTANTIATE

public:
	STGIMPL_CALL  DbfImpl();
	STGIMPL_CALL ~DbfImpl();

	// read/write functions
	STGIMPL_CALL FileResult Open(WeakStr filename, FileCreationMode filemode);
	FileResult OpenForRead  (WeakStr filename) { return Open(filename, FCM_OpenReadOnly); }
	STGIMPL_CALL FileResult OpenForAppend(WeakStr filename);
	STGIMPL_CALL void Close();
	STGIMPL_CALL bool CreateFromSource(WeakStr filename, const DbfImpl& srcDbf);
	STGIMPL_CALL bool GoBegin();
	STGIMPL_CALL bool GoEnd();
	STGIMPL_CALL bool GoTo(UInt32 recNo);

	STGIMPL_CALL bool ReadRecord  (void* buffer);
	STGIMPL_CALL bool AppendRecord(const void* buffer);
	STGIMPL_CALL bool WriteHeader();                            // write to dbf file
	STGIMPL_CALL bool IsAt(UInt32 recNo) const;

	// info functions
	STGIMPL_CALL SizeT        ColumnCount()	{ return m_ColumnDescriptions.size(); }
	STGIMPL_CALL UInt32       ColumnNameToIndex (CharPtr name);
	STGIMPL_CALL CharPtr      ColumnIndexToName (UInt32 columnindex);
	STGIMPL_CALL ValueClassID ColumnType        (UInt32 columnindex);
	STGIMPL_CALL UInt8        ColumnWidth       (UInt32 columnindex);
	STGIMPL_CALL UInt8        ColumnDecimalCount(UInt32 columnindex);
	STGIMPL_CALL UInt32       RecordLength();
	STGIMPL_CALL UInt32       RecordCount() { return m_RecordCount; }
	
	STGIMPL_CALL bool         SetRecordCount(UInt32 cnt, WeakStr filename);

private:
	std::vector<DbfColDescription> m_ColumnDescriptions; // dbf content 

	// header attributes, set when dbf file header is read
	UInt8   m_DbfVersion;
	UInt16	m_HeaderSize,											// size of header lines in .txt (nr of bytes)
	        m_RecordSize;
	UInt32	m_RecordCount;											// number of records

	bool    ColumnIndexDefined(UInt32 index) const;
	bool    RecordIndexDefined(UInt32 index) const;
	void    ColumnDescriptionAppend(CharPtr columnname,  TDbfType dbftype, UInt8 len, UInt8 deccount);
	void    ColumnDescriptionReplace(UInt32 columnindex, TDbfType dbftype, UInt8 len, UInt8 deccount);

	// helper functions
	FileResult ReadHeader();                             // read from dbf file
	FileResult Create(WeakStr filename);
	void    Clear();                                  // reset all
	UInt32	ColumnOffset(UInt32 columnindex) const;
	UInt32  ActualPosition(UInt32 recordindex) const;
	UInt32  ActualPosition(UInt32 recordindex, UInt32 columnindex) const;

	void    ReadDataElement (      void* data, UInt32 recordindex, UInt32 columnindex, ValueClassID vc, CharPtr formatspec);
	bool    WriteDataElement(const void* data, UInt32 recordindex, UInt32 columnindex, ValueClassID vc, CharPtr formatspec, UInt8 len);
};

/*****************************************************************************/
//										DbfImplSub
/*****************************************************************************/

template<class T> class	DbfImplStub
{
public:
	typedef typename sequence_traits<T>::seq_t  VecType;
	typedef typename sequence_traits<T>::cseq_t CVecType;
	FileResult m_Result;

	STGIMPL_CALL DbfImplStub(DbfImpl* p, VecType vec, CharPtr columnname, ValueClassID vc);
	STGIMPL_CALL DbfImplStub(DbfImpl* p, WeakStr filename, CVecType vec, CharPtr columnname, ValueClassID vc);

private:
	DbfImpl* m_DbfImpl;

	FileResult ReadData(VecType vec, CharPtr columnname, ValueClassID vc);
	FileResult WriteData(WeakStr filename, CVecType vec, CharPtr columnname, ValueClassID vc);
	FileResult WriteDataOverwrite(WeakStr filename, CVecType vec, UInt32 columnindex, ValueClassID vc);
	FileResult WriteDataAppend(WeakStr filename, CVecType vec, CharPtr columnname, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount);
	FileResult WriteDataReplace(WeakStr filename, CVecType vec, UInt32 columnindex, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount);

	void	DbfTypeInfo(CVecType vec, ValueClassID vc, TDbfType& dbftype, UInt8& len, UInt8& deccount);
	bool	TypeResolution(TDbfType dbftype, UInt8 len, UInt8 deccount, UInt32 columnindex);
};

#endif // __STGIMPL_DBFIMPL_H
