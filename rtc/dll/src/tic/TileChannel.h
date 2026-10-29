// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_TILECHANNEL_H)
#define __TIC_TILECHANNEL_H

#include "TileLock.h"

#include "mem/tiledata.h"
#include "ptr/OwningPtrSizedArray.h"
#include "vt/ElemTraits.h"

#include "parallel/dms_combinable.h"

#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

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
		m_LockedSeq = GetTileFunctor()->GetWritableTileLock(++m_Cursor.first, dms_rw_mode::write_only_all);
		m_Cursor.second = 0;
	}
	void SkipEOT()
	{
		while (IsEndOfTile())
			GetNextTile();
	}

	void FillWithUInt32Values(SizeT nrElems, UInt32 value)
	{
		while (true)
		{
			SizeT nrWritable = NrFreeInTile();
			SizeT nrToWrite = Min<SizeT>(nrWritable, nrElems);
			GetTileFunctor()->FillWithUInt32Values(m_Cursor, nrToWrite, value);
			nrElems -= nrToWrite;
			if (!nrElems)
			{
				m_Cursor.second += nrToWrite;
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
		dst.reserve(src.size() MG_DEBUG_ALLOCATOR_SRC("convert_assign"));
		for (auto s : src)
			dst.emplace_back(MG_DEBUG_ALLOCATOR_FIRST("convert_assign") Convert<T>(s));
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
		SizeT nrElems = last - first; // std::distance(first, last)
		while (true)
		{
			SizeT nrWritable = NrFreeInTile();
			SizeT nrToWrite = Min<SizeT>(nrWritable, nrElems);
			CIter oldFirst = first;
			first += nrToWrite; // std::advance(first, nrToWrite);
			fast_copy(oldFirst, first, Curr());
			nrElems -= nrToWrite;
			if (!nrElems)
			{
				m_Cursor.second += nrToWrite;
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
	void Write(CIter first, SizeT nrElems)
	{
		Write(first, first + nrElems);
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
	void WriteZeroes( SizeT nrElems)
	{
		while (true)
		{
			SizeT nrWritable = NrFreeInTile();
			SizeT nrToWrite = Min<SizeT>(nrWritable, nrElems);
			fast_zero(Curr(), Curr()+nrToWrite);
			nrElems -= nrToWrite;
			if (!nrElems)
			{
				m_Cursor.second += nrToWrite;
				return;
			}
			GetNextTile();
		}
	}

	void WriteConst(T value, SizeT nrElems)
	{
		while (true)
		{
			SizeT nrWritable = NrFreeInTile();
			SizeT nrToWrite = Min<SizeT>(nrWritable, nrElems);
			fast_fill(Curr(), Curr() + nrToWrite, value);
			nrElems -= nrToWrite;
			if (!nrElems)
			{
				m_Cursor.second += nrToWrite;
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

//----------------------------------------------------------------------
// #1327: a tiled result whose count is known only at the end, produced in parallel in a fixed order
//
// tile_appender writes the elements of one attribute in order, by one writer at a time: into the tiles of an array
// whose domain already has its count, or into a chain of tiles of its own. A chain has the tile size that a domain of
// a numeric value type gets from SetCount (default_tile_size, 2^16), so after the last element FinishChain cuts the
// last tile to its elements, the domain gets the count, and AdoptInto hands the tiles to the array of the attribute.
//
// row_queue and ordered_turn let parallel producers deliver their rows in the order of their item numbers (origin
// zones, say) without counting them first. The producer whose item is next writes it straight into the appenders. One
// that is ahead appends its rows to a queue of its own and goes on with its next item. Whoever has the turn then
// writes the finished successors from their queues, up to the first item that is not finished. A producer that gets
// its items in increasing order, as parallel_for and parallel_tileloop hand them out, empties its queue in the order it
// filled it; producer_queues gives an item that a waiting thread runs inside another item a queue of its own.
// ordered_row_sink puts these together for the attributes of one result (join_near_values, connect_matrix).
//----------------------------------------------------------------------

template <typename V>
struct tile_appender
{
	using locked_seq_t = typename TileFunctor<V>::locked_seq_t;
	static constexpr SizeT chain_tile_size = SizeT(1) << log2_default_segment_size;

	// into the tiles of target, whose domain has its count; a null target leaves the appender inactive
	void InitDirect(TileFunctor<V>* target)
	{
		assert(!m_IsActive);
		if (!target)
			return;
		m_Target = target;
		m_IsActive = true;
		m_MaxCount = target->GetTiledRangeData()->GetElemCount();
		// rows of a sequence attribute that are not written keep the empty sequence they were allocated with
		m_TileRwMode = has_fixed_elem_size_v<V> ? dms_rw_mode::write_only_all : dms_rw_mode::write_only_mustzero;
	}
	// into a chain of tiles of its own; maxCount is the highest count that the domain can take
	void InitChain(SizeT maxCount)
	{
		assert(!m_IsActive);
		m_IsActive = true;
		m_MaxCount = maxCount;
	}

	bool  IsActive()   const { return m_IsActive; }
	SizeT Count()      const { return m_Count; }
	bool  IsComplete() const { return m_Count == m_MaxCount; } // into a target: all its elements are written

	// elements of a fixed size, given as V or as a type that converts to it (UInt8 for Bool, see ordered_row_elem_t)
	template <typename S>
	void Append(const S* first, SizeT n)
	{
		AddCount(n);
		while (n)
		{
			if (m_Pos == m_TileSize)
				NextTile();
			SizeT m = std::min<SizeT>(n, m_TileSize - m_Pos);
			if constexpr (std::is_same_v<S, V>)
				fast_copy(first, first + m, m_Curr.begin() + m_Pos);
			else
			{
				auto out = m_Curr.begin() + m_Pos;
				for (SizeT i = 0; i != m; ++i, ++out)
					*out = V(first[i]);
			}
			first += m; n -= m; m_Pos += m;
		}
	}

	// one row of a sequence attribute, V being the container of E
	template <typename E>
	void AppendSeq(const E* first, SizeT n)
	{
		AddCount(1);
		if (m_Pos == m_TileSize)
			NextTile();
		auto row = m_Curr[m_Pos++];
		row.resize_uninitialized(n MG_DEBUG_ALLOCATOR_SRC("tile_appender"));
		std::copy(first, first + n, row.begin());
	}
	// rows of a sequence attribute that keep their empty sequence
	void AppendEmptySeqs(SizeT n)
	{
		AddCount(n);
		while (n)
		{
			if (m_Pos == m_TileSize)
				NextTile();
			SizeT m = std::min<SizeT>(n, m_TileSize - m_Pos);
			n -= m; m_Pos += m;
		}
	}

	// releases the tile being written, which a FileTileArray needs before its write ends
	void Close() { m_Curr = locked_seq_t(); m_TileSize = 0; }

	// chain: cuts the last tile to its elements and returns the count, which the domain gets next
	SizeT FinishChain()
	{
		assert(!m_Target);
		Close();
		// resizeSO keeps the elements; reallocSO need not, and a BitVector (Bool) empties its blocks before it reallocates
		if (!m_Tiles.empty() && m_Pos < chain_tile_size)
			m_Tiles.back()->resizeSO(m_Pos, false MG_DEBUG_ALLOCATOR_SRC("tile_appender"));
		return m_Count;
	}
	// chain: hands the tiles to the array of the attribute, after its domain got the count of FinishChain
	void AdoptInto(TileFunctor<V>* tf)
	{
		assert(!m_Target);
		auto trd = tf->GetTiledRangeData();
		MG_CHECK(trd->GetElemCount() == m_Count);
		if (!m_Count)
			return; // an empty chain has no tiles, where an empty domain can have one tile of no elements
		MG_CHECK(trd->GetNrTiles() == m_Tiles.size());
		for (tile_id t = 0, tn = trd->GetNrTiles(); t != tn; ++t)
			tf->AdoptTile(t, std::move(m_Tiles[t]));
		m_Tiles.clear();
	}
	// chain: hands the tiles over, for an attribute whose values are made from them tile by tile (IndexTileAssigner)
	auto TakeTiles() -> std::vector<std::shared_ptr<tile<V>>>
	{
		assert(!m_Target);
		return std::move(m_Tiles);
	}

private:
	void AddCount(SizeT n)
	{
		if (n > m_MaxCount - m_Count)
			throwErrorF("tile_appender", "the result has more than the {} elements that its domain can hold", m_MaxCount);
		m_Count += n;
	}
	void NextTile()
	{
		if (m_Target)
		{
			MG_CHECK(m_NextTile < m_Target->GetTiledRangeData()->GetNrTiles());
			m_Curr = m_Target->GetWritableTile(m_NextTile++, m_TileRwMode);
		}
		else
		{
			auto newTile = std::make_shared<tile<V>>();
			newTile->reallocSO(chain_tile_size, false MG_DEBUG_ALLOCATOR_SRC("tile_appender"));
			m_Curr = locked_seq_t(std::static_pointer_cast<void>(newTile), GetSeq(*newTile));
			m_Tiles.emplace_back(std::move(newTile));
		}
		m_TileSize = m_Curr.size();
		m_Pos = 0;
	}

	TileFunctor<V>* m_Target = nullptr;
	dms_rw_mode     m_TileRwMode = dms_rw_mode::write_only_all;
	tile_id         m_NextTile = 0;
	std::vector<std::shared_ptr<tile<V>>> m_Tiles; // the chain
	locked_seq_t    m_Curr;
	SizeT           m_TileSize = 0; // of m_Curr, kept here because an empty sequence_array_ref has no array to ask
	SizeT           m_Pos = 0; // in m_Curr
	SizeT           m_Count = 0, m_MaxCount = 0;
	bool            m_IsActive = false;
};

// A queue of elements in blocks of a tile's size from the allocation stock. One owner appends at the back, and the one
// who has the turn reads from the front, possibly in the block that the owner is still writing: that is allowed for
// elements of a fixed size, as the two use different parts of it. A block goes back to the stock once the owner has
// filled it and all its elements are read, so a block holds the rows of as many items as fit in it.
template <typename T>
struct row_queue
{
	static_assert(has_fixed_elem_size_v<T>);
	static constexpr SizeT block_size = SizeT(1) << log2_default_segment_size;

	// the owner
	void Push(const T* first, SizeT n)
	{
		while (n)
		{
			if (m_WritePos == block_size)
				AddBlock();
			SizeT m = std::min<SizeT>(n, block_size - m_WritePos);
			fast_copy(first, first + m, m_WriteData + m_WritePos);
			first += m; n -= m; m_WritePos += m;
		}
	}

	// the one who has the turn: hands the next n elements to consume(const T* first, SizeT n), in one or more parts
	template <typename Consumer>
	void Pop(SizeT n, Consumer&& consume)
	{
		while (n)
		{
			const T* data;
			{
				auto lock = std::lock_guard(m_BlocksMutex);
				MG_CHECK(m_Head < m_Blocks.size());
				data = m_Blocks[m_Head].begin();
			}
			SizeT m = std::min<SizeT>(n, block_size - m_ReadPos);
			consume(data + m_ReadPos, m);
			n -= m; m_ReadPos += m;
			if (m_ReadPos == block_size)
				DropFrontBlock();
		}
	}

private:
	void AddBlock()
	{
		OwningPtrSizedArray<T> block(block_size, dont_initialize MG_DEBUG_ALLOCATOR_SRC("row_queue"));
		m_WriteData = block.begin();
		m_WritePos = 0;
		auto lock = std::lock_guard(m_BlocksMutex);
		m_Blocks.emplace_back(std::move(block));
	}
	void DropFrontBlock()
	{
		OwningPtrSizedArray<T> dropped; // freed after the lock is released
		{
			auto lock = std::lock_guard(m_BlocksMutex);
			dropped.swap(m_Blocks[m_Head++]);
			if (2 * m_Head >= m_Blocks.size())
			{
				m_Blocks.erase(m_Blocks.begin(), m_Blocks.begin() + m_Head);
				m_Head = 0;
			}
		}
		m_ReadPos = 0;
	}

	std::mutex m_BlocksMutex; // guards m_Blocks and m_Head, not the elements
	std::vector<OwningPtrSizedArray<T>> m_Blocks;
	SizeT m_Head = 0;
	T*    m_WriteData = nullptr;
	SizeT m_WritePos = block_size; // in the last block; block_size means that the next Push needs a new block
	SizeT m_ReadPos = 0; // in m_Blocks[m_Head]
};

// Who writes next. Deliver(k, ...) is called once for each item k in [0, nrItems), by its producer, from any thread.
// writeDirect() writes item k into the appenders, enqueue() puts it in the producer's queue and returns what drain(record)
// needs to write it from there later. Only the one who has the turn writes into the appenders, so they need no lock.
template <typename Record>
struct ordered_turn
{
	explicit ordered_turn(SizeT nrItems)
		: m_Records(nrItems)
		, m_IsQueued(nrItems, false)
	{}

	template <typename WriteDirect, typename Enqueue, typename Drain>
	void Deliver(SizeT k, WriteDirect&& writeDirect, Enqueue&& enqueue, Drain&& drain)
	{
		assert(k < m_IsQueued.size());
		auto lock = std::unique_lock(m_Mutex);
		if (k == m_Next)
		{
			lock.unlock();
			writeDirect();
			lock.lock();
			m_Next = k + 1;
		}
		else
		{
			lock.unlock();
			Record record = enqueue();
			lock.lock();
			m_Records[k] = std::move(record);
			m_IsQueued[k] = true;
			if (k != m_Next)
				return; // whoever writes item k-1 will find it
		}
		// the turn is ours: write the finished successors
		while (m_Next < m_IsQueued.size() && m_IsQueued[m_Next])
		{
			SizeT j = m_Next;
			m_IsQueued[j] = false;
			lock.unlock();
			drain(m_Records[j]);
			lock.lock();
			m_Next = j + 1;
		}
	}

	SizeT NrWritten() const { return m_Next; }
	SizeT NrItems()   const { return m_IsQueued.size(); }

private:
	std::mutex          m_Mutex; // guards m_Next and m_IsQueued
	SizeT               m_Next = 0; // the first item that is not written yet
	std::vector<Record> m_Records;
	std::vector<bool>   m_IsQueued;
};

// The queues of the producers: one set per thread and per level of nesting. A thread that waits inside an item (for a
// tile that another thread computes: OperationContext::Join, DoWorkWhileWaiting) can steal a later item of the same
// loop and run it there. That item gets the set of the next level, so that each set receives its items in increasing
// order and its queues stay first in, first out. A scope lasts as long as the work of one item.
template <typename Queues>
struct producer_queues
{
private:
	struct state
	{
		SizeT depth = 0;
		std::vector<std::unique_ptr<Queues>> queues; // a Record of ordered_turn points to a set, which therefore stays where it is
	};
	dms_combinable<state> m_States;

public:
	struct scope
	{
		explicit scope(producer_queues& owner)
			: m_State(owner.m_States.local())
			, m_Level(m_State.depth++)
		{
			if (m_State.queues.size() == m_Level)
				m_State.queues.emplace_back(std::make_unique<Queues>());
		}
		~scope() { --m_State.depth; }
		scope(const scope&) = delete;
		scope& operator=(const scope&) = delete;

		Queues& queues() const { return *m_State.queues[m_Level]; }

	private:
		state& m_State;
		SizeT  m_Level;
	};
};

// How many elements of an input tile (points, features) an operator gives to an ordered_row_sink as one item. A producer
// holds the rows of one item in buffers of its own before it delivers them, so this bounds those buffers to the rows of
// so many elements; the turn is taken once per item.
constexpr SizeT ordered_item_size = 1024;

// The first item number of each tile, when each tile is cut into items of ordered_item_size elements: tile t has the
// items [result[t], result[t+1]). tileSize(t) gives the number of elements of tile t.
template <typename TileSize>
auto ordered_item_bases(tile_id nrTiles, TileSize&& tileSize) -> std::vector<SizeT>
{
	std::vector<SizeT> result(nrTiles + 1, 0);
	for (tile_id t = 0; t != nrTiles; ++t)
		result[t + 1] = result[t] + (SizeT(tileSize(t)) + ordered_item_size - 1) / ordered_item_size;
	return result;
}

// the type in which a producer gives the rows of an attribute of type T: a Bool as a byte, 0 or 1, as a packed bit cannot
// be pointed to and std::vector<bool> has no data()
template <typename T>
using ordered_row_elem_t = std::conditional_t<std::is_same_v<T, Bool>, UInt8, T>;

// The attributes of a result whose count is known only at the end, made by parallel producers in the order of their item
// numbers, each attribute in a chain of tiles of its own (tile_appender, row_queue and ordered_turn above). A producer
// opens a Producer() scope around all the work of one item and delivers the rows of the item once, one array per
// attribute in ordered_row_elem_t. After the run, Finish() returns the count for the domain, and each
// attribute takes its tiles with Column<i>().AdoptInto, or a relation with IndexTileAssigner (IndexAssigner.h).
template <typename... Ts>
struct ordered_row_sink
{
	static_assert(((has_fixed_elem_size_v<Ts> && (!is_bitvalue_v<Ts> || std::is_same_v<Ts, Bool>)) && ...));
	using queues_t = std::tuple<row_queue<ordered_row_elem_t<Ts>>...>;
	using producer = typename producer_queues<queues_t>::scope;

	// maxCount is the highest count that the domain of the result can take
	ordered_row_sink(SizeT nrItems, SizeT maxCount)
		: m_Turn(nrItems)
	{
		std::apply([maxCount](auto&... appender) { (appender.InitChain(maxCount), ...); }, m_Appenders);
	}

	producer Producer() { return producer(m_Queues); }

	// once for each item k in [0, nrItems), by the producer of the item: nrRows rows, one array per attribute
	void Deliver(SizeT k, producer& p, SizeT nrRows, const ordered_row_elem_t<Ts>*... columns)
	{
		m_Turn.Deliver(k
		,	[&] { Write(std::index_sequence_for<Ts...>(), nrRows, columns...); }
		,	[&] { queues_t& queues = p.queues(); Enqueue(std::index_sequence_for<Ts...>(), queues, nrRows, columns...); return Record{ &queues, nrRows }; }
		,	[this](const Record& record) { Drain(std::index_sequence_for<Ts...>(), *record.queues, record.nrRows); }
		);
	}

	// after the run: cuts the chains and returns the number of rows
	SizeT Finish()
	{
		MG_CHECK(m_Turn.NrWritten() == m_Turn.NrItems());
		SizeT nrRows = UNDEFINED_VALUE(SizeT);
		std::apply([&nrRows](auto&... appender)
			{
				auto finish = [&nrRows](auto& a)
				{
					SizeT n = a.FinishChain();
					MG_CHECK(!IsDefined(nrRows) || n == nrRows);
					nrRows = n;
				};
				(finish(appender), ...);
			}
		, m_Appenders);
		return nrRows;
	}

	template <std::size_t I>
	auto& Column() { return std::get<I>(m_Appenders); }

private:
	struct Record { queues_t* queues = nullptr; SizeT nrRows = 0; };

	template <std::size_t... I>
	void Write(std::index_sequence<I...>, SizeT nrRows, const ordered_row_elem_t<Ts>*... columns)
	{
		(std::get<I>(m_Appenders).Append(columns, nrRows), ...);
	}
	template <std::size_t... I>
	static void Enqueue(std::index_sequence<I...>, queues_t& queues, SizeT nrRows, const ordered_row_elem_t<Ts>*... columns)
	{
		(std::get<I>(queues).Push(columns, nrRows), ...);
	}
	template <std::size_t... I>
	void Drain(std::index_sequence<I...>, queues_t& queues, SizeT nrRows)
	{
		(std::get<I>(queues).Pop(nrRows, [this](const auto* first, SizeT n) { std::get<I>(m_Appenders).Append(first, n); }), ...);
	}

	std::tuple<tile_appender<Ts>...> m_Appenders;
	ordered_turn<Record>             m_Turn;
	producer_queues<queues_t>        m_Queues;
};

#endif //!defined(__TIC_TILECHANNEL_H)

