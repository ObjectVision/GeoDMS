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

/*****************************************************************************/
// Implementation of non-DMS based class DbfImpl. This class is used
// by DbfStorageManager to read and write dbf tables.
/*****************************************************************************/

#include "StoragePch.h"
#include "ImplMain.h"
#pragma hdrstop

#include "DbfImpl.h"

#include "dbg/debug.h"
#include "geo/BaseBounds.h"
#include "geo/Conversions.h"
#include "geo/SequenceArray.h"
#include "geo/StringBounds.h"
#include "mci/ValueClassID.h"
#include "ser/FormattedStream.h"
#include "ser/ReadValue.h"
#include "ser/SafeFileWriter.h"
#include "set/BitVector.h"
#include "utl/Environment.h"
#include "utl/IncrementalLock.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"
#include "utl/StringFunc.h"
#include "Parallel.h"

#include <string.h>
#include <share.h>
#include <time.h>
#include <minmax.h>
#include <math.h>

#include <boost/utility.hpp>

/*****************************************************************************/
//										DEFINES
/*****************************************************************************/

#define MG_DEBUG_DBF false

#define	DBF_HEADER_BLOCK_SIZE	32

#if defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
#	pragma warning( disable :1563) // taking the address of a temporary

#endif

/*****************************************************************************/
//										FUNCTIONS
/*****************************************************************************/

TDbfType	DbfTypeCharToDbfType(const UInt8 dbftype)
{
	switch (dbftype)
	{
		case	'C'	:	return	dtCharacter;
		case	'D'	:	return	dtDate;
		case  'F'   :
		case	'N'	:	return	dtNumeric;
		case	'L'	:	return	dtLogical;
		case	'M'	:	return	dtMemo;
		default		:	return	dtNotDefined;
	}
}

UInt8 DbfTypeToDbfTypeChar(const TDbfType dbftype)
{
	switch (dbftype)
	{
		case	dtCharacter	:	return	'C';
		case	dtDate		:	return	'D';
		case	dtNumeric	:	return	'N';
		case	dtLogical	:	return	'L';
		case	dtMemo		:	return	'M';
		default				:	return	32;
	}
}

ValueClassID DbfTypeToValueClassID(const TDbfType dt, const UInt8 len, const UInt8 dc)
{
	switch (dt)
	{
		case dtNumeric:
			return (dc > 0)
				?	(len > 6 ? VT_Float64 : VT_Float32)
				:	(len > 4) 
					?	VT_Int32 
					:	(len > 2)
						?	VT_Int16
						:	VT_Int8;

		case dtCharacter:
		case dtDate:
			return VT_SharedStr;

		case dtLogical:
			return VT_Bool;
	}
	return VT_Unknown;
}

TDbfType	ValueClassIDToDbfType(const ValueClassID vc)
{
	switch (vc)
	{
#define INSTANTIATE(T) case VT_##T:
		INSTANTIATE_NUM_ORG
#undef INSTANTIATE
			return dtNumeric; 
		case VT_SharedStr: return dtCharacter;
		case VT_Bool  : return dtLogical;
		default       : return dtNotDefined;
	} // switch
} // DbfTypeToValueClassID

//template <class T> T		Abs(const T& t)	{ return t < 0 ? 0 - t : t; }

void DecideLenDecCount(sequence_traits<SharedStr>::cseq_t vec, UInt8& len, UInt8& deccount)
{
	len	=	deccount	=	0;
	sequence_traits<SharedStr>::container_type::const_iterator
		i = vec.begin(),
		e = vec.end();
	while (i!=e)
		MakeMax(len, UInt8((*i++).size()));
}

void DecideLenDecCount(sequence_traits<Bool>::cseq_t vec, UInt8& len, UInt8& deccount)
{
	len		 = 1;
	deccount = 0;
}

void DecideLenDecCount(sequence_traits<UInt8 >::cseq_t vec, UInt8& len, UInt8& deccount) { len = 3; deccount = 0; }
void DecideLenDecCount(sequence_traits< Int8 >::cseq_t vec, UInt8& len, UInt8& deccount) { len = 4; deccount = 0; }
void DecideLenDecCount(sequence_traits<UInt16>::cseq_t vec, UInt8& len, UInt8& deccount) { len = 5; deccount = 0; }
void DecideLenDecCount(sequence_traits< Int16>::cseq_t vec, UInt8& len, UInt8& deccount) { len = 6; deccount = 0; }
void DecideLenDecCount(sequence_traits<UInt32>::cseq_t vec, UInt8& len, UInt8& deccount) { len =10; deccount = 0; }
void DecideLenDecCount(sequence_traits< Int32>::cseq_t vec, UInt8& len, UInt8& deccount) { len =11; deccount = 0; }
void DecideLenDecCount(sequence_traits<UInt64>::cseq_t vec, UInt8& len, UInt8& deccount) { len =20; deccount = 0; }
void DecideLenDecCount(sequence_traits< Int64>::cseq_t vec, UInt8& len, UInt8& deccount) { len =20; deccount = 0; }

