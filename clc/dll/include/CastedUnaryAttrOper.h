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

#if !defined(__CLC_CASTEDUNARYATTROPER_H)
#define __CLC_CASTEDUNARYATTROPER_H

#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "UnitClass.h"

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
		dms_assert(args.size() == 2);

		const AbstrDataItem* argDataA = AsDataItem(args[m_ReverseArgs ? 1 : 0]);
		dms_assert(argDataA);

		const AbstrUnit* argUnitA= AsUnit(args[m_ReverseArgs ? 0 : 1]);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(
				argDataA->GetAbstrDomainUnit() 
			,	argUnitA 
			,	m_VC
			);
		
		if (mustCalc)
		{
			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataReadLock arg1Lock(argDataA);

			tile_id nrTiles = argDataA->GetAbstrDomainUnit()->GetNrTiles();
			if (IsMultiThreaded3() && (nrTiles > 1) && !res->HasRepetitiveUsers())
			{
				auto valuesUnitA = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileCaster(valuesUnitA, argDataA, argUnitA MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + :  + GetGroup()->GetName().c_str()"));
			}
			else
			{
				DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

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
	virtual SharedPtr<const AbstrDataObject> CreateFutureTileCaster(const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;

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
		dms_assert(args.size() == 2);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		dms_assert(argDomainUnit);
		dms_assert(argValuesUnit);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(argDomainUnit, argValuesUnit);
		
		if (mustCalc)
		{
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			MG_PRECONDITION(res);

			if (res->GetValueComposition() == ValueComposition::Single) // IsMultiThreaded3()
			{
				auto binaryOper = this;

				auto tileRangeData = AsUnit(res->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
				auto valuesUnit = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
				visit<typelists::fields>(valuesUnit, [binaryOper, res, argDomainUnit, argValuesUnit, tileRangeData]<typename V>(const Unit<V>*valuesUnit) {
					SharedUnitInterestPtr retainedArgDomainUnit = argDomainUnit;
					SharedUnitInterestPtr retainedArgValuesUnit = argValuesUnit;
					auto lazyTileFunctor = make_unique_LazyTileFunctor<V>(tileRangeData, valuesUnit->m_RangeDataPtr, tileRangeData->GetNrTiles()
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
		dms_assert(args.size() == 3);

		const AbstrUnit* argDomainUnit = AsUnit(args[0]);
		const AbstrUnit* argValuesUnit = AsUnit(args[1]);
		const AbstrUnit* resValuesUnit = AsUnit(args[2]);
		dms_assert(argDomainUnit);
		dms_assert(argValuesUnit);
		dms_assert(resValuesUnit);

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
	SharedPtr<const AbstrDataObject> CreateFutureTileCaster(const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrUnit* argUnitA MG_DEBUG_ALLOCATOR_SRC_ARG) const override
	{
		auto tileRangeData = AsUnit(arg1A->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		const Arg1Type* arg1 = const_array_cast<Arg1Values>(arg1A);
		dms_assert(arg1);

		using prepare_data = SharedPtr<Arg1Type::future_tile>;
	
		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(tileRangeData, get_range_ptr_of_valuesunit(valuesUnit), tileRangeData->GetNrTiles()
			, [arg1](tile_id t) { return arg1->GetFutureTile(t); }
			, [](sequence_traits<ResultValueType>::seq_t resData, prepare_data arg1FutureData)
			{
				auto arg1Data = arg1FutureData->GetTile();
				TUniOper oper;
				std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}
	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrUnit* argUnit, tile_id t) const override
	{
		dms_assert(arg1A);
		const Arg1Type* arg1 = const_array_cast<Arg1Values>(arg1A);

		ResultType* result = mutable_array_cast<ResultValueType>(res);
		dms_assert(result);

		auto arg1Data = arg1->GetTile(t);
		auto resData = result->GetWritableTile(t);

		MG_DEBUGCODE( dms_assert(arg1Data.size() == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )
		MG_DEBUGCODE( dms_assert(resData.size()  == arg1A->GetAbstrDomainUnit()->GetTileCount(t)); )

		TUniOper oper; 
		std::transform(arg1Data.begin(), arg1Data.end(), resData.begin(), oper);
	}
};


#endif //!defined(__CLC_CASTEDUNARYATTROPER_H)
