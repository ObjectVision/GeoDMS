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
#pragma once

#if !defined(__CLC_OPERATTRTER_H)
#define __CLC_OPERATTRTER_H

#include "mci/ValueWrap.h"

#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "ParallelTiles.h"
#include "TileFunctorImpl.h"
#include "UnitCreators.h"
#include "UnitProcessor.h"


// *****************************************************************************
//										AbstrTernaryAttrOper
// *****************************************************************************

struct AbstrTernaryAttrOper : TernaryOperator
{
	AbstrTernaryAttrOper(AbstrOperGroup* gr
		,	const DataItemClass* resultCls
		,	const DataItemClass* arg1Cls
		,	const DataItemClass* arg2Cls
		,	const DataItemClass* arg3Cls
		,	UnitCreatorPtr ucp, ValueComposition vc
		,	bool needsUndefInfo
		)	:	TernaryOperator(gr, resultCls, arg1Cls, arg2Cls, arg3Cls)
			,	m_UnitCreatorPtr(std::move(ucp))
			,	m_ValueComposition(vc)
			,	m_NeedsUndefInfo(needsUndefInfo)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 3);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* arg3A = debug_cast<const AbstrDataItem*>(args[2]);

		assert(arg1A);
		assert(arg2A);
		assert(arg3A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit(); bool e1Void = e1->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit(); bool e2Void = e2->GetValueType() == ValueWrap<Void>::GetStaticClass();
		const AbstrUnit* e3 = arg3A->GetAbstrDomainUnit(); bool e3Void = e3->GetValueType() == ValueWrap<Void>::GetStaticClass();

		const AbstrUnit* e = e1Void ? (e2Void ? e3 : e2) : e1;
		if (!e1Void && e!=e1) e->UnifyDomain(e1, "e0", "e1", UM_Throw);
		if (!e2Void && e!=e2) e->UnifyDomain(e2, "e0", "e2", UM_Throw);
		if (!e3Void && e!=e3) e->UnifyDomain(e3, "e0", "e3", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, (*m_UnitCreatorPtr)(GetGroup(), args), m_ValueComposition);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);
			ArgFlags af = static_cast<ArgFlags>(
				(e1Void ? AF1_ISPARAM : 0)
			|	(e2Void ? AF2_ISPARAM : 0)
			|	(e3Void ? AF3_ISPARAM : 0)
			);
			if (m_NeedsUndefInfo)
				reinterpret_cast<UInt32&>(af) |= 
					(arg1A->HasUndefinedValues() ? AF1_HASUNDEFINED : 0 )
				|	(arg2A->HasUndefinedValues() ? AF2_HASUNDEFINED : 0 )
				|	(arg3A->HasUndefinedValues() ? AF3_HASUNDEFINED : 0 );

			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			assert(res);
			assert(res->GetAbstrDomainUnit() == e);
			auto tn = e->GetNrTiles();

			auto valuesUnitA = AsUnit(res->GetAbstrValuesUnit()->GetCurrRangeItem());
			if (IsMultiThreaded3() && (tn > 1) && (LTF_ElementWeight(arg1A) + LTF_ElementWeight(arg2A) + LTF_ElementWeight(arg3A) <= LTF_ElementWeight(res)))
				AsDataItem(resultHolder.GetOld())->m_DataObject = CreateFutureTileFunctor(res, res->GetLazyCalculatedState(), valuesUnitA, arg1A, arg2A, arg3A, af MG_DEBUG_ALLOCATOR_SRC("res->md_FullName + GetGroup()->GetName().c_str()"));
			else
			{
				DataWriteLock resLock(res);

				parallel_tileloop(tn,
					[=, &resLock](tile_id t)->void
					{
						Calculate(resLock.get(), arg1A, arg2A, arg3A, af, t);
					}
				);
				resLock.Commit();
			}
		}
		return true;
	}
	virtual auto CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> =0;
	virtual void Calculate(AbstrDataObject* borrowedDataHandle,	const AbstrDataItem* arg1A,	const AbstrDataItem* arg2A,	const AbstrDataItem* arg3A,	ArgFlags af, tile_id t) const =0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
	bool             m_NeedsUndefInfo;
};

