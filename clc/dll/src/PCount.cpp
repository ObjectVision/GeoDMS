// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "PCount.h"

#include "UnitProcessor.h"

#include "utl/TypeListOper.h"

#include "ParallelTiles.h"
#include "TreeItemClass.h"

#include "UnitCreators.h"

#include "AggrFuncNum.h"
//#include "AttrBinStruct.h"

// *****************************************************************************
//                            PartCountOperator
//	pcount: (D->E) -> E->#D
// *****************************************************************************

namespace {

	CommonOperGroup cog_pcount ("pcount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_has_any("has_any");
	CommonOperGroup cog_pcount_uint8("pcount_uint8");
	CommonOperGroup cog_pcount_uint16("pcount_uint16");
	CommonOperGroup cog_pcount_uint32("pcount_uint32");
	CommonOperGroup cog_pcount_uint64("pcount_uint64");

	template <typename EnumType, typename ResultCountType = Undefined>
	class PartCountOperator : public UnaryOperator
	{
 		using enum_t = EnumType;
		using range_t = typename Unit<enum_t>::range_t;
		using Arg1Type = DataArray<enum_t>;   // indices vector
		using arg_cseq_t = typename sequence_traits<enum_t>::cseq_t;
		using ResultType = std::conditional_t<std::is_same_v<ResultCountType, Undefined>, AbstrDataItem, DataArray<ResultCountType>>;
		
	public:
		PartCountOperator(AbstrOperGroup& og)
			:	UnaryOperator(&og, 
					ResultType::GetStaticClass(), 
					Arg1Type::GetStaticClass()
				) 
		{}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			dms_assert(args.size() == 1);

			const AbstrDataItem* arg1A = AsDataItem(args[0]);
			dms_assert(arg1A);

			const Unit<enum_t>* e1 = debug_cast<const Unit<enum_t>*>(arg1A->GetAbstrValuesUnit()); // LANDUSE CLASSES
			dms_assert(e1);

			if (!resultHolder)
			{
				SharedPtr<const AbstrUnit> resValuesUnit;
				if constexpr (std::is_same_v < ResultCountType, Undefined>)
					resValuesUnit = count_unit_creator(args);
				else
					resValuesUnit = default_unit_creator< ResultCountType>();
				resultHolder = CreateCacheDataItem(e1, resValuesUnit);
			}

			if (mustCalc)
			{
				AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

				const Arg1Type* arg1 = const_array_cast<enum_t>(arg1A);
				dms_assert(arg1);

				DataReadLock  arg1Lock(arg1A);
				DataWriteLock resLock (res, dms_rw_mode::write_only_mustzero);

				tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
				tile_id tnr = e1->GetNrTiles();
				DataCheckMode dcm = ((tnr>1) ? DCM_CheckRange : arg1A->GetCheckMode());

				SizeT nrRes = e1->GetCount();
				if (tn > 1 && arg1A->GetAbstrDomainUnit()->GetCount() > 8 * nrRes)
				{
					Concurrency::combinable<std::vector<SizeT>> counts;
					parallel_tileloop(arg1A->GetAbstrDomainUnit()->GetNrTiles(), [nrRes, arg1, indexRange = e1->GetRange(), dcm, &counts](tile_id t)
						{
							auto& localCounts = counts.local();
							localCounts.resize(nrRes);
							auto arg1Data = arg1->GetTile(t);
							pcount_container<enum_t, SizeT>(IterRange<SizeT*>(&localCounts), arg1Data, indexRange, dcm, false);
						}
					);
					visit<typelists::uints>(res->GetAbstrValuesUnit(), [resObj = resLock.get(), &counts] <typename E> (const Unit<E>*)
						{
							auto resData = mutable_array_cast<E>(resObj)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
							counts.combine_each(
								[&resData](auto& localCounts) {
									assert(resData.size() == localCounts.size());
									for (SizeT i = 0, n = resData.size(); i != n; ++i)
										SafeAccumulate(resData[i], localCounts[i]);
								});
						}
					);
				}
				else
				{
					visit<typelists::uints>(res->GetAbstrValuesUnit()
						, [resObj = resLock.get(), arg1, indexRange = e1->GetRange(), dcm, tn]<typename E>(const Unit<E>*) {
							auto resData = mutable_array_cast<E>(resObj)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
							for (tile_id t = 0; t != tn; ++t)
								pcount_container<enum_t, E>( resData.get_view(), arg1->GetTile(t).get_view(), indexRange, dcm, false);
						}
					);
				}
				resLock.Commit();
			}
			return true;
		}
	};

	template <typename ResultCountType>
	struct PartCountOperatorGenerator {
		template <typename EnumType> struct Operator : PartCountOperator<EnumType, ResultCountType> {
			using base_type = PartCountOperator<EnumType, ResultCountType>;
			using base_type::base_type;
		};
	};

