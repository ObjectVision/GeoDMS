// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// *****************************************************************************
//
// Non-DMS class that reads the fixed-width text records of an .xyz file. The .xdb column format it
// descends from (a header file describing the columns, appendable) is no longer read or written;
// the layout is set by the storage manager, see XyzStorageManager::UpdateColInfo.
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

#include "vt/BaseBounds.h"
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


class XdbImp
{
	using column_index = UInt32;
	using width_t = UInt32;
	using recno_t = UInt32;

public:
	STGIMPL_CALL  XdbImp();
	STGIMPL_CALL ~XdbImp();

	// read functions
	STGIMPL_CALL [[nodiscard]] FileResult OpenForRead(WeakStr name, CharPtr datExtension);

	STGIMPL_CALL [[nodiscard]] bool       ReadColumn (      void* data, recno_t cnt, column_index col_index);
	STGIMPL_CALL void Close();

	// info functions
	STGIMPL_CALL recno_t      NrOfRows() const;
	             column_index NrOfCols() const { return ColDescriptions.size(); };
	STGIMPL_CALL column_index ColIndex(CharPtr col_name)       const;
	STGIMPL_CALL CharPtr      ColName (column_index col_index) const;
	STGIMPL_CALL width_t      ColWidth(column_index col_index) const;
	STGIMPL_CALL ValueClassID ColType (column_index col_index) const;

	
private:

	ConstFileViewHandle m_FHD;				// the data file
	SharedStr m_DatFileName;				// its name: the storage name with the extension of the storage manager

	UInt32   nRecPos;						// read position
public:
	std::vector<XdbColDescription> ColDescriptions;		// the column layout, set by the storage manager (UpdateColInfo)
	// header attributes, set by the storage manager
	recno_t nrows;                          // number of records
	recno_t nrheaderlines;                  // number of header lines in .txt before first record
	width_t headersize;                     // size of header lines in .txt (nr of bytes)
	width_t m_LineBreakSize;                // size of linebreak ("0x0a0x0d": 2 or just "0x0d":1 ) default=2
	width_t m_RecSize;                      // excluding 0A0D

private:
	// helper functions
	void Clear();                           // reset all
	bool SetFileName(WeakStr name, CharPtr datExtension);             // extension swap

	// layout
	width_t RecSize() const { return m_RecSize + m_LineBreakSize;}; // including 0A0D

	bool IsValidColumnIndex(column_index i) const { return i < ColDescriptions.size(); }
};


#endif // _XdbImp_H_