template<typename SecArr> UInt8 DecideDecCount(const SecArr& vec, UInt8 len)
{
	Float64	maxval =	1; // we don't want negative logs here
	typename SecArr::const_iterator
		i = vec.begin(),
		e = vec.end();
	while (i!=e)
		MakeMax(maxval, Float64(Abs(*i++)));
	UInt8 reqIncSeparator = UInt8(log10(maxval))+3;
	if (reqIncSeparator >=len)
		return 0;
	return len - reqIncSeparator;
} // DecideDecCount

void DecideLenDecCount(sequence_traits<Float32>::cseq_t vec, UInt8& len, UInt8& deccount) { deccount = DecideDecCount(vec, len = 11); }
void DecideLenDecCount(sequence_traits<Float64>::cseq_t vec, UInt8& len, UInt8& deccount) { deccount = DecideDecCount(vec, len = 21); }
#if defined(DMS_TM_HAS_FLOAT80)
void DecideLenDecCount(sequence_traits<Float80>::cseq_t vec, UInt8& len, UInt8& deccount) { deccount = DecideDecCount(vec, len = 25); }
#endif

char* RightTrimEos(char *s, char * e)
{
	int c=0;
	while (s!=e && *s)
		if (*s++ == 32) 
			++c;
		else
			c = 0;
	return s-c;
}

char* RightTrim(char *s)
{
	for (int i = StrLen(s) - 1; i >= 0 && s[i] == 32; --i)
		s[i] = 0;
	return s;
} // RightTrim


void CommitFile(WeakStr srcName, SafeFileWriterArray* sfwa, WeakStr tmpFile)
{
	SharedStr dosFileName = ConvertDmsFileName( sfwa->GetWorkingFileName(srcName, FCM_CreateAlways) );

	auto r = remove(dosFileName.c_str());
	r = rename(ConvertDmsFileName(sfwa->GetWorkingFileName(tmpFile, FCM_CreateAlways)).c_str(), dosFileName.c_str());
	if (r)
		throwErrorF("std", "%d (%s) in CommitFile.rename working filename to intended filename", r, strerror(r));
}

/*****************************************************************************/
//										DbfImpl
/*****************************************************************************/

// Initialise privates
DbfImpl::DbfImpl()
{
	Clear();
} // DbfImpl

// Get rid of open files
DbfImpl::~DbfImpl()
{
	Close();
	Clear();
} // ~DbfImpl

// Reset 
void DbfImpl::Clear()
{
	// refresh, file is assumed to be closed
	CloseFH();
	m_DbfVersion  =  0;
	m_HeaderSize  =  0;
	m_RecordCount = -1;
	m_RecordSize  =	 0;
	m_ColumnDescriptions.resize(0);
} // Clear

// Opens the indicated (existing) dbf file for reading; the header is read.
bool DbfImpl::Open(WeakStr filename, SafeFileWriterArray* sfwa, FileCreationMode filemode)
{
	DBG_START("DbfImpl::Open", filename.c_str(), MG_DEBUG_DBF);
	
	Close();
	
	return 
		OpenFH(filename, sfwa, filemode, false, NR_PAGES_DATFILE)
	&&	ReadHeader();
} // Open

bool DbfImpl::CreateFromSource(WeakStr filename, SafeFileWriterArray* sfwa, const DbfImpl& srcDbf)
{
	if (!Create(filename, sfwa))
		return false;
	m_ColumnDescriptions = srcDbf.m_ColumnDescriptions;
	m_RecordCount = 0;
	WriteHeader();
	return true;
}

bool DbfImpl::GoBegin()
{
	dms_assert(GetFP());
	return fseek(GetFP(), ActualPosition(0), SEEK_SET) == 0;
}

bool DbfImpl::GoEnd()
{
	dms_assert(GetFP());
	return fseek(GetFP(), ActualPosition(m_RecordCount), SEEK_SET) == 0;
}

bool DbfImpl::GoTo(UInt32 recNo)
{
	dms_assert(GetFP());
	return fseek(GetFP(), ActualPosition(recNo), SEEK_SET) == 0;
}

bool DbfImpl::IsAt(UInt32 recNo) const
{
	return ftell(GetFP()) == ActualPosition(recNo);
}

bool DbfImpl::ReadRecord  (void* buffer)
{
	dms_assert(GetFP());

	return fread(buffer, m_RecordSize, 1, GetFP()) == 1;
}

bool DbfImpl::AppendRecord(const void* buffer)
{
	dms_assert(GetFP());

	dms_assert(ftell(GetFP()) == ActualPosition(m_RecordCount));
	if (fwrite(buffer, m_RecordSize, 1, GetFP()) != 1)
		return false;
	++m_RecordCount;
	return true;
}

bool DbfImpl::Create(WeakStr filename, SafeFileWriterArray* sfwa)
{
	DBG_START("DbfImpl", "Create", MG_DEBUG_DBF);
	
	dms_assert(!IsOpen());

	return OpenFH(filename, sfwa, FCM_CreateAlways, false, NR_PAGES_DATFILE);
} // Create


void DbfImpl::Close()
{
	DBG_START("DbfImpl", "Close", MG_DEBUG_DBF);

	CloseFH();

	Clear();
} // Close

