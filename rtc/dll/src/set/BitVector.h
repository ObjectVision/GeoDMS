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
#if !defined(__RTC_SET_BIT_VECTOR_H)
#define __RTC_SET_BIT_VECTOR_H

#include "RtcBase.h"

#include "dbg/check.h"
#include "geo/mpf.h"
#include "geo/Undefined.h"
#include "mem/FixedAlloc.h"
#include "ptr/IterCast.h"
#include "set/RangeFuncs.h"
#include "utl/swap.h"

#include <vector>

//======================== FORWARD DECL

template <bit_size_t N, typename Block> struct bit_reference;
template <bit_size_t N, typename Block> struct bit_iterator;

//======================== bit calculations

template <typename Block, bit_size_t NrBlockBits, bit_size_t NrBits> struct calc_bit_mask_impl                        { static const Block value = (Block(1) << NrBits) - Block(1); };
template <typename Block, bit_size_t NrBits>                         struct calc_bit_mask_impl<Block, NrBits, NrBits> { static const Block value = Block(-1); };

template <typename Block, bit_size_t NrUsedBits> struct calc_bit_mask: calc_bit_mask_impl<Block, 8*sizeof(Block), NrUsedBits> {};

template <bit_size_t N> struct unit_block {};
template <> struct unit_block<1>: boost::integral_constant<bit_size_t, 0xFFFFFFFF> {};
template <> struct unit_block<2>: boost::integral_constant<bit_size_t, 0x55555555> {};
template <> struct unit_block<4>: boost::integral_constant<bit_size_t, 0x11111111> {};

template <bit_size_t N, typename Block>
struct bit_info
{
	typedef Block        block_type;
	typedef SizeT        size_type;
	typedef UInt8        bit_index_type;
	typedef DiffT        difference_type;
	typedef bit_value<N> value_type;

	static const bit_index_type block_size             = sizeof(Block);
	static const bit_index_type nr_elem_per_block      = (8 * block_size) / N;
	static const bit_index_type nr_used_bits_per_block = nr_elem_per_block * N;
	static const Block  value_mask             = calc_bit_mask<Block, N>::value;
	static const Block  used_bits_mask         = calc_bit_mask<Block, nr_used_bits_per_block>::value;

	static size_type        calc_nr_blocks(size_type sz)  { return (sz + (nr_elem_per_block-1)) / nr_elem_per_block; }

	static size_type        block_index (size_type       pos) { return pos / nr_elem_per_block; } // roundoff towards -INF is towards 0
	static difference_type  block_diff  (difference_type pos) { return pos >> mpf::log2<nr_elem_per_block>::value; } // roundoff towards -INF
	static bit_index_type   elem_index  (size_type       pos) { return pos % nr_elem_per_block; }
	static Block            elem_mask   (size_type       pos) { return Block(bit_value<N>::mask) << elem_index(pos); }
};

template <bit_size_t N, typename Block>
struct bit_sequence_base : bit_info<N, Block>
{
	typedef bit_reference<N,       Block> reference;
	typedef bit_reference<N, const Block> const_reference;

	using bit_info_t = bit_info<N, Block>;
	using typename bit_info_t::size_type;

	bit_sequence_base(Block* blockData, SizeT elemCount)
		:	m_BlockData(blockData)
		,	m_NrElems (elemCount)
	{}

	Block*    data_begin() const { return m_BlockData; }
	Block*    data_end  () const { return m_BlockData + bit_info<N, Block>::calc_nr_blocks(m_NrElems); }
	size_type nr_elem   () const { return m_NrElems; }

	void swap(bit_sequence_base<N, Block>& oth)
	{
		omni::swap(m_BlockData, oth.m_BlockData);
		omni::swap(m_NrElems,   oth.m_NrElems  );
	}

	Block*    m_BlockData;
	size_type m_NrElems;
};


