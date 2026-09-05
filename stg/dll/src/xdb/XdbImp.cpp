// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StoragePCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// XdbImp: non-DMS class used by XdbStorageManager to read fixed-width text records (.xyz).

#include "ImplMain.h"

#include "XdbImp.h"

#include "dbg/Diagnostics.h"
#include "dbg/debug.h"
#include "vt/Conversions.h"
#include "vt/Undefined.h"
#include "ser/FormattedStream.h"
#include "set/rangefuncs.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/splitPath.h"

#include <string.h>
#if defined(_MSC_VER)
#include <share.h>
#endif

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
	assert(!m_FHD.IsUsable());

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


// Opens the data file of the indicated table for reading; the column layout is not read from a
// file but set by the storage manager beforehand.
FileResult XdbImp::OpenForRead(WeakStr name, CharPtr datExtension)
{
	DBG_START("XdbImp", "OpenForRead", MG_DEBUG_XDB);

	assert(!m_FHD.IsUsable());
	SetFileName(name, datExtension);
	Close();

	m_FHD = ConstFileViewHandle(std::make_shared<ConstMappedFileHandle>(m_DatFileName, true, false), 0, -1, -1);
	m_FHD.MapView();
	return {};
}






// Get table longs into an external buffer
bool XdbImp::ReadColumn(void * buf, recno_t cnt, column_index col_index)
{
	DBG_START("XdbImp", "ReadColumn", MG_DEBUG_XDB);
	DBG_TRACE(("cnt		  : {}", cnt));
	DBG_TRACE(("nRecPos   : {}", nRecPos));

	// Must be open
	if (! m_FHD.IsUsable() ) 
		return false;

	// Valid column?
	if (!IsValidColumnIndex(col_index))
		return false;

	// Get column offset and total width in bytes of record
	long width  = RecSize();
	long offset = headersize + ColDescriptions[col_index].m_Offset;
	long colWidth = ColWidth(col_index);
    DBG_TRACE(("width, offset: {} {}", width, offset));
	
	// Clip
	long stripped = nRecPos + cnt - NrOfRows();
	if (stripped < 0) stripped = 0;
	if (stripped > 0) cnt = cnt - stripped;
    DBG_TRACE(("stripped  : {}", stripped));

	// Read
	CharPtr dataPtr = m_FHD.DataBegin() + SizeT(nRecPos) * width + offset;
	nRecPos += cnt;

	switch(ColDescriptions[col_index].m_Type)
	{
		case ValueClassID::VT_UInt32:
		case ValueClassID::VT_Int32:
		{
			Int32* tbuf = reinterpret_cast<Int32*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
			break;
		}
		case ValueClassID::VT_Float32:
		{
			Float32* tbuf = reinterpret_cast<Float32*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
			break;
		}
		case ValueClassID::VT_Float64:
		{
			Float64* tbuf = reinterpret_cast<Float64*>(buf);
			for (; cnt; dataPtr+=width, ++tbuf, --cnt)
				AssignValueFromCharPtrs( *tbuf, dataPtr, dataPtr+colWidth);
			fast_zero(tbuf, tbuf+stripped);
			break;
		}
		default:
			return false; // any other column type left the buffer untouched and reported success
	}

	return true;
}








// Get rid of file connections
void XdbImp::Close()
{
	DBG_START("XdbImp", "Close", MG_DEBUG_XDB);

	// reset
	nRecPos = 0;

	// close all
	m_FHD = ConstFileViewHandle();
}


// The data file name differs only in extension from the storage name
bool XdbImp::SetFileName(WeakStr src, CharPtr datExtension)
{
	DBG_START("XdbImp", "SetFileName", MG_DEBUG_XDB);

	CharPtr fileNameExtension = getFileNameExtension(src.c_str());
	if (!*fileNameExtension)
		return false;

	m_DatFileName = SharedStr(CharPtrRange(src.c_str(), fileNameExtension)) + datExtension;
	DBG_TRACE(("Name: {}", m_DatFileName.c_str()));
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
	if (nrows == UInt32(-1))
		const_cast<XdbImp*>(this)->nrows = ThrowingConvert<XdbImp::recno_t>((m_FHD.GetViewSize() -headersize) / RecSize());
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
		if (! stricmp(col_name, ColDescriptions[i].m_Name.c_str()) ) 
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
	column_index next_i = i + 1;
	assert(next_i != 0);
	if (next_i == ColDescriptions.size())
		return m_RecSize - ColDescriptions[i].m_Offset;
	return ColDescriptions[next_i].m_Offset - ColDescriptions[i].m_Offset;
}

// Column type
ValueClassID XdbImp::ColType(column_index i) const
{
	// Subtractn offsets
	if (!IsValidColumnIndex(i)) 
		return ValueClassID::VT_Unknown;
	return ColDescriptions[i].m_Type;
}

