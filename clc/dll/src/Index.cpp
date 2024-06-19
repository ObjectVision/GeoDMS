// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "geo/StringBounds.h"
#include "geo/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "set/VectorFunc.h"

#include "DataArray.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "Metric.h"
#include "CheckedDomain.h"

#include "OperRelUni.h"
#include "UnitProcessor.h"
#include "LispTreeType.h"

// *****************************************************************************
//                         Helper Funcs
// *****************************************************************************

CommonOperGroup cog_dir_index("direct_index");
CommonOperGroup cog_index    ("index", oper_policy::dynamic_result_class);
CommonOperGroup cog_ordinal  (token::ordinal);
CommonOperGroup cog_subindex ("subindex", oper_policy::dynamic_result_class);

// *****************************************************************************
//                         Index
// *****************************************************************************

class AbstrIndexOperator : public UnaryOperator
{
public:
	// Override Operator
	AbstrIndexOperator(const Class* argClass)
		:	UnaryOperator(&cog_index, AbstrDataItem::GetStaticClass(), argClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = AsDataItem(args[0]);

		const AbstrUnit*  e = adi->GetAbstrDomainUnit();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, e);

		if (mustCalc)
		{
			DataReadLock arg1Lock(adi);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);
			Calculate(resLock, adi);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(DataWriteLock& res, const AbstrDataItem* adi) const =0;
};

template <class V>
struct IndexOperator : public AbstrIndexOperator
{
	IndexOperator()
		:	AbstrIndexOperator(DataArray<V>::GetStaticClass())
	{}

	void Calculate(DataWriteLock& res, const AbstrDataItem* adi) const override
	{
		visit<typelists::domain_objects>(adi->GetAbstrDomainUnit(), 
			[&res, adi] <typename D> (const Unit<D>*du) 
			{
				make_index_container(
					mutable_array_cast<D>(res)->GetDataWrite(no_tile, dms_rw_mode::write_only_all),
					const_array_cast<V>(adi)->GetDataRead(),
					du->GetRange(),
					TYPEID(elem_traits<D>)
				);
			}
		);
	}
};

// *****************************************************************************
//                         DirectIndex
// *****************************************************************************

class AbstrDirectIndexOperator : public UnaryOperator
{
public:
	typedef DataArray<UInt32> ResultType;
	// Override Operator
	AbstrDirectIndexOperator(const Class* argClass)
		:	UnaryOperator(&cog_dir_index, ResultType::GetStaticClass(), argClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = AsDataItem(args[0]);

		const AbstrUnit*  e = adi->GetAbstrDomainUnit();

		const UnitClass* eCls = e->GetUnitClass();
		const UnitClass* vCls = Unit<UInt32>::GetStaticClass();

		const AbstrUnit*  v = ((eCls == vCls) ? e : vCls->CreateDefault());

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, v);

		if (mustCalc)
		{
			DataReadLock arg1Lock(adi);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(mutable_array_cast<UInt32>(resLock), adi);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(ResultType* result, const AbstrDataItem* adi) const =0;
};

template <class V>
class DirectIndexOperator : public AbstrDirectIndexOperator
{
	typedef DataArray<V> ArgType;

public:
	// Override Operator
	DirectIndexOperator()
		:	AbstrDirectIndexOperator(ArgType::GetStaticClass())
	{}

	void Calculate(ResultType* result, const AbstrDataItem* adi) const override
	{
		const ArgType* di = const_array_cast<V>(adi);
		dms_assert(di);

		auto resData = result->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		make_index_in_existing_span(resData.begin(), resData.end(), di->GetDataRead().begin());
	}
};

// *****************************************************************************
//                         Ordinal
// *****************************************************************************

// REMOVE, TODO: AbstrOrdinalOperator

template <class V>
class OrdinalOperator : public UnaryOperator
{
	typedef DataArray<V> ArgType;
	typedef typename cardinality_type<V>::type result_type;
public:
	// Override Operator
	OrdinalOperator()
		:	UnaryOperator(&cog_ordinal,
				DataArray<UInt32>::GetStaticClass(), 
				ArgType::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 1);

