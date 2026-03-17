// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_BINARYATTRFUNC_H)
#define __CLC_BINARYATTRFUNC_H

#include "mem/tiledata.h"

#include "DataLocks.h"
#include "operator.h"
#include "UnitProcessor.h"

template <typename V>
struct ConstTileFunctor : GeneratedTileFunctor<V>
{
	using typename TileFunctor<V>::locked_cseq_t;
	using typename TileFunctor<V>::locked_seq_t;

	using cache_t = OwningPtrSizedArray<std::weak_ptr<tile<V>>>;

	ConstTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<V> valueRangePtr, tile_offset maxTileSize, V value MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
		: GeneratedTileFunctor<V>(tiledDomainRangeData, valueRangePtr MG_DEBUG_ALLOCATOR_SRC_PARAM)
		, m_MaxTileSize(maxTileSize)
		, m_Value(value)
	{}

//	auto GetWritableTile(tile_id t, dms_rw_mode rwMode)->locked_seq_t override;

	//	auto GetWritableTile(tile_id t, dms_rw_mode rwMode) ->locked_seq_t override;
	auto GetTile(tile_id t) const->locked_cseq_t override
	{
		assert(t < this->GetTiledRangeData()->GetNrTiles());

		std::lock_guard lock(cs_Handle);

		auto tileSPtr = m_ActiveTile.lock();
		if (!tileSPtr)
		{
			tileSPtr = std::make_shared<tile<V>>();
			tileSPtr->resizeSO(m_MaxTileSize, false MG_DEBUG_ALLOCATOR_SRC("this->md_SrcStr"));
			fast_fill(tileSPtr->begin(), tileSPtr->end(), m_Value);

			m_ActiveTile = tileSPtr;
		}
		
		tile_offset currTileSize = this->GetTiledRangeData()->GetTileSize(t);
		assert(tileSPtr->size() >= currTileSize);

		return locked_cseq_t(tileSPtr, sequence_traits<V>::cseq_t(tileSPtr->begin(), tileSPtr->begin()+currTileSize));
	}

	tile_offset m_MaxTileSize;
	V m_Value;
	mutable std::mutex cs_Handle;
	mutable std::weak_ptr < tile<V> > m_ActiveTile;
};

template <typename V>
auto make_unique_ConstTileFunctor(const AbstrTileRangeData* tiledDomainRangeData, range_data_ptr_or_void<V> valueRangePtr, tile_offset maxTileSize, V v MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr))
{
	return std::make_unique<ConstTileFunctor<V>>(tiledDomainRangeData, valueRangePtr, maxTileSize, v MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

// *****************************************************************************
//			ABSTR CONST
// *****************************************************************************

struct AbstrConstOperator : public BinaryOperator
{
	AbstrConstOperator(AbstrOperGroup* gr, const DataItemClass* resultType, ValueComposition vc)
		:	BinaryOperator(gr
			,	resultType
			,	resultType
			,	AbstrUnit::GetStaticClass()
			)
		,	m_VC(vc)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);

		if (!arg1A->HasVoidDomainGuarantee())
			arg1A->throwItemError("Should have a void domain");

		const AbstrUnit* arg2U = AsUnit(args[1]);
		assert(arg2U);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(arg2U, arg1A->GetAbstrValuesUnit(), m_VC);
			resultHolder.GetNew()->SetFreeDataState(true); // never cache
		}

		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataReadLock arg1Lock(arg1A);

			auto trd = arg2U->GetTiledRangeData();
			MG_CHECK(trd);

			auto tn = trd->GetNrTiles();
			if (tn > 1 || tn == 1 && arg2U->GetTiledRangeData()->GetTileSize(0) >= 256)
				res->m_DataObject = CreateConstFunctor(arg1A, arg2U MG_DEBUG_ALLOCATOR_SRC(res->md_FullName + " := ConstFunctor()"));
			else
			{
				DataWriteLock resLock(res);

				for (tile_id t = 0; t != tn; ++t)
				{
					Calculate(resLock.get(), arg1A, t);
				}
				resLock.Commit();
			}
		}
		return true;
	}
	virtual SharedPtr<const AbstrDataObject> CreateConstFunctor(const AbstrDataItem* arg1A, const AbstrUnit* arg2U MG_DEBUG_ALLOCATOR_SRC(SharedStr srcStr)) const = 0;
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* arg1A, tile_id t) const =0;

private:
	ValueComposition m_VC;
};

// *****************************************************************************
//			ABSTR CONST PARAM
// *****************************************************************************

struct AbstrConstParamOperator : public Operator
{
	AbstrConstParamOperator(AbstrOperGroup* gr, const DataItemClass* resultType, const UnitClass* resultValuesUnitType, ValueComposition vc)
		:	Operator(gr, resultType)
		,	m_ValuesUnitType(resultValuesUnitType)
		,	m_VC(vc)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 0);

		if (!resultHolder)
		{
			resultHolder = 
				CreateCacheDataItem(
					Unit<Void>::GetStaticClass()->CreateDefault()
				,	m_ValuesUnitType->CreateDefault()
				,	m_VC
				);
			resultHolder.GetNew()->SetFreeDataState(true); // never cache
		}
		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(resLock);

			resLock.Commit();
		}
		return true;
	}

	virtual void Calculate(DataWriteLock& res) const =0;

private:
	const UnitClass* m_ValuesUnitType;
	ValueComposition m_VC;
};

// *****************************************************************************
//			CONST PARAM
// *****************************************************************************

template <typename TR, typename TV=TR>
class ConstParamOperator : public AbstrConstParamOperator
{
public:
	ConstParamOperator(AbstrOperGroup* gr, TV value)
		:	AbstrConstParamOperator(gr, DataArray<TR>::GetStaticClass(), Unit<field_of_t<TR>>::GetStaticClass(), composition_of_v<TR>) 
		,	m_Value(value)
	{}

	// Override Operator
	void Calculate(DataWriteLock& res) const override
	{
		auto result = mutable_array_cast<TR>(res);
		Assign( result->GetWritableTile(0)[0], m_Value);
	}

private:
	TV m_Value;
};



#endif //!defined(__CLC_BINARYATTRFUNC_H)