template <bit_size_t N, typename Block>
struct bit_reference : private bit_sequence_base<N, Block>
{
	using bit_info_t = bit_info<N, Block>;
	using typename bit_info_t::value_type;

	bit_reference(bit_value<N>& elem)
		:	bit_sequence_base<N, Block>(reinterpret_cast<Block*>(&elem), 0)
	{}

	bit_reference(Block* blockData, SizeT elemNr)
		:	bit_sequence_base<N, Block>(blockData, elemNr)
	{
		assert(elemNr < bit_info_t::nr_elem_per_block);
		assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
	}

	value_type value() const { return (*this->m_BlockData >> (this->m_NrElems * N)) & Block(bit_value<N>::mask); }
	operator value_type() const { return value(); }

	void operator =(bit_reference<N, Block> rhs) { do_assign(this, rhs.value()); }
	void operator =(value_type rhs)              { do_assign(this, rhs); }

	bool operator <  (bit_value<N> oth) const { return value_type(*this) <  oth; }
	bool operator >  (bit_value<N> oth) const { return value_type(*this) >  oth; }
	bool operator == (bit_value<N> oth) const { return value_type(*this) == oth; }
	bool operator != (bit_value<N> oth) const { return value_type(*this) != oth; }
	bool operator >= (bit_value<N> oth) const { return value_type(*this) >= oth; }
	bool operator <= (bit_value<N> oth) const { return value_type(*this) <= oth; }
	bool operator !  ()                 const { return value_type(*this) == value_type(); }

	bit_iterator<N, Block>       operator &()       { return bit_iterator<N,       Block>(this->m_BlockData, this->m_NrElems); }
	bit_iterator<N, const Block> operator &() const { return bit_iterator<N, const Block>(this->m_BlockData, this->m_NrElems); }

	void do_set() { block() |= mask(); }
	void do_clear() { block() &= ~mask(); }
	void do_flip() { block() ^= mask(); }

	typename bit_info_t::bit_index_type shift() const { return this->m_NrElems * N;  }
	Block  mask() const { return Block(bit_value<N>::mask) << shift(); }
	Block& block()      { return *this->m_BlockData; }
};

template <bit_size_t N, typename Block>
void do_assign(bit_reference < N, Block>* self, typename bit_info<N, Block>::value_type x)
{
	setbits(self->block(), self->mask(), Block(x.base_value()) << self->shift());
}

template <typename Block>
void do_assign(bit_reference <1, Block>* self, typename bit_info<1, Block>::value_type x)
{
	if (x.base_value()) 
		self->do_set(); 
	else 
		self->do_clear();
}



template <bit_size_t N, typename Block>
void Assign(bit_reference<N, Block> lhs, bit_value<N> rhs)
{
	lhs = rhs;
}

template <bit_size_t N, typename Block>
void Assign(bit_reference<N, Block> lhs, Undefined rhs)
{
	lhs.do_clear();
}

template <bit_size_t N, typename Block, typename CBlock>
void Assign(bit_reference<N, Block> lhs, bit_reference<N, CBlock> rhs)
{
	lhs = rhs;
}

