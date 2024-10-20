// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "RtcTypeLists.h"

#include "geo/CheckedCalc.h"
#include "utl/TypeListOper.h"

#include "AbstrUnit.h"

#include "AggrFuncNum.h"
#include "AggrUniStruct.h"

namespace Cumulate
{

// *****************************************************************************
//											CLASSES
// *****************************************************************************

	struct AbstrCumulateTotUni: UnaryOperator
	{
		AbstrCumulateTotUni(AbstrOperGroup* gr
		,	ClassCPtr resultCls, ClassCPtr arg1Cls
		,	UnitCreatorPtr ucp, ValueComposition vc
		)	:	UnaryOperator(gr, resultCls, arg1Cls)
			,	m_ValueComposition(vc)
			,	m_UnitCreatorPtr(std::move(ucp))
		{}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			assert(args.size() == 1);

			const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
			assert(arg1A);

			if (!resultHolder)
				resultHolder = CreateCacheDataItem(
					arg1A->GetAbstrDomainUnit(),
					(*m_UnitCreatorPtr)(GetGroup(), args), 
					m_ValueComposition
				);

			if (mustCalc)
			{
				DataReadLock arg1Lock(arg1A);
				AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
				assert(res);

				DataWriteLock resLock(res);

				Calculate(resLock, arg1A);

				resLock.Commit();
			}
			return true;
		}
		virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A) const =0;

	private:
		UnitCreatorPtr   m_UnitCreatorPtr;
		ValueComposition m_ValueComposition;
	};

	template <typename TUniAssigner, typename TInitAssigner>
	struct CumulateTot: AbstrCumulateTotUni
	{
		typedef typename TUniAssigner::arg1_type       ValueType;
		typedef typename TUniAssigner::assignee_type   AccumulationType;
		typedef typename TUniAssigner::assignee_type   ResultValueType;
		typedef DataArray<ValueType>                   ResType;
		typedef DataArray<ValueType>                   ArgType;
			
		CumulateTot(AbstrOperGroup* gr, const TUniAssigner& uniAsigner = TUniAssigner()) 
			:	AbstrCumulateTotUni(gr
				,	ResType::GetStaticClass(), ArgType::GetStaticClass()
				,	arg1_values_unit, COMPOSITION(ResultValueType)
				)
			,	m_UniAssigner(uniAsigner)
		{}

		// Override Operator
		void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A) const override
		{
			const ArgType* arg1 = const_array_cast<ValueType>(arg1A);
			assert(arg1);

			ResType* result = mutable_array_cast<ValueType>(res);
			assert(result);

			ResultValueType value;
			TInitAssigner init;
			init(value);

			bool arg1HasUndefinedValues = arg1A->HasUndefinedValues();
			const AbstrUnit* e = arg1A->GetAbstrDomainUnit();
			for (tile_id t = 0, te = e->GetNrTiles(); t!=te; ++t)
			{
				auto arg1Data = arg1->GetTile(t);
				auto resData  = result->GetWritableTile(t, dms_rw_mode::write_only_all);
				assert(arg1Data.size() == resData.size());
					
				auto resPtr = resData.begin();
				if (arg1HasUndefinedValues)
					for (auto arg1Ptr = arg1Data.begin(), arg1End = arg1Data.end(); arg1Ptr != arg1End; ++resPtr, ++arg1Ptr)
					{
						if (IsDefined(*arg1Ptr))
							m_UniAssigner(value, *arg1Ptr);
						*resPtr = value;
					}
				else
					for (auto arg1Ptr = arg1Data.begin(), arg1End = arg1Data.end(); arg1Ptr != arg1End; ++resPtr, ++arg1Ptr)
					{
						m_UniAssigner(value, *arg1Ptr);
						*resPtr = value;
					}
			}
		}
	protected:
		TUniAssigner m_UniAssigner;
	};

	struct AbstrCumulatePartUni: BinaryOperator
	{
		AbstrCumulatePartUni(AbstrOperGroup* gr
		,	ClassCPtr resultCls, ClassCPtr arg1Cls, ClassCPtr arg2Cls
		,	UnitCreatorPtr ucp, ValueComposition vc
		)	:	BinaryOperator(gr, resultCls, arg1Cls, arg2Cls)
			,	m_ValueComposition(vc)
			,	m_UnitCreatorPtr(std::move(ucp))
		{}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			assert(args.size() == 2);

			const AbstrDataItem* arg1A = debug_cast<const AbstrDataItem*>(args[0]);
			assert(arg1A);
			const AbstrDataItem* arg2A = debug_cast<const AbstrDataItem*>(args[1]);
			assert(arg2A);

			const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
			const AbstrUnit* e2 = arg2A->GetAbstrDomainUnit();
			e2->UnifyDomain(e1, "e2", "e1", UM_Throw);

			if (!resultHolder)
				resultHolder = CreateCacheDataItem(
					arg1A->GetAbstrDomainUnit(),
					(*m_UnitCreatorPtr)(GetGroup(), args), 
					m_ValueComposition
				);

			if (mustCalc)
			{
				DataReadLock arg1Lock(arg1A);
				DataReadLock arg2Lock(arg2A);

				AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
				assert(res);

				DataWriteLock resLock(res);

				Calculate(resLock, arg1A, arg2A);

				resLock.Commit();
			}
			return true;
		}
		virtual void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const =0;

	private:
		UnitCreatorPtr   m_UnitCreatorPtr;
		ValueComposition m_ValueComposition;
	};

	template <typename TUniAssigner, typename TInitAssigner, typename PartType>
	struct CumulatePart: AbstrCumulatePartUni
	{
		typedef typename TUniAssigner::arg1_type       ValueType;
		typedef typename TUniAssigner::assignee_type   AccumulationType;
		typedef typename TUniAssigner::assignee_type   ResultValueType;
		typedef DataArray<ValueType>                   ResType;
		typedef DataArray<ValueType>                   Arg1Type;
		typedef DataArray<PartType>                    Arg2Type;
			
		CumulatePart(AbstrOperGroup* gr, const TUniAssigner& uniAsigner = TUniAssigner()) 
			:	AbstrCumulatePartUni(gr
				,	ResType::GetStaticClass(), Arg1Type::GetStaticClass(), Arg2Type::GetStaticClass()
				,	arg1_values_unit, COMPOSITION(ResultValueType)
				)
			,	m_UniAssigner(uniAsigner)
		{}

		// Override Operator
		void Calculate(DataWriteLock& res, const AbstrDataItem* arg1A, const AbstrDataItem* arg2A) const override
		{
			const Arg1Type* arg1 = const_array_cast<ValueType>(arg1A);
			assert(arg1);
			const Arg2Type* arg2 = const_array_cast<PartType >(arg2A);
			assert(arg2);

			ResType* result = mutable_array_cast<ValueType>(res);
			assert(result);

			bool arg1HasUndefinedValues = arg1A->HasUndefinedValues();
			const AbstrUnit* e = arg1A->GetAbstrDomainUnit();

			auto partitionSet = arg2->GetValueRangeData();
			for (tile_id tp=0, tpe= partitionSet->GetNrTiles(); tp!=tpe; ++tp)
			{
				auto partRange = partitionSet->GetTileRange(tp);
				SizeT partRangeSize = Cardinality(partRange);
				OwningPtrSizedArray<ResultValueType> valueArray( partRangeSize, dont_initialize MG_DEBUG_ALLOCATOR_SRC("Cumulate: valueArray"));
				TInitAssigner init;
				for (auto valuePtr = valueArray.begin(), valueEnd = valueArray.end(); valuePtr!=valueEnd; ++valuePtr)
					init(*valuePtr);

				for (tile_id t = 0, te = e->GetNrTiles(); t!=te; ++t)
				{
					auto arg1Data = arg1->GetTile(t);
					auto arg2Data = arg2->GetTile(t);
					auto resData  = result->GetWritableTile(t);
					assert(arg1Data.size() == resData.size());
					assert(arg2Data.size() == resData.size());
					
					auto arg1Ptr = arg1Data.begin(), arg1End = arg1Data.end();
					auto arg2Ptr = arg2Data.begin();
					auto resPtr = resData.begin();

					if (arg1HasUndefinedValues)
						for (; arg1Ptr != arg1End; ++resPtr, ++arg2Ptr, ++arg1Ptr)
						{
							SizeT ppos = Range_GetIndex_checked(partRange, *arg2Ptr);
							if (IsDefined(ppos))
							{
								assert(ppos < partRangeSize);
								auto valuePtr = valueArray.begin() + ppos;
								if (IsDefined(*arg1Ptr))
								{
									m_UniAssigner(*valuePtr, *arg1Ptr);
								}
								*resPtr = *valuePtr;
							}
							else if (tp == 0)
								*resPtr = UNDEFINED_VALUE(ResultValueType);
						}
					else
						for (; arg1Ptr != arg1End; ++resPtr, ++arg2Ptr, ++arg1Ptr)
						{
							SizeT ppos = Range_GetIndex_checked(partRange, *arg2Ptr);
							if (IsDefined(ppos))
							{
								assert(ppos < partRangeSize);
								auto valuePtr = valueArray.begin() + ppos;
								m_UniAssigner(*valuePtr, *arg1Ptr);
								*resPtr = *valuePtr;
							}
							else if (tp == 0)
								*resPtr = UNDEFINED_VALUE(ResultValueType);
						}
				}
			}
		}
	protected:
		TUniAssigner m_UniAssigner;
	};


	template<class UniAssigner, class IniAssign>
	struct CumulOperInstances
	{
		CumulOperInstances(AbstrOperGroup* gr)
			:	m_AggrTotlOperator(gr)
			,	m_AggrPartOperator(gr)
		{}
	private:
		CumulateTot<UniAssigner, IniAssign> m_AggrTotlOperator;

		template <typename T> using CumulatePartThis = CumulatePart<UniAssigner, IniAssign, T>;

		tl_oper::inst_tuple_templ<typelists::partition_elements, CumulatePartThis, AbstrOperGroup*>
			m_AggrPartOperator;
	};

	template<
		template <typename> class UniAssigner, 
		class IniAssigner, 
		typename ValueTypes
	>
	struct CumulOperators
	{
		CumulOperators(AbstrOperGroup* gr)
			: m_AggrOperators(gr) 
		{}

	private:
		template <typename T> using ThisCumulOperInstances = CumulOperInstances<UniAssigner<T>, IniAssigner>;

		tl_oper::inst_tuple_templ<ValueTypes, ThisCumulOperInstances, AbstrOperGroup*>
			m_AggrOperators;
	};

	int x;

	template <typename T>
	struct unary_assign_accumulate : unary_assign_add<acc_type_t<T>, T>
	{};

	template <template <typename> class InitAssigner>
	struct IniAssigner
	{
		template <typename R>
		void operator ()(R& v) const { InitAssigner<R> init; init(v); }
	};


	CommonOperGroup cogCumulate("cumulate");
	CumulOperators<unary_assign_accumulate, IniAssigner<assign_default>, typelists::ranged_unit_objects> s_CumulateOpers(&cogCumulate);
}
