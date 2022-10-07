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
// *****************************************************************************
//
// Implementation of non-DMS based class XdbImp. This class is used
// by XdbStorageManager to read and write 'Xdb-grids'.
//
// *****************************************************************************

#include "ImplMain.h"
#pragma hdrstop

#include "XdbImp.h"

#include "dbg/Check.h"
#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/Undefined.h"
#include "ser/FormattedStream.h"
#include "set/RangeFuncs.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"

#include <string.h>
#include <share.h>

#define MG_DEBUG_XDB false

// Initialise privates
XdbImp::XdbImp()
{
	DBG_START("XdbImp", "XdbImp", MG_DEBUG_XDB);

	Clear();
}


// Reset 
void XdbImp::Clear()
{
	DBG_START("XdbImp", "Clear", MG_DEBUG_XDB);

	// Refresh, files are assumed closed
	dms_assert(!IsOpen());
	dms_assert(!m_FHD.IsOpen());

	m_FileName.clear();
	m_DatFileName.clear();
	nRecPos = 0;
	nrows = -1;
	nrheaderlines = 0;
	headersize = 0;
	m_RecSize = 0;
	m_LineBreakSize = 2;
	ColDescriptions.resize(0);
}


// Get rid of open files
XdbImp::~XdbImp()
{
	DBG_START("XdbImp", "~XdbImp", MG_DEBUG_XDB);
	Close();
	Clear();
}


// Opens the indicated Xdb-table for reading. 
// The header is read.
bool XdbImp::OpenForRead(WeakStr name, SafeFileWriterArray* sfwa, CharPtr datExtension, bool saveColInfo)
{
	return Open(name, sfwa, FCM_OpenReadOnly, datExtension, saveColInfo);
}


// Opens the indicated Xdb-table for reading. 
// The header is read.
bool XdbImp::Open(WeakStr name, SafeFileWriterArray* sfwa, FileCreationMode fileMode, CharPtr datExtension, bool saveColInfo)
{
	DBG_START("XdbImp", "Open", MG_DEBUG_XDB);
	
	dms_assert(!m_FHD.IsOpen());
	// Retain name and corresponding dat-file name
	SetFileName(name, datExtension, saveColInfo);

	// Open files
	Close();

	if (saveColInfo && !OpenFH(m_FileName, sfwa, FCM_OpenReadOnly, true, NR_PAGES_HDRFILE))
		return false;

	bool alsoWrite = (fileMode != FCM_OpenReadOnly);
	if (!alsoWrite)
		m_FHD.OpenForRead( m_DatFileName, sfwa, true, false );
	else
		m_FHD.OpenRw( m_DatFileName, sfwa, UNDEFINED_VALUE(std::size_t), dms_rw_mode::read_write, false);
	m_FHD.Map( alsoWrite ? dms_rw_mode::read_write : dms_rw_mode::read_only, m_DatFileName, sfwa );

	// Read header info
	return (!saveColInfo) || ReadHeader();
}




// Creates a new file
bool XdbImp::Create(WeakStr name, SafeFileWriterArray* sfwa, CharPtr datExtension, bool saveColInfo)
{
	DBG_START("XdbImp", "Create", MG_DEBUG_XDB);
	
	// Retain name and corresponding dat file name
	SetFileName(name, datExtension, saveColInfo);

	// Create dummy files
	Close();

	GetWritePermission(m_FileName);

	if (saveColInfo && ! OpenFH(m_FileName, sfwa, FCM_CreateAlways, true, NR_PAGES_HDRFILE) ) 
		return false;

	if ( ! FilePtrHandle().OpenFH( m_DatFileName, sfwa, FCM_CreateAlways, false, NR_PAGES_DIRECTIO) )
	{
		Close();
		return false;
	}

	// Write rec-count = 0 and nr header-lines = 0 to first line
	if (saveColInfo)
		fprintf(*this, "%d %d\n", 0, 0);

	nrows = 0;
	nrheaderlines = 0;
	Close();
	
	// Regular open
	return Open(name, sfwa, FCM_OpenRwFixed, datExtension, saveColInfo);
}


