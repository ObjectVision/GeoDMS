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

#if !defined(__RTC_MEM_GRID_H)
#define  __RTC_MEM_GRID_H

#include "geo/SequenceTraits.h"
#include "geo/Point.h"

// *****************************************************************************
//											INTERFACE
// *****************************************************************************

template <typename T>
struct ptr_type
{
	using type = typename sequence_traits<T>::seq_t::iterator;
};

template <typename T>
struct ptr_type<const T>
{
	using type = typename sequence_traits<T>::cseq_t::const_iterator;
};

template <typename T> using ptr_type_t = typename ptr_type<T>::type;

typedef Int32             ICoordType;
typedef Point<ICoordType> IGridPoint;
typedef Range<IGridPoint> IGridRect;

typedef UInt32            UCoordType;
typedef Point<UCoordType> UGridPoint;
typedef Range<UGridPoint> UGridRect;

template <typename T, typename UInt>
struct TGridBase
{
	typedef UInt             CoordType;
	typedef Point<CoordType> GridPoint;
	typedef Range<GridPoint> GridRect;

	using data_ptr = ptr_type_t<T>;

	TGridBase() : m_Size(0, 0), m_Data() 
	{}

	TGridBase(const GridPoint& size, data_ptr data)
		: m_Size(size), m_Data(data)
	{}

	data_ptr  begin()                const { return m_Data; }
	data_ptr  end  ()                const { return begin() + size(); }
	data_ptr  rowptr (CoordType row) const { return m_Data + SizeT(row) *  SizeT(m_Size.Col()); }
	data_ptr  elemptr(GridPoint pos) const { return rowptr(pos.Row())+pos.Col(); }
	data_ptr  GetDataPtr()           const { assert(m_Data); return begin(); }
	void SetDataPtr(data_ptr dataPtr)      { m_Data = dataPtr; }
	const GridPoint& GetSize()       const { return m_Size; }
	SizeT size()                     const { return Cardinality(m_Size); }
	bool  empty()                    const { return !m_Size.X() || !m_Size.Y(); }
	SizeT NrBytesPerRow()            const { return SizeT(m_Size.Col()) * sizeof(T); }
private:
	GridPoint  m_Size;
	data_ptr   m_Data;
};

template <typename T> using UGrid = TGridBase<T, UInt32>;
template <typename T> using U64Grid = TGridBase<T, UInt64>;

using UGridPoint = UPoint;

typedef UGrid<      Int32>   UInt32Grid;
typedef UGrid<const Int32>   UCInt32Grid;
typedef UGrid<      UInt32>  UUInt32Grid;
typedef UGrid<const UInt32>  UCUInt32Grid;
typedef UGrid<      UInt16>  UUInt16Grid;
typedef UGrid<const UInt16>  UCUInt16Grid;
typedef UGrid<      Int16  > UInt16Grid;
typedef UGrid<const Int16  > UCInt16Grid;
typedef UGrid<      UInt8 >  UUInt8Grid;
typedef UGrid<const UInt8 >  UCUInt8Grid;
typedef UGrid<      Bool  >  UBoolGrid;
typedef UGrid<const Bool  >  UCBoolGrid;

typedef UGrid<      Float32> UFloat32Grid;
typedef UGrid<const Float32> UCFloat32Grid;
typedef UGrid<      Float64> UFloat64Grid;
typedef UGrid<const Float64> UCFloat64Grid;

#endif //!defined(__RTC_MEM_GRID_H)
