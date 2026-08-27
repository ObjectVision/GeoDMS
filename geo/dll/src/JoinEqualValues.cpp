// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "GeoPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// join_equal_values operator: relational join of two attributes on equal
// values.

#include "vt/RangeIndex.h"
#include "utl/StrFormat.h"

#include "DataItemClass.h"
#include "UnitClass.h"

#include "PCount.h"
#include "mem/MyContainers.h"

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

	void CreateResultCaller(TreeItemDualRef& resultHolder, const ArgRefs& args, LispPtr) const override
	{
		assert(args.size() == 2);

		if (resultHolder)
			return;

		const AbstrDataItem* arg1A = AsDataItem(args[0]);
		assert(arg1A);
		const AbstrUnit* arg1_DomainUnit = arg1A->GetAbstrDomainUnit();
		assert(arg1_DomainUnit);

		MG_CHECK(AsDynamicDataItem(GetItem(args[1])));

		arg1A->GetAbstrValuesUnit()->UnifyValues(AsDataItem(args[1])->GetAbstrValuesUnit(), "v1", "v2", UnifyMode(UM_Throw));

		auto AB = static_cast<const UnitClass*>(GetResultClass())->CreateResultUnit(resultHolder.GetNew());

		AbstrDataItem* resSubA = CreateDataItem(AB.get(), GetTokenID_mt("first_rel"), AB.get(), AsDataItem(args[0])->GetAbstrDomainUnit()).get(); // owned by AB
		AbstrDataItem* resSubB = CreateDataItem(AB.get(), GetTokenID_mt("second_rel"), AB.get(), AsDataItem(args[1])->GetAbstrDomainUnit()).get(); // owned by AB
		AbstrDataItem* resSubX = CreateDataItem(AB.get(), GetTokenID_mt("X_rel"), AB.get(), AsDataItem(args[0])->GetAbstrValuesUnit()).get(); // owned by AB
		resultHolder = AB;
	}
};

template <typename ResultElement, typename ArgValuesElement>
struct JoinEqualValuesOperator : AbstrJoinEqualValuesOperator
{
	JoinEqualValuesOperator(AbstrOperGroup& gr)
		: AbstrJoinEqualValuesOperator(gr, Unit<ResultElement>::GetStaticClass(), DataArray<ArgValuesElement>::GetStaticClass())
	{}

	// The join builds six arrays with one slot per candidate common value. Which values are
	// candidates is decided by an x-index; the two implementations below differ only in that.
	//
	// dense_x_index: one slot per value in the range of the values unit X, addressed directly.
	// No lookup cost and no index to build, but it costs 6 * #X * sizeof(ResultElement) bytes
	// and a sweep over all of #X, however few values actually occur.
	struct dense_x_index
	{
		typename Unit<ArgValuesElement>::range_t m_XRange;
		SizeT                                    m_NrSlots;

		SizeT size() const { return m_NrSlots; }

		bool find(ArgValuesElement x, SizeT& slot) const
		{
			if (!IsIncluding(m_XRange, x))
				return false;
			slot = Range_GetIndex_naked(m_XRange, x);
			assert(slot < m_NrSlots);
			return true;
		}

		template <typename Iter, typename CheckModeT>
		void count(Iter first, Iter last, ResultElement* counts, CheckModeT cm) const
		{
			pcount_best(first, last, counts, m_NrSlots, m_XRange, cm, false);
		}
	};

	// sparse_x_index: one slot per distinct value that actually occurs in the first argument.
	// A value that occurs only in the second argument has aCount == 0, thus abCount == 0, and
	// can never produce a result row, so #slots <= #A regardless of the range of X. The keys
	// are kept sorted, so both indexes number the result rows in the same ascending-value
	// order and produce identical results; only the resource use differs.
	struct sparse_x_index
	{
		my_vec_t<ArgValuesElement> m_Keys; // sorted, unique, all within m_XRange

		SizeT size() const { return m_Keys.size(); }

		bool find(ArgValuesElement x, SizeT& slot) const
		{
			auto i = std::lower_bound(m_Keys.begin(), m_Keys.end(), x);
			if (i == m_Keys.end() || !(*i == x))
				return false;
			slot = i - m_Keys.begin();
			return true;
		}

		template <typename Iter, typename CheckModeT>
		void count(Iter first, Iter last, ResultElement* counts, CheckModeT) const
		{
			for (; first != last; ++first)
			{
				SizeT slot;
				if (find(*first, slot))
					++counts[slot];
			}
		}
	};

	// bit-valued keys (Bool, UInt2, UInt4) have at most 16 values and are always ordinal and
	// zero-based, so they always take the dense path; my_vec_t cannot hold them anyway.
	static constexpr bool has_sparse_path = !is_bitvalue_v<ArgValuesElement>;

	bool CalcResult(TreeItemDualRef& resultHolder, const ArgRefs& args, std::vector<ItemReadLock> readLocks, Explain::Context* context = nullptr) const override
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
		auto nr_A = A->GetCount();
		auto nr_B = B->GetCount();
		auto xRange = X->GetRange();