bool	DbfImpl::ColumnIndexDefined(UInt32 columnindex) const
{
	return columnindex < m_ColumnDescriptions.size();
} // ColumnIndexDefined

bool	DbfImpl::RecordIndexDefined(UInt32 pos) const
{
	return pos < m_RecordCount; 
} // RecordIndexDefined

UInt32 DbfImpl::ColumnOffset(UInt32 columnindex) const
{
	MGD_PRECONDITION( ColumnIndexDefined(columnindex) );
	return m_ColumnDescriptions[columnindex].m_Offset;
}

UInt32 DbfImpl::ActualPosition(UInt32 recordindex) const
{
	MGD_PRECONDITION(!recordindex || RecordIndexDefined(recordindex-1));
	return m_HeaderSize + recordindex * m_RecordSize;
} // ActualPosition

UInt32 DbfImpl::ActualPosition(UInt32 recordindex, UInt32 columnindex) const
{
	MGD_PRECONDITION(ColumnIndexDefined(columnindex));
	return ActualPosition(recordindex) + ColumnOffset(columnindex);
} // ActualPosition

ValueClassID DbfImpl::ColumnType(UInt32 columnindex)
{
	MGD_PRECONDITION( ColumnIndexDefined(columnindex) );
	return	DbfTypeToValueClassID(
		m_ColumnDescriptions[columnindex].m_DbfType, 
		m_ColumnDescriptions[columnindex].m_Length, 
		m_ColumnDescriptions[columnindex].m_DecimalCount
	);
} // ColumnType

CharPtr DbfImpl::ColumnIndexToName(UInt32 columnindex)
{
	MGD_PRECONDITION( ColumnIndexDefined(columnindex) );
	return m_ColumnDescriptions[columnindex].m_Name.c_str();
} // ColumnIndexToName

// Calculate column width based on offsets
UInt8 DbfImpl::ColumnWidth(UInt32 columnindex)
{
	MGD_PRECONDITION( ColumnIndexDefined(columnindex) );
	return m_ColumnDescriptions[columnindex].m_Length;
}

UInt8 DbfImpl::ColumnDecimalCount(UInt32 columnindex)
{
	MGD_PRECONDITION( ColumnIndexDefined(columnindex) );
	return	m_ColumnDescriptions[columnindex].m_DecimalCount;
}

UInt32 DbfImpl::RecordLength()
{
	UInt32 nrColumns = m_ColumnDescriptions.size();
	return nrColumns
		?	m_ColumnDescriptions.back().m_Offset + m_ColumnDescriptions.back().m_Length 
		:	0; 
}

SharedStr NameToDbfColumnName(CharPtr src)
{
	return SharedStr(src, src+ Min<UInt32>(StrLen(src), DBF_COLNAME_SIZE));
} // NameToDbfColumnName

UInt32 DbfImpl::ColumnNameToIndex(CharPtr name)
{
	SharedStr s = NameToDbfColumnName(name);

	for (UInt32 index = 0; ColumnIndexDefined(index); index++) 
		if (_stricmp(s.c_str(), m_ColumnDescriptions[index].m_Name.c_str()) == 0)	
			return index;
	return UNDEFINED_VALUE(UInt32);
} // ColumnNameToIndex

void DbfImpl::ColumnDescriptionAppend(CharPtr columnName, TDbfType dbftype, UInt8 len, UInt8 deccount)
{
	long	columnindex	=	m_ColumnDescriptions.size(),
			offset		=	RecordLength() > 0 ? RecordLength() : 1;

	m_ColumnDescriptions.resize(m_ColumnDescriptions.size() + 1);
	DbfColDescription& colDescr = m_ColumnDescriptions[columnindex];

	colDescr.m_Name         = columnName;
	colDescr.m_DbfType      = dbftype;
	colDescr.m_Offset       = offset;
	colDescr.m_Length       = len;
	colDescr.m_DecimalCount = deccount;
} // ColumnDescriptionAppend

void DbfImpl::ColumnDescriptionReplace(UInt32 columnindex, TDbfType dbftype, UInt8 len, UInt8 deccount)
{
	dms_assert(columnindex < m_ColumnDescriptions.size());
	std::vector<DbfColDescription>::iterator
		colDescrPtr = m_ColumnDescriptions.begin() + columnindex,
		colDescrEnd = m_ColumnDescriptions.end();

	long deltaLength = long(len) - long(colDescrPtr->m_Length);

	colDescrPtr->m_DbfType      = dbftype;
	colDescrPtr->m_Length       = len;
	colDescrPtr->m_DecimalCount = deccount;

	// adjust subsequent offsets with deltaLength
	while (++colDescrPtr != colDescrEnd)
		colDescrPtr->m_Offset += deltaLength;
} // ColumnDescriptionReplace

