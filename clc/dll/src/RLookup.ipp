// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "ser/AsString.h"
#include "set/VectorFunc.h"
#include "geo/StringBounds.h"
#include "geo/GeoSequence.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "Metric.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"

#include "lookup.h"

#include "rlookup.h"

// *****************************************************************************
//                         IndexedSearchOperator
// *****************************************************************************

// rlookup(E1->V, E->V): E1->E

static TokenID s_nrA= GetTokenID_st("nr_1");

class AbstrIndexedSearchOperator : public BinaryOperator
{
	typedef AbstrDataItem      ResultType;

public:
	AbstrIndexedSearchOperator(AbstrOperGroup* gr, const Class* argClass)
		:	BinaryOperator(gr, ResultType::GetStaticClass(), argClass, argClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A= AsDataItem(args[0]);
		const AbstrDataItem* arg2A= AsDataItem(args[1]);
		assert(arg1A);
		assert(arg2A);

		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* arg2_DomainUnit = arg2A->GetAbstrDomainUnit();
		assert(arg1_DomainUnit);
		assert(arg2_DomainUnit);


		if (!resultHolder)
		{
			compatible_values_unit_creator_func(0, GetGroup(), args, true);
			resultHolder = CreateCacheDataItem(arg1_DomainUnit, arg2_DomainUnit);
			resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			tile_id nrTiles = arg1_DomainUnit->GetNrTiles();

			auto index = MakeIndex(arg2A, arg2_DomainUnit);
			const AbstrUnit* arg2Domain = arg2A->GetAbstrDomainUnit();
			auto arg2DomainRange = arg2Lock->GetTiledRangeData();
			if (IsMultiThreaded3() && (nrTiles > 1) && (LTF_ElementWeight(arg1A) <= LTF_ElementWeight(res)) && (nrTiles > arg2DomainRange->GetNrTiles()))
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileIndexer(res, res->GetLazyCalculatedState(), arg2_DomainUnit, arg1A, arg2Domain, arg2DomainRange, std::move(index) MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + RLookup()"));
			else
			{
				DataWriteLock resLock(res);
				parallel_tileloop(nrTiles, [this, &resLock, arg1A, arg2Domain, &index](tile_id t)->void
					{
						this->Calculate(resLock.get(), arg1A, arg2Domain, index, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual std::any MakeIndex(const AbstrDataItem* arg2A, const AbstrUnit* arg2_DomainUnit) const = 0;
	virtual auto CreateFutureTileIndexer(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain, const AbstrTileRangeData* arg2DomainRange, std::any index MG_DEBUG_ALLOCATOR_SRC_ARG) const->SharedPtr<const AbstrDataObject> = 0;
	virtual void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrUnit* arg2Domain, const std::any&, tile_id t) const =0;
};

template <class V>
class SearchIndexOperator : public AbstrIndexedSearchOperator
{
	typedef DataArray<V> ArgumentType;

public:
	SearchIndexOperator(AbstrOperGroup* og)
		: AbstrIndexedSearchOperator(og, ArgumentType::GetStaticClass())
	{}

	std::any MakeIndex(const AbstrDataItem* arg2A, const AbstrUnit* arg2DomainA) const override
	{
		auto values = const_array_cast<V>(arg2A)->GetDataRead();
		std::any result;
		visit<typelists::domain_elements>(arg2DomainA, [&result, &values]<typename E>(const Unit<E>* arg2Domain) 
		{
			result = make_index_array< index_type_t<E>, V>(values);
		});
		return result;
	}
};

template <class V, class IndexApplicator>
class SearchIndexOperatorImpl : public SearchIndexOperator<V>
{
	typedef DataArray<V> ArgumentType;

public:
	SearchIndexOperatorImpl(AbstrOperGroup* og)
		: SearchIndexOperator<V>(og)
	{}

	auto CreateFutureTileIndexer(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* arg2DomainA, const AbstrTileRangeData* arg2DomainRange, std::any indexBox MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
//		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<V>(arg1A));
		dms_assert(arg1);
		auto indexBoxPtr = std::make_shared<std::any>(std::move(indexBox));
		using prepare_data = SharedPtr<typename TileFunctor<V>::future_tile>;
		std::unique_ptr<AbstrDataObject> futureTileFunctor;

		visit<typelists::domain_elements>(arg2DomainA
		,	[&futureTileFunctor, resultAdi, lazy, arg2DomainRange, arg1, indexBoxPtr, tileRangeData MG_DEBUG_ALLOCATOR_SRC_PARAM]<typename E>(const Unit<E>* arg2Domain)
		{
			using index_type = index_type_t<E>;
			using index_tile = indexed_tile_t<index_type, V>;
			using res_seq_t = sequence_traits<E>::seq_t;

			futureTileFunctor = make_unique_FutureTileFunctor<E, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(arg2Domain)
				, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
				, [arg2DomainRange = dynamic_cast<const typename Unit<E>::range_data_t*>(arg2DomainRange)->GetRange(), indexBoxPtr](res_seq_t resData, prepare_data arg1FutureData)
				{
					auto arg1Data = arg1FutureData->GetTile();

					static_assert(!std::is_same_v<E, WPoint> || std::is_same_v<index_type, UInt32>);
					static_assert(std::is_same_v<index_type, index_tile::first_type::value_type>);

					auto indexPtr = std::any_cast<index_tile>(indexBoxPtr.get());
					assert(indexPtr);

					CalcTile<res_seq_t, E>(resData, arg1Data, indexPtr, arg2DomainRange);
				}
				MG_DEBUG_ALLOCATOR_SRC_PARAM
			);
		});

		return futureTileFunctor.release();
	}

	template <typename ResultIndexView, typename E>
	static void CalcTile(ResultIndexView resData, typename sequence_traits<V>::cseq_t arg1Data, const indexed_tile_t<index_type_t<E>, V>* indexPtr, typename Unit<E>::range_t arg2DomainRange)
	{
		IndexApplicator indexApplicator;
		indexApplicator.apply<ResultIndexView, typename sequence_traits<V>::cseq_t>(resData, std::move(arg1Data), indexPtr->second.get_view(), arg2DomainRange, indexPtr->first);

		assert(resData.size() == arg1Data.size());
	}

	void Calculate(AbstrDataObject* resObj, const AbstrDataItem* arg1A, const AbstrUnit* arg2DomainA, const std::any& indexBox, tile_id t) const override
	{
		visit<typelists::domain_elements>(arg2DomainA,
			[resObj, arg1Data = const_array_cast<V>(arg1A)->GetTile(t), &indexBox, t]<typename E>(const Unit<E>*arg2Domain)
		{
			auto resData = mutable_array_cast<E>(resObj)->GetWritableTile(t);

			using index_type = index_type_t<E>;
			using index_tile = indexed_tile_t<index_type, V>;
			using res_seq_t = sequence_traits<E>::seq_t;

			auto indexPtr = std::any_cast<index_tile>(&indexBox);
			assert(indexPtr);

			CalcTile<res_seq_t, E>(resData.get_view(), arg1Data.get_view(), indexPtr, arg2Domain->GetRange());
		}
		);
	}
};
