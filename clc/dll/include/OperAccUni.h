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

#if !defined(__CLC_OPERACCUNI_H)
#define __CLC_OPERACCUNI_H

#include "UnitClass.h"

#include "AggrUniStruct.h"
#include "AggrUniStructString.h"
#include "FutureTileArray.h"
#include "Explain.h"
#include "OperAcc.h"
#include "TreeItemClass.h"
#include "IndexGetterCreator.h"

// *****************************************************************************
//											AbstrOperAccTotUni
// *****************************************************************************

struct AbstrOperAccTotUni: UnaryOperator
{
	AbstrOperAccTotUni(AbstrOperGroup* gr, ClassCPtr resultCls, ClassCPtr arg1Cls, UnitCreatorPtr ucp, ValueComposition vc)
		:	UnaryOperator(gr, resultCls, arg1Cls)
		,	m_UnitCreatorPtr(std::move(ucp))
		,	m_ValueComposition(vc)
	{
		gr->SetCanExplainValue();
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		assert(args.size() == 1);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);

		auto argSeq = GetItems(args);
		resultHolder = CreateCacheDataItem(
			Unit<Void>::GetStaticClass()->CreateDefault(),
			(*m_UnitCreatorPtr)(GetGroup(), argSeq),
			m_ValueComposition
		);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, Explain::Context* context) const override
	{
		dms_assert(resultHolder);
		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		dms_assert(res);

		dms_assert(args.size() == 1);
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A);

		bool dontRecalc = res->m_DataObject;
		dms_assert(dontRecalc || !context);
		if (!dontRecalc)
		{
			DataReadLock arg1Lock(arg1A);
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero); 
			Calculate(resLock, arg1A);
			resLock.Commit();
		}
		if (context)
		{
			const AbstrUnit* adu = arg1A->GetAbstrDomainUnit();
			SizeT n = adu->GetCount();
			MakeMin(n, Explain::MaxNrEntries);
			for (SizeT i=0; i!=n; ++i)
				DMS_CalcExpl_AddQueueEntry(context->m_CalcExpl, adu, i);
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A) const =0;

private:
	UnitCreatorPtr m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};

template <class TAcc1Func> 
struct OperAccTotUni : AbstrOperAccTotUni
{
	typedef typename TAcc1Func::value_type1     ValueType;
	typedef typename TAcc1Func::assignee_type   AccumulationType;
	typedef typename TAcc1Func::dms_result_type ResultValueType;
	typedef DataArray<ResultValueType>          ResultType;
	typedef DataArray<ValueType>                ArgType;
			
	OperAccTotUni(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func())
		:	AbstrOperAccTotUni(gr
			,	ResultType::GetStaticClass(), ArgType::GetStaticClass()
			,	&TAcc1Func::template unit_creator<ResultValueType>, COMPOSITION(ResultValueType)
			)
		,	m_Acc1Func(acc1Func)
	{}

protected:
	TAcc1Func m_Acc1Func;
};

// *****************************************************************************
//											AbstrOperAccPartUni
// *****************************************************************************

struct AbstrOperAccPartUni: BinaryOperator
{
	AbstrOperAccPartUni(AbstrOperGroup* gr, 
			ClassCPtr resultCls, ClassCPtr arg1Cls, ClassCPtr arg2Cls, 
			UnitCreatorPtr ucp, ValueComposition vc
		)
		:	BinaryOperator(gr, resultCls, arg1Cls, arg2Cls)
		,	m_UnitCreatorPtr(std::move(ucp))
		,	m_ValueComposition(vc)
	{
		gr->SetCanExplainValue();
	}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		MG_PRECONDITION(args.size() == 2);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);

		const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();
		e2->UnifyDomain(e1, "e2", "e1", UM_Throw);

		const AbstrUnit* p2 = arg2A->GetAbstrValuesUnit();
		MG_PRECONDITION(p2);

		auto argSeq = GetItems(args);
		resultHolder = CreateCacheDataItem(p2, (*m_UnitCreatorPtr)(GetGroup(), argSeq), m_ValueComposition);
	}

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, Explain::Context* context) const override
	{
		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		const AbstrDataItem* arg2A = AsDataItem(args[1]);
		dms_assert(arg1A);
		dms_assert(arg2A);
		const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();

		dms_assert(resultHolder);
//		DataReadLock arg2Lock(arg2A);

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
		dms_assert(res);
		bool doRecalc = !res->m_DataObject;
		dms_assert(context || doRecalc);
		if (doRecalc)
		{
//			DataReadLock arg1Lock(arg1A);

			DataWriteLock resLock(res);

			Calculate(resLock, arg1A, arg2A);

			resLock.Commit();
		}
		if (context)
		{
			SizeT f = context->m_Coordinate->first;

			// search in partitioning for values with index f.
			const AbstrDataObject* ado = arg2A->GetCurrRefObj();
			SizeT i = 0, k = 0, n = ado->GetTiledRangeData()->GetElemCount();
			while (i < n) {
				i = ado->FindPosOfSizeT(f, i);
				if (!IsDefined(i))
					break;
				DMS_CalcExpl_AddQueueEntry(context->m_CalcExpl, e2, i);
				if (++k >= Explain::MaxNrEntries)
					break;
				++i;
			}
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A,  const AbstrDataItem* arg2A) const =0;

private:
	UnitCreatorPtr   m_UnitCreatorPtr;
	ValueComposition m_ValueComposition;
};