template <bit_size_t N, typename Block>
struct bit_iterator
	:	bit_sequence_base<N, Block>
		// TODO: EMPTY MEMBER OPTIMIZATION
	, 	std::iterator<std::random_access_iterator_tag
		,	bit_value<N>
		,	typename bit_info    <N, Block>::difference_type
		,	bit_iterator<N, Block>
		,	typename bit_sequence_base<N, Block>::reference
		>
{
	using base_type = bit_sequence_base<N, Block>;
	using typename base_type::value_type;
	using typename base_type::difference_type;
	using typename base_type::reference;
	using base_type::data_begin;

	using bit_info_t = bit_info<N, Block>;
	using typename bit_info_t::size_type;

	bit_iterator() 
		:	base_type(nullptr, 0)
	{}

	bit_iterator(Block* blockData, difference_type elemCount)
		: base_type(blockData + bit_info_t::block_diff(elemCount), bit_info_t::elem_index(elemCount) )
	{
		assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
	}

	bit_iterator(Block* blockData, size_type elemCount)
		: base_type(blockData + bit_info_t::block_index(elemCount), bit_info_t::elem_index(elemCount) )
	{
		assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
	}

	template <typename OthBlock>
	bit_iterator(const bit_iterator<N, OthBlock>& src)
		: base_type(src.data_begin(), src.nr_elem())
	{
		static_assert(sizeof(Block) == sizeof(OthBlock));
		assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
	}

	explicit operator bool () const { return this->m_BlockData != nullptr; }

	bit_iterator<N, Block>& operator ++()
	{
		assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
		assert(this->m_BlockData);

		if (++this->m_NrElems == bit_info_t::nr_elem_per_block)
		{
			this->m_NrElems = 0;
			++this->m_BlockData;
		}
		return *this;
	}

	bit_iterator<N, Block> operator ++(int)
	{
		bit_iterator tmp(*this);
		this->operator ++();
		return tmp;
	}

	bit_iterator<N, Block>& operator --()
	{
		dms_assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
		dms_assert(this->m_BlockData);

		if (this->m_NrElems)
			--this->m_NrElems;
		else
		{
			this->m_NrElems = (bit_info_t::nr_elem_per_block-1);
			--this->m_BlockData;
		}
		return *this;
	}

	bit_iterator<N, Block> operator --(int)
	{
		bit_iterator tmp(*this);
		this->operator --();
		return tmp;
	}

	void operator +=(difference_type i)
	{
		dms_assert(this->m_NrElems < bit_info_t::nr_elem_per_block);
		dms_assert(this->m_BlockData || !i);

		difference_type elemNr = this->m_NrElems + i;
		this->m_BlockData += bit_info_t::block_diff (elemNr);
		this->m_NrElems   = bit_info_t::elem_index (elemNr);
	}

	void skip_block()
	{
		dms_assert(this->m_BlockData);
		++this->m_BlockData;
		this->m_NrElems = 0;
	}

	void skip_full_block()
	{
		dms_assert(!this->m_NrElems);
		dms_assert(this->m_BlockData);
		++this->m_BlockData;
	}

	void operator -=(difference_type i) { operator +=(-i); }
	bit_iterator<N, Block> operator + (difference_type i) const { return bit_iterator<N, Block>(this->m_BlockData, difference_type(this->m_NrElems+i)); }
	bit_iterator<N, Block> operator - (difference_type i) const { return bit_iterator<N, Block>(this->m_BlockData, difference_type(this->m_NrElems-i)); }

	difference_type operator -(const bit_iterator<N, Block>& rhs) const
	{
		return (this->m_BlockData - rhs.m_BlockData) * bit_info_t::nr_elem_per_block + difference_type(this->m_NrElems - rhs.m_NrElems);
	}
	bool operator < (const bit_iterator<N, Block>& rhs) const { return data_begin() < rhs.data_begin() || (data_begin() == rhs.data_begin() && this->m_NrElems < rhs.m_NrElems); }
	bool operator ==(const bit_iterator<N, Block>& rhs) const { return data_begin()== rhs.data_begin() && this->m_NrElems == rhs.m_NrElems; }
	bool operator !=(const bit_iterator<N, Block>& rhs) const { return !operator ==(rhs); }
	bool operator <=(const bit_iterator<N, Block>& rhs) const { return !(rhs < *this); }
	bool operator >=(const bit_iterator<N, Block>& rhs) const { return !(*this < rhs); }
	bool operator > (const bit_iterator<N, Block>& rhs) const { return  (rhs < *this); }

	reference operator *()  const { return bit_reference<N, Block>(this->m_BlockData, this->m_NrElems); }
	reference operator [](difference_type i) const { return *(*this + i); }
};