bool DbfImpl::ReadHeader()
{
	DBG_START("DbfImpl", "ReadHeader", MG_DEBUG_DBF);

	// Must be open
	if (!IsOpen()) 
		return false;

	// 1. read general header info
	FILE* fp = GetFP();
	m_HeaderSize = m_RecordCount = m_RecordSize	=	0;
	fseek(fp, 0, 0);
	fread(&m_DbfVersion, sizeof(m_DbfVersion), 1, fp);
	fseek(fp, 4, 0);
	fread(&m_RecordCount, sizeof(m_RecordCount), 1, fp);
	fread(&m_HeaderSize, sizeof(m_HeaderSize), 1, fp);
	fread(&m_RecordSize, sizeof(m_RecordSize), 1, fp);
	DBG_TRACE(("m_HeaderSize  : %d", m_HeaderSize));
	DBG_TRACE(("m_RecordSize  : %d", m_RecordSize));
	DBG_TRACE(("m_RecordCount : %d", m_RecordCount));

	// 2. read column info
	MG_CHECK(m_HeaderSize > DBF_HEADER_BLOCK_SIZE);
	MG_CHECK(m_RecordSize > 0);
	MG_CHECK(((m_HeaderSize - 1) % DBF_HEADER_BLOCK_SIZE) == 0);

	if (m_HeaderSize <= DBF_HEADER_BLOCK_SIZE)
		return false;
	auto nrColumns = m_HeaderSize;
	--nrColumns; assert(nrColumns >= DBF_HEADER_BLOCK_SIZE);
	nrColumns /= DBF_HEADER_BLOCK_SIZE; assert(nrColumns >= 1);
	--nrColumns;
	m_ColumnDescriptions.resize(nrColumns);
	MG_CHECK(m_ColumnDescriptions.size() > 0);

	int	offset	=	1;

	for (UInt32 i = 0, n = ColumnCount(); i != n; ++i)
	{
		fseek(fp, (i + 1) * DBF_HEADER_BLOCK_SIZE, 0);

		// read name
		char	name[DBF_COLNAME_SIZE + 1];
		name[0]						=	0;
		fread(name, DBF_COLNAME_SIZE + 1, 1, fp);
		name[DBF_COLNAME_SIZE]	=	0;
		m_ColumnDescriptions[i].m_Name = name;

		// read type
		UInt8		dbftype;
		fread(&dbftype, sizeof(dbftype), 1, fp);
		m_ColumnDescriptions[i].m_DbfType = DbfTypeCharToDbfType(dbftype);

		// read length
		m_ColumnDescriptions[i].m_Offset = offset;
		fseek(fp, ftell(fp) + 4, 0);
		fread(&m_ColumnDescriptions[i].m_Length, sizeof(m_ColumnDescriptions[i].m_Length), 1, fp);
		offset	+=	m_ColumnDescriptions[i].m_Length;

		// read decimal count
		fread(&m_ColumnDescriptions[i].m_DecimalCount, sizeof(m_ColumnDescriptions[i].m_DecimalCount), 1, fp);
	}
	MG_CHECK(m_RecordSize == RecordLength());

	GoBegin();
	return true;
} // ReadHeader

bool WriteByte(FILE *fp, char byte)
{
	dms_assert(fp != NULL); // PRECONDITION

	return fwrite(&byte, sizeof(byte), 1, fp) == 1;
} // WriteByte

bool WriteBytes(FILE *fp, char byte, int count)
{
	dms_assert(fp != NULL); // PRECONDITION

	if (count >= 4)
	{
		UInt32 dword = (byte << 8) | byte;
		dword |= dword << 16;
		for  (; count >= 4; count -= 4)
			if (fwrite(&dword, sizeof(UInt32), 1, fp) != 1)
				return false;
	}
	while (count--)
		if (!WriteByte(fp, byte))
			return false;
	
	return true;
} // WriteBytes

bool DbfImpl::SetRecordCount(UInt32 cnt, WeakStr filename, SafeFileWriterArray* sfwa)
{
	dms_assert(!IsOpen());
	if (! OpenForRead(filename, sfwa))
		return false;

	if (m_RecordCount == cnt)
		return true;
	CloseFH();

	if (! Open(filename, sfwa, FCM_OpenRwFixed))
		return false;

	m_RecordCount = cnt;
	bool result = WriteHeader();
	CloseFH();
	return result;
}

