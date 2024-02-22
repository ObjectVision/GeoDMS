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

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mci/CompositeCast.h"
#include "geo/CheckedCalc.h"

#include "ParallelTiles.h"
#include "UnitClass.h"

#include "AttrBinStruct.h"
#include "Operator.h"
#include "OperUnit.h"
#include "UnitCreators.h"

// *****************************************************************************
//			VariadicAttrFunc operators
// *****************************************************************************

enum class MinMaxOperType {
	Minimum,
	Maximum
};

template <typename ArgValue>
auto // typename sequence_traits<ArgValue>::container_type
GetTileAsContainer(const AbstrDataItem* arg, tile_id t, SizeT n)
{
	auto tileData = const_array_cast<ArgValue>(arg)->GetLockedDataRead(arg->HasVoidDomainGuarantee() ? 0 : t);
	if (arg->HasVoidDomainGuarantee())
	{
		ArgValue v = tileData[0];
		return typename sequence_traits<ArgValue>::container_type( n, v );
	}
	return typename sequence_traits<ArgValue>::container_type{ tileData.begin(), tileData.end() };
}

enum class null_policy {
	simple,
	fast,
	certain,
	known
};

template <typename ResValue, typename ArgValue, null_policy NP, bool IsArgIndex>
void InitializeResTile(auto& valueSoFarContainer, auto& valueSoFar, typename sequence_traits<ResValue>::seq_t resTile, const AbstrDataItem* arg1, tile_id t, tile_id n)
{
	DataReadLock lock(arg1);
	if constexpr (IsArgIndex)
	{
		valueSoFarContainer = GetTileAsContainer<ArgValue>(arg1, t, n);
		valueSoFar = typename sequence_traits<ArgValue>::seq_t(begin_ptr(valueSoFarContainer), end_ptr(valueSoFarContainer));

		// The following code will undefine zero-initialized indices that would refer to null values
		if constexpr (NP == null_policy::known || NP == null_policy::certain)
		{
			for (auto i = valueSoFarContainer.begin(), b = i, e = valueSoFarContainer.end(); i != e; ++i)
				if (!IsDefined(*i))
					MakeUndefined(resTile[i - b]);
		}
	}
	else
	{
		if (arg1->HasVoidDomainGuarantee())
		{
			ArgValue argValue = const_array_cast<ArgValue>(arg1)->GetTile(0)[0];
				fast_fill(resTile.begin(), resTile.end(), argValue);
		}
		else
		{
			auto argValues = const_array_cast<ArgValue>(arg1)->GetTile(t);
			dms_assert(argValues.size() == resTile.size());
			fast_copy(argValues.begin(), argValues.end(), resTile.begin());
		}
		valueSoFar = resTile; // write directly to resTile;
	}
}

template <typename ResIndex, typename ArgValue, MinMaxOperType OperType, null_policy NP, bool IsArgIndex>
struct ArgMinMaxOper : UnaryOperator
{
	using ResValue = std::conditional_t<IsArgIndex, ResIndex, ArgValue>;

