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

#if !defined(__TIC_PARALLELTILES_H)
#define __TIC_PARALLELTILES_H

#include "Parallel.h"

#include "act/MainThread.h"
#include "ser/DebugOutStream.h"
#include "DataStoreManagerCaller.h" 
#include "OperationContext.h" 

template <typename IndexType, typename Func>
void serial_for(IndexType first, IndexType last, Func&& func)
{
	for (; first != last; ++first)
		func(first);
}

#include <ppl.h>

#if defined(MG_DEBUG)

#include "utl/swapper.h"
TIC_CALL extern std::atomic<UInt32> gd_nrActiveLoops;

#endif // defined(MG_DEBUG)

template <typename IndexType, typename Func>
void parallel_for_impl(IndexType first, IndexType last, const Func& func)
{
#if defined(MG_DEBUG)
	StaticMtIncrementalLock<gd_nrActiveLoops> lockLoops;
#endif // defined(MG_DEBUG)

	if (first != last)
	{
		if (first + 1 == last)
			func(first);
		else if (IsMultiThreaded1() )
		{
			Concurrency::parallel_for<IndexType>(first, last, func);
			if (IsMainThread())
				ProcessMainThreadOpers();
		}
		else
			serial_for<IndexType>(first, last, func);
	}
}

template <typename ...Args, typename Func>
auto CreateTaskWithContext(Func&& func)
{
	return [func = std::move(func), currContext = OperationContext::CancelableFrame::CurrActive()](Args&& ...args)->void
	{
		dms_assert(!std::uncaught_exceptions());
		try {
			OperationContext::CancelableFrame frame(currContext);
			if (currContext)
				DSM::CancelIfOutOfInterest();

			UpdateMarker::PrepareDataInvalidatorLock preventInvalidations;
			func(std::forward<Args>(args)...);
		}
		catch (const concurrency::task_canceled&)
		{
			dms_assert(currContext);
			if (currContext)
				currContext->CancelIfNoInterestOrForced(true);
		}
	};
}

template <typename IndexType, typename Func>
void parallel_for(IndexType first, IndexType last, Func&& func)
{
	UpdateMarker::PrepareDataInvalidatorLock preventInvalidations;

	parallel_for_impl<IndexType>(first, last, CreateTaskWithContext<const IndexType& >(std::move(func)));
}


template <typename IndexType, typename E, typename Func>
typename std::enable_if<is_separable_v<E>>::type
parallel_for_if_separable(IndexType first, IndexType last, Func&& func)
{
	if (last - first >= 8192)
	{
		IndexType nrBlocks = (last - first) / 4096;
		parallel_for<IndexType>(0, nrBlocks, [&func, first](IndexType blockNr)
			{
				Func funcCopy = func;
				serial_for<IndexType>(first + blockNr * 4096, first + (blockNr + 1) * 4096, funcCopy);
			}
		);
		first += nrBlocks * 4096;
	}
	serial_for<IndexType>(first, last, std::move(func));
}

template <typename IndexType, typename E, typename Func>
typename std::enable_if<!is_separable_v<E>>::type
parallel_for_if_separable(IndexType first, IndexType last, Func&& func)
{
	serial_for<IndexType>(first, last, std::move(func));
}


template <typename Func>
void parallel_tileloop(tile_id last, Func&& func)
{
	std::atomic<tile_id> sequentialTileNumber;
	parallel_for<tile_id>(0, last, [func = std::move(func), &sequentialTileNumber](tile_id t) { func(sequentialTileNumber++); });
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
