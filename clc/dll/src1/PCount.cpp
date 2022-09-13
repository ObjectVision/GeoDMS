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

#include "ClcPCH.h"
#pragma hdrstop

#include "PCount.h"

#include "UnitProcessor.h"

#include "utl/TypeListOper.h"

#include "ParallelTiles.h"
#include "TreeItemClass.h"
#include "UnitCreators.h"

// *****************************************************************************
//                            PartCountOperator
//	pcount: (D->E) -> E->#D
// *****************************************************************************

namespace {

	CommonOperGroup cog_pcount ("pcount", oper_policy::dynamic_result_class);
	CommonOperGroup cog_pcount_uint8("pcount_uint8");
	CommonOperGroup cog_pcount_uint16("pcount_uint16");
	CommonOperGroup cog_pcount_uint32("pcount_uint32");

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
									dms_assert(resData.size() == localCounts.size());
									for (SizeT i = 0, n = resData.size(); i != n; ++i)
										resData[i] += localCounts[i];
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

	tl_oper::inst_tuple<typelists::partition_elements, PartCountOperatorGenerator<Undefined>::Operator<_>, AbstrOperGroup& > partCountOpers(cog_pcount);
	tl_oper::inst_tuple<typelists::partition_elements, PartCountOperatorGenerator<UInt8    >::Operator<_>, AbstrOperGroup& > partCountOpers8(cog_pcount_uint8);
	tl_oper::inst_tuple<typelists::partition_elements, PartCountOperatorGenerator<UInt16   >::Operator<_>, AbstrOperGroup& > partCountOpers16(cog_pcount_uint16);
	tl_oper::inst_tuple<typelists::partition_elements, PartCountOperatorGenerator<UInt32   >::Operator<_>, AbstrOperGroup& > partCountOpers32(cog_pcount_uint32);
} // end anonymous namespace

