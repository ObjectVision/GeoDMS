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
// Non DMS-based class used to stream to and from Xdb tables
//
// *****************************************************************************


#ifndef _XdbImp_H_
#define _XdbImp_H_


#include "ImplMain.h"

#include <vector>	// ColDescriptions
#include "stdio.h"	// FP

#include "geo/BaseBounds.h"
#include "mci/ValueClassID.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedStr.h"
#include "ser/FileMapHandle.h"

#include "FilePtrHandle.h"

// wrapper for columnparameters
struct XdbColDescription
{
	XdbColDescription(): m_Offset(0), m_Type(ValueClassID::VT_Unknown) {}

	XdbColDescription& operator =(const XdbColDescription&) = default;

	SharedStr    m_Name;
	long         m_Offset;
	ValueClassID m_Type; // defined in rtc/mci/ValueClassID.h
};


class XdbImp : FilePtrHandle
{
	typedef UInt32 column_index;
	typedef UInt32 width_t;
	typedef UInt32 recno_t;

public:
	STGIMPL_CALL  XdbImp();
	STGIMPL_CALL ~XdbImp();

	// read/write functions
	STGIMPL_CALL [[nodiscard]] bool Open       (WeakStr name, SafeFileWriterArray* sfwa, FileCreationMode mode, CharPtr datExtension, bool saveColInfo);
	STGIMPL_CALL [[nodiscard]] bool OpenForRead(WeakStr name, SafeFileWriterArray* sfwa, CharPtr datExtension, bool saveColInfo);
	STGIMPL_CALL [[nodiscard]] bool Create     (WeakStr name, SafeFileWriterArray* sfwa, CharPtr datExtension, bool saveColInfo);

	STGIMPL_CALL [[nodiscard]] bool ReadColumn (      void* data, recno_t cnt, column_index col_index);
	STGIMPL_CALL [[nodiscard]] bool WriteColumn(const void* data, recno_t cnt, column_index col_index);
	STGIMPL_CALL [[nodiscard]] bool AppendColumn(CharPtr name, SafeFileWriterArray* sfwa, width_t size, ValueClassID type, recno_t rows, bool saveColInfo);
	STGIMPL_CALL void Close();

	// info functions
	STGIMPL_CALL recno_t      NrOfRows() const;
	             column_index NrOfCols() const { return ColDescriptions.size(); };
	STGIMPL_CALL column_index ColIndex(CharPtr col_name)       const;
	STGIMPL_CALL CharPtr      ColName (column_index col_index) const;
	STGIMPL_CALL width_t      ColWidth(column_index col_index) const;
	STGIMPL_CALL ValueClassID ColType (column_index col_index) const;

	
private:

	FileViewHandle  m_FHD;					// .dat file
	SharedStr m_FileName;					// .xdb file name
	SharedStr m_DatFileName;				// .dat file name
	CharPtr   m_DatExtension;               // .dat could be .txt or .kml or .xyz

	UInt32   nRecPos;						// read/write position
public:
	std::vector<XdbColDescription> ColDescriptions;		// .xdb content 
	// header attributes, set when .xdb is read
	recno_t nrows;                          // number of records
	recno_t nrheaderlines;                  // number of header lines in .txt before first record
	width_t headersize;                     // size of header lines in .txt (nr of bytes)
	width_t m_LineBreakSize;                // size of linebreak ("0x0a0x0d": 2 or just "0x0d":1 ) default=2
	width_t m_RecSize;                      // excluding 0A0D

private:
	// helper functions
	[[nodiscard]] bool ReadHeader();                      // read from .xdb
	bool WriteHeader();                     // write to .xdb
	void Clear();                           // reset all
	bool SetFileName(WeakStr xdb_name, CharPtr datExtension, bool saveColInfo);             // extension swap

	// layout
	width_t RecSize() const { return m_RecSize + m_LineBreakSize;}; // including 0A0D

	bool IsValidColumnIndex(column_index i) const { return i < ColDescriptions.size(); }
};


#endif // _XdbImp_H_