	ArgMinMaxOper(AbstrOperGroup& gr)
		: UnaryOperator(&gr, DataArray<ResValue>::GetStaticClass(), DataArray<ArgValue>::GetStaticClass())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() >= 1);

		if (!resultHolder)
		{
			const AbstrDataItem* arg1A = AsDataItem(args[0]);
			const AbstrUnit* e1 = arg1A->GetAbstrDomainUnit();
			const AbstrUnit* v1 = arg1A->GetAbstrValuesUnit();

			for (arg_index i = 1; i != args.size(); ++i)
			{
				if (!IsDataItem(args[i]))
					GetGroup()->throwOperErrorF("argument %d is not an attribute", i);

				auto argA = AsDataItem(args[i]);
				auto e = argA->GetAbstrDomainUnit();
				auto v = argA->GetAbstrValuesUnit();
				if (e1->GetValueType() == ValueWrap<Void>::GetStaticClass())
					e1 = e;
				e1->UnifyDomain(e, "e1", "Domain of a subsequent attribute", UnifyMode(UM_Throw | UM_AllowVoidRight));
				v1->UnifyValues(v, "v1", "Values of a subsequent attribute ", UnifyMode(UM_Throw | UM_AllowDefault));
				if (v1->IsDefaultUnit())
					v1 = v;
			}
// REMOVE	resultHolder = CreateCacheDataItem(e1, v1, ValueComposition::Single);
			if (IsArgIndex)
				v1 = Unit<ResValue>::GetStaticClass()->CreateDefault();
			resultHolder = CreateCacheDataItem(e1, v1, ValueComposition::Single);
		}
		if (mustCalc)
		{
			auto res = AsDataItem(resultHolder.GetNew());
			auto e1 = res->GetAbstrDomainUnit();

			DataWriteLock resLock(res, IsArgIndex ? dms_rw_mode::write_only_mustzero : dms_rw_mode::write_only_all);

			parallel_tileloop(e1->GetNrTiles(),
				[=, &resLock](tile_id t)
				{
					auto n = e1->GetTileCount(t);
					auto arg1 = AsDataItem(args[0]);

					auto valueSoFarContainer = sequence_traits<ArgValue>::container_type();
					auto valueSoFar = sequence_traits<ArgValue>::seq_t();

					auto resTile = mutable_array_cast<ResValue>(resLock)->GetWritableTile(t, IsArgIndex ? dms_rw_mode::write_only_mustzero : dms_rw_mode::write_only_all);
					InitializeResTile<ResValue, ArgValue, NP, IsArgIndex>(valueSoFarContainer, valueSoFar, resTile.get_view(), arg1, t, n);

					for (arg_index j = 1; j != args.size(); ++j)
					{
						auto argAttr = AsDataItem(args[j]);
						bool isParameter = argAttr->HasVoidDomainGuarantee();
						auto argData = const_array_cast<ArgValue>(argAttr);
						auto argTile = argData->GetTile(isParameter ? 0 : t);
						ArgValue argValue = ArgValue();
						if (isParameter)
							argValue = argTile[0];
						for (SizeT i = 0; i != n; ++i)
						{
							if (!isParameter)
								argValue = argTile[i];
							if constexpr (NP != null_policy::fast)
							{
								if (!IsDefined(argValue))
								{
									if constexpr (NP == null_policy::certain)
									{
										valueSoFar[i] = UNDEFINED_VALUE(ArgValue);
										if constexpr (IsArgIndex)
											resTile[i] = UNDEFINED_VALUE(ResIndex);
									}
									continue;
								}
							}
							bool argAndValueSoFarComparable = true;
							if constexpr (NP != null_policy::fast)
								if (!IsDefined(valueSoFar[i]))
								{
									if constexpr (NP == null_policy::certain)
										continue;
									argAndValueSoFarComparable = false;
								}
							if (argAndValueSoFarComparable)
								if constexpr (OperType == MinMaxOperType::Maximum)
								{
									if (argValue <= valueSoFar[i])
										continue;
								}
								else
								{
									if (argValue >= valueSoFar[i])
										continue;
								}
							valueSoFar[i] = argValue;
							if constexpr (IsArgIndex)
								resTile[i] = j;
						}
					}
				}
			);
			resLock.Commit();
		}
		return true;
	}
};

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

template <typename ResIndex, MinMaxOperType Oper, null_policy NP, bool IsArgIndex>
struct ArgMinMaxOperGenerator
{
	template <typename ArgValue> struct apply 
		: ArgMinMaxOper<ResIndex, ArgValue, Oper, is_bitvalue_v<ArgValue> ? null_policy::fast : NP, IsArgIndex >
	{
		using ArgMinMaxOper<ResIndex, ArgValue, Oper, is_bitvalue_v<ArgValue> ? null_policy::fast : NP, IsArgIndex >::ArgMinMaxOper;
	};
};

#include "RtcTypeLists.h"
using namespace typelists;

