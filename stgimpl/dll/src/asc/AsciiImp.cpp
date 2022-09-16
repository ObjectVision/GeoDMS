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
// Implementation of non-DMS based class AsciiImp. This class is used
// by AsciiStorageManager to read and write 'ascii-grids'.
//
// *****************************************************************************

#include "ImplMain.h"
#pragma hdrstop

#include "AsciiImp.h"

#include "RtcTypeModel.h"
#include "act/Actor.h"
#include "act/TriggerOperator.h"
#include "dbg/Check.h"
#include "dbg/debug.h"
#include "geo/Conversions.h"
#include "geo/Undefined.h"
#include "geo/MinMax.h"
#include "geo/PointOrder.h"
#include "geo/Round.h"
#include "set/BitVector.h"
#include "utl/Environment.h"

#include <share.h>

// Initialise privates
AsciiImp::AsciiImp()
{
	// header
	m_NrCols = 0;
	m_NrRows = 0;
	m_dXllCorner = 0;
	m_dYllCorner = 0;
	m_dCellSize = 0;
	m_dNODATA_InFile = UNDEFINED_VALUE(Float64);
	m_bNODATA_IsIntegral = false;
}

// Get rid of open files
AsciiImp::~AsciiImp()
{
	Close();
}

void  AsciiImp::SetCellSize(double c)
{ 
	dms_assert(c>0); 
	m_dCellSize = c; 
};

// Opens the indicated 'ascii-grid' for reading. 
// The header is read.
bool AsciiImp::OpenForRead(WeakStr name, SafeFileWriterArray* sfwa)
{
	DBG_START("AsciiImp", "OpenForRead", false);

	// try to open
	Close();
	if(!OpenFH(name, sfwa, FCM_OpenReadOnly, true, NR_PAGES_DATFILE))
		return false;

	DBG_TRACE(("Opened: %s", name.c_str()));

	// read header info
	return ReadHeader();
}

// Opens the indicated 'ascii-grid' for writing
// The header is written
bool AsciiImp::OpenForWrite(WeakStr name, SafeFileWriterArray* sfwa)
{
	DBG_START("AsciiImp", "OpenForWrite", true);

	// try to open
	Close();

	if(!OpenFH(name, sfwa, FCM_CreateAlways, true, NR_PAGES_DATFILE)) 
		return false;

	DBG_TRACE(("Opened: %s", name.c_str()));
 
	// write header info
	return WriteHeader();
}

struct reader_base
{
	FilePtrHandle m_FH;

	double m_NoDataInFile = UNDEFINED_VALUE(double);

	UInt32 m_GridPos = 0;

	reader_base()
	{
		dms_assert(!m_FH.IsOpen());
	}

	reader_base(const AsciiImp& imp)
		:	m_FH(imp)
	{
	}
	void Init(const AsciiImp& imp)
	{
		m_FH             = imp;
		m_NoDataInFile   = imp.NoDataInFile();

		dms_assert(m_GridPos == 0);
		dms_assert(m_FH.IsOpen());
	}

	bool Inc16()
	{
		m_GridPos += 16;
		return (m_GridPos & 0xFF0) != 0xFF0 
			|| !SuspendTrigger::MustSuspend();
	}
};

template <typename T> CharPtr GetFormat16();
template <> CharPtr GetFormat16<long double>() { return "%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf%Lf"; }
template <> CharPtr GetFormat16<double>() { return "%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf"; }
template <> CharPtr GetFormat16<float >() { return "%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f"; }
template <> CharPtr GetFormat16<Int16 >() { return "%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd%hd"; }
template <> CharPtr GetFormat16<UInt16>() { return "%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu%hu"; }
template <> CharPtr GetFormat16<Int32 >() { return "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d"; }
template <> CharPtr GetFormat16<UInt32>() { return "%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u%u"; }
template <> CharPtr GetFormat16<Int64 >() { return "%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d%I64d"; }
template <> CharPtr GetFormat16<UInt64>() { return "%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u%I64u"; }

template <typename T> CharPtr GetFormat1();
template <> CharPtr GetFormat1<long double>() { return "%Lf"; }
template <> CharPtr GetFormat1<double>() { return "%lf"; }
template <> CharPtr GetFormat1<float >() { return "%f"; }
template <> CharPtr GetFormat1<Int16 >() { return "%hd"; }
template <> CharPtr GetFormat1<UInt16>() { return "%hu"; }
template <> CharPtr GetFormat1<Int32 >() { return "%d"; }
template <> CharPtr GetFormat1<UInt32>() { return "%u"; }
template <> CharPtr GetFormat1<Int64 >() { return "%I64d"; }
template <> CharPtr GetFormat1<UInt64>() { return "%I64u"; }

