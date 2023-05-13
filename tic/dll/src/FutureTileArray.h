#pragma once

#if !defined(__TIC_FUTURETILEARRAY_H)
#define __TIC_FUTURETILEARRAY_H

#include "DataArray.h"

using abstr_future_tile_ptr = SharedPtr<abstr_future_tile>;
using abstr_future_tile_array = OwningPtrSizedArray< abstr_future_tile_ptr >;

TIC_CALL auto GetAbstrFutureTileArray(const AbstrDataObject* ado) -> abstr_future_tile_array;

template<typename V>
auto GetFutureTileArray(const DataArrayBase<V>* da)
{
	using future_tile = typename DataArrayBase<V>::future_tile;
	using future_tile_ptr = SharedPtr<future_tile>;
	using future_tile_array = OwningPtrSizedArray< future_tile_ptr >;
	assert(da); // PRECONDITION
	auto tn = da->GetTiledRangeData()->GetNrTiles();
	auto result = future_tile_array(tn, ValueConstruct_tag());
	for (tile_id t = 0; t != tn; ++t)
		result[t] = da->GetFutureTile(t);
	return result;
}

#endif // __TIC_FUTURETILEARRAY_H