bool DbfImpl::WriteHeader()
{
	DBG_START("DbfImpl", "WriteHeader", MG_DEBUG_DBF);

	// 1. write general header info
	m_DbfVersion = 3;
	m_HeaderSize = DBF_HEADER_BLOCK_SIZE * (ColumnCount() + 1) + 1;
	m_RecordSize = RecordLength();

	time_t		ltime;
	struct tm	*today;
	time(&ltime);
	today = gmtime(&ltime);
	today->tm_mon++;

//	DEBUG, REMOVE, NYI: Fixed date to support current working of tst/Storage/cfg
	today->tm_year = 109;
	today->tm_mon  = 9;
	today->tm_mday = 9;
//	END DEBUG

	FILE* fp = GetFP();
	rewind(fp);
	fwrite(&m_DbfVersion, sizeof(m_DbfVersion), 1, fp);
	fwrite(&today->tm_year, 1, 1, fp);
	fwrite(&today->tm_mon , 1, 1, fp);
	fwrite(&today->tm_mday, 1, 1, fp);
	fwrite(&m_RecordCount, sizeof(m_RecordCount), 1, fp);
	fwrite(&m_HeaderSize , sizeof(m_HeaderSize ), 1, fp);
	fwrite(&m_RecordSize , sizeof(m_RecordSize ), 1, fp);
	WriteBytes(fp, 0, 20);

	// 2. write column info
	for (UInt32 i = 0, n = ColumnCount(); i != n; i++)
	{
		// write name
		fwrite(m_ColumnDescriptions[i].m_Name.c_str(), m_ColumnDescriptions[i].m_Name.ssize(), 1, fp);
		WriteBytes(fp, 0, DBF_COLNAME_SIZE + 1 - m_ColumnDescriptions[i].m_Name.ssize());

		// write type
		UInt8		dbftype	=	DbfTypeToDbfTypeChar(m_ColumnDescriptions[i].m_DbfType);
		fwrite(&dbftype, sizeof(dbftype), 1, fp);

		WriteBytes(fp, 0, 4);

		// write length
		fwrite(&m_ColumnDescriptions[i].m_Length, sizeof(m_ColumnDescriptions[i].m_Length), 1, fp);

		// write decimal count
		fwrite(&m_ColumnDescriptions[i].m_DecimalCount, sizeof(m_ColumnDescriptions[i].m_DecimalCount), 1, fp);

		WriteBytes(fp, 0, 14);
	}

	WriteByte(fp, 0x0D);

	return true;
} // WriteHeader

void FormatSpecification(ValueClassID vc, UInt8 colwidth, UInt8 deccount, bool read, SharedStr& formatspec)
{
	char	s[10];

	formatspec	=	"%";
	sprintf(s, "%hu", colwidth);
	formatspec	+=	s;

	switch (vc)
	{
		case VT_SharedStr:
		case VT_Bool   : formatspec	+=	"c"; break;
		case VT_Int8   :
		case VT_Int16  : formatspec	+= "hd"; break;
		case VT_Int32  : formatspec	+= "d";  break;
		case VT_UInt16 : 
		case VT_UInt8  : formatspec	+= "hu"; break;
		case VT_UInt32 : formatspec	+= "u";  break;
		case VT_Float32:	
		case VT_Float64: 
			if (! read)
			{
				sprintf(s, "%hd", Int16(deccount));
				formatspec	+=	".";
				formatspec	+=	s;
			} 
			if (vc == VT_Float64)
				formatspec += "l";
			formatspec += "f";
			break;
	} // switch
} // FormatSpecification

Float64 ReadAsFloat64(CharPtr fieldBuffer, CharPtr filedBufferEnd)
{
	CharPtr iter = fieldBuffer;
	SkipSpace(iter, filedBufferEnd);
	Float64 tmp;
	if (ScanValue(tmp, char_scanner(iter, filedBufferEnd)))
	{
		SkipSpace(iter, filedBufferEnd);
		if (iter == filedBufferEnd)
			return tmp;
	}
	throwErrorF("DBF","unexpected character in parsing '%s' as numeric", SharedStr(fieldBuffer, filedBufferEnd).c_str());
}

template <typename T>
void ReadAsFloat64AndConvert(T* dataPtr, char* fieldBuffer, char* fieldBufferEnd)
{
	*dataPtr = Convert<T>(ReadAsFloat64(fieldBuffer, fieldBufferEnd));
}

