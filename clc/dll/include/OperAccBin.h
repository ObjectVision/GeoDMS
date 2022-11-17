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

#if !defined(__CLC_OPERACCBIN_H)
#define __CLC_OPERACCBIN_H

#include "mci/CompositeCast.h"

#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include "IndexGetterCreator.h"
#include "OperAcc.h"
#include "UnitCreators.h"
#include "AggrUniStruct.h"

// *****************************************************************************
//											CLASSES
// *****************************************************************************

struct AbstrOperAccTotBin : public BinaryOperator
{
	AbstrOperAccTotBin(AbstrOperGroup* gr
	,	ClassCPtr resultCls, ClassCPtr arg1Cls, ClassCPtr arg2Cls
	,	UnitCreatorPtr ucp, ValueComposition vc
	)	:	BinaryOperator(gr, resultCls, arg1Cls, arg2Cls) 
		,	m_UnitCreatorPtr(std::move(ucp))
		,	m_ValueComposition(vc)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);
		
		const AbstrDataItem *arg1A= AsDataItem(args[0]);
		const AbstrDataItem *arg2A= AsDataItem(args[1]);

		dms_assert(arg1A);
		dms_assert(arg2A);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(
				Unit<Void>::GetStaticClass()->CreateDefault(), 
				(*m_UnitCreatorPtr)(GetGroup(), args), 
				m_ValueComposition
			);

		if (mustCalc)
		{		
			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			MG_PRECONDITION(res);

			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);

			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero); 

			Calculate(resLock, arg1A, arg2A);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(
		DataWriteLock& res,
		const AbstrDataItem* arg1A, 
		const AbstrDataItem* arg2A
	) const =0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};

template <class TAcc2Func> 
struct OperAccTotBin : AbstrOperAccTotBin
{
	typedef typename TAcc2Func::value_type1     ValueType1;
	typedef typename TAcc2Func::value_type2     ValueType2;
	typedef typename TAcc2Func::assignee_type   AccumulationType;
	typedef typename TAcc2Func::dms_result_type ResultValueType;
	
	typedef DataArray<ValueType1>      Arg1Type;
	typedef DataArray<ValueType2>      Arg2Type;
	typedef DataArray<ResultValueType> ResultType;

	OperAccTotBin(AbstrOperGroup* gr, const TAcc2Func& acc2Func = TAcc2Func()) 
		:	AbstrOperAccTotBin(gr
			,	ResultType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
			,	&TAcc2Func::unit_creator, COMPOSITION(ResultValueType)
			)
		,	m_Acc2Func(acc2Func)
	{}

	// Override Operator
	void Calculate(
		DataWriteLock& res,
		const AbstrDataItem* arg1A, 
		const AbstrDataItem* arg2A
	) const override
	{
		const Arg1Type* arg1 = const_array_cast<ValueType1>(arg1A);
		const Arg2Type* arg2 = const_array_cast<ValueType2>(arg2A);

		dms_assert(arg1);
		dms_assert(arg2);

		ResultType* result = mutable_array_cast<ResultValueType>(res);
		dms_assert(result);

		AccumulationType value;
		m_Acc2Func.Init(value);
		const AbstrUnit* e = arg1A->GetAbstrDomainUnit();
		for (tile_id t = 0, te = e->GetNrTiles(); t!=te; ++t)
		{
			m_Acc2Func(value, 
				arg1->GetTile(t), 
				arg2->GetTile(t),
				arg1A->HasUndefinedValues() || arg2A->HasUndefinedValues()
			);
		}

//		TAcc2Func::dms_result_type resValue;
//		m_Acc2Func.AssignOutput(resValue, value);
//		Assign(result->GetDataWrite()[0], Convert<ResultValueType>(resValue) );

		m_Acc2Func.AssignOutput(result->GetDataWrite()[0], value);
	}
private:
	TAcc2Func m_Acc2Func;
};

// *****************************************************************************
//											AbstrOperAccPartBin
// *****************************************************************************

