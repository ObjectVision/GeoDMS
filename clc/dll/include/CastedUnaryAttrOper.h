// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__CLC_CASTEDUNARYATTROPER_H)
#define __CLC_CASTEDUNARYATTROPER_H

#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "UnitClass.h"
#include "TileFunctorImpl.h"

// *****************************************************************************
//									AbstrCastedUnaryAttrOperator
// *****************************************************************************

class AbstrCastedUnaryAttrOperator : public BinaryOperator
{
public:
	AbstrCastedUnaryAttrOperator(AbstrOperGroup* gr, const DataItemClass* resultType, const DataItemClass* argDataType, const UnitClass* argUnitType, ValueComposition vc, bool reverseArgs)
		:	BinaryOperator(gr
			,	resultType 
			,	reverseArgs ? typesafe_cast<const Class*>(argUnitType) : typesafe_cast<const Class*>(argDataType)
			,	reverseArgs ? typesafe_cast<const Class*>(argDataType) : typesafe_cast<const Class*>(argUnitType)
			)
		,	m_VC(vc)
		,	m_ReverseArgs(reverseArgs)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrDataItem* argDataA = AsDataItem(args[m_ReverseArgs ? 1 : 0]);
		assert(argDataA);

		const AbstrUnit* argUnitA= AsUnit(args[m_ReverseArgs ? 0 : 1]);

		if (!resultHolder)
		{
			resultHolder = CreateCacheDataItem(argDataA->GetAbstrDomainUnit(), argUnitA, m_VC);
			if (argUnitA->GetTSF(TSF_Categorical))
				resultHolder->SetTSF(TSF_Categorical);
		}

		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataReadLock arg1Lock(argDataA);