		const AbstrDataItem* adi = AsDataItem(args[0]);
		dms_assert(adi);

		const AbstrUnit*  e = adi->GetAbstrDomainUnit(); 

		const AbstrUnit*  vRes = Unit<UInt32>::GetStaticClass()->CreateDefault();

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, vRes);

		if (!mustCalc)
			return true;

		const Unit<V>* v = const_unit_cast<V>( adi->GetAbstrValuesUnit() );
		typename Unit<V>::range_t vRange = v->GetRange();

		AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

		DataReadLock arg1Lock(adi);
		DataWriteLock resLock(res, dms_rw_mode::write_only_all);
	
		for (tile_id t=0, nt = e->GetNrTiles(); t!=nt; ++t)
		{
			auto argData = const_array_cast<V>(adi)->GetTile(t);
			auto resData = mutable_array_cast<result_type>(resLock)->GetWritableTile(t);

			auto ri = resData.begin();
			Range_Value2Index_checked(vRange, argData.begin(), argData.end(), resData.begin());
		}

		resLock.Commit();
		return true;
	}
};

// *****************************************************************************
//                         SubIndex
//  subindex(E->E', E'->P(UInt32), E->V) -> (E->E'')
// *****************************************************************************

// REMOVE, TODO: class AbstrSubIndexOperator

template <class V>
class SubIndexOperator : public TernaryOperator
{
	typedef AbstrDataItem     Arg1Type;
	typedef DataArray<UInt32> Arg2Type;
	typedef DataArray<V>      Arg3Type;


public:
	SubIndexOperator()
		:	TernaryOperator(&cog_subindex,
				AbstrDataItem::GetStaticClass(), 
				Arg1Type::GetStaticClass(), 
				Arg2Type::GetStaticClass(), 
				Arg3Type::GetStaticClass()
			)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 3);

		const AbstrDataItem* adi1 = AsDataItem(args[0]);
		const AbstrDataItem* adi2 = AsDataItem(args[1]);
		const AbstrDataItem* adi3 = AsDataItem(args[2]);
		dms_assert(adi1);
		dms_assert(adi2);
		dms_assert(adi3);

		const AbstrUnit*  e = adi1->GetAbstrDomainUnit();
		e->UnifyDomain(adi2->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		e->UnifyDomain(adi3->GetAbstrDomainUnit(), "e1", "e3", UM_Throw);
		e->UnifyDomain(adi1->GetAbstrValuesUnit(), "e1", "v1", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, e);

		if (mustCalc)
		{
			const Arg3Type* di = const_array_cast<V>(adi3);
			dms_assert(di);

			DataReadLock arg1Lock(adi1);
			DataReadLock arg2Lock(adi2);
			DataReadLock arg3Lock(adi3);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero); 

			visit<typelists::domain_objects>(e, 
				[&resLock, adi1, adi2, di] <typename D> (const Unit<D>* du)
				{
					make_subindex_container(
						mutable_array_cast<D>(resLock)->GetDataWrite(),
						const_array_cast<D>(adi1)->GetDataRead(),
						const_array_cast<UInt32>(adi2)->GetDataRead(),
						di->GetDataRead(),
						du->GetRange(),
						TYPEID(elem_traits<D>)
					);
				}
			);
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

namespace 
{
	template <typename X>
	struct Operators {
	   IndexOperator<X>       m_Index;
	   DirectIndexOperator<X> m_DirIndex;
	   SubIndexOperator<X>    m_SubIndex;
	};

	tl_oper::inst_tuple_templ<typelists::fields, Operators > indexOperators;
	tl_oper::inst_tuple_templ<typelists::domain_elements, OrdinalOperator > ordinalOperators;
} // end anonymous namespace
