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

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, ArgRefs args, std::vector<ItemReadLock> readLocks) const override
	{
		auto arg1 = const_array_cast<typename OperAccTotUniNum::ValueType>(arg1A);
		assert(arg1);

		auto result = mutable_array_cast<typename OperAccTotUniNum::ResultValueType>(res);
		assert(result);

		typename TAcc1Func::assignee_type value;
		this->m_Acc1Func.Init(value); // set value to MIN_VALUE, MAX_VALUE, or NULL depending on the TAcc1Func type

		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();

		// TODO G8: OPTIMIZE, use parallel_for and ThreadLocal container and aggregate afterwards.
		auto values_fta = (DataReadLock(arg1A), GetFutureTileArray(arg1));
		for (tile_id t = 0, te = e->GetNrTiles(); t!=te; ++t)
		{
			auto arg1Data = values_fta[t]->GetTile(); values_fta[t] = nullptr;
			this->m_Acc1Func(value, arg1Data.get_view());
		}

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