// Get table longs into an external buffer
bool XdbImp::ReadColumn(void * buf, recno_t cnt, column_index col_index)
{
	DBG_START("XdbImp", "ReadColumn", MG_DEBUG_XDB);
	DBG_TRACE(("cnt		  : %ld", cnt));
	DBG_TRACE(("nRecPos   : %ld", nRecPos));

	// Must be open
	if (! m_FHD.IsOpen() ) 
		return false;

	// Valid column?
	if (!IsValidColumnIndex(col_index))
		return false;

	// Get column offset and total width in bytes of record
	long width  = RecSize();
	long offset = headersize + ColDescriptions[col_index].m_Offset;
	long colWidth = ColWidth(col_index);
    DBG_TRACE(("width, offset: %d %d", width, offset));
	
	// Clip
	long stripped = nRecPos + cnt - NrOfRows();
	if (stripped < 0) stripped = 0;
	if (stripped > 0) cnt = cnt - stripped;
    DBG_TRACE(("stripped  : %ld", stripped));

	// Read
	CharPtr dataPtr = m_FHD.DataBegin() + nRecPos * width + offset;
	nRecPos += cnt;

	switch(ColDescriptions[col_index].m_Type)
	{
		case VT_UInt32:
		case VT_Int32:
		{
			Int32* tbuf = reinterpret_cast<Int32*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
		}
		case VT_Float32:
		{
			Float32* tbuf = reinterpret_cast<Float32*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
		}
		case VT_Float64:
		{
			Float64* tbuf = reinterpret_cast<Float64*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
		}
	}

	return true;
}



// Write the provided shorts to the table
bool XdbImp::WriteColumn(const void * buf, recno_t cnt, column_index col_index)
{
	DBG_START("XdbImp", "WriteColumn", MG_DEBUG_XDB);
	DBG_TRACE(("cnt		  : %ld", cnt));
	DBG_TRACE(("nRecPos   : %ld", nRecPos));

	throwErrorD("Xdb", "XdbWriteColumn::Temporary Disabled Due To Maintenance");
	// Must be open

	if (! m_FHD.IsOpen() || UInt32(col_index) >= ColDescriptions.size())
		return false;

	// Get column offset and total width in bytes of record
	long width  = RecSize();
	long offset = headersize + ColDescriptions[col_index].m_Offset;
	long colwidth = ColWidth(col_index);
    DBG_TRACE(("width, offset: %d %d", width, offset));

	// Clip
	if (cnt < 0) cnt = 0;
	long stripped = nRecPos + cnt -  NrOfRows();
	if (stripped < 0) stripped = 0;
	if (stripped > 0) cnt = cnt - stripped;
    DBG_TRACE(("stripped  : %ld", stripped));
/* DEBUG, NYI
	fseek(m_FHD, 0, SEEK_END);
	long srcfilesize = ftell(m_FHD);

	// Write
	recno_t i=0;
	char sbuf[255]; sbuf[0] = 0;
	long c = 0;
	for (i=0; i<cnt; i++)
	{
		// Move to write position
		long newpos = nRecPos * width + offset;
		fseek( m_FHD, newpos, 0);
		
		// Construct string to write
		switch(ColDescriptions[col_index].Type)
		{
			case VT_UInt32:
			case VT_Int32:   WriteLong  (sbuf,  *(((long    *)buf)+i)); break;
			case VT_Float32: WriteFloat (sbuf,  *(((Float32 *)buf)+i)); break;
			case VT_Float64: WriteDouble(sbuf,  *(((Float64 *)buf)+i)); break;
		}
		for (c=StrLen(sbuf); c<colwidth; ++c)
			sbuf[c] = ' ';

		sbuf[colwidth] = 0;
	    //DBG_TRACE(("sbuf  : %s", sbuf));

		// Write string
		long result = fprintf( m_FHD, "%s", sbuf);
		//DBG_TRACE(("result: %d", result));
		nRecPos++;
		if (newpos >= srcfilesize)
		{
			fseek(m_FHD, nRecPos * width - 2, 0);
			fprintf(m_FHD, "\n");
		}
	}
*/
	// Done
	return true;
}

void freadln(FILE* fp)
{
	char ch = getc(fp);
	while (ch != '\n' && !feof(fp))
	{
		ch = getc(fp);
	}
	// see if we get a chr(10) after the chr(13)
	if (!feof(fp))
	{
		ch = getc(fp);
		if (ch != 10)
			ungetc(ch, fp); // apparently not
	}
}

