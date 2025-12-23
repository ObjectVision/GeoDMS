#if !defined(__RTC_MEM_MYALLOCATOR_H)
#define __RTC_MEM_MYALLOCATOR_H

#include "RtcBase.h"
#include "mem/FixedAlloc.h"
#include <memory>

template <typename T> struct my_allocator; // forward reference

// =================================================== struct my_allocator

template <bit_size_t N> 
struct my_allocator<bit_value<N> >
{
	typedef bit_value<N>                                     value_type;
	typedef typename sequence_traits<value_type>::block_type block_type;
	typedef bit_iterator<N, block_type>                      iterator;
	typedef bit_info<N, block_type>                          info_t;
	typedef typename info_t::size_type                       size_type;

	iterator allocate(size_type sz MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		return iterator( m_BlockAllocator.allocate( info_t::calc_nr_blocks(sz) MG_DEBUG_ALLOCATOR_SRC_PARAM), SizeT(0));
	}
	void deallocate(iterator i, size_type sz)
	{
		assert(!i.nr_elem());
		m_BlockAllocator.deallocate(i.data_begin(), info_t::calc_nr_blocks(sz));
	}
	size_type max_size() const
	{
		assert(m_BlockAllocator.max_size() > info_t::calc_nr_blocks( MAX_VALUE(std::size_t) ) );
		return MAX_VALUE(std::size_t);
	}

	template <typename U>
	struct rebind
	{
		using other = my_allocator<U>;
	};

private:
	my_allocator<block_type> m_BlockAllocator;
};

//=======================================
// Wrapping of my_allocator facilities library
//=======================================

template<size_t nrBits> inline
size_t safe_size_n(size_t n)
{	// gets the size of _Count copies of a type sized sz
	if constexpr (nrBits == 8)
		return n;
	else if constexpr (nrBits > 8)
	{
		static_assert(nrBits % 8 == 0, "nrBits must be a multiple of 8");
		constexpr size_t sz = nrBits / 8;
		constexpr size_t max = static_cast<size_t>(-1) / sz;
		if (n > max)
			throwDmsErrF("Cannot represent the size of %d elements of %d bytes each as a size_t, a.k.a. a %d bit unsigned integer", n, sz, 8 * sizeof(size_t));

		return n * sz;
	}
	else
	{
		static_assert((nrbits_of_v<bit_block_t> % nrBits) == 0, "block_bits must be a multiple of nrBits");
		auto nrBitsInBlock = sizeof(bit_block_t) * 8 / nrBits;
		auto ceiledNrBlocks = (n / nrBitsInBlock); if (n % nrBitsInBlock != 0) ++ceiledNrBlocks;
		return ceiledNrBlocks * sizeof(bit_block_t);
	}
}

template <typename T>
struct my_allocator {
	using value_type = T;
	using is_always_equal = std::true_type;

	my_allocator() = default;
	template <class U> constexpr my_allocator(const my_allocator <U>&) noexcept {}
	template <class U> bool operator==(const my_allocator <U>&) const { return true; }
	template <class U> bool operator!=(const my_allocator <U>&) const { return false; }

	T* allocate(SizeT n MG_DEBUG_ALLOCATOR_SRC_ARG)
	{
		auto result = reinterpret_cast<T*>(AllocateFromStock(safe_size_n<nrbits_of_v<T>>(n) MG_DEBUG_ALLOCATOR_SRC_PARAM));
		return result;
	}
	void deallocate(T* ptr, size_t n)
	{
		LeaveToStock(ptr, sizeof(T)*n);
	}
	SizeT max_size() const
	{
		return SizeT(-1) / sizeof(T);
	}
};

//=======================================
// Wrapping of my_allocator facilities library
//=======================================

template <typename V>
auto CreateMyAllocator()
{
	static my_allocator<V> allocator;
	return &allocator;
}

#endif //!defined(__RTC_MEM_MYALLOCATOR_H)