template <bit_size_t N, typename Block>
struct bit_sequence : bit_sequence_base<N, Block>
{
	typedef bit_iterator<N, Block>        iterator;
	typedef bit_iterator<N, const Block>  const_iterator;
	typedef bit_reference<N, Block>       reference;
	typedef bit_reference<N, const Block> const_reference;

	using bit_info_t = bit_info<N, Block>;
	using typename bit_info_t::size_type;
	using bit_sequence_base<N, Block>::data_begin;
	using bit_sequence_base<N, Block>::data_end;


	bit_sequence( )
		:	bit_sequence_base<N, Block>(0,  0)
	{}

	bit_sequence(Block* blockData, size_type elemCount)
		:	bit_sequence_base<N, Block>(blockData, elemCount)
	{}

	template <typename OthBlock>
	bit_sequence(bit_iterator<N, OthBlock> start, size_type elemCount)
		:	bit_sequence_base<N, Block>(start.data_begin(), elemCount)
	{
		dms_assert(start.data_begin() == start.data_end()); // implies that m_NrElems == 0
	}

	template <typename OthBlock>
	bit_sequence(bit_iterator<N, OthBlock> start, bit_iterator<N, OthBlock> end)
		:	bit_sequence_base<N, Block>(start.data_begin(),  end-start)
	{
		dms_assert(start.data_begin() == start.data_end()); // implies that m_NrElems == 0
	}

	template <typename OthBlock>
	bit_sequence(const bit_sequence<N, OthBlock>& src)
		:	bit_sequence_base<N, Block>( src.data_begin(), src.size() )
	{
		dms_assert( data_begin() == src.data_begin() );
		dms_assert( data_end  () == src.data_end  () );
	}

	template <typename OthBlock, typename Alloc>
	bit_sequence(BitVector<N, OthBlock, Alloc>* cont)
		:	bit_sequence_base<N, Block>(cont->data_begin(),  cont->size())
	{}

	size_type size () const { return this->m_NrElems; }
	bool  empty() const { return this->m_NrElems == 0; }

	void grow(int delta)         { this->m_NrElems += delta; }
	void setsize(std::size_t sz) { this->m_NrElems = sz; }

	iterator begin() { return iterator(this->m_BlockData, SizeT(0)); }
	iterator end()   { return iterator(this->m_BlockData, this->m_NrElems); }

	const_iterator begin() const { return iterator(this->m_BlockData, SizeT(0)); }
	const_iterator end()   const { return iterator(this->m_BlockData, this->m_NrElems); }

	reference operator [] (size_type i)
	{
		assert(i < this->m_NrElems);
		return bit_reference<N, Block>(this->m_BlockData + bit_info_t::block_index(i), bit_info_t::elem_index(i) );
	}
	const_reference operator[](size_type i) const
	{
		assert(i < this->m_NrElems);
		return bit_reference<N, Block>(this->m_BlockData + bit_info_t::block_index(i), bit_info_t::elem_index(i) );
	}

	void set_range(bit_value<N> v)
	{
		iterator 
			b = begin(),
			e = end();
		do {
			if (b==e)
				return;
			*b = v;
		}
		while ( (++b).nr_elem());
		std::fill(b.data_begin(), e.data_begin(), *begin().data_begin());
		std::fill(iterator(e.data_begin(), SizeT(0)), e, v);
	}

	void clear_range()
	{
		size_type i = bit_info_t::block_index(this->m_NrElems);
		typename bit_info_t::block_type* afterLastFullBlock = this->m_BlockData + i;

		std::fill(this->m_BlockData, afterLastFullBlock, 0);

		typename bit_info_t::bit_index_type elemIndex = bit_info_t::elem_index(this->m_NrElems);
		if (elemIndex)
			*afterLastFullBlock &= ~((bit_info_t::block_type(1) << (elemIndex*N))-1);
	}