template <typename T> CharPtr GetSkipFormat1();
template <> CharPtr GetSkipFormat1<long double>() { return "%*Lf"; }
template <> CharPtr GetSkipFormat1<double>() { return "%*lf"; }
template <> CharPtr GetSkipFormat1<float >() { return "%*f"; }
template <> CharPtr GetSkipFormat1<Int16 >() { return "%*hd"; }
template <> CharPtr GetSkipFormat1<UInt16>() { return "%*hu"; }
template <> CharPtr GetSkipFormat1<Int32 >() { return "%*d"; }
template <> CharPtr GetSkipFormat1<UInt32>() { return "%*u"; }
template <> CharPtr GetSkipFormat1<Int64 >() { return "%*I64d"; }
template <> CharPtr GetSkipFormat1<UInt64>() { return "%*I64u"; }


template <typename T>
struct read_direct : reader_base
{
	bool Read16(typename sequence_traits<T>::pointer buffI)
	{
		if (fscanf( m_FH.GetFP(), GetFormat16<T>(), 
			buffI + 0, buffI + 1, buffI + 2, buffI + 3,
			buffI + 4, buffI + 5, buffI + 6, buffI + 7,
			buffI + 8, buffI + 9, buffI +10, buffI +11,
			buffI +12, buffI +13, buffI +14, buffI +15
		) != 16)
			return false;
		buffI += 16;
		return Inc16();
	}
	bool Read1(typename sequence_traits<T>::pointer buffI)
	{
		if (fscanf( m_FH.GetFP(), GetFormat1<T>(), buffI) != 1)
			return false;
		++m_GridPos;
		return true;
	}
	bool Skip1()
	{
		if (fscanf( m_FH.GetFP(), GetSkipFormat1<T>()) != 0)
			return false;
		++m_GridPos;
		return true;
	}
	void SubstituteNoData(typename sequence_traits<T>::pointer buffI, typename sequence_traits<T>::pointer buffE)
	{
		// Overwrite nodata values()
		if (m_NoDataInFile != double(UNDEFINED_OR_ZERO(T)))
		{
			T noDataInFile = m_NoDataInFile;
			for (; buffI != buffE; ++buffI)
				if (*buffI == noDataInFile)
					*buffI = UNDEFINED_OR_ZERO(T);		
		}
	}
};

template <typename T>
struct read_buffered : reader_base
{
	bool Read16(typename sequence_traits<T>::pointer buffI)
	{
		int buffInt[16];
		if (fscanf( m_FH.GetFP(), "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d", 
			buffInt + 0, buffInt + 1, buffInt + 2, buffInt + 3,
			buffInt + 4, buffInt + 5, buffInt + 6, buffInt + 7,
			buffInt + 8, buffInt + 9, buffInt +10, buffInt +11,
			buffInt +12, buffInt +13, buffInt +14, buffInt +15
		) != 16)
			return false;
		int* buffIntPtr = buffInt;
		typename sequence_traits<T>::pointer
			buffInext  = buffI+16;
		int noDataInFile = m_NoDataInFile;
		if (T(noDataInFile) != UNDEFINED_OR_ZERO(T))
		{
			for (;buffI != buffInext; ++buffIntPtr, ++buffI) 
				*buffI = ((*buffIntPtr == noDataInFile) ? UNDEFINED_OR_ZERO(T) : T(*buffIntPtr));
		}
		else
			for (; buffI != buffInext; ++buffI) 
				*buffI = *buffIntPtr++;

		return Inc16();
	}
	bool Read1(typename sequence_traits<T>::pointer buffI)
	{
		int buffInt;
		if (fscanf( m_FH.GetFP(), "%d", &buffInt) != 1)
			return false;

		*buffI = (buffInt == int(m_NoDataInFile) ? UNDEFINED_OR_ZERO(T) : T(buffInt));
		++m_GridPos;
		return true;
	}
	bool Skip1()
	{
		if (fscanf( m_FH.GetFP(), "%*d") != 0)
			return false;

		++m_GridPos;
		return true;
	}
	void SubstituteNoData(typename sequence_traits<T>::pointer buffI, typename sequence_traits<T>::pointer buffE)
	{}
};

template <typename T>
struct elem_reader;

template <bit_size_t N>
struct elem_reader<bit_value<N> > : read_buffered< bit_value<N> > {};

