// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_OPERACCUNINUM_H)
#define __CLC_OPERACCUNINUM_H

#include "set/VectorFunc.h"
#include "mci/CompositeCast.h"
#include "geo/Conversions.h"
#include "geo/Point.h"

#include "Param.h"
#include "DataItemClass.h"

#include "AggrUniStructNum.h"
#include "OperAccUni.h"
#include "OperRelUni.h"

// *****************************************************************************
//											CLASSES
// *****************************************************************************

template <class TAcc1Func> 
struct OperAccTotUniNum : OperAccTotUni<TAcc1Func>
{
	OperAccTotUniNum(AbstrOperGroup* gr, TAcc1Func&& acc1Func = TAcc1Func()) 
		: OperAccTotUni<TAcc1Func>(gr, std::move(acc1Func))
	{}

//	using accumulator_type = typename decltype TAcc1Func::accumulator_type;
	using value_type = typename TAcc1Func::value_type1;
	using ftptr = future_tile_ptr<value_type>;

	auto AggregateTiles(ftptr* values_fta, tile_id t, tile_id te, SizeT availableThreads) const -> decltype(this->m_Acc1Func.InitialValue())
	{
		if (te - t > 1)
		{
			auto m = t + (te - t) / 2;
			auto mt = availableThreads / 2;
			auto futureFirstHalfValue = std::async(mt ? std::launch::async : std::launch::deferred, [this, values_fta, t, m, mt]()
				{
					return AggregateTiles(values_fta, t, m, mt);
				});
			t = m;
			availableThreads -= mt;
			auto secondHalfValue = AggregateTiles(values_fta, t, te, availableThreads);
			auto firstHalfValue  = futureFirstHalfValue.get();

			this->m_Acc1Func.CombineValues(firstHalfValue, secondHalfValue);
			return firstHalfValue;
		}

		auto value = this->m_Acc1Func.InitialValue(); // set value to MIN_VALUE, MAX_VALUE, or NULL depending on the TAcc1Func type
		if (t<te)
		{
			auto arg1Data = values_fta[t]->GetTile(); values_fta[t] = nullptr;
			this->m_Acc1Func(value, arg1Data.get_view());
		}

		return value;
	}

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		auto arg1 = const_array_cast<typename OperAccTotUniNum::ValueType>(arg1A);
		assert(arg1);

		auto result = mutable_array_cast<typename OperAccTotUniNum::ResultValueType>(res);
		assert(result);

		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();

		// TODO G8: OPTIMIZE, use parallel_for and ThreadLocal container and aggregate afterwards.
		auto values_fta = GetFutureTileArray(arg1);

		auto value = AggregateTiles(values_fta.begin(), 0, e->GetNrTiles(), MaxConcurrentTreads());

		auto resData = result->GetDataWrite();
		assert(resData.size() == 1);
		this->m_Acc1Func.AssignOutput(resData[0], value );
	}
};


// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace OperAccUniNum
{
	template<
		template <typename> class TotlOper, 
		template <typename> class PartOper, 
		typename ValueTypes
	>
	struct AggrOperators
	{
		AggrOperators(AbstrOperGroup* gr) 
			: m_TotlAggrOpers(gr)
			, m_PartAggrOpers(gr)
		{}

		template <typename T> struct OperAccTotUniNumOper : OperAccTotUniNum   <TotlOper<T>> {
			using OperAccTotUniNum<TotlOper<T>>::OperAccTotUniNum; 
		};
		template <typename T> struct OperAccPartUniNumOper : OperAccPartUniBest<PartOper<T>> {
			using OperAccPartUniBest<PartOper<T>>::OperAccPartUniBest;
		};

	private:
		tl_oper::inst_tuple_templ<ValueTypes, OperAccTotUniNumOper , AbstrOperGroup*> m_TotlAggrOpers;
		tl_oper::inst_tuple_templ<ValueTypes, OperAccPartUniNumOper, AbstrOperGroup*> m_PartAggrOpers;
	};
}

#endif //!defined(__CLC_OPERACCUNINUM_H)
