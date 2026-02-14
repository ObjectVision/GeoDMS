// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// *****************************************************************************
//
// Non DMS-based class used to stream to and from Xdb tables
//
// *****************************************************************************

#if defined(_MSC_VER)
#pragma once
#endif


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
	using column_index = UInt32;
	using width_t = UInt32;
	using recno_t = UInt32;

public:
	STGIMPL_CALL  XdbImp();
	STGIMPL_CALL ~XdbImp();

	// read/write functions
	STGIMPL_CALL [[nodiscard]] FileResult Open       (WeakStr name, FileCreationMode mode, CharPtr datExtension, bool saveColInfo);
	STGIMPL_CALL [[nodiscard]] FileResult OpenForRead(WeakStr name, CharPtr datExtension, bool saveColInfo);
	STGIMPL_CALL [[nodiscard]] FileResult Create     (WeakStr name, CharPtr datExtension, bool saveColInfo);

	STGIMPL_CALL [[nodiscard]] bool       ReadColumn (      void* data, recno_t cnt, column_index col_index);
	STGIMPL_CALL [[nodiscard]] FileResult WriteColumn(const void* data, recno_t cnt, column_index col_index);
	STGIMPL_CALL [[nodiscard]] FileResult AppendColumn(CharPtr name, width_t size, ValueClassID type, recno_t rows, bool saveColInfo);
	STGIMPL_CALL void Close();

	// info functions
	STGIMPL_CALL recno_t      NrOfRows() const;
	             column_index NrOfCols() const { return ColDescriptions.size(); };
	STGIMPL_CALL column_index ColIndex(CharPtr col_name)       const;
	STGIMPL_CALL CharPtr      ColName (column_index col_index) const;
	STGIMPL_CALL width_t      ColWidth(column_index col_index) const;
	STGIMPL_CALL ValueClassID ColType (column_index col_index) const;

	
private:

	ConstFileViewHandle m_FHD;				// .dat file
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