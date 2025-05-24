// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__TIC_PARALLELTILES_H)
#define __TIC_PARALLELTILES_H

#include "Parallel.h"

#include "act/MainThread.h"
#include "ser/DebugOutStream.h"
//REMOVE #include "utl/AddFakeCopyCTor.h"
#include "utl/MemGuard.h"

#include "DataStoreManagerCaller.h" 
#include "OperationContext.h" 

#include <ppl.h>

struct tile_task_group
{
	using IndexType = SizeT;
	using task_func = std::function<void(IndexType)>;

private:
	IndexType m_Last;
	OperationContext* m_CallingContext = nullptr;

	mutable IndexType m_Commissioned = 0;
	mutable std::atomic<IndexType> m_NrCompleted = 0;

protected:
	task_func m_Func;
	mutable std::exception_ptr m_Exception;
	mutable std::condition_variable m_TileTasksDone;

public:
	TIC_CALL tile_task_group(IndexType last, task_func func);
	TIC_CALL ~tile_task_group();

	TIC_CALL void Join();

private:
	tile_task_group(const tile_task_group&) = delete;
	tile_task_group& operator=(const tile_task_group&) = delete; 

	void decommission();
	IndexType getNextCommissioned();
	IndexType GetNextCommissioned();
	void RegisterCompletion(IndexType nr = 1);
	void DoWork(IndexType i, bool doMore);

	void DoWork()
	{
		DoWork(GetNextCommissioned(), true);
	}

	void DoThisOrThat();
	friend auto takeOneTileTask() -> std::pair<tile_task_group*, tile_task_group::IndexType>;
	friend bool StealOneTileTask(bool doMore);
	friend void DoThisOrThatAndDecommission();
};


template <typename Functor>
using functor_result_t = std::invoke_result_t<Functor>;


template <typename R>
struct tile_task_result : tile_task_group
{
	template <typename Functor>
	tile_task_result(Functor&& func_)
		: tile_task_group(1, [func = std::forward<Functor>(func_), this](SizeT t) { assert(t == 0);  this->result = func();  })
	{}

	tile_task_result(R&& r)
		: tile_task_group(0, nullptr)
	{
		result = std::move(r);
	}
	R get()
	{
		this->Join(); // throws exception, if any
		this->m_Func = tile_task_group::task_func();
		assert(result.has_value()); // post condition of join, or preset.
		return std::move(*result);
	}	
	std::optional<R> result;
};

template <>
struct tile_task_result<void> : tile_task_group
{
	template <typename Functor>
	tile_task_result(Functor&& func_)
		: tile_task_group(1, [func = std::forward<Functor>(func_), this](SizeT t) { assert(t == 0); func(0); this->result = true; })
	{}

	tile_task_result()
		: tile_task_group(0, nullptr)
	{
		result = true;
	}
	void get()
	{
		this->Join(); // throws exception, if any
		this->m_Func = tile_task_group::task_func();
		assert(result); // post condition of join, or preset.
	}
	bool result = false;
};

template <typename Functor>
using func_task_result = tile_task_result<functor_result_t<Functor>>;

template <typename Functor>
using func_task_result_sptr = std::shared_ptr<func_task_result<Functor>>;

template <typename Functor>
auto throttled_async(Functor&& f) -> func_task_result_sptr<Functor>
{
	using R = functor_result_t<Functor>;

	if (IsMultiThreaded1() && !IsLowOnFreeRAM())
	{
		//		return std::async(std::launch::async, [fn = std::forward<Functor>(f)]
		return std::make_shared<func_task_result<Functor>>(f);
	}
	if constexpr (std::is_void_v<R>)
	{
		f();
		return std::make_shared<tile_task_result<R>>();
	}
	else
		return std::make_shared<tile_task_result<R>>(f());
}


template <typename IndexType, typename Func>
void serial_for(IndexType first, IndexType last, Func&& func)
{
	for (; first != last; ++first)
		func(first);
}

template <typename IndexType, typename Func>
void parallel_for(IndexType last, Func&& func)
{
	if (!last)
		return;

	UpdateMarker::PrepareDataInvalidatorLock preventInvalidations;

	if (last == 1)
	{
		func(0);
		return;
	}

	tile_task_group taskArray{ last, func };
	taskArray.Join();
}


template <typename IndexType, typename E, typename Func>
void parallel_for_if_separable(IndexType first, IndexType last, Func&& func)
{
	if constexpr (is_separable_v<E>)
	{
		if (last - first >= 8192)
		{
			IndexType nrBlocks = (last - first) / 4096;
			parallel_for<IndexType>(nrBlocks, [&func, first](IndexType blockNr)
				{
					Func funcCopy = func;
					serial_for<IndexType>(first + blockNr * 4096, first + (blockNr + 1) * 4096, funcCopy);
				}
			);
			first += nrBlocks * 4096;
		}
	}
	serial_for<IndexType>(first, last, std::move(func));
}

template <typename Func>
void parallel_tileloop(tile_id last, Func&& func)
{
	std::atomic<tile_id> sequentialTileNumber;
	parallel_for<tile_id>(last, [func = std::move(func), &sequentialTileNumber](tile_id t) { func(sequentialTileNumber++); });
}

template <typename Func>
void parallel_tileloop_if(bool canRunParallel, tile_id last, Func&& func)
{
	if (canRunParallel)
		parallel_tileloop(last, std::move(func));
	else
		serial_for<tile_id>(0, last, std::move(func));
}

template <typename E, typename Func>
typename std::enable_if<is_separable_v<E>>::type
parallel_tileloop_if_separable(tile_id last, Func&& func)
{
	parallel_tileloop<Func>(last, std::move(func));
}

template <typename E, typename Func>
typename std::enable_if<!is_separable_v<E>>::type
parallel_tileloop_if_separable(tile_id last, Func&& func)
{
	serial_for<tile_id>(0, last, std::move(func));
}



#endif // __TIC_PARALLELTILES_H
