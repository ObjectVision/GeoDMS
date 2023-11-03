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

#include "geo/RangeIndex.h"
#include "utl/mySPrintF.h"

#include "DataItemClass.h"
#include "UnitClass.h"

#include "pcount.h"

// *****************************************************************************
//                         IndexedSearchOperator
// *****************************************************************************

#include "Unit.h"
#include "UnitProcessor.h"
#include "RtcTypeLists.h"

// *****************************************************************************
// join_equal_values: (A->X, B->X): AB { ->X, ->A, ->B }
// *****************************************************************************

class AbstrJoinEqualValuesOperator : public BinaryOperator
{
public:
	AbstrJoinEqualValuesOperator(AbstrOperGroup& gr, const UnitClass* resultUnitClass, const DataItemClass* argClass)
		: BinaryOperator(&gr, resultUnitClass, argClass, argClass)
	{}

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, OperationContext*, LispPtr) const override
	{
		dms_assert(args.size() >= 2);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		dms_assert(arg1A);
		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit();
		dms_assert(arg1_DomainUnit);
		for (arg_index i = 1; i != args.size(); ++i)
		{
			MG_CHECK(AsDynamicDataItem(GetItem(args[i])));

			arg1A->GetAbstrValuesUnit()->UnifyValues(AsDataItem(args[i])->GetAbstrValuesUnit(), "v1", "Values of a following argument", UnifyMode(UM_Throw));
		}
		auto res  = static_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder);
		resultHolder = res;
		for (arg_index i = 0; i != args.size(); ++i)
		{
			TokenID refToken = GetTokenID_mt(myArrayPrintF<20>("nr_%d_rel", i+1));
			AbstrDataItem* resSub1 = CreateDataItem(res, refToken, res, AsDataItem(args[i])->GetAbstrDomainUnit());
		}
		AbstrDataItem* resSubX = CreateDataItem(res, GetTokenID_mt("nr_X_rel"), res, AsDataItem(args[0])->GetAbstrValuesUnit());
	}
};

template <typename ResultElement, typename ArgValuesElement>
struct JoinEqualValuesOperator : AbstrJoinEqualValuesOperator
{
	JoinEqualValuesOperator(AbstrOperGroup& gr)
		: AbstrJoinEqualValuesOperator(gr, Unit<ResultElement>::GetStaticClass(), DataArray<ArgValuesElement>::GetStaticClass())
	{}

