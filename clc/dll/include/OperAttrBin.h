// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_OPERATTRBIN_H)
#define __CLC_OPERATTRBIN_H

#include "dbg/DmsCatch.h"
#include "mci/ValueWrap.h"
#include "ASync.h"

#include "AbstrUnit.h"
#include "DataItemClass.h"

#include "Operator.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "UnitCreators.h"
#include "UnitProcessor.h"

// *****************************************************************************
//			AttrAttr operators
// *****************************************************************************

struct AbstrBinaryAttrOper : BinaryOperator
{
	AbstrBinaryAttrOper(AbstrOperGroup* gr
		,	const DataItemClass* resCls
		,	const DataItemClass* arg1Cls
		,	const DataItemClass* arg2Cls
		,	UnitCreatorPtr ucp
		,	ValueComposition vc
		,	ArgFlags possibleArgFlags
		)	:	BinaryOperator(gr, resCls, arg1Cls, arg2Cls)
			,	m_UnitCreatorPtr(std::move(ucp))
			,	m_ValueComposition(vc)
			,	m_PossibleArgFlags(possibleArgFlags)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);

		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit(); bool e1Void = e1->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit(); bool e2Void = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();

		const AbstrUnit* e= e1Void ? e2 : e1;
		if (!mustCalc && !e1Void && !e2Void)
			e1->UnifyDomain(e2, "e1", "e2", UM_Throw);

		if (!resultHolder)
		{
			dms_assert(!mustCalc);
			resultHolder = CreateCacheDataItem(e, (*m_UnitCreatorPtr)(GetGroup(), args), m_ValueComposition);
		}
		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			ArgFlags af = static_cast<ArgFlags>((e1Void ? AF1_ISPARAM : 0) | (e2Void ? AF2_ISPARAM : 0) );
			if (m_PossibleArgFlags & AF1_HASUNDEFINED) reinterpret_cast<UInt32&>(af) |= (arg1A->HasUndefinedValues() ? AF1_HASUNDEFINED : 0);
			if (m_PossibleArgFlags & AF2_HASUNDEFINED) reinterpret_cast<UInt32&>(af) |= (arg2A->HasUndefinedValues() ? AF2_HASUNDEFINED : 0);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

			auto tn = e->GetNrTiles();
			auto valuesUnitA = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
			if (IsMultiThreaded3() && (tn > 1) && (LTF_ElementWeight(arg1A) + LTF_ElementWeight(arg2A) <= LTF_ElementWeight(res)))
				res->m_DataObject = CreateFutureTileFunctor(res, res->GetLazyCalculatedState(), valuesUnitA, arg1A, arg2A, af MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + GetGroup()->GetName().c_str()"));
			else
			{
				DataWriteLock resLock(res);

				parallel_tileloop(tn,
					[=, &resLock](tile_id t)->void
					{
						Calculate(resLock, arg1A, arg2A, af, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}

	virtual SharedPtr<const AbstrDataObject> CreateFutureTileFunctor(SharedPtr<AbstrDataItem>, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;
	virtual void Calculate(AbstrDataObject* borrowedDataHandle, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A,	ArgFlags af, tile_id t
	) const=0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
	ArgFlags         m_PossibleArgFlags;
};

template <typename ResultValueType, typename Arg1ValueType, typename Arg2ValueType>
struct BinaryAttrOper : AbstrBinaryAttrOper
{
	using Arg1Type = DataArray<Arg1ValueType>;
	using Arg2Type = DataArray<Arg2ValueType>;
	using ResultType = DataArray<ResultValueType>;

public:
	BinaryAttrOper(AbstrOperGroup* gr, UnitCreatorPtr ucp, ValueComposition vc, ArgFlags possibleArgFlags)
		: AbstrBinaryAttrOper(gr
			, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			, ucp, vc, ArgFlags(AF1_HASUNDEFINED | AF2_HASUNDEFINED)
		)
	{}

	SharedPtr<const AbstrDataObject> CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto rangedArg = (af & AF1_ISPARAM) ? arg2A : arg1A;
		auto tileRangeData = AsUnit(rangedArg->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<Arg1ValueType>(arg1A)); assert(arg1);
		auto arg2 = MakeShared(const_array_cast<Arg2ValueType>(arg2A)); assert(arg2);

		using prepare_data = std::pair<SharedPtr<typename Arg1Type::future_tile>, SharedPtr<typename Arg2Type::future_tile>>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1, arg2, af](tile_id t) { return prepare_data{ arg1->GetFutureTile(af & AF1_ISPARAM ? 0 : t), arg2->GetFutureTile(af & AF2_ISPARAM ? 0 : t) }; }
			, [resultAdi, this, af MG_DEBUG_ALLOCATOR_SRC_PARAM](sequence_traits<ResultValueType>::seq_t resData, prepare_data futureData)
			{
				if (resultAdi->WasFailed(FR_Data))
					resultAdi->ThrowFail();
				try {
					auto futureTileA = throttled_async([&futureData] { return futureData.first->GetTile();  });
					auto tileB = futureData.second->GetTile();
					this->CalcTile(resData, futureTileA.get().get_view(), tileB.get_view(), af MG_DEBUG_ALLOCATOR_SRC_PARAM);
				}
				catch (...)
				{
					resultAdi->CatchFail(FailType::FR_Data);
					throw;
				}
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, ArgFlags af, tile_id t) const override
	{
		auto arg1Data = const_array_cast<Arg1ValueType>(arg1A)->GetTile(af & AF1_ISPARAM ? 0 : t);
		auto arg2Data = const_array_cast<Arg2ValueType>(arg2A)->GetTile(af & AF2_ISPARAM ? 0 : t);
		auto resData = mutable_array_cast<ResultValueType>(res)->GetWritableTile(t);

		CalcTile(resData, arg1Data, arg2Data, af MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
	}

	virtual void CalcTile(sequence_traits<ResultValueType>::seq_t resData, sequence_traits<Arg1ValueType>::cseq_t arg1Data, sequence_traits<Arg2ValueType>::cseq_t arg2Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;
};

#endif //!defined(__CLC_OPERATTRBIN_H)
