#pragma once

#if !defined(__TIC_FUTURETILEARRAY_H)
#define __TIC_FUTURETILEARRAY_H

#include "DataArray.h"
#include "ptr/OwningPtrSizedArray.h"

using abstr_future_tile_ptr = SharedPtr<abstr_future_tile>;
using abstr_future_tile_array = OwningPtrSizedArray< abstr_future_tile_ptr >;

template <typename V> using future_tile = typename DataArrayBase<V>::future_tile;
template <typename V> using future_tile_ptr = SharedPtr<future_tile<V>>;
template <typename V> using future_tile_array = OwningPtrSizedArray< future_tile_ptr<V> >;

TIC_CALL auto GetAbstrFutureTileArray(const AbstrDataObject* ado) -> abstr_future_tile_array;

template<typename V>
auto GetFutureTileArray(const DataArrayBase<V>* da) -> future_tile_array<V>
{
	assert(da); // PRECONDITION
	auto tn = da->GetTiledRangeData()->GetNrTiles();
	auto result = future_tile_array<V>(tn, ValueConstruct_tag() MG_DEBUG_ALLOCATOR_SRC("GetFutureTileArray"));
	for (tile_id t = 0; t != tn; ++t)
		result[t] = da->GetFutureTile(t);
	return result;
}

#endif // __TIC_FUTURETILEARRAY_H