template<> struct elem_reader<UInt8>   : read_buffered< UInt8 > {};
template<> struct elem_reader<Int8>    : read_buffered< Int8  > {};
template<> struct elem_reader<Float32> : read_direct< Float32 > {};
template<> struct elem_reader<Float64> : read_direct< Float64 > {};
#if defined(DMS_TM_HAS_FLOAT80)
template<> struct elem_reader<Float80> : read_direct< Float80 > {};
#endif
template<> struct elem_reader<UInt16>  : read_direct< UInt16  > {};
template<> struct elem_reader<Int16>   : read_direct< Int16   > {};
template<> struct elem_reader<Int32>   : read_direct< Int32   > {};
template<> struct elem_reader<UInt32>  : read_direct< UInt32  > {};
template<> struct elem_reader<Int64>   : read_direct< Int64   > {};
template<> struct elem_reader<UInt64>  : read_direct< UInt64  > {};


UInt32 AsciiImp::CalcStripped(UInt32& cnt) const
{
	UInt32 stripped = cnt;
	MakeMin(cnt, NrOfCells());
	stripped -= cnt;
	return stripped;
}

template <typename T>
bool ReadSequence(elem_reader<T>& reader, typename sequence_traits<T>::pointer buffB, typename sequence_traits<T>::pointer buffE)
{
	typename sequence_traits<T>::pointer buffI = buffB;

	bool result = false;
	for (; buffE - buffI>= 16; buffI += 16)
		if (!reader.Read16(buffI))
			goto finish;

	for (; buffI != buffE; ++buffI)
		if (!reader.Read1(buffI))
			goto finish;
	result = true;
finish:
	reader.SubstituteNoData(buffB, buffI); // Fill stripped zone with noDataVal

	return result;
}

template <typename T>
bool SkipSequence(elem_reader<T>& reader, UInt32 cnt)
{
	for (; cnt; --cnt)
		if (!reader.Skip1())
			return false;
	return true;
}

template <typename T>
bool AsciiImp::ReadCells(typename sequence_traits<T>::pointer buff, const StgViewPortInfo& vpi)
{
	DBG_START("AsciiImp", "ReadCells", false);
	SizeT cnt = vpi.GetNrViewPortCells();
	DBG_TRACE(("cnt		  : %ld", cnt));

	dms_assert(IsOpen());
	UInt32 nrFileRows = NrOfRows();
	UInt32 nrFileCols = NrOfCols();

	dms_assert(vpi.IsNonScaling());
	dms_assert(vpi.GetCountColor() == -1);

	IPoint dataOffset = Round<4>(vpi.Offset());
	IRect dataRect = vpi.GetViewPortExtents() + dataOffset;
	IRect fileRect(IPoint(0,0), shp2dms_order(Int32(nrFileCols), Int32(nrFileRows)));

	elem_reader<T> reader;
	reader.Init(*this);

	if(!IsIntersecting(dataRect, fileRect))
	{
		fast_fill(buff, buff+cnt, UNDEFINED_OR_ZERO(T));
		return true;
	}
	dms_assert(dataRect.second.Row() > 0); // follows from IsIntersecting and nonnegativity of fileRect
	dms_assert(dataRect.second.Col() > 0); // follows from IsIntersecting and nonnegativity of fileRect

	if (dataRect == fileRect && vpi.IsNonScaling())
	{
		dms_assert(vpi.GetNrViewPortCells() == NrOfCells());

		return ReadSequence<T>(reader, buff, buff + cnt);
	}

#ifdef MG_DEBUG_DATA
	typename sequence_traits<T>::pointer debugBuffOrg = buff;
#endif

	if (dataRect.first.Row() < 0)
	{
		UInt32 skipRowCount = - dataRect.first.Row();
		typename sequence_traits<T>::pointer buffE = buff + _Width(dataRect) * skipRowCount;
		fast_fill(buff, buffE, UNDEFINED_OR_ZERO(T));
		buff = buffE;
		dataRect.first.Row() = 0;
	}

	UInt32 currFileRow = 0;
	UInt32 currDataRow = dataRect.first.Row();
	for (; currFileRow < currDataRow; ++currFileRow)
		if (!SkipSequence<T>(reader, nrFileCols))
			return false;


	UInt32 leftFillCount  = (dataRect.first.Col() < 0) ? (- dataRect.first.Col()) : 0;
	UInt32 leftSkipCount  = dataRect.first.Col() + leftFillCount;

	UInt32 rightFillCount = (UInt32(dataRect.second.Col()) > nrFileCols) ? (dataRect.second.Col() - nrFileCols) : 0;
	UInt32 rightSkipCount = rightFillCount + nrFileCols - dataRect.second.Col();

	UInt32 nrReadCols = nrFileCols - (leftSkipCount + rightSkipCount);
	dms_assert(nrReadCols + leftFillCount + rightFillCount == vpi.GetViewPortSize().Col());

	UInt32 lastDataRow = dataRect.second.Row();
	UInt32 lastReadRow = Min<UInt32>(lastDataRow, fileRect.second.Row());
	for (; currDataRow < lastReadRow; ++currDataRow)
	{
		if (leftFillCount)
		{
			typename sequence_traits<T>::pointer buffE = buff + leftFillCount;
			fast_fill(buff, buffE, UNDEFINED_OR_ZERO(T));
			buff = buffE;
		}
		else if (!SkipSequence<T>(reader, leftSkipCount))
			return false;

		typename sequence_traits<T>::pointer buffE = buff + nrReadCols;
		if (!ReadSequence<T>(reader, buff, buffE))
			return false;
		buff = buffE;

		if (rightFillCount)
		{
			buffE = buff + rightFillCount;
			fast_fill(buff, buffE, UNDEFINED_OR_ZERO(T));
			buff = buffE;
		}
		else if (!SkipSequence<T>(reader, rightSkipCount))
			return false;
	}
	if (currDataRow < lastDataRow)
	{
		typename sequence_traits<T>::pointer buffE = buff + vpi.GetViewPortSize().Col() * (lastDataRow - currDataRow);
		fast_fill(buff, buffE, UNDEFINED_OR_ZERO(T));
		buff = buffE;
		currDataRow = lastDataRow;
	}

	MGD_CHECKDATA(debugBuffOrg + vpi.GetNrViewPortCells() == buff);

	return true;
}