void DbfImpl::ReadDataElement(void* data, UInt32 recordindex, UInt32 columnindex, ValueClassID vc, CharPtr formatspec)
{
	MGD_PRECONDITION(GetFP() != NULL);
	MGD_PRECONDITION(RecordIndexDefined(recordindex));
	MGD_PRECONDITION(ColumnIndexDefined(columnindex));

	UInt8 fieldSize = ColumnWidth(columnindex);

	dms_assert(fieldSize < 256);
	char fieldBuffer[256];

	fseek(GetFP(), ActualPosition(recordindex, columnindex), 0);
	fread(fieldBuffer, fieldSize, 1, GetFP());

//	fieldBuffer[fieldSize] = 0;

	switch (vc)
	{
		case VT_SharedStr:
		{
			CharPtr fieldBufferEnd = RightTrimEos(fieldBuffer, fieldBuffer+fieldSize);
			if ((fieldBufferEnd - fieldBuffer == 4) 
				&& (fieldBuffer[0] == 'n')
				&& (fieldBuffer[1] == 'u')
				&& (fieldBuffer[2] == 'l')
				&& (fieldBuffer[3] == 'l')
			)
				(* ( StringArray::reference * ) data).assign(Undefined());
			else
				(* ( StringArray::reference * ) data).assign(fieldBuffer, fieldBufferEnd);
			break;
		}
		case VT_Bool    :
			* (sequence_traits<bit_value<1> >::reference*) data	
				= *fieldBuffer == 'Y' || *fieldBuffer == 'y' || *fieldBuffer == 'T' || *fieldBuffer == 't' ? true : false;
			break;

		case VT_Int8    : ReadAsFloat64AndConvert(( Int8  *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_UInt8   : ReadAsFloat64AndConvert((UInt8  *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_Int16   : ReadAsFloat64AndConvert(( Int16 *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_UInt16  : ReadAsFloat64AndConvert((UInt16 *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_Int32   : ReadAsFloat64AndConvert(( Int32 *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_UInt32  : ReadAsFloat64AndConvert((UInt32 *) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_Float32 : ReadAsFloat64AndConvert((Float32*) data, fieldBuffer, fieldBuffer+fieldSize); break;
		case VT_Float64 : ReadAsFloat64AndConvert((Float64*) data, fieldBuffer, fieldBuffer+fieldSize); break;
	}
}

bool DbfImpl::WriteDataElement(const void *data, UInt32 recordindex, UInt32 columnindex, ValueClassID vc, CharPtr formatspec, UInt8 len)
{
	dms_assert(GetFP() != NULL );
	dms_assert(RecordIndexDefined(recordindex) );
	dms_assert(ColumnIndexDefined(columnindex) );

	const SizeT BUFFER_SIZE = 64;
	char buff[BUFFER_SIZE];
	CharPtrRange strRange("");
	switch (vc)
	{
		case VT_SharedStr:
			{
				StringArray::const_reference& s	= *(StringArray::const_reference*) data;
				if (s.IsDefined())
				{
					strRange = CharPtrRange(s.begin(), s.end()); 
				}
				else
				{
					strRange.first = UNDEFINED_VALUE_STRING;
					strRange.second = strRange.first + UNDEFINED_VALUE_STRING_LEN;
				}
			}
			break;
		case VT_Bool    : strRange.first = bit_value<1>(* (bit_sequence<1>::const_reference*) data) ? "T" : "F"; strRange.second = strRange.first + 1; break;
		case VT_Int8    : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec,  Int16(*( Int8*) data)); break;
		case VT_UInt8   : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, UInt16(*(UInt8*) data)); break;
		case VT_Int16   : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (Int16*) data);	break;
		case VT_UInt16  : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (UInt16*) data);	break;
		case VT_Int32   : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (Int32*) data);	break;
		case VT_UInt32  : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (UInt32*) data);	break;
		case VT_Float32 : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (Float32*) data);	break;
		case VT_Float64 : strRange = myFixedBufferAsCharPtrRange(buff, BUFFER_SIZE, formatspec, * (Float64*) data);	break;
		default         : return false;
	} // switch
	UInt32 sz = strRange.size();
	if (sz > len)
		throwErrorF("DBF", "Writing column %s of record %d failed because '%s' exceeds the field length %d",
			ColumnIndexToName(columnindex),
			recordindex,
			strRange.begin(),
			len
		);
	fwrite(strRange.begin(), sz, 1, GetFP());
	WriteBytes(GetFP(), 32, len - sz);
	return	true;
}

#define INSTANTIATE(T) template class DbfImplStub<T>;
INSTANTIATE_NUM_ORG
INSTANTIATE_OTHER
#undef INSTANTIATE

/* REMOVE
#define	READ_DATA_TYPE(type)	\
{ \
	DbfImplStub<type>	impstub(this, * (sequence_traits<type>::seq_t*) data, columnName, vc); \
	return	impstub.m_Result; \
} 

bool DbfImpl::ReadData(void* data, CharPtr columnName, ValueClassID vc)
{
	DBG_START("DbfImpl_ReadData", columnName, MG_DEBUG_DBF);

	switch (vc)
	{
#define INSTANTIATE(T) case VT_##T: READ_DATA_TYPE(T)
		INSTANTIATE_NUM_ORG
		INSTANTIATE_OTHER
#undef INSTANTIATE
		default			: return false;
	} // switch
} // ReadData

#define	WRITE_DATA_TYPE(type)	\
{ \
	DbfImplStub<type> impstub(this, filename, sfwa, * (sequence_traits<type>::cseq_t*) data, columnName, vc); \
	return	impstub.m_Result; \
} 

bool DbfImpl::WriteData(WeakStr filename, SafeFileWriterArray* sfwa, const void *data, CharPtr columnName, ValueClassID vc)
{
	DBG_START("DbfImpl_WriteData", columnName, MG_DEBUG_DBF);

	switch (vc)
	{
#define INSTANTIATE(T) case VT_##T	: WRITE_DATA_TYPE(T)	
		INSTANTIATE_NUM_ORG
		INSTANTIATE_OTHER
#undef INSTANTIATE
		default			:	return false;
	} // switch
} // WriteData
*/

/*****************************************************************************/
//										DbfImplStub
/*****************************************************************************/

template<class T> bool DbfImplStub<T>::ReadData(VecType vec, CharPtr columnName, ValueClassID vc)
{
	DBG_START("DbfImplStub", "ReadData", MG_DEBUG_DBF);

	 // BEGIN TODO, OPT: Move to caller(s)
	UInt32 columnindex = m_DbfImpl->ColumnNameToIndex(NameToDbfColumnName(columnName).c_str());

	if (! m_DbfImpl->ColumnIndexDefined(columnindex) )
		return false;

	SharedStr formatspec;
	FormatSpecification(vc, m_DbfImpl->ColumnWidth(columnindex), m_DbfImpl->ColumnDecimalCount(columnindex), true, formatspec);
	CharPtr formatspecCharPtr = formatspec.c_str();
	 // END TODO, OPT: Move to caller(s)

	UInt32 nrRecs = vec.size();
	dms_assert(nrRecs == m_DbfImpl->RecordCount()); 

	for (UInt32 recordindex = 0; recordindex != nrRecs; ++recordindex)
	{
		typename VecType::reference ref = vec[recordindex];
		m_DbfImpl->ReadDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr);
	}

	return true;
} // ReadData

template<class T> bool DbfImplStub<T>::WriteDataOverwrite(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, UInt32 columnindex, ValueClassID vc)
{
	DBG_START("DbfImplStub", "WriteDataOverwrite", MG_DEBUG_DBF);

	 // BEGIN TODO, OPT: Move to caller(s)
	if (! m_DbfImpl->Open(filename, sfwa, FCM_OpenRwFixed))
		return false;

	bool res = true;
	SharedStr	formatspec;
	UInt8 width = m_DbfImpl->ColumnWidth(columnindex);
	FormatSpecification(vc, width, m_DbfImpl->ColumnDecimalCount(columnindex), false, formatspec);
	CharPtr formatspecCharPtr = formatspec.c_str();
	 // END TODO, OPT: Move to caller(s)
	
	UInt32 nrRecs = vec.size();
	dms_assert(nrRecs == m_DbfImpl->RecordCount()); 

	for (UInt32 recordindex = 0; recordindex != nrRecs; ++recordindex)
	{
		fseek(m_DbfImpl->GetFP(), m_DbfImpl->ActualPosition(recordindex, columnindex), 0);
		typename CVecType::const_reference ref = vec[recordindex];
		if (!m_DbfImpl->WriteDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr, width))
		{
			res = false;
			break;
		}
	}

	m_DbfImpl->Close(); // TODO, OPT: Move to caller(s)

	return	res;
}

template<class T> bool DbfImplStub<T>::WriteDataReplace(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, UInt32 columnindex, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount)
{
	DBG_START("DbfImplStub", "WriteDataReplace", MG_DEBUG_DBF);

	bool    res	=	true;
	DbfImpl	dbftarget;
	SharedStr	filename_target	= SharedStr(filename) + ".replace";
   
	if (! dbftarget.Create(filename_target, sfwa))
		return false;

	m_DbfImpl->OpenForRead(filename, sfwa);

	dms_assert(m_DbfImpl->IsOpen()&& m_DbfImpl->RecordLength() > 0); // we are replacing something, not?

	dbftarget.m_RecordCount        = vec.size();
	dbftarget.m_ColumnDescriptions = m_DbfImpl->m_ColumnDescriptions;
	dbftarget.ColumnDescriptionReplace(columnindex, dbftype, len, deccount);
	dbftarget.WriteHeader();
	
	UInt32
		buflen	   = m_DbfImpl->RecordLength(),                    // size   of old record
		offset     = m_DbfImpl->ColumnOffset(columnindex),         // offset of repl field in old records
		nextoffset = offset + m_DbfImpl->ColumnWidth(columnindex), // offset of next field in old records
		restwidth  = buflen - nextoffset,                          // size of fields after replaced field
		nrRecs     = m_DbfImpl->RecordCount();

	std::vector<char> bufContainer(buflen);
	char* buf =	begin_ptr( bufContainer );

	SharedStr	formatspec("");
	FormatSpecification(vc, len, deccount, false, formatspec);
	CharPtr formatspecCharPtr = formatspec.c_str();

	fseek(m_DbfImpl->GetFP(), m_DbfImpl->ActualPosition(0), 0);

	MakeMin(nrRecs, vec.size());
	UInt32 recordindex = 0;
	for (; recordindex < nrRecs && res; recordindex++)
	{
		typename CVecType::const_reference ref = vec[recordindex];
		res =	fread (buf, sizeof(char), buflen, m_DbfImpl->GetFP()) == buflen 
			&&	fwrite(buf, sizeof(char), offset, dbftarget. GetFP()) == offset
			&&	dbftarget.WriteDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr, len)
			&&	fwrite(buf+nextoffset, sizeof(char), restwidth, dbftarget.GetFP() ) == restwidth;
	}

	fast_fill(buf, buf+buflen, char(32));
	for (; recordindex < vec.size() && res; recordindex++)
	{
		typename CVecType::const_reference ref = vec[recordindex];
		res =	fwrite(buf, sizeof(char), offset, dbftarget.GetFP() ) == offset
			&&	dbftarget.WriteDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr, len)
			&&	fwrite(buf+nextoffset, sizeof(char), restwidth, dbftarget.GetFP() ) == restwidth;
	}

	m_DbfImpl->Close();
	dbftarget.Close();

	CommitFile(filename, sfwa, filename_target);

	return res;
}