template <typename ResultValueType, typename Arg1ValueType, typename Arg2ValueType, typename Arg3ValueType>
struct TernaryAttrOper : AbstrTernaryAttrOper
{
	using Arg1Type = DataArray<Arg1ValueType>;
	using Arg2Type = DataArray<Arg2ValueType>;
	using Arg3Type = DataArray<Arg3ValueType>;
	using ResultType = DataArray<ResultValueType>;

public:
	TernaryAttrOper(AbstrOperGroup* gr, UnitCreatorPtr ucp, ValueComposition vc, bool needsUndefInfo)
		: AbstrTernaryAttrOper(gr, ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), Arg3Type::GetStaticClass(), ucp, vc, needsUndefInfo)
	{}

	auto CreateFutureTileFunctor(SharedPtr<AbstrDataItem> resultAdi, bool lazy, const AbstrUnit* valuesUnitA, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const -> SharedPtr<const AbstrDataObject> override
	{
		auto rangedArg = (af & AF1_ISPARAM) ? (af & AF2_ISPARAM) ? arg3A : arg2A : arg1A;
		auto tileRangeData = AsUnit(rangedArg->GetAbstrDomainUnit()->GetCurrRangeItem())->GetTiledRangeData();
		auto valuesUnit = debug_cast<const Unit<field_of_t<ResultValueType>>*>(valuesUnitA);

		auto arg1 = MakeShared(const_array_cast<Arg1ValueType>(arg1A)); assert(arg1);
		auto arg2 = MakeShared(const_array_cast<Arg2ValueType>(arg2A)); assert(arg2);
		auto arg3 = MakeShared(const_array_cast<Arg3ValueType>(arg3A)); assert(arg3);

		using prepare_data = std::tuple<SharedPtr<typename Arg1Type::future_tile>, SharedPtr<typename Arg2Type::future_tile>, SharedPtr<typename Arg3Type::future_tile>>;
		auto futureTileFunctor = make_unique_FutureTileFunctor<ResultValueType, prepare_data, false>(resultAdi, lazy, tileRangeData, get_range_ptr_of_valuesunit(valuesUnit)
			, [arg1, arg2, arg3, af](tile_id t) { return prepare_data{ arg1->GetFutureTile(af & AF1_ISPARAM ? 0 : t), arg2->GetFutureTile(af & AF2_ISPARAM ? 0 : t), arg3->GetFutureTile(af & AF3_ISPARAM ? 0 : t) }; }
			, [this, af MG_DEBUG_ALLOCATOR_SRC_PARAM](sequence_traits<ResultValueType>::seq_t resData, prepare_data futureData)
			{
				this->CalcTile(resData, std::get<0>(futureData)->GetTile().get_view(), std::get<1>(futureData)->GetTile().get_view(), std::get<2>(futureData)->GetTile().get_view(), af MG_DEBUG_ALLOCATOR_SRC_PARAM);
			}
			MG_DEBUG_ALLOCATOR_SRC_PARAM
		);

		return futureTileFunctor.release();
	}

	void Calculate(AbstrDataObject* res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A, ArgFlags af, tile_id t) const override
	{
		auto arg1Data = const_array_cast<Arg1ValueType>(arg1A)->GetTile(af & AF1_ISPARAM ? 0 : t);
		auto arg2Data = const_array_cast<Arg2ValueType>(arg2A)->GetTile(af & AF2_ISPARAM ? 0 : t);
		auto arg3Data = const_array_cast<Arg3ValueType>(arg3A)->GetTile(af & AF3_ISPARAM ? 0 : t);
		auto resData = mutable_array_cast<ResultValueType>(res)->GetWritableTile(t);

		CalcTile(resData, arg1Data, arg2Data, arg3Data, af MG_DEBUG_ALLOCATOR_SRC("res->md_SrcStr"));
	}

	virtual void CalcTile(sequence_traits<ResultValueType>::seq_t resData, sequence_traits<Arg1ValueType>::cseq_t arg1Data, sequence_traits<Arg2ValueType>::cseq_t arg2Data, sequence_traits<Arg3ValueType>::cseq_t arg3Data, ArgFlags af MG_DEBUG_ALLOCATOR_SRC_ARG) const = 0;
};


#endif //!defined(__CLC_OPERATTRTER_H)