struct AbstrOperAccPartBin: TernaryOperator
{
	AbstrOperAccPartBin(AbstrOperGroup* gr
	,	ClassCPtr resultCls, ClassCPtr arg1Cls, ClassCPtr arg2Cls, ClassCPtr arg3Cls
	,	UnitCreatorPtr ucp, ValueComposition vc
	)	:	TernaryOperator(gr, resultCls, arg1Cls, arg2Cls, arg3Cls)
		,	m_ValueComposition(vc)
		,	m_UnitCreatorPtr(std::move(ucp))
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		MG_PRECONDITION(args.size() == 3);

		const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
		const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);
		const AbstrDataItem* arg3A = debug_cast<const AbstrDataItem*>(args[2]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		dms_assert(arg3A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();
		const AbstrUnit* e3 = arg3A->GetAbstrDomainUnit();
		e3->UnifyDomain(e1, "e3", "e1", UM_Throw);
		e3->UnifyDomain(e2, "e3", "e2", UM_Throw);

		const AbstrUnit* p3 = arg3A->GetAbstrValuesUnit();
		dms_assert(p3);

		if (!resultHolder)
			resultHolder
				=	CreateCacheDataItem(
						p3,
						(*m_UnitCreatorPtr)(GetGroup(), args),
						m_ValueComposition
					);

		if (mustCalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataReadLock arg2Lock(arg2A);
			DataReadLock arg3Lock(arg3A);

			AbstrDataItem* res = debug_cast<AbstrDataItem*>(resultHolder.GetNew());
			dms_assert(res);
			DataWriteLock resLock(res);

			Calculate(resLock, arg1A, arg2A, arg3A);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(
		DataWriteLock& res,
		const AbstrDataItem* arg1A, 
		const AbstrDataItem* arg2A, 
		const AbstrDataItem* arg3A
	) const =0;
private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};

// *****************************************************************************
//											OperAccPartBin
// *****************************************************************************

template <class TAcc2Func> 
struct OperAccPartBin : AbstrOperAccPartBin
{
	typedef typename TAcc2Func::value_type1            ValueType1;
	typedef typename TAcc2Func::value_type2		       ValueType2;
	typedef typename TAcc2Func::accumulation_seq       AccumulationSeq;
	typedef typename TAcc2Func::dms_result_type        ResultValueType;

	typedef DataArray<ValueType1>      Arg1Type; // value vector
	typedef DataArray<ValueType2>      Arg2Type; // value vector
	typedef DataArray<ResultValueType> ResultType;

	OperAccPartBin(AbstrOperGroup* gr, const TAcc2Func& acc2Func = TAcc2Func()) 
		:	AbstrOperAccPartBin(gr
			,	ResultType::GetStaticClass()
			,	Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass(), AbstrDataItem::GetStaticClass()
			,	&TAcc2Func::unit_creator, COMPOSITION(ResultValueType)
			)
		,	m_Acc2Func(acc2Func)
	{}

	// Override Operator
	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A, const AbstrDataItem* arg3A) const override
	{
		const Arg1Type *arg1 = const_array_cast<ValueType1>(arg1A);
		const Arg2Type *arg2 = const_array_cast<ValueType2>(arg2A);

		dms_assert(arg1);
		dms_assert(arg2);
		dms_assert(arg3A);

		tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();

		typename sequence_traits<typename AccumulationSeq::value_type>::container_type
			resBuffer(arg3A->GetAbstrValuesUnit()->GetCount());

		for (tile_id t=0; t!=tn; ++t)
		{
			auto arg1Data = arg1->GetTile(t);
			auto arg2Data = arg2->GetTile(t);

			dms_assert(arg1Data.size() == arg2Data.size());

			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg3A, t);
	
			m_Acc2Func(
				AccumulationSeq( &resBuffer ) // explicit constructor
			,	arg1Data, arg2Data
			,	indexGetter
			,	arg1A->HasUndefinedValues() || arg2A->HasUndefinedValues()
			);
		}
		using buffered_assigner_functor = assign_partial_output_from_buffer<assign_output_direct<ResultValueType>>;
		buffered_assigner_functor outputAssigner;
		auto resultData = mutable_array_cast<ResultValueType>(res)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
		outputAssigner.AssignOutput(resultData, resBuffer);
	}
private:
	TAcc2Func m_Acc2Func;
};

#endif //!defined(__CLC_OPERACCBIN_H)