	bool CalcResult(TreeItemDualRef& resultHolder, ArgRefs args, std::vector<ItemReadLock> readLocks, OperationContext*, Explain::Context* context = nullptr) const override
	{
		const AbstrDataItem* axRef = AsDataItem(args[0]);
		const AbstrDataItem* bxRef = AsDataItem(args[1]);
		DataReadLock axRefLock(axRef); auto axRefData = const_array_cast<ArgValuesElement>(axRef)->GetLockedDataRead();
		DataReadLock bxRefLock(bxRef); auto bxRefData = const_array_cast<ArgValuesElement>(bxRef)->GetLockedDataRead();

		const AbstrUnit* A = axRef->GetAbstrDomainUnit();
		const AbstrUnit* B = bxRef->GetAbstrDomainUnit();
		const Unit<ArgValuesElement>* X = debug_cast<const Unit<ArgValuesElement>*>(axRef->GetAbstrValuesUnit());
		MG_CHECK(A->IsOrdinalAndZeroBased());
		MG_CHECK(B->IsOrdinalAndZeroBased());
		MG_CHECK(X->IsOrdinalAndZeroBased());
		auto nr_A = A->GetCount();
		auto nr_B = B->GetCount();
		auto nr_X = X->GetCount();

		AbstrUnit* AB = AsUnit(resultHolder.GetNew());

		std::vector<ResultElement> aCounts(nr_X), aUsed(nr_X); 
		std::vector<ResultElement> bCounts(nr_X), bUsed(nr_X);
		std::vector<ResultElement> abCounts; abCounts.reserve(nr_X);
		std::vector<ResultElement> abOffsets; abOffsets.reserve(nr_X);
		auto xRange = X->GetRange();
		pcount_best(axRefData.begin(), axRefData.end(), begin_ptr(aCounts), nr_X, xRange, axRef->GetCheckMode(), false);
		pcount_best(bxRefData.begin(), bxRefData.end(), begin_ptr(bCounts), nr_X, xRange, bxRef->GetCheckMode(), false);
		ResultElement nr_AB = 0;
		for (auto aCountPtr = aCounts.begin(), bCountPtr = bCounts.begin(), aCountEnd = aCounts.end(); aCountPtr != aCountEnd; ++aCountPtr, ++bCountPtr)
		{
			ResultElement aCount = ThrowingConvertNonNull<ResultElement>(*aCountPtr);
			ResultElement bCount = ThrowingConvertNonNull<ResultElement>(*bCountPtr);
			ResultElement abCount = aCount * bCount;
			// SafeMul
			MG_USERCHECK2(!bCount || abCount / bCount == aCount,
				"join_equal_values operator: the product of the cardinalities of a common value exceeds the maximum value of the resulting unit");

			abCounts.emplace_back(abCount);
			abOffsets.emplace_back(nr_AB);
			ResultElement old_nr_AB = nr_AB;
			nr_AB += abCount;
			MG_USERCHECK2(nr_AB >= old_nr_AB,
				"join_equal_values operator: the cumulation of the cardinalities of common values exceeds the maximum value of the resulting unit");
		}
		AB->SetCount(nr_AB);

		AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("nr_1_rel"), AB, AsDataItem(args[0])->GetAbstrDomainUnit());
		AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("nr_2_rel"), AB, AsDataItem(args[1])->GetAbstrDomainUnit());
		AbstrDataItem* resSubX = CreateDataItem(AB, GetTokenID_mt("nr_X_rel"), AB, AsDataItem(args[0])->GetAbstrValuesUnit());

		DataWriteLock resSubALock(resSubA);

		visit<typelists::domain_elements>(A, 
			[&] <typename a_type> (const Unit<a_type>* unitA) 
			{
				auto aRange = unitA->GetRange();
				auto subAData = mutable_array_cast<a_type>(resSubALock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT ab_index = 0, aIndex = 0, aSize = axRefData.size(); aIndex != aSize; ++aIndex)
				{
					ArgValuesElement x = axRefData[aIndex];
					if (IsIncluding(xRange, x))
					{
						SizeT x_index = Range_GetIndex_naked(xRange, x);
						dms_assert(x_index < nr_X);
						SizeT b_count = bCounts[x_index];

						SizeT resIndex = abOffsets[x_index] + aUsed[x_index]++ * b_count;

						while (b_count)
						{
							dms_assert(aIndex < nr_A);
							subAData[resIndex + --b_count] = Range_GetValue_naked(aRange, aIndex);
						}
					}
				}
			}
		);
		resSubALock.Commit();

		DataWriteLock resSubBLock(resSubB);

		visit<typelists::domain_elements>(B, 
			[&] <typename b_type> (const Unit<b_type>* unitB) 
			{
				auto bRange = unitB->GetRange();
				auto subBData = mutable_array_cast<b_type>(resSubBLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
				for (SizeT ab_index = 0, bIndex = 0, bSize = bxRefData.size(); bIndex != bSize; ++bIndex)
				{
					ArgValuesElement x = bxRefData[bIndex];
					if (IsIncluding(xRange, x))
					{
						SizeT x_index = Range_GetIndex_naked(xRange, x);
						dms_assert(x_index < nr_X);
						SizeT a_count = aCounts[x_index];
						SizeT b_count = bCounts[x_index];

						SizeT resIndex = abOffsets[x_index]; resIndex += bUsed[x_index]++;

						while (a_count--)
						{
							dms_assert(bIndex < nr_B);
							subBData[resIndex] = Range_GetValue_naked(bRange, bIndex);
							resIndex += b_count;
						}
					}
				}
			}
		);
		resSubBLock.Commit();

		DataWriteLock resSubXLock(resSubX);
		auto dataSubX = mutable_array_cast<ArgValuesElement>(resSubXLock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);

		fast_zero(aUsed.begin(), aUsed.end());
		for (SizeT ab_index = 0, aIndex = 0, aSize = axRefData.size(); aIndex != aSize; ++aIndex)
		{
			ArgValuesElement x = axRefData[aIndex];
			if (IsIncluding(xRange, x))
			{
				SizeT x_index = Range_GetIndex_naked(xRange, x);
				SizeT b_count = bCounts[x_index];

				SizeT resIndex = abOffsets[x_index] + aUsed[x_index]++ * b_count;

				while (b_count)
					dataSubX[ resIndex + --b_count ] = x;
			}
		}
		resSubXLock.Commit();

		return true;
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

namespace 
{
	static const oper_policy op = oper_policy::none; // op_allow_extra_args;
	CommonOperGroup cog_jev("join_equal_values", op);
	CommonOperGroup cog_jev_u8("join_equal_values_uint8", op);
	CommonOperGroup cog_jev_u16("join_equal_values_uint16", op);
	CommonOperGroup cog_jev_u32("join_equal_values_uint32", op);
	CommonOperGroup cog_jev_u64("join_equal_values_uint64", op);

	using domains = typelists::domain_ints; // domain_elements;
	tl_oper::inst_tuple<domains, JoinEqualValuesOperator<UInt32, _>, AbstrOperGroup&> jevOpers    (cog_jev);
	tl_oper::inst_tuple<domains, JoinEqualValuesOperator<UInt8 , _>, AbstrOperGroup&> jevOpers_u8 (cog_jev_u8);
	tl_oper::inst_tuple<domains, JoinEqualValuesOperator<UInt16, _>, AbstrOperGroup&> jevOpers_u16(cog_jev_u16);
	tl_oper::inst_tuple<domains, JoinEqualValuesOperator<UInt32, _>, AbstrOperGroup&> jevOpers_u32(cog_jev_u32);
	tl_oper::inst_tuple<domains, JoinEqualValuesOperator<UInt64, _>, AbstrOperGroup&> jevOpers_u64(cog_jev_u64);
} // end anonymous namespace

/******************************************************************************/