// Read column info from header file
bool XdbImp::ReadHeader()
{
	DBG_START("XdbImp", "ReadHeader", MG_DEBUG_XDB);

	// Must be open
	if (!IsOpen()) 
		return false;

	// Read nrRows from first line
	auto nrFieldsRead = fscanf(*this, "%u %u", &nrows, &nrheaderlines);
	MG_CHECK(nrFieldsRead == 2);

	DBG_TRACE(("nrows:         %u", nrows));
	DBG_TRACE(("nrheaderlines: %u", nrheaderlines));
	
	// Read structs until end of file
	ColDescriptions.reserve(100);
	ColDescriptions.resize(0);
	char fldName[400]; fldName[0] = 0;
	long len = 0;
	long offset = 0;

	ValueClassID type = VT_Unknown;
	while (fscanf(*this, "%s %d %d", fldName, &len, &type) != EOF)
	{	
		MG_CHECK(StrLen(fldName) < 400);

		DBG_TRACE(("name, len: %s, %d", fldName, len));
		ColDescriptions.resize(ColDescriptions.size()+1);


		ColDescriptions[ColDescriptions.size()-1].m_Name   = fldName;
		ColDescriptions[ColDescriptions.size()-1].m_Offset = offset;
		offset += len;
		ColDescriptions[ColDescriptions.size()-1].m_Type = type;
	}
	m_RecSize = offset;
	DBG_TRACE(("nrecsize: %d", m_RecSize ));

	headersize = nrheaderlines * RecSize();

	return true; // Done
}


// Write header file
bool XdbImp::WriteHeader()
{
	DBG_START("XdbImp", "WriteHeader", MG_DEBUG_XDB);

	// Must be open
	if (!IsOpen())
		return false;

	// Write rec-count to first line
	fprintf(*this, "%u %u\n", nrows, nrheaderlines);

	// Write the column descriptions
	for (UInt32 i=0; i<ColDescriptions.size(); i++)
		fprintf(*this, "%s %u %d\n"
			,	ColDescriptions[i].m_Name.c_str()
			,	ColWidth(i)
			,	ColDescriptions[i].m_Type
		);

	// Done
	return true;
}

// Get rid of file connections
void XdbImp::Close()
{
	DBG_START("XdbImp", "Close", MG_DEBUG_XDB);

	// reset
	nRecPos = 0;

	// close all
	if (m_FHD.IsMapped())
		m_FHD.Unmap();
	if (m_FHD.IsOpen())
		m_FHD.CloseFMH();

	CloseFH();
}


// Dat file name differs only in extension from the filename
bool XdbImp::SetFileName(WeakStr src, CharPtr datExtension, bool saveColInfo)
{
	DBG_START("XdbImp", "SetFileName", MG_DEBUG_XDB);

	// fill membervariable
	m_FileName = src;

	m_DatExtension = datExtension;

	// scan extension
	CharPtr fileNameExtension = getFileNameExtension(m_FileName.c_str());
	if (!*fileNameExtension)
		return false;

	// fill membervariable
	m_DatFileName = SharedStr(m_FileName.c_str(), fileNameExtension) + datExtension;
	if (!saveColInfo)
		m_FileName = "";
	DBG_TRACE(("Names: %s %s", m_FileName.c_str(), m_DatFileName.c_str()));
	return true;
}


// Column-index to column-name
CharPtr XdbImp::ColName(column_index i) const
{
	if (!IsValidColumnIndex(i)) 
		return "invalid";
	return ColDescriptions[i].m_Name.c_str();
}

XdbImp::recno_t XdbImp::NrOfRows() const
{
	if (nrows == -1)
		const_cast<XdbImp*>(this)->nrows = ThrowingConvert<XdbImp::recno_t>((m_FHD.GetFileSize() -headersize) / RecSize());
	return nrows;
};

// Column-name to column-index
XdbImp::column_index XdbImp::ColIndex(CharPtr col_name) const
{
	// Scan
	column_index i = 0;
	while (true) 
	{
		if (i == ColDescriptions.size()) 
			return -1;
		if (! _stricmp(col_name, ColDescriptions[i].m_Name.c_str()) ) 
			return i;
		++i;
	}
}


// Calc column width from offsets
XdbImp::width_t XdbImp::ColWidth(column_index i) const
{
	// Subtractn offsets
	if (!IsValidColumnIndex(i)) 
		return 0;
	if (i+1 == ColDescriptions.size())
		return m_RecSize - ColDescriptions[i].m_Offset;
	return ColDescriptions[i+1].m_Offset - ColDescriptions[i].m_Offset;
}