			tile_id nrTiles = argDataA->GetAbstrDomainUnit()->GetNrTiles();
			if (IsMultiThreaded3() && (nrTiles > 1) && !res->HasRepetitiveUsers() && (LTF_ElementWeight(argDataA) <= LTF_ElementWeight(res)))
			{
				auto valuesUnitA = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileCaster(res, res->GetLazyCalculatedState(), valuesUnitA, argDataA, argUnitA MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + :  + GetGroup()->GetName().c_str()"));
			}
			else
			{
				DataWriteLock resLock(res, dms_rw_mode::write_only_all);

				parallel_tileloop(nrTiles, [this, argDataA, argUnitA, &resLock, res](tile_id t)->void
					{
						try {
							this->Calculate(resLock, argDataA, argUnitA, t);
						}
						catch (const DmsException& x)
						{
							res->Fail(x.GetAsText(), FR_Data);
						}
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrDataItem* argDataA, const AbstrUnit* argUnit, tile_id t) const =0;
	virtual auto CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> = 0;

private:
	ValueComposition m_VC;
	bool             m_ReverseArgs;
};

class AbstrMappingOperator : public BinaryOperator
{
public:
	AbstrMappingOperator(AbstrOperGroup* gr, const DataItemClass* resultType, const UnitClass* argDomainUnitType, const UnitClass* argValuesUnitType)
		:	BinaryOperator(gr
			,	resultType 
			,	argDomainUnitType
			,	argValuesUnitType
			)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 2);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		assert(argDomainUnit);
		assert(argValuesUnit);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argDomainUnit, argValuesUnit);
		
		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			MG_PRECONDITION(res);

			assert(res->GetValueComposition() == ValueComposition::Single); 
			if (IsMultiThreaded3())
			{
				auto binaryOper = this;

				auto tileRangeData = AsUnit(res->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
				auto valuesUnit = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
				visit<typelists::fields>(valuesUnit, [binaryOper, res, argDomainUnit, argValuesUnit, tileRangeData]<typename V>(const Unit<V>*valuesUnit) {
					SharedUnitInterestPtr retainedArgDomainUnit = argDomainUnit;
					SharedUnitInterestPtr retainedArgValuesUnit = argValuesUnit;
					auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(res, tileRangeData, valuesUnit->m_RangeDataPtr
						, [binaryOper, res, retainedArgDomainUnit, retainedArgValuesUnit](AbstrDataObject* self, tile_id t) {
							binaryOper->Calculate(self, retainedArgDomainUnit, retainedArgValuesUnit, t); // write into the same tile.
						}
						MG_DEBUG_ALLOCATOR_SRC("res->md_FullName +  := MappingOperator()")
					);
					res->m_DataObject = lazyTileFunctor.release();
				}
				);
			}
			else
			{
				DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

				parallel_tileloop(argDomainUnit->GetNrTiles(), [this, argDomainUnit, argValuesUnit, &resLock](tile_id t)->void
					{
						this->Calculate(resLock, argDomainUnit, argValuesUnit, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual void Calculate(AbstrDataObject* res, const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit, tile_id t) const =0;
};

class AbstrMappingCountOperator : public TernaryOperator
{
public:
	AbstrMappingCountOperator(AbstrOperGroup* gr, const DataItemClass* resultType, 
			const UnitClass* argDomainUnitType, 
			const UnitClass* argValuesUnitType, 
			const UnitClass* resultUnitType
)
		: TernaryOperator(gr
			, resultType
			, argDomainUnitType
			, argValuesUnitType
			, resultUnitType
		)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 3);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		const AbstrUnit* resValuesUnit = AsUnit(args[2]);
		assert(argDomainUnit);
		assert(argValuesUnit);
		assert(resValuesUnit);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argValuesUnit, resValuesUnit);

		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

			for (tile_id t = 0, te = argDomainUnit->GetNrTiles(); t != te; ++t)
				Calculate(resLock, argDomainUnit, argValuesUnit, t);
			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrUnit* argDomainUnit, const AbstrUnit* argValuesUnit, tile_id t) const = 0;
};

// *****************************************************************************
//									CastedUnaryAttrSpecialFuncOperator
// *****************************************************************************

template <typename TUniOper>
class CastedUnaryAttrSpecialFuncOperator : public AbstrCastedUnaryAttrOperator
{
	typedef typename TUniOper::arg1_type  Arg1Values;
	typedef DataArray<Arg1Values>         Arg1Type;
	typedef typename TUniOper::res_type   ResultValueType;
	typedef field_of_t<ResultValueType>   ResultFieldType;
	typedef DataArray<ResultValueType>    ResultType;
	typedef Unit     <ResultFieldType>    ValuesUnitType;
			
public:
	CastedUnaryAttrSpecialFuncOperator(AbstrOperGroup* gr)
		:	AbstrCastedUnaryAttrOperator(gr 
			,	ResultType::GetStaticClass() 
			,	Arg1Type::GetStaticClass()
			,	ValuesUnitType::GetStaticClass()
			,	composition_of<typename TUniOper::res_type>::value
			,	false
			)
	{}

	// Override Operator
	auto CreateFutureTileCaster(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<Arg1Values>(arg1A));
		assert(arg1);

		using prepare_data = SharedPtr<Arg1Type::future_tile>;
	
		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [](sequence_traits<ResultValueType>::seq_t resData, prepare_data arg1FutureData)
			{
				auto arg1Data = arg1FutureData->GetTile();
				auto oper = TUniOper();
				std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}
	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrUnit* argUnit, tile_id t) const override
	{
		assert(arg1A);
		const Arg1Type* arg1 = const_array_cast<Arg1Values>(arg1A);

		ResultType* result = mutable_array_cast<ResultValueType>(res);
		assert(result);

		auto arg1Data = arg1->GetTile(t);
		auto resData = result->GetWritableTile(t);

		MG_DEBUGCODE(dms_assert(arg1Data.size() == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )
		MG_DEBUGCODE(dms_assert(resData.size() == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )

		auto oper = TUniOper();
		std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
	}
};


#endif //!defined(__CLC_CASTEDUNARYATTROPER_H)