template<class T> bool DbfImplStub<T>::WriteDataAppend(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, CharPtr columnName, ValueClassID vc, TDbfType dbftype, UInt8 len, UInt8 deccount)
{
	DBG_START("DbfImplStub", "WriteDataAppend", MG_DEBUG_DBF);

	bool		res	=	true;
	DbfImpl	dbftarget;
	SharedStr	filename_target	= filename + ".append";
   
	MakeDirsForFile(filename);
	if (! dbftarget.Create(filename_target, sfwa))
		return false;

	m_DbfImpl->OpenForRead(filename, sfwa);

	dbftarget.m_RecordCount         = vec.size();
	dbftarget.m_ColumnDescriptions	= m_DbfImpl->m_ColumnDescriptions;
	dbftarget.ColumnDescriptionAppend(columnName, dbftype, len, deccount);
	dbftarget.WriteHeader();
	
	UInt32 columnindex = dbftarget.m_ColumnDescriptions.size() - 1,
	       buflen      = m_DbfImpl->RecordLength() > 0 ? m_DbfImpl->RecordLength() : 1;

	std::vector<char> bufContainer(buflen);
	char* buf = &*bufContainer.begin();

	SharedStr formatspec("");
	FormatSpecification(vc, len, deccount, false, formatspec);
	CharPtr formatspecCharPtr = formatspec.c_str();

	UInt32 nrRecs = 0;
	if (m_DbfImpl->IsOpen())
	{
		fseek(m_DbfImpl->GetFP(), m_DbfImpl->ActualPosition(0, 0) - 1, 0);
		nrRecs = m_DbfImpl->RecordCount();
	}

	MakeMin(nrRecs, vec.size());
	UInt32 recordindex = 0;
	for (; recordindex != nrRecs && res; recordindex++)
	{
		typename CVecType::const_reference ref = vec[recordindex];
		res =	fread (buf, sizeof(char), buflen, m_DbfImpl->GetFP()) == buflen
			&&	fwrite(buf, sizeof(char), buflen, dbftarget.GetFP()) == buflen
			&&	dbftarget.WriteDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr, len);
	}

	std::fill(buf, buf+buflen, char(32));
	for (;recordindex < vec.size() && res; recordindex++)
	{
		typename CVecType::const_reference ref = vec[recordindex];
		res =	fwrite(buf, sizeof(char), buflen, dbftarget.GetFP()) == buflen
			&&	dbftarget.WriteDataElement(boost::addressof(ref), recordindex, columnindex, vc, formatspecCharPtr, len);
	}

	m_DbfImpl->Close();
	dbftarget.Close();

	CommitFile(filename, sfwa, filename_target);

	return	res;
}