	template <typename OthBlock>
	bool operator < (const bit_sequence<N, OthBlock>& right) const
	{
		auto sz = Min<size_type>(size(), right.size());
		
		const Block* db = data_begin();
		const Block* de = db + bit_info_t::block_index(sz);
		const OthBlock* rdb = right.data_begin();
		for (; db != de; ++rdb, ++db)
		{
			if (*db != *rdb)
				break;
		}
		const_iterator
			b(db, SizeT(0))
		,	e = end();
		typename bit_sequence<N, OthBlock>::const_iterator
			rb(rdb, SizeT(0))
		,	re = right.end();
		for (; b != e && rb != re; ++rb, ++b)
		{
			if (*b != *rb)
				return *b < *rb;
		}
		return rb != re;
	}

	template <typename OthBlock>
	bool operator == (const bit_sequence<N, OthBlock>& right) const
	{
		auto sz = size();
		if (sz != right.size())
			return false;
		
		const Block* db = data_begin();
		const Block* de = db + bit_info_t::block_index(sz);
		const OthBlock* rdb = right.data_begin();
		for (; db != de; ++rdb, ++db)
		{
			if (*db != *rdb)
				return false;
		}
		return true;
	}

	size_type FindLowestNonZeroPos() const
	{
		size_type result = 0;
		
		const Block* i = data_begin();
		const Block* e = data_end();
		while (true)
		{
			if (i == e)
				return -1;
			if (*i != 0)
				break;
			result += bit_info_t::nr_elem_per_block;
		}

		// FindLowestNonZeroPos(nonZeroBlock);
		Block nonZeroBlock = *i;
		dms_assert(nonZeroBlock);

		size_type nrZeroesInBlock = 0;

		Block mask = bit_info_t::value_mask;

		while (!(mask &nonZeroBlock))
		{
			mask <<= N;
			++nrZeroesInBlock;
			dms_assert(nrZeroesInBlock < bit_info_t::nr_elem_per_block);
		}
		return result + nrZeroesInBlock;
	}
};

template <bit_size_t N, typename Block /*= UInt32*/, typename Allocator /*= std::allocator<Block>*/ >
struct BitVector : bit_info<N, Block>
{
	typedef Allocator                      allocator;
	typedef std::vector<Block, allocator>  block_container;
	typedef bit_reference<N, Block>        reference;
	typedef bit_value<N>                   const_reference;

	typedef bit_iterator<N, Block>         iterator;
	typedef bit_iterator<N, const Block>   const_iterator;

	using bit_info_t = bit_info<N, Block>;
	using typename bit_info_t::size_type;

	BitVector(size_type sz = 0)
		:	m_bits( bit_info_t::calc_nr_blocks(sz) )
		,	m_NrElems(sz)
	{}

	BitVector(size_type sz, bool v, allocator alloc = allocator())
		:	m_bits(
			bit_info_t::calc_nr_blocks(sz),
				(v)
				?   bit_info_t::used_bits_mask
				:	Block(),
				alloc
			)
		,	m_NrElems(sz)
	{
		if (v)
			clear_unused_bits();
	}

	template <typename Iter>
	BitVector(Iter first, Iter last)
		:	m_bits( bit_info_t::calc_nr_blocks(last - first) )
		,	m_NrElems(last - first)
	{
		std::copy(first, last, begin());	//	OPTIMIZE: if first.elem_offset == 0, direct insertion into m_Bits prevents double passing it. 
	}

	BitVector(bit_iterator<N, const Block> first, bit_iterator<N, const Block> last)
		:	m_bits(bit_info_t::calc_nr_blocks(last - first) )
		,	m_NrElems(last - first)
	{
		dms_assert(first.m_NrElems < bit_info_t::nr_elem_per_block);
		dms_assert(last .m_NrElems < bit_info_t::nr_elem_per_block);
		if (first.m_NrElems == 0)
		{
			fast_copy(first.m_BlockData, first.m_BlockData + m_bits.size(), begin_ptr( m_bits ));
			clear_unused_bits();
		}
		else
			std::copy(first, last, begin());	//	OPTIMIZE: if first.elem_offset == 0, direct insertion into m_Bits prevents double passing it. 
	}
	BitVector(BitVector&& rhs) noexcept
	{
		this->swap(rhs);
	}
	void operator =(BitVector&& rhs) noexcept
	{
		this->swap(rhs);
	}

