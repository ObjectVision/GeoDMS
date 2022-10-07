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

#if !defined(__TIC_TILECHANNEL_H)
#define __TIC_TILECHANNEL_H

#include "TileLock.h"

//----------------------------------------------------------------------
// struct  : abstr_tile_write_channel
//----------------------------------------------------------------------

struct abstr_tile_write_channel
{
	abstr_tile_write_channel(AbstrDataObject* tf)
		: m_TileFunctor(tf)
	{
		if (tf && tf->GetTiledRangeData()->GetNrTiles())
			m_LockedSeq = tf->GetWritableTileLock(0, dms_rw_mode::write_only_all);
	}

	auto GetTiledRangeData() const
	{
		return GetTileFunctor()->GetTiledRangeData();
	}

	SizeT NrFreeInTile()
	{
		return GetTiledRangeData()->GetTileSize(m_Cursor.first) - m_Cursor.second;
	}

/* REMOVE
	iterator Curr()
	{
		return m_LockedSeq.begin() + m_Cursor.second;
	}
	*/

	AbstrDataObject* GetTileFunctor() const
	{
		return m_TileFunctor;
	}

	void GetNextTile()
	{
		MG_CHECK(m_Cursor.first < GetTiledRangeData()->GetNrTiles());
		m_LockedSeq = GetTileFunctor()->GetWritableTileLock(++m_Cursor.first);
		m_Cursor.second = 0;
	}
	void SkipEOT()
	{
		while (IsEndOfTile())
			GetNextTile();
	}
/*
	template <typename CIter>
	void Write(CIter first, CIter last)
	{
		SizeT numElems = std::distance(first, last);
		while (true)
		{
			SizeT numWritable = NrFreeInTile();
			SizeT numWrite = Min<SizeT>(numWritable, numElems);
			CIter oldFirst = first;
			first += numWrite;
			fast_copy(oldFirst, first, Curr());
			numElems -= numWrite;
			if (!numElems)
			{
				m_Cursor.second += numWrite;
				return;
			}
			GetNextTile();
		}
	}
	template <typename CIter>
	void Write(CIter first, SizeT numElems)
	{
		Write(first, first + numElems);
	}

	template<typename V>
	void Write(V&& value)
	{
		SkipEOT();
		dms_assert(m_Cursor.second < m_LockedSeq.size());
		m_LockedSeq[m_Cursor.second++] = Convert<T>(std::forward<V>(value));
	}

	void WriteCounter(SizeT n)
	{
		SizeT i = 0;
		while (n)
		{
			SkipEOT();

			SizeT m = Min<SizeT>(NrFreeInTile(), n);

			typename tile_write_channel<T>::iterator curr = Curr();
			for (SizeT e = i + m; i != e; ++curr, ++i)
				*curr = i;
			n -= m;
			m_Cursor.second += m;
		}
	}
*/
	void FillWithUInt32Values(SizeT numElems, UInt32 value)
	{
		while (true)
		{
			SizeT numWritable = NrFreeInTile();
			SizeT numWrite = Min<SizeT>(numWritable, numElems);
			GetTileFunctor()->FillWithUInt32Values(m_Cursor, numWrite, value);
			numElems -= numWrite;
			if (!numElems)
			{
				m_Cursor.second += numWrite;
				return;
			}
			GetNextTile();
		}
	}
	void WriteUInt32(UInt32 value)
	{
		FillWithUInt32Values(1, value);
	}

	explicit operator bool() const { return m_TileFunctor; }
	bool IsEndOfTile() const
	{
//		dms_assert(m_Cursor.second <= m_LockedSeq.size());
		return m_Cursor.second >= GetTiledRangeData()->GetTileSize(m_Cursor.first);
	}

	bool IsEndOfChannel()
	{
		auto tn = GetTiledRangeData()->GetNrTiles();
		if (tn == 0)
		{
			dms_assert(m_Cursor.first == 0);
			dms_assert(m_Cursor.second == 0);
			return true;
		}
		while (IsEndOfTile() && m_Cursor.first < tn - 1)
			GetNextTile();
		dms_assert(m_Cursor.first == tn - 1 || !IsEndOfTile());
		return IsEndOfTile();
	}
private:
	TileRef           m_LockedSeq;
	tile_loc          m_Cursor = { 0, 0 };
	AbstrDataObject*  m_TileFunctor = nullptr;
};

struct locked_abstr_tile_write_channel : DataWriteLock, abstr_tile_write_channel
{
	locked_abstr_tile_write_channel(AbstrDataItem* res)
		: DataWriteLock(res)
		, abstr_tile_write_channel(this->get())
	{}
};

template <typename T, typename U>
void convert_assign(T& dst, U&& src)
{
	dst = Convert<T>(std::forward<U>(src));
}

template <typename T, typename USeq>
void convert_assign(SA_Reference<T> dst, USeq&& src)
{
	dst.reserve(src.size());
	for (auto s : src)
		dst.emplace_back(Convert<T>(s));
}