// *****************************************************************************
//											OperAccPartUni
// *****************************************************************************

template <typename TAcc1Func, typename ResultValueType> 
struct OperAccPartUniMixin
{
	typedef typename TAcc1Func::value_type1            ValueType;
	typedef typename TAcc1Func::accumulation_seq       AccumulationSeq;
	typedef DataArray<ResultValueType>                 ResultType;
	typedef DataArray<ValueType>                       Arg1Type; // value vector
	typedef AbstrDataItem                              Arg2Type; // index vector, must be partition type
};

namespace impl {
	namespace has_dms_result_type_details {
		template< typename U> static char test(typename U::dms_result_type* v);
		template< typename U> static int  test(...);
	}

	template< typename T>
	struct has_dms_result_type
	{
		static const bool value = sizeof(has_dms_result_type_details::test<T>(nullptr)) == sizeof(char);
	};
}

//template <typename TAcc1Func, typename ResultValueType = typename dms_result_type_of<TAcc1Func>::type> 
template <typename TAcc1Func, typename ResultValueType = typename TAcc1Func::accumulation_seq::value_type> 
struct OperAccPartUni: AbstrOperAccPartUni, OperAccPartUniMixin<TAcc1Func, ResultValueType>
{
	OperAccPartUni(AbstrOperGroup* gr, const TAcc1Func& acc1Func) 
	:	AbstrOperAccPartUni(gr
		,   OperAccPartUni::ResultType::GetStaticClass(), OperAccPartUni::Arg1Type::GetStaticClass(), OperAccPartUni::Arg2Type::GetStaticClass()
		,	&TAcc1Func::template unit_creator<ResultValueType>, COMPOSITION(ResultValueType)
		)
	,	m_Acc1Func(acc1Func)

	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A,  const AbstrDataItem* arg2A) const override
	{
		auto arg1 = (DataReadLock(arg1A), MakeShared(const_array_cast<typename OperAccPartUni::ValueType>(arg1A)));
		assert(arg1);
		assert(arg2A);

		tile_id tn  = arg1A->GetAbstrDomainUnit()->GetNrTiles();

		ProcessData(
			mutable_array_cast<ResultValueType>(res)
		,	std::move(arg1), arg2A
		, 	tn,	arg1A->HasUndefinedValues()
		);
	}
	virtual void ProcessData(typename OperAccPartUni::ResultType* result, SharedPtr<const typename OperAccPartUni::Arg1Type> arg1, const AbstrDataItem* arg2, tile_id nrTiles, bool arg1HasUndefinedValues) const=0;
protected:
	TAcc1Func m_Acc1Func;
};

template <class TAcc1Func> 
struct OperAccPartUniBuffered : OperAccPartUni<TAcc1Func, typename TAcc1Func::dms_result_type>
{
	OperAccPartUniBuffered(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		:	OperAccPartUni<TAcc1Func, typename TAcc1Func::dms_result_type>(gr, acc1Func)
	{}

	void ProcessData(typename OperAccPartUniBuffered::ResultType* result,
		SharedPtr<const typename OperAccPartUniBuffered::Arg1Type> arg1,
		const AbstrDataItem* arg2, tile_id nrTiles, bool arg1HasUndefinedValues) const override
	{
		using res_buffer_type = typename sequence_traits<typename TAcc1Func::accumulation_type>::container_type;
		SizeT resCount = arg2->GetAbstrValuesUnit()->GetCount();
//		auto resBuffer = res_buffer_type(resCount);
		res_buffer_type resBuffer(resCount);

		this->m_Acc1Func.Init(OperAccPartUniBuffered::AccumulationSeq( &resBuffer ) );

		for (tile_id t = 0; t!=nrTiles; ++t)
		{
			auto arg1Data = arg1->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2, t);

			this->m_Acc1Func(
				OperAccPartUniBuffered::AccumulationSeq( &resBuffer )
			,	arg1Data
			,	indexGetter 
			,	arg1HasUndefinedValues
			);
		}

		this->m_Acc1Func.AssignOutput(result->GetDataWrite(no_tile, dms_rw_mode::write_only_all), resBuffer);
	}
};

