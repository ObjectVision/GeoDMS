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
// Non DMS-based class used to stream to and from 'ascii-grids' (Idrissi?)
//
// *****************************************************************************


#if !defined(__STG_ASCIMPL_H)
#define __STG_ASCIMPL_H

#include "FilePtrHandle.h"
#include "ViewPortInfo.h"

#include "geo/SequenceTraits.h"
#include "ptr/SharedStr.h"
struct SafeFileWriterArray;

struct AsciiImp
{
	FilePtrHandle m_FH;
	STGIMPL_CALL  AsciiImp();
	STGIMPL_CALL ~AsciiImp();

	// read functions
	STGIMPL_CALL bool OpenForRead (WeakStr name, SafeFileWriterArray* sfwa);
	STGIMPL_CALL bool OpenForWrite(WeakStr name, SafeFileWriterArray* sfwa);

	template <typename T> STGIMPL_CALL bool ReadCells (typename sequence_traits<T>::pointer buf, const StgViewPortInfo& vpi);
	template <typename T> STGIMPL_CALL bool WriteCells(WeakStr fileName, SafeFileWriterArray* sfwa, typename sequence_traits<T>::const_pointer buf, UInt32 cnt);
	template <typename T> STGIMPL_CALL void CountData (const StgViewPortInfo& vpi, T* colorCount); // buffer for colorcount output (preallocated)

	// headerinfo. to be made dynamic
	UInt32  NrOfRows()  const { return m_NrRows; }
	UInt32  NrOfCols()  const { return m_NrCols; }
	UInt32  NrOfCells() const { return NrOfRows() * NrOfCols(); }
	double XLLCorner() { return m_dXllCorner; }
	double YLLCorner() { return m_dYllCorner; }
	double CellSize()  { return m_dCellSize; }
	void  SetNrOfRows(long r)   { m_NrRows = r; }
	void  SetNrOfCols(long c)   { m_NrCols = c; }
	void  SetXLLCorner(double x) { m_dXllCorner = x; }
	void  SetYLLCorner(double y) { m_dYllCorner = y; }
	STGIMPL_CALL void  SetCellSize(double c);

	// missing value handling
	double NoDataInFile  () const { return m_dNODATA_InFile;     } void SetNoDataInFile    (double val)      { m_dNODATA_InFile = val; }
	bool   NoDataIntegral() const { return m_bNODATA_IsIntegral; } void SetNoDataIsIntegral(bool isIntegral) { m_bNODATA_IsIntegral = isIntegral; }
	// write functions
	STGIMPL_CALL void CopyHeaderInfoFrom(const AsciiImp & src);


	STGIMPL_CALL void Close();

private:
	UInt32 CalcStripped(UInt32& cnt) const;

	// header attributes
	UInt32 m_NrCols;
	UInt32 m_NrRows;
	double m_dXllCorner;
	double m_dYllCorner;
	double m_dCellSize;

	double m_dNODATA_InFile; // no data value indicator in file (found in header, can be overrriden by client config
	bool m_bNODATA_IsIntegral;

	// helper functions
	bool ReadHeader();
	bool WriteHeader();

	friend struct reader_base;
};


#endif //!defined(__STG_ASCIMPL_H)
