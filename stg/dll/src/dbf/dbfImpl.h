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

#ifndef __STGIMPL_DBFIMPL_H
#define __STGIMPL_DBFIMPL_H

#include "ImplMain.h"

#include <vector>		// ColDescriptions
#include "stdio.h"	// m_FP

#include "geo/BaseBounds.h"
#include "geo/IterRange.h"
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
	STGIMPL_CALL bool Open(WeakStr filename, SafeFileWriterArray* sfwa, FileCreationMode filemode);
	             bool OpenForRead  (WeakStr filename, SafeFileWriterArray* sfwa) { return Open(filename, sfwa, FCM_OpenReadOnly); }
	             bool OpenForAppend(WeakStr filename, SafeFileWriterArray* sfwa) { return Open(filename, sfwa, FCM_OpenRwGrowable) && GoEnd(); }
//REMOVE 	STGIMPL_CALL bool ReadData(void *data, CharPtr columnname, ValueClassID vc);
//REMOVE	STGIMPL_CALL bool WriteData(WeakStr filename, SafeFileWriterArray* sfwa, const void *data, CharPtr columnname, ValueClassID vc);
	STGIMPL_CALL void Close();
	STGIMPL_CALL bool CreateFromSource(WeakStr filename, SafeFileWriterArray* sfwa, const DbfImpl& srcDbf);
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
	
	STGIMPL_CALL bool         SetRecordCount(UInt32 cnt, WeakStr filename, SafeFileWriterArray* sfwa);

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
	bool    ReadHeader();                             // read from dbf file
	bool    Create(WeakStr filename, SafeFileWriterArray* sfwa);
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
	bool		m_Result;

	STGIMPL_CALL DbfImplStub(DbfImpl* p, VecType vec, CharPtr columnname, ValueClassID vc);
	STGIMPL_CALL DbfImplStub(DbfImpl* p, WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, CharPtr columnname, ValueClassID vc);

private:
	DbfImpl* m_DbfImpl;

	bool	ReadData(VecType vec, CharPtr columnname, ValueClassID vc);
	bool	WriteData(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, CharPtr columnname, ValueClassID vc);
	bool	WriteDataOverwrite(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, UInt32 columnindex, ValueClassID vc);
	bool	WriteDataAppend(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, CharPtr columnname, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount);
	bool	WriteDataReplace(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, UInt32 columnindex, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount);

	void	DbfTypeInfo(CVecType vec, ValueClassID vc, TDbfType& dbftype, UInt8& len, UInt8& deccount);
	bool	TypeResolution(TDbfType dbftype, UInt8 len, UInt8 deccount, UInt32 columnindex);
};

#endif // __STGIMPL_DBFIMPL_H