// Column type
ValueClassID XdbImp::ColType(column_index i) const
{
	// Subtractn offsets
	if (!IsValidColumnIndex(i)) 
		return VT_Unknown;
	return ColDescriptions[i].m_Type;
}

// Add a new column or create the first column in a fresh table
bool XdbImp::AppendColumn
(
	CharPtr      fldName
,	SafeFileWriterArray* sfwa
,	width_t      size
,	ValueClassID type 
,	recno_t      rows
,	bool         saveColInfo
)
{
	DBG_START("XdbImp", "AppendColumn", MG_DEBUG_XDB);

	// Columnnames should be unique
	if (ColIndex(fldName) != -1) return false;
	if (size < 1) return false;

	dms_assert(m_FHD.IsOpen()|| NrOfCols() == 0);
	// Close open files
	Close();

	// New file?
	if (NrOfCols() == 0) 
	{
		DBG_TRACE(("new file"));
		if (rows <= 0) return false;
		if (!Create(m_FileName, sfwa, m_DatExtension, true))
			return false;
		Close();
		nrows = rows;
	}

	// Local streams
	FilePtrHandle src;
	FilePtrHandle dst;

	// Name of temporary dat-file
	SharedStr tmpDatFile =  m_DatFileName + ".tmp";

	// Open dat-files
	if (!src.OpenFH( m_DatFileName, sfwa, FCM_OpenReadOnly, true, NR_PAGES_DATFILE))
		return false;

	DBG_TRACE(("padding data"));

	
	// Read lines, write lines + padding
	width_t width = RecSize();
	recno_t row=0;
	width_t i=0;

	bool copyMade = false;
	fseek(src, 0, SEEK_END);
	width_t srcfilesize = ftell(src);
	if (srcfilesize > headersize)
	{
		if(!dst.OpenFH(tmpDatFile, sfwa, FCM_CreateAlways, true, NR_PAGES_DATFILE)) 
			return false;

		fseek(src, 0, 0);
		copyMade = true;
		for (i=0; i<headersize; i++)
		{
			fputc(fgetc(src), dst);
		}

		std::vector<char> buf(width);
		for (row=0; row<nrows; row++)
		{
			// Get complete line in a string
			width_t nextpos = row * width + headersize;
			if (nextpos >= srcfilesize) 
				break;
			fseek( src, nextpos, 0);

			std::vector<char>::iterator bufPtr = buf.begin(), bufEnd = bufPtr + m_RecSize;
			for (; bufPtr != bufEnd; ++bufPtr)
				*bufPtr = fgetc(src);
			*bufEnd = 0;
			//DBG_TRACE(("rec: %s", buf));

			// Write line + padding to new dat-file
			fputs(reinterpret_cast<CharPtr>( &* buf.begin() ), dst);
			fputc('0', dst);
			for (i=1; i<size; i++) 
				fputc(' ', dst);
			fputs("\n", dst);
		}
	}

	// Done, with dat-files
	src.CloseFH();
	dst.CloseFH();

	// Change header info
	if (saveColInfo)
	{
		DBG_TRACE(("changing header"));

		// Add new column to description
		XdbImp local;
		local.nrows = nrows;
		local.nrheaderlines = nrheaderlines;
		local.headersize = headersize;
		local.m_RecSize = m_RecSize + size;
		local.ColDescriptions = ColDescriptions;
		local.ColDescriptions.resize(local.ColDescriptions.size()+1);
		local.ColDescriptions[local.ColDescriptions.size()-1].m_Name   = fldName;
		local.ColDescriptions[local.ColDescriptions.size()-1].m_Offset = m_RecSize;
		local.ColDescriptions[local.ColDescriptions.size()-1].m_Type   = type;

		// Write new header
		local.OpenFH(m_FileName, sfwa, FCM_CreateAlways, true, NR_PAGES_HDRFILE);
		local.WriteHeader();
	}

	// Swap files
	if (copyMade)
	{
		if (remove(m_DatFileName.c_str()))
			return false;
		if (rename(tmpDatFile.c_str(), m_DatFileName.c_str()))
			return false;
	}

	// Reopen
	return Open(m_FileName, sfwa, FCM_OpenRwFixed, m_DatExtension, saveColInfo);
}
