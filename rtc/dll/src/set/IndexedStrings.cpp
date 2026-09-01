// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"
#include "utl/MgFormat.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif //defined(_MSC_VER)

#include "set/IndexedStrings.h"
#include "sym/Token.h"
#include "utl/Environment.h"
#include "utl/Registry.h"
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
//  Never park on the token registry
//
//  GetOrCreateID_mt takes cs_GetOrCreateID exclusively, and counted_mutex::lock() waits -- without
//  a deadline -- for every outstanding shared usage to be released. A thread that still holds a
//  shared usage of its own is thus waiting for itself: nothing can wake it, and what the user sees
//  is a process at 0% CPU with no error, no log line and no window title to go on.
//
//  A live TokenStr / TokenStrRange is exactly such a usage; TokenID::GetStrLock(),
//  TokenID::AsStrRangeLock() and Object::GetNameLock() all hand one out, and it lives until the end
//  of the full expression at the very least. Keeping one alive across a call that can tokenize a
//  string hangs the process, and it has cost three separate debugging sessions:
//    - the fn_test_shadow hang of 2026-07-14, in FunctionChecker::InferApplication;
//    - stg/dll/src/gdal/gdal_vect.cpp, whose two hand-scoped blocks carried the comment "destructor
//      of TokenStr gives up lock on tokenlist to allow for GetTokenID_mt to be called" until #1227
//      replaced them with a materialized name;
//    - issue #1226, where ExportTab::showEvent held Object::GetName() across
//      isItemOrItsSubItemsMappable.
//  So that a fourth case reports itself rather than being diagnosed by hand, count the usages each
//  thread holds and refuse the acquire that provably cannot succeed.
//
//  This is deliberately NOT a timeout. A deadline detects nothing: it cannot distinguish a
//  self-deadlock from a loaded machine, its constant is arbitrary, and it would report the one case
//  that is certain no faster than the cases that are not. The count is exact -- a usage held by
//  this thread can only be released after the acquire returns -- so the answer is known before the
//  acquire is attempted, and that is where it is given.
//
//  Nor is it a new idea: it duplicates, for this one section and in Release, what the lock-level
//  checker of Parallel.h already decides. level_type::Allow() permits an equal level only when BOTH
//  sides are shared, so entering the exclusive IndexedString level while holding the shared one
//  fails EnterLevel's dms_assert -- before the lock is even taken. That checker is broader (it
//  orders every section against every other) and remains the primary guard. But it lives under
//  MG_DEBUG_LOCKLEVEL, which is defined only for MG_DEBUG, and a Release dms_assert is CC_ASSUME --
//  an optimizer hint, not a check. All three hangs above were met in Release builds, where the
//  checker does not exist. Hence a counter narrow enough to be affordable unconditionally.
//
//  The check cannot have a false positive by construction: any code that reaches GetOrCreateID_mt
//  while holding a usage of its own hangs today, so no working configuration can be doing it.
//
//  Retiring the class rather than reporting it is #1227: the accessors that hand out a registry
//  lock now say so (TokenID::GetStrLock() / AsStrRangeLock(), Object::GetNameLock() /
//  GetClsNameLock() / GetXmlClassNameLock()), the plain names materialize into a SharedStr, and
//  DMS_ENTERS (LockLevels.h) declares lock-level ceilings so the ordering becomes checkable across
//  the virtual and indirect calls a static call graph cannot see.

namespace {

	THREAD_LOCAL UInt32 td_TokenRegistrySharedUsages = 0;

	// Set while the self-deadlock diagnostic is being built. Generating an error message walks the
	// context handles (ErrMsg -> GenerateContext -> AbstrContextHandle::Describe), which may want to
	// tokenize -- while this thread still holds the usage that started all this. The flag keeps that
	// nested attempt from reporting the same self-deadlock again; it fails fast and is swallowed by
	// GenerateContext's own catch, costing at most a line of context.
	THREAD_LOCAL bool td_ReportingTokenRegistrySelfDeadlock = false;

	struct SelfDeadlockReportScope
	{
		SelfDeadlockReportScope() { td_ReportingTokenRegistrySelfDeadlock = true; }
		~SelfDeadlockReportScope() { td_ReportingTokenRegistrySelfDeadlock = false; }
	};

	[[noreturn]] void throwTokenRegistrySelfDeadlock(CharPtrRange newToken, UInt32 nrOwnUsages)
	{
		// Nested: the outer report's own context generation wants a token, with the usage that caused
		// all this still held. Say that briefly rather than repeat the full diagnosis inside itself;
		// GenerateContext's catch swallows it, and its g_DumpContextCount guard stops the walk one
		// level further down, so this cannot recurse.
		if (td_ReportingTokenRegistrySelfDeadlock)
			throwErrorF("TOKEN"
				, "cannot register the name '{}' while reporting a token registry self-deadlock"
				, newToken
			);

		SelfDeadlockReportScope reportScope;

		throwErrorF("TOKEN"
			, "cannot register the name '{}': this thread still holds {} shared usage(s) of the token"
			  " registry and would have to wait for itself to release them.\n"
			  "A live TokenStr or TokenStrRange -- as returned by any of the ...Lock() accessors,"
			  " TokenID::GetStrLock(), TokenID::AsStrRangeLock() or Object::GetNameLock() -- is such a"
			  " usage. Use the plain, materializing accessor (Object::GetName(), TokenID::AsSharedStr())"
			  " before calling anything that can create a token. See GeoDMS issues #1226 and #1227."
			, newToken, nrOwnUsages
		);
	}

}	// end anonymous namespace

RTC_CALL void IncTokenRegistrySharedUsage() noexcept
{
	++td_TokenRegistrySharedUsages;
}

RTC_CALL void DecTokenRegistrySharedUsage() noexcept
{
	assert(td_TokenRegistrySharedUsages);
	--td_TokenRegistrySharedUsages;
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
	// Refuse before the acquire rather than park inside counted_mutex::lock(): see above. Deciding it
	// here, instead of after taking the section, also keeps Debug and Release on the same message --
	// the lock-level checker's own dms_assert inside EnterLevel would otherwise reach a Debug build
	// first, with only a level name to go on.
	if (auto nrOwnUsages = td_TokenRegistrySharedUsages)
		throwTokenRegistrySelfDeadlock(CharPtrRange(keyFirst, keyLast), nrOwnUsages);

	// No DMS_ENTERS here: taking the section below already publishes IndexedString/exclusive as this
	// thread's level, and that is what a caller's declared ceiling is checked against. Declaring it
	// as well would make level_type::Allow reject this very acquire -- equal level, not both shared.
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
						auto warningStr = mgFormat2string("Depreciated mix-up of cases, tokenized '{}' as token {} and then seen '{}'", foundValue, foundIndex, keyValue);
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