namespace {
	CommonOperGroup cog_argmin("argmin", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmax("argmax", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmin_fast("argmin_fast", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmax_fast("argmax_fast", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmin8("argmin_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmax8("argmax_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmin16("argmin_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmax16("argmax_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminC("argmin_alldefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxC("argmax_alldefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminK("argmin_ifdefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxK("argmax_ifdefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminC8("argmin_alldefined_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxC8("argmax_alldefined_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminK8("argmin_ifdefined_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxK8("argmax_ifdefined_uint8", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminC16("argmin_alldefined_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxC16("argmax_alldefined_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_argminK16("argmin_ifdefined_uint16", oper_policy::allow_extra_args);
	CommonOperGroup cog_argmaxK16("argmax_ifdefined_uint16", oper_policy::allow_extra_args);

	CommonOperGroup cog_minelem("min_elem", oper_policy::allow_extra_args);
	CommonOperGroup cog_maxelem("max_elem", oper_policy::allow_extra_args);
	CommonOperGroup cog_minelem_fast("min_elem_fast", oper_policy::allow_extra_args);
	CommonOperGroup cog_maxelem_fast("max_elem_fast", oper_policy::allow_extra_args);
	CommonOperGroup cog_minelemC("min_elem_alldefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_maxelemC("max_elem_alldefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_minelemK("min_elem_ifdefined", oper_policy::allow_extra_args);
	CommonOperGroup cog_maxelemK("max_elem_ifdefined", oper_policy::allow_extra_args);


	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Minimum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMinOpers   (cog_argmin);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Maximum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMaxOpers   (cog_argmax);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8 , MinMaxOperType::Minimum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMinOpersU8 (cog_argmin8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8 , MinMaxOperType::Maximum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMaxOpersU8 (cog_argmax8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Minimum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMinOpersU16(cog_argmin16);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Maximum, null_policy::simple, true>::template apply<_>, AbstrOperGroup& > argMaxOpersU16(cog_argmax16);

	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Minimum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMinCOpers(cog_argminC);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Maximum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMaxCOpers(cog_argmaxC);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8, MinMaxOperType::Minimum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMinCOpersU8(cog_argminC8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8, MinMaxOperType::Maximum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMaxCOpersU8(cog_argmaxC8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Minimum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMinCOpersU16(cog_argminC16);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Maximum, null_policy::certain, true>::template apply<_>, AbstrOperGroup& > argMaxCOpersU16(cog_argmaxC16);

	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Minimum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMinKOpers(cog_argminK);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Maximum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMaxKOpers(cog_argmaxK);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8, MinMaxOperType::Minimum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMinKOpersU8(cog_argminK8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt8, MinMaxOperType::Maximum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMaxKOpersU8(cog_argmaxK8);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Minimum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMinKOpersU16(cog_argminK16);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt16, MinMaxOperType::Maximum, null_policy::known, true>::template apply<_>, AbstrOperGroup& > argMaxKOpersU16(cog_argmaxK16);

	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Minimum, null_policy::fast,  true>::template apply<_>, AbstrOperGroup& > argMinOpers_fast(cog_argmin_fast);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<UInt32, MinMaxOperType::Maximum, null_policy::fast,  true>::template apply<_>, AbstrOperGroup& > argMaxOpers_fast(cog_argmax_fast);

	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Minimum, null_policy::simple, false>::template apply<_>, AbstrOperGroup& > minElemOpers(cog_minelem);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Maximum, null_policy::simple, false>::template apply<_>, AbstrOperGroup& > maxElemOpers(cog_maxelem);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Minimum, null_policy::fast,  false>::template apply<_>, AbstrOperGroup& > minElemFastOpers_fast(cog_minelem_fast);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Maximum, null_policy::fast,  false>::template apply<_>, AbstrOperGroup& > maxElemFastOpers_fast(cog_maxelem_fast);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Minimum, null_policy::certain, false>::template apply<_>, AbstrOperGroup& > minElemCOpers(cog_minelemC);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Maximum, null_policy::certain, false>::template apply<_>, AbstrOperGroup& > maxElemCOpers(cog_maxelemC);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Minimum, null_policy::known, false>::template apply<_>, AbstrOperGroup& > minElemKOpers(cog_minelemK);
	tl_oper::inst_tuple<typelists::numerics, ArgMinMaxOperGenerator<void, MinMaxOperType::Maximum, null_policy::known, false>::template apply<_>, AbstrOperGroup& > maxElemKOpers(cog_maxelemK);
} // namespace
