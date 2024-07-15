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
		dms_assert(!i.nr_elem());
		m_BlockAllocator.deallocate(i.data_begin(), info_t::calc_nr_blocks(sz));
	}
	size_type max_size() const
	{
		dms_assert(m_BlockAllocator.max_size() > info_t::calc_nr_blocks( MAX_VALUE(std::size_t) ) );
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

template<size_t sz> inline
size_t safe_size_n(size_t n)
{	// gets the size of _Count copies of a type sized sz
	constexpr size_t max = static_cast<size_t>(-1) / sz;
	if (n >= max)
		throwDmsErrF("Cannot represent the size of %d elements of %d bytes each as a size_t, ak.a. a %d bit unsigned integer", n, sz, 8*sizeof(size_t));
	return n * sz;
}

template<> inline
size_t safe_size_n<1>(const size_t n)
{	
	return n;
}

template <typename T>
struct my_allocator {
	using value_type = T;
	using is_always_equal = std::true_type;

	my_allocator() = default;
	template <class U> constexpr my_allocator(const my_allocator <U>&) noexcept {}
	template <class U> bool operator==(const my_allocator <U>&) const { return true; }
	template <class U> bool operator!=(const my_allocator <U>&) const { return false; }

	T* allocate(SizeT n MG_DEBUG_ALLOCATOR_SRC_ARG_D)
	{
		auto result = reinterpret_cast<T*>(AllocateFromStock(safe_size_n<sizeof(T)>(n) MG_DEBUG_ALLOCATOR_SRC_PARAM));
		return result;
	}
	void deallocate(T* ptr, size_t n)
	{
		LeaveToStock(ptr, sizeof(T)*n);
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