template<class T> void DbfImplStub<T>::DbfTypeInfo(CVecType vec, ValueClassID vc, TDbfType& dbftype, UInt8& len, UInt8& deccount)
{
	dbftype	=	ValueClassIDToDbfType(vc);
	DecideLenDecCount(vec, len, deccount);
}

template<class T> bool DbfImplStub<T>::TypeResolution(TDbfType dbftype, UInt8 len, UInt8 decCount, UInt32 columnindex)
{
	DbfColDescription& colDesc = m_DbfImpl->m_ColumnDescriptions[columnindex];
	return	dbftype == colDesc.m_DbfType 
		&& (dbftype != dtNumeric 
			|| (len - decCount <= colDesc.m_Length - colDesc.m_DecimalCount 
//				&& deccount <= coldesc.DecimalCount dbf storage leidt soms tot nauwkeurigheidsproblemen
				)
			);
} // TypeResolution

template<class T> bool DbfImplStub<T>::WriteData(WeakStr filename, SafeFileWriterArray* sfwa, CVecType vec, CharPtr columnName, ValueClassID vc)
{
	DBG_START("DbfImplStub", "WriteData", MG_DEBUG_DBF);

	TDbfType	dbftype;
	UInt8		len, deccount;

	SharedStr dbfcolumnname = NameToDbfColumnName(columnName);
	DbfTypeInfo(vec, vc, dbftype, len, deccount);

	if (m_DbfImpl->OpenForRead(filename, sfwa))
	{
		UInt32 columnindex = m_DbfImpl->ColumnNameToIndex(dbfcolumnname.c_str());

		if (m_DbfImpl->ColumnIndexDefined(columnindex))
		{
			if (TypeResolution(dbftype, len, deccount, columnindex) && vec.size() == m_DbfImpl->RecordCount())
				return WriteDataOverwrite(filename, sfwa, vec, columnindex, vc);
			else 
    			return WriteDataReplace  (filename, sfwa, vec, columnindex, vc, dbftype, len, deccount);
		}
	}
	return WriteDataAppend(filename, sfwa, vec, dbfcolumnname.c_str(), vc, dbftype, len, deccount);
}

template<class T> DbfImplStub<T>::DbfImplStub(
		DbfImpl *p, 
		typename sequence_traits<T>::seq_t vec, 
		CharPtr columnName, ValueClassID vc) 
{
	m_DbfImpl = p; 
	m_Result  = ReadData(vec, columnName, vc);
}

template<class T> DbfImplStub<T>::DbfImplStub(
		DbfImpl *p
	,	WeakStr fileName
	,	SafeFileWriterArray* sfwa
	,	typename sequence_traits<T>::cseq_t vec
	,	CharPtr columnName, ValueClassID vc)
{
	m_DbfImpl = p; 
	m_Result  = WriteData(fileName, sfwa, vec, columnName, vc);
}

/*****************************************************************************/
//										END OF FILE
/*****************************************************************************/
