// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif //defined(_MSC_VER)

#include "set/IndexedStrings.h"
#include "utl/Environment.h"
#include "LockLevels.h"

/****************** IndexedStrings *******************/

template<bool MustZeroTerminate>
CharPtrRange StringIndexer::GetPtrs(index_type x) const noexcept
{
	assert(IsDefined(x));

	StringCRef ref = r_Container[x];
	assert(!MustZeroTerminate || ref.size()); // even empty string has a nonzero size because of the null terminator
	CharPtrRange ix = CharPtrRange(ref.begin(), MustZeroTerminate ? &ref.back() : ref.end()); // exclude null terminator in compare
	assert(!MustZeroTerminate || !*ix.end());  // check that it is a null terminator that is excluded 
	return ix;
}

//  -----------------------------------------------------------------------

static UInt32 scc_GetOrCreateID = 0;
Byte cs_GetOrCreateID[sizeof(IndexedString_critical_section)];

IndexedString_critical_section& GetCS()
{
	dms_assert(scc_GetOrCreateID);
	return *reinterpret_cast<IndexedString_critical_section*>(cs_GetOrCreateID);
}

IndexedStringsComponent::IndexedStringsComponent()
{
	if (!scc_GetOrCreateID++)
		new (cs_GetOrCreateID) IndexedString_critical_section(item_level_type(0), ord_level_type::IndexedString, "IndexedStringsComponent");
}

IndexedStringsComponent::~IndexedStringsComponent()
{
	if (!--scc_GetOrCreateID)
		reinterpret_cast<IndexedString_critical_section*>(cs_GetOrCreateID)->~IndexedString_critical_section();
}

//  -----------------------------------------------------------------------

IndexedStringsBase::IndexedStringsBase()
{}

IndexedStringsBase::~IndexedStringsBase()
{}

void IndexedStringsBase::reserve(index_type sz MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	m_Vec.reserve(sz MG_DEBUG_ALLOCATOR_SRC_PARAM);
	m_Vec.reserve_data(SizeT(sz)*4 MG_DEBUG_ALLOCATOR_SRC_PARAM); // estimated lowerbound of total stringlengths
}

//  -----------------------------------------------------------------------

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::IndexedStrings()
	:	m_Idx(4096, hasher(m_Vec), equality_compare(m_Vec))
{}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type 
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetOrCreateID_mt(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	IndexedString_scoped_lock lock(GetCS());

	return GetOrCreateID_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetOrCreateID_st(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	return GetOrCreateID_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetOrCreateID_impl(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	CharPtrRange keyValue(keyFirst, keyLast);
	index_iterator i = m_Idx.find(keyValue);
	if (i != m_Idx.end() && m_Idx.key_eq()(keyValue, *i))
	{
		// warn for mixing up upper and lower case writngs of whatever
		index_type foundIndex = *i;
		if constexpr (std::is_same_v<CharPtrRangeEqCmp, AsciiFoldedCaseInsensitiveEqual>)
		{
			GenericEqual eq;
			StringIndexer indexer(m_Vec);

			auto foundValue = indexer.GetPtrs<MustZeroTerminate>(foundIndex);
			if (not eq(foundValue, keyValue))
			{
				static std::vector<bool> s_AlreadyReportedBitmap;
				auto tooSmall = s_AlreadyReportedBitmap.size() <= foundIndex;
				if (tooSmall or not s_AlreadyReportedBitmap[foundIndex])
				{
					if (tooSmall)
					{
						auto newSize = s_AlreadyReportedBitmap.size() * 2;
						MakeMax(newSize, foundIndex + 1);
						s_AlreadyReportedBitmap.resize(newSize);
					}
					s_AlreadyReportedBitmap[foundIndex] = true;
					if (!EventLog_HideDepreciatedCaseMixupWarnings())
					{
						auto warningStr = mgFormat2string("Depreciated mix-up of cases, tokenized '%s' as token %d and then seen '%s'", foundValue, foundIndex, keyValue);
						PostMainThreadOper([warningStr] {
							reportD(SeverityTypeID::ST_CaseMixup, warningStr.c_str());
							}
						);
					}
				}
			}
		}

		return foundIndex; //	return found ID.
	}

	index_type nextID = m_Vec.size();
	m_Vec.push_back_seq(keyFirst, keyLast MG_DEBUG_ALLOCATOR_SRC("IndexedStrings.GetOrCreateID_impl"));
	if (MustZeroTerminate)
		m_Vec.back().push_back(0 MG_DEBUG_ALLOCATOR_SRC("IndexedStrings.GetOrCreateID_impl")); // add null terminator
	m_Idx.insert(nextID);
	return nextID;
}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetExisting_st(CharPtr keyFirst, CharPtr keyLast) const
{
	dbg_assert(scc_GetOrCreateID == 0);

	return GetExisting_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetExisting_mt(CharPtr keyFirst, CharPtr keyLast) const
{
	IndexedString_shared_lock lock(GetCS());

	return GetExisting_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate, typename CharPtrRangeEqCmp, typename CharPtrRangeHasher>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate, CharPtrRangeEqCmp, CharPtrRangeHasher>::GetExisting_impl(CharPtr keyFirst, CharPtr keyLast) const
{
	CharPtrRange keyValue(keyFirst, keyLast);
	auto i = m_Idx.find(keyValue);
	if (i != m_Idx.end() && m_Idx.key_eq()(keyValue, *i))
		return *i; //	return found ID.

	return UNDEFINED_VALUE(index_type);
}

using IndexedStringValues = IndexedStrings<false, GenericEqual, GenericHasher>;

template struct IndexedStrings<true, AsciiFoldedCaseInsensitiveEqual, AsciiFoldedChunkedCaseInsensitiveHasher>;
template struct IndexedStrings<false, GenericEqual, GenericHasher>;