template <typename T>
void convert_assign(SA_Reference<T> dst, SA_ConstReference<T> src)
{
	dst.reserve(src.size());
	for (auto s : src)
		dst.emplace_back(s);
}

template <bit_size_t N, typename Block, typename U>
void convert_assign(bit_reference<N, Block> dst, U&& src)
{
	dst = Convert<bit_value<N>>(std::forward<U>(src));
}

//----------------------------------------------------------------------
// struct  : tile_write_channel
//----------------------------------------------------------------------

template <typename T>
struct tile_write_channel
{
	typedef typename sequence_traits<T>::seq_t seq_type;
	typedef locked_seq<seq_type, TileRef>      locked_seq_type;
	typedef typename seq_type::iterator        iterator;

	tile_write_channel(TileFunctor<T>* tf)
		: m_TileFunctor(tf)
	{
		if (tf && tf->GetTiledRangeData()->GetNrTiles())
			m_LockedSeq = tf->GetWritableTile(0, dms_rw_mode::write_only_all);
	}

	TileFunctor<T>* GetTileFunctor() const
	{
		return m_TileFunctor;
	}
	auto GetTiledRangeData() const
	{
		return GetTileFunctor()->GetTiledRangeData();
	}

	SizeT NrFreeInTile()
	{
		return m_LockedSeq.size() - m_Cursor.second;
	}
	iterator Curr()
	{
		return m_LockedSeq.begin() + m_Cursor.second;
	}

	void GetNextTile()
	{
		MG_CHECK(m_Cursor.first < GetTileFunctor()->GetTiledRangeData()->GetNrTiles());
		m_LockedSeq = GetTileFunctor()->GetWritableTile(++m_Cursor.first);
		m_Cursor.second = 0;
	}
	void SkipEOT()
	{
		while (IsEndOfTile())
			GetNextTile();
	}

	template <typename CIter>
	void Write(CIter first, CIter last)
	{
		SizeT numElems = std::distance(first, last);
		while (true)
		{
			SizeT numWritable = NrFreeInTile();
			SizeT numWrite = Min<SizeT>(numWritable, numElems);
			CIter oldFirst = first;
			first += numWrite;
			fast_copy(oldFirst, first, Curr());
			numElems -= numWrite;
			if (!numElems)
			{
				m_Cursor.second += numWrite;
				return;
			}
			GetNextTile();
		}
	}
	template <typename CIter>
	void Write(CIter first, SizeT numElems)
	{
		Write(first, first + numElems);
	}

	template<typename V>
	void Write(V&& value)
	{
		SkipEOT();
		dms_assert(m_Cursor.second < m_LockedSeq.size());
		convert_assign(m_LockedSeq[m_Cursor.second++], std::forward<V>(value));
	}

	void WriteCounter(SizeT n)
	{
		SizeT i = 0;
		while (n)
		{
			SkipEOT();

			SizeT m = Min<SizeT>(NrFreeInTile(), n);

			typename tile_write_channel<T>::iterator curr = Curr();
			for (SizeT e = i+m; i!=e; ++curr, ++i)
				*curr = i;
			n -= m;
			m_Cursor.second += m;
		}
	}
	void WriteConst(T value, SizeT numElems)
	{
		while (true)
		{
			SizeT numWritable = NrFreeInTile();
			SizeT numWrite = Min<SizeT>(numWritable, numElems);
			fast_fill(Curr(), Curr()+numWrite, value);
			numElems -= numWrite;
			if (!numElems)
			{
				m_Cursor.second += numWrite;
				return;
			}
			GetNextTile();
		}
	}

	explicit operator bool() const { return m_LockedSeq.GetObj(); }
	bool IsEndOfTile() const 
	{
		dms_assert(m_Cursor.second <= m_LockedSeq.size());
		return m_Cursor.second >= m_LockedSeq.size();
	}

	bool IsEndOfChannel()
	{
		auto tn = GetTiledRangeData()->GetNrTiles();
		if (tn == 0)
		{
			dms_assert(m_Cursor.first == 0);
			dms_assert(m_Cursor.second == 0);
			return true;
		}
		while (IsEndOfTile() && m_Cursor.first < tn-1)
			GetNextTile();
		dms_assert(m_Cursor.first == tn - 1 || !IsEndOfTile());
		return IsEndOfTile();
	}
private:
	locked_seq_type  m_LockedSeq;
	tile_loc         m_Cursor = { 0, 0 };
	TileFunctor<T>*  m_TileFunctor = nullptr;
};

template <typename T>
struct locked_tile_write_channel : DataWriteLock, tile_write_channel<T>
{
	locked_tile_write_channel(AbstrDataItem* res)
		:	DataWriteLock(res)
		,	tile_write_channel<T>(mutable_opt_array_cast<T>(this->get()))
	{}
};


#endif //!defined(__TIC_TILECHANNEL_H)

