#include "RtcPCH.h"


/****************** IndexedStrings *******************/

#include "set/IndexedStrings.h"
#include "LockLevels.h"

//  -----------------------------------------------------------------------

inline bool lex_caseinsensitive_compare(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	dms_assert_without_debugonly_lock(f1 &&  l1 || f1 == l1);
	dms_assert_without_debugonly_lock(f2 &&  l2 || f2 == l2);

	SizeT 
		sz1 = l1-f1, 
		sz2 = l2-f2;
	SizeT sz_min = Min<SizeT>(sz1, sz2);
	if (!sz_min)
	{
		dms_assert_without_debugonly_lock(f1 == l1 || f2 == l2);
		return sz2;
	}
	dms_assert_without_debugonly_lock(f1 && l1 && f2 && l2 && f1!=l1 && f2!=l2);
	int cmpRes = _strnicmp(f1, f2, sz_min);
	return (cmpRes < 0)
		|| (cmpRes == 0 && sz1 < sz2);
}

inline bool lex_caseinsensitive_compare(CharPtr f1, CharPtr f2)
{
	if (!f1 || !*f1)
		return f2 && *f2;
	if (!f2 )
		return false;
	return stricmp(f1, f2) < 0;
}

bool SharedPtrInsensitiveCompare::operator ()(CharPtr lhs, CharPtr rhs) const
{
	return lex_caseinsensitive_compare(lhs, rhs);
}

//  -----------------------------------------------------------------------


template <bool MustZeroTerminate>
StringIndexCompare<MustZeroTerminate>::StringIndexCompare(const StringArray& container)
	: r_Container(container)
{}

template <bool MustZeroTerminate>
CharPtrRange StringIndexCompare<MustZeroTerminate>::GetPtrs(index_type x) const
{
	dms_assert(IsDefined(x));

	StringCRef ref = r_Container[x];
	dms_assert( !MustZeroTerminate || ref.size() ); // even empty string has a nonzero size because of the null terminator
	CharPtrRange ix = CharPtrRange(ref.begin(), MustZeroTerminate ? &ref.back() : ref.end()); // exclude null terminator in compare
	dms_assert(!MustZeroTerminate || !*ix.end());  // check that it is a null terminator that is excluded 
	return ix;
}

template <bool MustZeroTerminate>
bool StringIndexCompare<MustZeroTerminate>::operator()(index_type a, index_type b) const 
{ 
	CharPtrRange ia = GetPtrs(a), ib= GetPtrs(b);
	return lex_caseinsensitive_compare(ia.begin(), ia.end(), ib.begin(), ib.end());
}

template <bool MustZeroTerminate>
bool StringIndexCompare<MustZeroTerminate>::operator()(index_type a, CharPtrRange ib) const
{
	CharPtrRange ia = GetPtrs(a);
	return lex_caseinsensitive_compare(ia.begin(), ia.end(), ib.begin(), ib.end());
}

template <bool MustZeroTerminate>
bool StringIndexCompare<MustZeroTerminate>::operator()(CharPtrRange ia, index_type b) const
{
	CharPtrRange ib = GetPtrs(b);
	return lex_caseinsensitive_compare(ia.begin(), ia.end(), ib.begin(), ib.end());
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
	m_Vec.reserve_data(sz*4 MG_DEBUG_ALLOCATOR_SRC_PARAM); // estimated lowerbound of total stringlengths
}

//  -----------------------------------------------------------------------

template <bool MustZeroTerminate>
IndexedStrings<MustZeroTerminate>::IndexedStrings()
	:	m_Idx(compare_type(m_Vec)) 
{}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type 
IndexedStrings<MustZeroTerminate>::GetOrCreateID_mt(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	IndexedString_scoped_lock lock(GetCS());

	return GetOrCreateID_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate>::GetOrCreateID_st(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	return GetOrCreateID_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate>::GetOrCreateID_impl(CharPtr keyFirst, CharPtr keyLast) // range of chars excluding null terminator
{
	CharPtrRange keyValue(keyFirst, keyLast);
	index_iterator i = m_Idx.lower_bound(keyValue);
	if (i != m_Idx.end() && !m_Idx.key_comp()(keyValue, *i))
		return *i; //	return found ID.

	index_type nextID = m_Vec.size();
	m_Vec.push_back_seq(keyFirst, keyLast);
	if (MustZeroTerminate)
		m_Vec.back().push_back(0); // add null terminator
	m_Idx.insert(i, nextID);
	return nextID;
}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate>::GetExisting_st(CharPtr keyFirst, CharPtr keyLast) const
{
	dbg_assert(scc_GetOrCreateID == 0);

	return GetExisting_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate>::GetExisting_mt(CharPtr keyFirst, CharPtr keyLast) const
{
	IndexedString_shared_lock lock(GetCS());

	return GetExisting_impl(keyFirst, keyLast);
}

template <bool MustZeroTerminate>
IndexedStringsBase::index_type
IndexedStrings<MustZeroTerminate>::GetExisting_impl(CharPtr keyFirst, CharPtr keyLast) const
{
	CharPtrRange keyValue(keyFirst, keyLast);
	index_iterator i = m_Idx.lower_bound(keyValue);
	if (i != m_Idx.end() && !m_Idx.key_comp()(keyValue, *i))
		return *i; //	return found ID.

	return UNDEFINED_VALUE(index_type);
}

template struct IndexedStrings<false>;
template struct IndexedStrings<true >;