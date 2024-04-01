// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TILECHANNEL_H)
#define __TIC_TILECHANNEL_H

#include "TileLock.h"

//----------------------------------------------------------------------
// struct  : tile_read_channel
//----------------------------------------------------------------------

template <typename V>
struct tile_read_channel
{
	tile_id currTile = 0, lastTile = 0;

	SharedPtr<const DataArray<V>> tileArray;

	typename DataArray<V>::locked_cseq_t currTileData;
	typename DataArray<V>::const_iterator currPtr = {}, lastPtr = {};

	tile_read_channel(const DataArray<V>* tileArray_)
		: tileArray(tileArray_)
		, lastTile(tileArray_->GetTiledRangeData()->GetNrTiles())
	{
		InitCurrTile();
	}

	void InitCurrTile()
	{
		if (currTile == lastTile)
			return;
		currTileData = tileArray->GetTile(currTile);
		currPtr = currTileData.begin();
		lastPtr = currTileData.end();
		assert(!AtEnd());
	}

	auto operator *() const
	{
		assert(!AtEnd());
		return *currPtr;
	}

	void operator ++ ()
	{
		assert(!AtEnd());
		++currPtr;
		if (currPtr == lastPtr)
		{
			assert(currTile != lastTile);
			++currTile;
			InitCurrTile();
		}
	}
	bool AtEnd() const
	{
		assert(currPtr != lastPtr || currTile == lastTile);
		return currPtr == lastPtr;
	}

};

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
	if (!IsDefined(src))
		MakeUndefined(dst);
	else
	{
		dst.reserve(src.size());
		for (auto s : src)
			dst.emplace_back(Convert<T>(s));
	}
}

template <typename T>
void convert_assign(SA_Reference<T> dst, SA_ConstReference<T> src)
{
	dst = src;
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

	template <std::random_access_iterator CIter>
	void Write(CIter first, CIter last)
	{
		SizeT numElems = last - first; // std::distance(first, last)
		while (true)
		{
			SizeT numWritable = NrFreeInTile();
			SizeT numWrite = Min<SizeT>(numWritable, numElems);
			CIter oldFirst = first;
			first += numWrite; // std::advance(first, numWrite);
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
	void Write(CIter first, CIter last)
	{
		if (first == last)
			return;

		while (true)
		{
			auto nrRemaining = NrFreeInTile();
			auto tileIter = Curr();
			assert(first != last);

			while (nrRemaining--)
			{
				*tileIter++ = *first++;
				if (first == last)
				{
					m_Cursor.second = m_LockedSeq.size() - nrRemaining;	
					return;
				}
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
		assert(m_Cursor.second < m_LockedSeq.size());
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
		assert(m_Cursor.second <= m_LockedSeq.size());
		return m_Cursor.second >= m_LockedSeq.size();
	}

	bool IsEndOfChannel()
	{
		auto tn = GetTiledRangeData()->GetNrTiles();
		if (tn == 0)
		{
			assert(m_Cursor.first == 0);
			assert(m_Cursor.second == 0);
			return true;
		}
		while (IsEndOfTile() && m_Cursor.first < tn-1)
			GetNextTile();
		assert(m_Cursor.first == tn - 1 || !IsEndOfTile());
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