#if defined(DMS_TM_HAS_FLOAT80)
template STGIMPL_CALL bool AsciiImp::ReadCells<Float80>(Float80* buf, const StgViewPortInfo& vpi);
#endif
template STGIMPL_CALL bool AsciiImp::ReadCells<Float64>(Float64* buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<Float32>(Float32* buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<Int8   >(Int8*    buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt8  >(UInt8*   buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<Int16  >(Int16*   buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt16 >(UInt16*  buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<Int32  >(Int32*   buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt32 >(UInt32*  buf, const StgViewPortInfo& vpi);
#if defined(DMS_TM_HAS_INT64)
template STGIMPL_CALL bool AsciiImp::ReadCells<Int64  >(Int64*   buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt64 >(UInt64*  buf, const StgViewPortInfo& vpi);
#endif
template STGIMPL_CALL bool AsciiImp::ReadCells<Bool >(sequence_traits<Bool >::pointer buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt2>(sequence_traits<UInt2>::pointer buf, const StgViewPortInfo& vpi);
template STGIMPL_CALL bool AsciiImp::ReadCells<UInt4>(sequence_traits<UInt4>::pointer buf, const StgViewPortInfo& vpi);

#if defined(DMS_TM_HAS_FLOAT80)
template STGIMPL_CALL bool AsciiImp::WriteCells<Float80>(WeakStr fileName, SafeFileWriterArray* sfwa, const Float80* buf, UInt32 cnt);
#endif
template STGIMPL_CALL bool AsciiImp::WriteCells<Float64>(WeakStr fileName, SafeFileWriterArray* sfwa, const Float64* buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<Float32>(WeakStr fileName, SafeFileWriterArray* sfwa, const Float32* buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<Int8   >(WeakStr fileName, SafeFileWriterArray* sfwa, const Int8*    buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt8  >(WeakStr fileName, SafeFileWriterArray* sfwa, const UInt8*   buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<Int16  >(WeakStr fileName, SafeFileWriterArray* sfwa, const Int16*   buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt16 >(WeakStr fileName, SafeFileWriterArray* sfwa, const UInt16*  buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<Int32  >(WeakStr fileName, SafeFileWriterArray* sfwa, const Int32*   buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt32 >(WeakStr fileName, SafeFileWriterArray* sfwa, const UInt32*  buf, UInt32 cnt);
#if defined(DMS_TM_HAS_INT64)
template STGIMPL_CALL bool AsciiImp::WriteCells<Int64  >(WeakStr fileName, SafeFileWriterArray* sfwa, const Int64*   buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt64 >(WeakStr fileName, SafeFileWriterArray* sfwa, const UInt64*  buf, UInt32 cnt);
#endif
template STGIMPL_CALL bool AsciiImp::WriteCells<Bool >(WeakStr fileName, SafeFileWriterArray* sfwa, sequence_traits<Bool >::const_pointer buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt2>(WeakStr fileName, SafeFileWriterArray* sfwa, sequence_traits<UInt2>::const_pointer buf, UInt32 cnt);
template STGIMPL_CALL bool AsciiImp::WriteCells<UInt4>(WeakStr fileName, SafeFileWriterArray* sfwa, sequence_traits<UInt4>::const_pointer buf, UInt32 cnt);



template <typename T>
void AsciiImp::CountData(const StgViewPortInfo& vpi, T* colorCount)
{
	dms_assert(vpi.GetCountColor() != -1);
	throwErrorD("NYI", "AsciiImp::CountData");
}

template STGIMPL_CALL void AsciiImp::CountData<UInt8 >(const StgViewPortInfo& vpi, UInt8 *);
template STGIMPL_CALL void AsciiImp::CountData<UInt16>(const StgViewPortInfo& vpi, UInt16*);
template STGIMPL_CALL void AsciiImp::CountData<UInt32>(const StgViewPortInfo& vpi, UInt32*);

template <typename T> struct write_traits;

template <> struct write_traits<Int32>
{
	typedef int write_type;
	static CharPtr GetFormat16() { return "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d "; }
	static CharPtr GetFormat1 () { return "%d "; }
};

template <> struct write_traits<UInt32>
{
	typedef unsigned int write_type;
	static CharPtr GetFormat16() { return "%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u "; }
	static CharPtr GetFormat1 () { return "%u "; }
};

template <> struct write_traits<Int64>
{
	typedef int write_type;
	static CharPtr GetFormat16() { return "%I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d %I64d "; }
	static CharPtr GetFormat1 () { return "%I64d "; }
};

template <> struct write_traits<UInt64>
{
	typedef unsigned int write_type;
	static CharPtr GetFormat16() { return "%I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u %I64u "; }
	static CharPtr GetFormat1 () { return "%I64u "; }
};

template <> struct write_traits<Int16>
{
	typedef short write_type;
	static CharPtr GetFormat16() { return "%hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd %hd "; }
	static CharPtr GetFormat1 () { return "%hd "; }
};

template <> struct write_traits<UInt16>
{
	typedef unsigned short write_type;
	static CharPtr GetFormat16() { return "%hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu %hu "; }
	static CharPtr GetFormat1 () { return "%hu "; }
};

template <>
struct write_traits<UInt8> : write_traits<Int16> 
{};

template <>
struct write_traits<Int8> : write_traits<Int16> 
{};

template <bit_size_t N>
struct write_traits<bit_value<N> > : write_traits<Int16> 
{};


template <> struct write_traits<Float32>
{
	typedef float write_type;
	static CharPtr GetFormat16() { return "%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g "; }
	static CharPtr GetFormat1 () { return "%g "; }
};

template <> struct write_traits<Float64>
{
	typedef double write_type;
	static CharPtr GetFormat16() { return "%lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg %lg "; }
	static CharPtr GetFormat1 () { return "%lg "; }
};

#if defined(DMS_TM_HAS_FLOAT80)
template <> struct write_traits<Float80> : write_traits<Float64> {};
#endif

template<typename T>
bool AsciiImp::WriteCells(WeakStr fileName, SafeFileWriterArray* sfwa, typename sequence_traits<T>::const_pointer buf, UInt32 cnt)
{
	DBG_START("AsciiImp", "WriteCells", false);
	DBG_TRACE(("cnt		  : %ld", cnt));

	// Clip
	CalcStripped(cnt);

	// Write
	dms_assert(NrOfCols() != 0);
//	dms_assert(NoDataInMem() == NoDataInFile()); // substitution for write is not defined

	typedef write_traits<T> wtType;
	typedef typename wtType::write_type elem_type;
	SetNoDataInFile(UNDEFINED_VALUE(elem_type));
	SetNoDataIsIntegral(is_integral<elem_type>::value);
	if (! OpenForWrite(fileName, sfwa) )
		return false;

	typename sequence_traits<T>::const_pointer
		buffI = buf, 
		buffE = buf + cnt;

	while (buffI != buffE)
	{
		UInt32 remainingOnThisLine = NrOfCols();
		while (remainingOnThisLine && (buffE - buffI) > 0)
		{
			// optimization:
			if (remainingOnThisLine >= 16 && (buffE - buffI) >= 16)
			{
				fprintf( GetFP(), wtType::GetFormat16(), 
					Convert<elem_type>(buffI[ 0]), Convert<elem_type>(buffI[ 1]), Convert<elem_type>(buffI[ 2]), Convert<elem_type>(buffI[ 3]), 
					Convert<elem_type>(buffI[ 4]), Convert<elem_type>(buffI[ 5]), Convert<elem_type>(buffI[ 6]), Convert<elem_type>(buffI[ 7]), 
					Convert<elem_type>(buffI[ 8]), Convert<elem_type>(buffI[ 9]), Convert<elem_type>(buffI[10]), Convert<elem_type>(buffI[11]), 
					Convert<elem_type>(buffI[12]), Convert<elem_type>(buffI[13]), Convert<elem_type>(buffI[14]), Convert<elem_type>(buffI[15])
				);
				buffI += 16;
				remainingOnThisLine -= 16;
			}
			else
			{
				fprintf( GetFP(), wtType::GetFormat1(), Convert<elem_type>(*buffI) ); ++buffI;
				--remainingOnThisLine;
			}
		}
		// line breaks
		if (remainingOnThisLine == 0)
			fprintf(GetFP(), "\n");
	}
	return true;
}

// Fixed header format. Will be generalised when needed.
bool AsciiImp::ReadHeader()
{
    DBG_START("AsciiImp", "ReadHeader", false);

	if (!IsOpen() )
		return false;

	// format:
	//		ncols         560    (Altijd UInt32 )
	//		nrows         650    (Altijd UInt32 )
	//		xllcorner     0	     (Kan ook float zijn) 
	//		yllcorner     300000 (Kan ook float zijn, zoals bij Corine Aggr)
	//		cellsize      500    (Kan ook float zijn, zoals bij Corine Aggr)
	//		NODATA_value  -9999	 (is Int32 of Float32, afh. van datatype).
	return 
		fscanf( GetFP(), "%*s %uld\n", &m_NrCols        ) == 1
	&&	fscanf( GetFP(), "%*s %uld\n", &m_NrRows        ) == 1
	&&	fscanf( GetFP(), "%*s %lf\n", &m_dXllCorner    ) == 1
	&&	fscanf( GetFP(), "%*s %lf\n", &m_dYllCorner    ) == 1
	&&	fscanf( GetFP(), "%*s %lf\n", &m_dCellSize     ) == 1
	&&	fscanf( GetFP(), "%*s %lf\n", &m_dNODATA_InFile) == 1;
}


// Fixed header format. Will be generalised when needed.
bool AsciiImp::WriteHeader()
{
    DBG_START("AsciiImp", "WriteHeader", false);

	if (!IsOpen() )
		return false;

	// format:
	//		m_NrCols         560
	//		m_NrRows         650
	//		m_dXllCorner     0	
	//		m_dYllCorner     300000
	//		m_dCellSize      500
	//		NODATA_value  -9999	
	fprintf( GetFP(), "%s %ld\n", "ncols       ", m_NrCols);
	fprintf( GetFP(), "%s %ld\n", "nrows       ", m_NrRows );
	fprintf( GetFP(), "%s %lf\n", "xllcorner    ", m_dXllCorner );
	fprintf( GetFP(), "%s %lf\n", "yllcorner    ", m_dYllCorner );
	fprintf( GetFP(), "%s %lf\n", "cellsize     ", m_dCellSize );

	if (m_bNODATA_IsIntegral)
		fprintf( GetFP(), "%s %lg\n", "NODATA_value ", m_dNODATA_InFile);
	else
		fprintf( GetFP(), "%s %lf\n", "NODATA_value ", m_dNODATA_InFile);

	return true;
}


// Black box copy
void AsciiImp::CopyHeaderInfoFrom(const AsciiImp & src)
{
    DBG_START("AsciiImp", "CopyHeaderInfoFrom", false);
	
	m_NrCols = src.m_NrCols;     
	m_NrRows = src.m_NrRows;    
	m_dXllCorner = src.m_dXllCorner;
	m_dYllCorner = src.m_dYllCorner;
	m_dCellSize = src.m_dCellSize;     
	m_dNODATA_InFile = src.m_dNODATA_InFile; 

}

// Get rid of file connection
void AsciiImp::Close()
{
    DBG_START("AsciiImp", "Close", false);

	CloseFH();
}