template <class TAcc1Func> 
struct OperAccPartUniDirect : OperAccPartUni<TAcc1Func, typename TAcc1Func::result_type>
{
	OperAccPartUniDirect(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		:	OperAccPartUni<TAcc1Func, typename TAcc1Func::result_type>(gr, acc1Func)
	{}

	void ProcessData(typename OperAccPartUniDirect::ResultType* result, SharedPtr<const typename OperAccPartUniDirect::Arg1Type> arg1, const AbstrDataItem* arg2, tile_id nrTiles, bool arg1HasUndefinedValues) const override
	{
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all); // check what Init does
		m_Acc1Func.Init(resData);

		auto values_fta = GetFutureTileArray(arg1.get()); arg1 = nullptr;
		auto part_fta = (DataReadLock(arg2), GetAbstrFutureTileArray(arg2->GetRefObj()));

		for (tile_id t = 0; t!=nrTiles; ++t)
		{
			auto arg1Data = arg1->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2, t);

			m_Acc1Func(
				resData
			,	arg1Data
			,	indexGetter
			,	arg1HasUndefinedValues
			);
		}
	}
private:
	TAcc1Func m_Acc1Func;
};

template <class TAcc1Func> struct make_direct   { typedef OperAccPartUniDirect  <TAcc1Func> type; };
template <class TAcc1Func> struct make_buffered { typedef OperAccPartUniBuffered<TAcc1Func> type; };

template <class TAcc1Func> using base_of = std::conditional_t< impl::has_dms_result_type<TAcc1Func>::value,	make_buffered<TAcc1Func>, make_direct<TAcc1Func> >;

template <class TAcc1Func> 
struct OperAccPartUniBest: base_of<TAcc1Func>
{
	OperAccPartUniBest(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		:	base_of<TAcc1Func>(gr, acc1Func)
	{}
};

template <typename TAcc1Func> 
struct OperAccPartUniSer_impl : OperAccPartUniMixin<TAcc1Func, SharedStr>
{
	OperAccPartUniSer_impl(const TAcc1Func& acc1Func = TAcc1Func())
		:	m_Acc1Func(acc1Func)
	{}

	void operator() (typename OperAccPartUniSer_impl::ResultType* result, const typename OperAccPartUniSer_impl::Arg1Type *arg1, const typename OperAccPartUniSer_impl::Arg2Type *arg2, tile_id nrTiles, bool arg1HasUndefinedValues)
	{
		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
//		m_Acc1Func.Init(resData);
		{
			length_finder_array lengthFinderArray(resData.size());
			for (tile_id t = 0; t!=nrTiles; ++t)
			{
				auto arg1Data = arg1->GetTile(t);
				OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2, t);

				m_Acc1Func.InspectData(
					lengthFinderArray
				,	arg1Data
				,	indexGetter 
				,	arg1HasUndefinedValues
				);
			}
			// allocate sufficent space in outputs
			InitOutput(resData, lengthFinderArray);
			// can destruct lengthFinderArray now
		}

		// build memo_out_stream_array that refers to each of the allocated output cells.
		memo_out_stream_array outStreamArray;
		m_Acc1Func.ReserveData(outStreamArray, resData);

		for (tile_id t = 0; t!=nrTiles; ++t)
		{
			auto arg1Data = arg1->GetTile(t);
			OwningPtr<IndexGetter> indexGetter = IndexGetterCreator::Create(arg2, t);

			m_Acc1Func.ProcessTileData(
				outStreamArray
			,	arg1Data
			,	indexGetter
			,	arg1HasUndefinedValues
			);
		}
	}
	private:
		TAcc1Func m_Acc1Func;
};


template <typename TAcc1Func> 
struct OperAccPartUniSer : OperAccPartUni<TAcc1Func, SharedStr>
{
	OperAccPartUniSer(AbstrOperGroup* gr, const TAcc1Func& acc1Func = TAcc1Func()) 
		:	OperAccPartUni<TAcc1Func, SharedStr>(gr, acc1Func)
	{}

	void ProcessData(typename OperAccPartUniSer::ResultType* result, SharedPtr<const typename OperAccPartUniSer::Arg1Type> arg1, const AbstrDataItem* arg2, tile_id nrTiles, bool arg1HasUndefinedValues) const override
	{
		OperAccPartUniSer_impl<TAcc1Func> impl(this->m_Acc1Func);
		impl(result, arg1, arg2, nrTiles, arg1HasUndefinedValues);
	}
};

#endif