	iterator       begin ()       { return iterator(begin_ptr( m_bits ), SizeT(0) ); }
	iterator       end   ()       { return iterator(begin_ptr( m_bits ), size ( ) ); }
	const_iterator begin () const { return const_iterator(begin_ptr( m_bits ), SizeT(0) ); }
	const_iterator end   () const { return const_iterator(begin_ptr( m_bits ), size ( ) ); }
	const_iterator cbegin() const { return begin(); }
	const_iterator cend  () const { return end  (); }

	reference operator [] (size_type i)
	{
		dms_assert(i < m_NrElems);
		return bit_reference<N, Block>(begin_ptr(m_bits) + bit_info_t::block_index(i), bit_info_t::elem_index(i) );
	}
	const_reference operator[](size_type i) const
	{
		dms_assert(i < m_NrElems);
		return bit_reference<N, const Block>(begin_ptr(m_bits) + bit_info_t::block_index(i), bit_info_t::elem_index(i) );
	}

	Block*    data_begin() { return begin_ptr( m_bits ); }
	size_type size()  const { return m_NrElems; }
	bool      empty() const { return m_NrElems == 0; }
	size_type num_blocks() const { return m_bits.size(); }

	void resize(size_type num_values, bit_value<N> value = 0)
	{
		size_type old_num_blocks = num_blocks();
		size_type required_blocks = bit_info_t::calc_nr_blocks(num_values);

		typename bit_info_t::block_type v = value ? ~Block(0) : Block(0);

		if (required_blocks != old_num_blocks) {
			m_bits.resize(required_blocks, v); // s.g. (copy) [gps]
		}


		// At this point:
		//
		//  - if the buffer was shrunk, there's nothing to do, except
		//    a call to clear_unused_bits()
		//
		//  - if it it is enlarged, all the (used) bits in the new blocks have
		//    the correct value, but we should also take care of the bits,
		//    if any, that were 'unused bits' before enlarging: if value == true,
		//    they must be set.

		if (value && (num_values > m_NrElems)) {
			size_type extra_values = count_extra_values(); // gps
			if (extra_values) {
				dms_assert(old_num_blocks >= 1 && old_num_blocks <= m_bits.size());

				// Set them.
				m_bits[old_num_blocks - 1] |= (v << (extra_values * N)); // gps
			}
		}

		m_NrElems = num_values;
		clear_unused_bits();
	}
	void resizeSO(size_type num_values, bool mustClear  MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		resize(num_values, !mustClear); 
	}
	void reallocSO(size_type num_values, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		size_type old_num_blocks = num_blocks();
		size_type required_blocks = bit_info_t::calc_nr_blocks(num_values);

//		typename bit_info_t::block_type v = value ? ~Block(0) : Block(0);

		if (required_blocks < old_num_blocks)
			m_bits.resize(required_blocks); // TODO: avoid zeroing if not required
		else if (required_blocks > old_num_blocks)
			m_bits = block_container(required_blocks); // TODO: avoid zeroing if not required

		m_NrElems = num_values;
		clear_unused_bits();
	}
	void insert(iterator ip, size_type k, bit_info_t::value_type v)
	{
		dms_assert(m_NrElems || ip == begin());
		if (!k)
			return;
		SizeT i = m_NrElems ? ip - begin() : 0;
		SizeT newNrElems = m_NrElems + k;
		dms_assert(newNrElems);
		enable_nr_blocks(bit_info_t::calc_nr_blocks(newNrElems) );
		ip = begin() + i;
		fast_fill(
			ip,
			fast_copy_backward(ip, end(), end()+k),
			v
		);
		m_NrElems = newNrElems;
	}

