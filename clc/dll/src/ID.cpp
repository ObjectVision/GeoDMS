// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/CheckedCalc.h"
#include "mci/CompositeCast.h"
#include "ptr/InterestHolders.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "LispTreeType.h"
#include "ParallelTiles.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "TileFunctorImpl.h"

// *****************************************************************************
//                         UNARY RELATIONAL FUNCTIONS
// *****************************************************************************
// Generate_key(E1->Bool): E1->E2 x (E2) NOT!
// ID(E1)          : E1->E1
// Subset(E1->Bool): E2 x (E2->E1)
// Unique
// Relate

// *****************************************************************************
//                         ID
// *****************************************************************************

CommonOperGroup cog_id(token::id);

class AbstrIDOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrIDOperator(const Class* resultClass, const Class* arg1Class)
		: UnaryOperator(&cog_id, resultClass, arg1Class) 
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrUnit* e1 = AsUnit(args[0]);
		assert(e1);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(e1, e1);
			resultHolder.GetNew()->SetFreeDataState(true); // never cache
			resultHolder->SetTSF(TSF_Categorical);
		}
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			const AbstrIDOperator* idOper = this;
			auto trd = e1->GetTiledRangeData();
			auto lazyFunctorCreator = [idOper, res, trd]<typename V>(const Unit<V>*domainUnit) {
				auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(res, trd, domainUnit->m_RangeDataPtr
				,	[idOper, res, trd](AbstrDataObject* self, tile_id t) {
						idOper->Calculate(self, trd, t); // write into the same tile.
					}
					MG_DEBUG_ALLOCATOR_SRC("res->md_FullName +  := id()")
				);
				res->m_DataObject = lazyTileFunctor.release();
			};

			e1 = AsUnit(e1->GetCurrRangeItem());
			visit<typelists::domain_elements>(e1, std::move(lazyFunctorCreator));
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrTileRangeData* anyRange, tile_id t) const =0;
};

template <class E1>
class IDOperator : public AbstrIDOperator
{
	typedef Unit<E1>       Arg1Type;
	typedef DataArray<E1>  ResultType;

public:
	// Override Operator
	IDOperator() : AbstrIDOperator(ResultType::GetStaticClass(), Arg1Type::GetStaticClass()) 
	{}

	void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrTileRangeData* tileRanges, tile_id t) const override
	{
		ResultType* result = mutable_array_cast<E1>(borrowedDataHandle);
		assert(result);

		auto resData = result->GetWritableTile(t, dms_rw_mode::write_only_all);
		auto resRange = debug_cast<const typename Unit<E1>::range_data_t*>(tileRanges)->GetTileRange(t);
		CalcTile(resData, resRange MG_DEBUG_ALLOCATOR_SRC("borrowedDataHandle->md_SrcStr"));
	}

	void CalcTile(sequence_traits<E1>::seq_t resData, Unit<E1>::range_t resRange MG_DEBUG_ALLOCATOR_SRC_ARG) const
	{
		auto
			i = resData.begin(),
			e = resData.end();

		dms_assert((e-i) == Cardinality(resRange));

		for (row_id count = 0; i != e; ++i)
			*i = Range_GetValue_naked(resRange, count++);
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	tl_oper::inst_tuple_templ<typelists::domain_elements, IDOperator > 
		operInstances;
} // end anonymous namespace