		auto calculate = [&] <typename XIndex> (const XIndex& xIndex) -> bool
		{
			SizeT nr_slots = xIndex.size();

			AbstrUnit* AB = AsUnit(resultHolder.GetNew());

			my_vec_t<ResultElement> aCounts(nr_slots), aUsed(nr_slots);
			my_vec_t<ResultElement> bCounts(nr_slots), bUsed(nr_slots);
			my_vec_t<ResultElement> abCounts; abCounts.reserve(nr_slots);
			my_vec_t<ResultElement> abOffsets; abOffsets.reserve(nr_slots);
			xIndex.count(axRefData.begin(), axRefData.end(), begin_ptr(aCounts), axRef->GetCheckMode());
			xIndex.count(bxRefData.begin(), bxRefData.end(), begin_ptr(bCounts), bxRef->GetCheckMode());
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

			AbstrDataItem* resSubA = CreateDataItem(AB, GetTokenID_mt("first_rel"), AB, AsDataItem(args[0])->GetAbstrDomainUnit()).get(); // owned by AB
			AbstrDataItem* resSubB = CreateDataItem(AB, GetTokenID_mt("second_rel"), AB, AsDataItem(args[1])->GetAbstrDomainUnit()).get(); // owned by AB
			AbstrDataItem* resSubX = CreateDataItem(AB, GetTokenID_mt("X_rel"), AB, AsDataItem(args[0])->GetAbstrValuesUnit()).get(); // owned by AB

			DataWriteLock resSubALock(resSubA);

			visit<typelists::domain_elements>(A,
				[&] <typename a_type> (const Unit<a_type>* unitA)
				{
					auto aRange = unitA->GetRange();
					auto subAData = mutable_array_cast<a_type>(resSubALock)->GetDataWrite(no_tile, dms_rw_mode::write_only_all);
					for (SizeT ab_index = 0, aIndex = 0, aSize = axRefData.size(); aIndex != aSize; ++aIndex)
					{
						ArgValuesElement x = axRefData[aIndex];
						SizeT x_index;
						if (xIndex.find(x, x_index))
						{
							dms_assert(x_index < nr_slots);
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
						SizeT x_index;
						if (xIndex.find(x, x_index))
						{
							dms_assert(x_index < nr_slots);
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
				SizeT x_index;
				if (xIndex.find(x, x_index))
				{
					SizeT b_count = bCounts[x_index];

					SizeT resIndex = abOffsets[x_index] + aUsed[x_index]++ * b_count;

					while (b_count)
						dataSubX[ resIndex + --b_count ] = x;
				}
			}
			resSubXLock.Commit();

			return true;
		};

		// A dense slot array only pays off when the range of X is not much larger than the data.
		// A join key that is typed by its value type instead of by a domain unit (a plain uint32
		// attribute, say) has #X == 2^32-2, which is 60 GB of slots and a two-minute sweep for
		// three rows joined on three rows; and a values unit that is not ordinal and zero-based
		// cannot be addressed directly at all. Both cases take the index over the values that
		// actually occur. See https://github.com/ObjectVision/GeoDMS/issues/1175
		if constexpr (has_sparse_path)
		{
			bool useDenseIndex = X->IsOrdinalAndZeroBased() && X->GetCount() <= SizeT(nr_A) + SizeT(nr_B);
			if (!useDenseIndex)
			{
				sparse_x_index xIndex;
				xIndex.m_Keys.reserve(axRefData.size());
				for (auto xPtr = axRefData.begin(), xEnd = axRefData.end(); xPtr != xEnd; ++xPtr)
					if (IsIncluding(xRange, *xPtr)) // excludes undefined values, as the dense path does
						xIndex.m_Keys.emplace_back(*xPtr);
				std::sort(xIndex.m_Keys.begin(), xIndex.m_Keys.end());
				xIndex.m_Keys.erase(std::unique(xIndex.m_Keys.begin(), xIndex.m_Keys.end()), xIndex.m_Keys.end());

				return calculate(xIndex);
			}
		}
		else
			MG_CHECK(X->IsOrdinalAndZeroBased());

		return calculate(dense_x_index{ xRange, X->GetCount() });
	}
};


// *****************************************************************************
//                               INSTANTIATION
// *****************************************************************************

#include "utl/TypeListOper.h"

namespace
{
	static const oper_policy op = oper_policy::better_not_in_meta_scripting; // op_allow_extra_args;
	CommonOperGroup cog_jev("join_equal_values", op);
	CommonOperGroup cog_jev_u8("join_equal_values_uint8", op);
	CommonOperGroup cog_jev_u16("join_equal_values_uint16", op);
	CommonOperGroup cog_jev_u32("join_equal_values_uint32", op);
	CommonOperGroup cog_jev_u64("join_equal_values_uint64", op);

	using domains = typelists::domain_ints; // domain_elements;
	tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinEqualValuesOperator, UInt32, ph::_1>> jevOpers    (cog_jev);
	tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinEqualValuesOperator, UInt8 , ph::_1>> jevOpers_u8 (cog_jev_u8);
	tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinEqualValuesOperator, UInt16, ph::_1>> jevOpers_u16(cog_jev_u16);
	tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinEqualValuesOperator, UInt32, ph::_1>> jevOpers_u32(cog_jev_u32);
	tl_oper::inst_tuple<domains, tl::bind_placeholders<JoinEqualValuesOperator, UInt64, ph::_1>> jevOpers_u64(cog_jev_u64);
} // end anonymous namespace
