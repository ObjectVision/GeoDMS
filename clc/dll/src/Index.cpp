// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// index operator: the ordering permutation (sort index) of an attribute,
// instantiated per value type.

#include "vt/StringBounds.h"
#include "vt/GeoSequence.h"
#include "mci/CompositeCast.h"
#include "ser/AsString.h"
#include "set/VectorFunc.h"

#include "CheckedDomain.h"
#include "DataArray.h"
#include "DataItemClass.h"
#include "OperSignature.h"
#include "Metric.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"
#include "UnitProcessor.h"
#include "TileChannel.h"

#include "OperRelUni.h"
#include "LispTreeType.h"
#include "mem/MyContainers.h"

// *****************************************************************************
//                         Helper Funcs
// *****************************************************************************

CommonOperGroup cog_dir_index(token::direct_index, oper_policy::dynamic_result_class);
CommonOperGroup cog_index    (token::index, oper_policy::dynamic_result_class);
CommonOperGroup cog_ordinal  (token::ordinal);
CommonOperGroup cog_subindex (token::subindex, oper_policy::dynamic_result_class);

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

	// mirrors CreateResult below: index(x: V[E]) -> R[E] -- the result ranges over
	// x's domain. Its VALUES unit is that same domain unit, but the claim stays a
	// SEPARATE variable R: the result item is not flagged categorical, so a
	// declared values unit is discharged by UnifyValues at reduction, where a
	// key-identity claim would over-reject (the batch-U S1 rule)
	bool DescribeSignature(AbstrSignatureBuilder& sb) const override
	{
		auto argCls = dynamic_cast<const DataItemClass*>(GetArgClass(0));
		if (!argCls)
			return false;
		sig_var E = sb.UnitVar("E"), V = sb.UnitVar("V"), R = sb.UnitVar("R");
		sb.MemberValueClass(V, argCls->GetValuesType());
		sb.ArgAttr(0, V, E, argCls->GetValuesType()->GetValueComposition());
		sb.ResultAttr(R, E, ValueComposition::Single);
		return true;
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

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
//	typedef DataArray<UInt32> ResultType;
	// Override Operator
	AbstrDirectIndexOperator(const Class* argClass)
		:	UnaryOperator(&cog_dir_index, AbstrDataItem::GetStaticClass(), argClass)
	{}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		assert(args.size() == 1);

		const AbstrDataItem* adi = AsDataItem(args[0]);

		const AbstrUnit*  e = adi->GetAbstrDomainUnit();

		const UnitClass* eCls = e->GetUnitClass();
		const UnitClass* vCls = Unit<UInt32>::GetStaticClass();
		if (eCls->GetValueType()->GetBitSize() > 32)
			vCls = Unit<UInt64>::GetStaticClass();

		const AbstrUnit*  v = ((eCls == vCls) ? e : vCls->CreateDefault());

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, v);

		if (mustCalc)
		{
			DataReadLock arg1Lock(adi);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res);

			Calculate(v, resLock.get(), adi);

			resLock.Commit();
		}
		return true;
	}
	virtual void Calculate(const AbstrUnit* resultValuesUnit, AbstrDataObject* result, const AbstrDataItem* adi) const =0;
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

	void Calculate(const AbstrUnit* resultValuesUnit, AbstrDataObject* result, const AbstrDataItem* adi) const override
	{

		const ArgType* di = const_array_cast<V>(adi);
		assert(di);

		// TODO: Optimize for sequence-arrays, similar to GetDataRead(no_tile);
		auto trd = di->GetTiledRangeData();
		// Full-domain buffers: route them through the allocation stocks so the census sees them.
		my_elem_vec_t<V> argData; argData.reserve(trd->GetElemCount());
		for (tile_id t = 0, te = trd->GetNrTiles(); t != te; ++t)
			for (const auto& v : di->GetTile(t))
					argData.emplace_back(v); // in-place: a sequence element (SA_ConstReference<char> for
					                         // string V) converts unambiguously only in direct-init; BitVector
					                         // grew a forwarding emplace_back for the bit branch

		visit<typelists::ulongs>(resultValuesUnit, [result, &argData]<typename I>(const Unit<I>* values)
		{
			auto resObj = mutable_array_cast<I>(result);
//			auto resData = resObj->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

			// TODO, OPTIMIZE: try to avoid zero-initialisation of resData
			my_vec_t<I> resData(resObj->GetTiledRangeData()->GetElemCount());
			make_index_in_existing_span(resData.begin(), resData.end(), argData.begin());
			
			tile_write_channel<I> resultWriter(resObj);
			resultWriter.Write(resData.begin(), resData.size());

		});

	}
};

// *****************************************************************************
//                         Ordinal
// *****************************************************************************

// TODO: AbstrOrdinalOperator

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
		assert(args.size() == 1);

		const AbstrDataItem* adi = AsDataItem(args[0]);
		assert(adi);

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

// TODO: class AbstrSubIndexOperator

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
		assert(args.size() == 3);

		const AbstrDataItem* adi1 = AsDataItem(args[0]);
		const AbstrDataItem* adi2 = AsDataItem(args[1]);
		const AbstrDataItem* adi3 = AsDataItem(args[2]);
		assert(adi1);
		assert(adi2);
		assert(adi3);

		const AbstrUnit*  e = adi1->GetAbstrDomainUnit();
		e->UnifyDomain(adi2->GetAbstrDomainUnit(), "e1", "e2", UM_Throw);
		e->UnifyDomain(adi3->GetAbstrDomainUnit(), "e1", "e3", UM_Throw);
		e->UnifyDomain(adi1->GetAbstrValuesUnit(), "e1", "v1", UM_Throw);

		if (!resultHolder)
			resultHolder = CreateCacheDataItem(e, e);

		if (mustCalc)
		{
			const Arg3Type* di = const_array_cast<V>(adi3);
			assert(di);

			DataReadLock arg1Lock(adi1);
			DataReadLock arg2Lock(adi2);
			DataReadLock arg3Lock(adi3);

			AbstrDataItem* res = AsDataItem(resultHolder.GetNew());
			DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero); 

			visit<typelists::domain_objects>(e, 
				[&resLock, adi1, adi2, di] <typename D> (const Unit<D>* du)
				{
					make_subindex_container(
						mutable_array_cast<D>(resLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero),
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
