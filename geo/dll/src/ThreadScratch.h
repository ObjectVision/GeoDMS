// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#pragma once

#if !defined(__GEO_THREADSCRATCH_H)
#define __GEO_THREADSCRATCH_H

#include <algorithm>
#include <functional>
#include <memory>

#include "Parallel.h"
#include "parallel/dms_combinable.h"

// The number of elements per block of a parallel loop over the n elements of a tile: at most
// maxBlockSize, which keeps what a block costs apart from its elements small against their work,
// and fewer when the tile has few elements, so that every thread still gets a few blocks (#1205).
// With 32 threads, a tile of 300 elements in blocks of 256 ran on 2 of them; it now gets blocks of
// 2. The work of an element is not estimated: a tile of a few very large polygons is split over the
// threads by count, and one polygon is never split (a modeller can split it explicitly, for instance
// with dms_split_convex_polygon). With parallel processing off there is one thread, and the blocks
// keep their largest size.
inline SizeT element_block_size(SizeT n, SizeT maxBlockSize)
{
	constexpr SizeT blocksPerThread = 4;
	SizeT nrThreads = MaxConcurrentTreads();
	return std::clamp<SizeT>(n / (nrThreads * blocksPerThread), 1, maxBlockSize);
}

// thread_scratch<T>: one scratch object per thread for the tiles of one parallel tile loop, so
// that an object whose buffers have grown serves every tile its thread works on instead of being
// rebuilt per tile. The functor of the loop holds it by reference: parallel_tileloop stores that
// functor once and calls it from every worker thread, so the functor itself is shared, and what
// must be per thread is looked up per thread, through dms_combinable. It lives as long as the
// operation that makes it; its objects are made on a thread's first tile and released with it.
//
// A thread may take up another tile of the same loop while its first is still in progress (a
// task that waits can pick up waiting tasks); such a nested use gets an object of its own for the
// duration, so no object is ever used by two tiles at once.
template <typename T>
class thread_scratch
{
	struct Slot
	{
		std::unique_ptr<T> obj;
		bool inUse = false;
	};

public:
	using factory_t = std::function<std::unique_ptr<T>()>;

	explicit thread_scratch(factory_t factory)
		: m_Factory(std::move(factory))
	{}
	thread_scratch(const thread_scratch&) = delete;
	thread_scratch& operator=(const thread_scratch&) = delete;

	// This thread's object, for as long as the returned use lives.
	class use
	{
	public:
		use(const use&) = delete;
		use& operator=(const use&) = delete;
		~use()
		{
			if (m_Slot)
				m_Slot->inUse = false;
		}

		T& operator*()  const { return *m_Obj; }
		T* operator->() const { return m_Obj; }
		T* get()        const { return m_Obj; }

	private:
		friend class thread_scratch;
		use(Slot* slot, T* obj) : m_Slot(slot), m_Obj(obj) {}
		explicit use(std::unique_ptr<T> own) : m_Own(std::move(own)), m_Obj(m_Own.get()) {}

		Slot*              m_Slot = nullptr;
		std::unique_ptr<T> m_Own;  // a nested use's object of its own
		T*                 m_Obj = nullptr;
	};

	use local()
	{
		Slot& slot = m_Slots.local();
		if (slot.inUse)
			return use(m_Factory());
		if (!slot.obj)
			slot.obj = m_Factory();
		slot.inUse = true;
		return use(&slot, slot.obj.get());
	}

private:
	factory_t            m_Factory;
	dms_combinable<Slot> m_Slots;
};

#endif // __GEO_THREADSCRATCH_H