	void push_back(bit_info_t::value_type v)
	{
		// substitute a call to insert(end(), 1, v); with iterator ip = end() and size_type k=1;
		SizeT oldNrElems = m_NrElems;
		SizeT newNrElems = oldNrElems + 1;
		dms_assert(newNrElems);
		enable_nr_blocks(bit_info_t::calc_nr_blocks(newNrElems) );
		m_NrElems = newNrElems;
		operator [](oldNrElems) = v;
	}

	void erase(iterator b, iterator e)
	{
		m_NrElems = fast_copy(e, end(), b) - begin();
		m_bits.resize(bit_info_t::calc_nr_blocks(m_NrElems) );
	}

	void swap(BitVector& oth) { 
		m_bits.swap(oth.m_bits);
		std::swap(m_NrElems, oth.m_NrElems);
	}

    void reserve(size_type n)
	{
		m_bits.reserve(bit_info_t::calc_nr_blocks(n));
	}

    size_type capacity() const
	{
		// capacity is m_bits.capacity() * nr_elem_per_block
		// unless that one overflows
		const size_type m = static_cast<size_type>(-1);
		const size_type q = m / bit_info_t::nr_elem_per_block;

		const size_type c = m_bits.capacity();

		return c <= q ?
			c * bit_info_t::nr_elem_per_block :
			m;
	}

private:
	void enable_nr_blocks(size_type nrBlocks)
	{
		if (nrBlocks > m_bits.capacity())
			m_bits.reserve(Max<size_type>(m_bits.capacity() * 1.5, nrBlocks));
		m_bits.resize(nrBlocks);
	}
	void             clear_unused_bits();
    Block&           m_highest_block()       { return m_bits.back(); }
    const Block&     m_highest_block() const { return m_bits.back(); }

	auto count_extra_values() const { return bit_info_t::elem_index(size()); }

private:
	block_container m_bits;
	size_type       m_NrElems;
};

// If size() is not a multiple of nr_elem_per_block
// then not all the bits in the last block are used.
// This function resets the unused bits (convenient
// for the implementation of many member functions)
//
template <bit_size_t N, typename Block, typename Allocator>
void BitVector<N, Block, Allocator>::clear_unused_bits()
{
    dms_assert (num_blocks() == bit_info_t::calc_nr_blocks(m_NrElems));

    // if != 0 this is the number of bits used in the last block
    auto extra_bits = count_extra_values() * N;

    if (extra_bits)
        m_highest_block() &= ~(~static_cast<Block>(0) << extra_bits);
}

//----------------------------------------------------------------------
// Section      : Undefined
//----------------------------------------------------------------------

template <bit_size_t N, typename Block>
constexpr bit_value<N> UndefinedOrZero(const bit_reference<N, Block>* ) { return 0; }

template <bit_size_t N, typename Block> inline bool IsDefined(bit_reference<N, Block>) { return true; }

template <bit_size_t N, typename Block>
inline void MakeUndefinedOrZero(bit_reference<N,Block> v)
{
	v = bit_value<N>(0);
}

template <bit_size_t N, typename Block>
auto GetSeq(bit_sequence<N, Block>& so) -> bit_sequence<N, Block>
{
	return so;
}

template <bit_size_t N, typename Block>
auto GetConstSeq(const bit_sequence<N, Block>& so) -> bit_sequence<N, const Block>
{
	return { so.begin(), so.end() };
}

template <bit_size_t N, typename Block, typename Alloc>
auto GetSeq(BitVector<N, Block, Alloc>& so)
{
	return bit_sequence<N, Block>(so.data_begin(), so.size());
}

template <bit_size_t N, typename Block, typename Alloc>
auto GetConstSeq(const BitVector<N, Block, Alloc>& so) -> bit_sequence<N, const Block>
{
	return { so.begin(), so.end() };
}


#endif // __RTC_SET_BIT_VECTOR_H