	template <typename EnumType>
	class HasAnyOperator : public UnaryOperator
	{
		using enum_t = EnumType;
		using range_t = typename Unit<enum_t>::range_t;
		using Arg1Type = DataArray<enum_t>;   // indices vector
		using arg_cseq_t = typename sequence_traits<enum_t>::cseq_t;
		using ResultType = DataArray<Bool>;

	public:
		HasAnyOperator(AbstrOperGroup& og)
			: UnaryOperator(&og,
				ResultType::GetStaticClass(),
				Arg1Type::GetStaticClass()
			)
		{}

		bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
		{
			assert(args.size() == 1);

			const AbstrDataItem* arg1A = AsDataItem(args[0]);
			assert(arg1A);

			const Unit<enum_t>* e1 = debug_cast<const Unit<enum_t>*>(arg1A->GetAbstrValuesUnit()); // LANDUSE CLASSES
			assert(e1);

			if (!resultHolder)
			{
				SharedPtr<const AbstrUnit> resValuesUnit;
				resValuesUnit = default_unit_creator<Bool>();
				resultHolder = CreateCacheDataItem(e1, resValuesUnit);
			}

			if (mustCalc)
			{
				AbstrDataItem* res = AsDataItem(resultHolder.GetNew());

				const Arg1Type* arg1 = const_array_cast<enum_t>(arg1A);
				assert(arg1);

				DataReadLock  arg1Lock(arg1A);
				DataWriteLock resLock(res, dms_rw_mode::write_only_mustzero);

				tile_id tn = arg1A->GetAbstrDomainUnit()->GetNrTiles();
				tile_id tnr = e1->GetNrTiles();
				DataCheckMode dcm = ((tnr > 1) ? DCM_CheckRange : arg1A->GetCheckMode());

				SizeT nrRes = e1->GetCount();
				auto resObj = resLock.get();
				auto resData = mutable_array_cast<Bool>(resObj)->GetDataWrite(no_tile, dms_rw_mode::write_only_mustzero);
				if (tn > 1 && arg1A->GetAbstrDomainUnit()->GetCount() > 8 * nrRes)
				{
					Concurrency::combinable<sequence_traits<Bool>::container_type> hasAnies;
					parallel_tileloop(arg1A->GetAbstrDomainUnit()->GetNrTiles(), [nrRes, arg1, indexRange = e1->GetRange(), dcm, &hasAnies](tile_id t)
						{
							auto& localHasAnies = hasAnies.local();
							localHasAnies.resize(nrRes);
							auto arg1Data = arg1->GetTile(t);
							has_any_container<enum_t>(sequence_traits<Bool>::seq_t(&localHasAnies), arg1Data, indexRange, dcm);
						}
					);
					hasAnies.combine_each(
						[&resData](auto& localHasAnies) {
							assert(resData.size() == localHasAnies.size());
							for (SizeT i = 0, n = resData.size(); i != n; ++i)
								if (localHasAnies[i].value())
									resData[i] = true;
						});
				}
				else
				{
					for (tile_id t = 0; t != tn; ++t)
						has_any_container<enum_t>(resData.get_view(), arg1->GetTile(t).get_view(), e1->GetRange(), dcm);
				}
				resLock.Commit();
			}
			return true;
		}
	};

	struct HasAnyOperatorGenerator {
		template <typename EnumType> struct Operator : HasAnyOperator<EnumType> {
			using base_type = HasAnyOperator<EnumType>;
			using base_type::base_type;
		};
	};

	tl_oper::inst_tuple_templ<typelists::partition_elements, PartCountOperatorGenerator<Undefined>::Operator, AbstrOperGroup& > partCountOpers(cog_pcount);
	tl_oper::inst_tuple_templ<typelists::partition_elements, PartCountOperatorGenerator<UInt8    >::Operator, AbstrOperGroup& > partCountOpers8(cog_pcount_uint8);
	tl_oper::inst_tuple_templ<typelists::partition_elements, PartCountOperatorGenerator<UInt16   >::Operator, AbstrOperGroup& > partCountOpers16(cog_pcount_uint16);
	tl_oper::inst_tuple_templ<typelists::partition_elements, PartCountOperatorGenerator<UInt32   >::Operator, AbstrOperGroup& > partCountOpers32(cog_pcount_uint32);
	tl_oper::inst_tuple_templ<typelists::partition_elements, PartCountOperatorGenerator<UInt64   >::Operator, AbstrOperGroup& > partCountOpers64(cog_pcount_uint64);

	tl_oper::inst_tuple<typelists::partition_elements, HasAnyOperatorGenerator::Operator<_>, AbstrOperGroup& > hasAnyOpers(cog_has_any);
} // end anonymous namespace

