// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "UsingCache.h"

#include "dbg/DebugCast.h"
#include "vt/StringBounds.h"
#include "mci/register.h"
#include "utl/IncrementalLock.h"
#include "utl/swapper.h"

#include "SessionData.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//  -----------------------------------------------------------------------
// m_Usings and m_SortedItemCache are now std::vector<std::weak_ptr<const TreeItem>> (non-owning, with
// liveness): the tree owns these namespaces/items (parent-owns-child); an owning ref here would form a
// retain cycle up to the config root (the teardown leak). The namespaces are alive during all cache
// operations (they are tree-owned siblings/ancestors), so these helpers lock to a raw pointer for the
// identity/ID comparisons the cache relies on. An entry that has expired locks to nullptr.

using weak_ti = std::weak_ptr<const TreeItem>;

static const TreeItem* lock_raw(const weak_ti& wp) noexcept { return wp.lock().get(); }
static weak_ti make_weak(const TreeItem* p) { return p ? weak_ti(p->weak_from_this()) : weak_ti(); }

// Comparator/equality over weak entries (and raw / TokenID keys), by item ID, locking each weak entry.
// An expired entry orders before any live entry (and is equivalent to any other expired entry),
// keeping a strict weak ordering when entries expire while in a container.
struct CompareLtWeakItemId
{
	bool operator ()(const weak_ti& a, const weak_ti& b) const { auto pa = a.lock(); auto pb = b.lock(); if (!pa || !pb) return pb && !pa; return pa->GetID() < pb->GetID(); }
	bool operator ()(const weak_ti& a, TokenID bID)       const { auto pa = a.lock(); return !pa || pa->GetID() <  bID; }
	bool operator ()(TokenID aID, const weak_ti& b)       const { auto pb = b.lock(); return pb && aID <  pb->GetID(); }
	bool operator ()(const weak_ti& a, const TreeItem* b) const { auto pa = a.lock(); return !pa || pa->GetID() <  b->GetID(); }
	bool operator ()(const TreeItem* a, const weak_ti& b) const { auto pb = b.lock(); return pb && a->GetID() <  pb->GetID(); }
};

// Find the first entry whose locked target == p (raw identity); returns index or size() if absent.
static SizeT weak_find(const TreeItemCPtrArray& arr, const TreeItem* p)
{
	for (SizeT i = 0, n = arr.size(); i != n; ++i)
		if (lock_raw(arr[i]) == p)
			return i;
	return arr.size();
}

//  -----------------------------------------------------------------------

#if defined(MG_DEBUG)

static UInt32 sd_NrInstances = 0;

bool TestOrder(
	UsingCache::const_item_array_iterator b,
	UsingCache::const_item_array_iterator e)
{
	// Entries may have expired in place since the container was ordered; CompareLtWeakItemId treats an
	// expired entry as ordering before (and equivalent to) any other expired entry, so an expired entry
	// carries no ID to check against. Only require strictly increasing IDs over the live entries.
	std::shared_ptr<const TreeItem> lastItem;
	for (; b!=e; ++b)
	{
		auto currItem = b->lock();
		if (!currItem)
			continue;
		dms_assert(!lastItem || lastItem->GetID() < currItem->GetID());
		lastItem = std::move(currItem);
	}
	return true;
}


#endif 

//  -----------------------------------------------------------------------
#if defined(MG_DEBUG_DATA)
static SizeT g_Count = 0;
#endif

UsingCache::UsingCache(const TreeItem* context)
:	m_Context(context)
#if defined(MG_DEBUG_DATA)
,	md_SeqNr(++g_Count)
#endif
{
	dms_assert(context);
	MG_DEBUGCODE( ++sd_NrInstances; )

	AddParent();
}

UsingCache::~UsingCache()
{
	MG_DEBUGCODE( --sd_NrInstances; )

	// m_Usings is non-owning: an incoming cache may still hold a raw pointer to m_Context.
	// Detach ourselves from each incoming so its m_Usings doesn't dangle and recomputes
	// without us. In the well-ordered (parent) teardown m_Incoming is already empty here.
	while (!m_Incoming.empty())
	{
		UsingCache* incoming = m_Incoming.back();
		m_Incoming.pop_back();
		{
			SizeT pos = weak_find(incoming->m_Usings, m_Context);
			if (pos < incoming->m_Usings.size())
				incoming->m_Usings.erase(incoming->m_Usings.begin() + pos);
		}
		incoming->SetDirty();
	}

	ClearUsings(false);
}

void UsingCache::AddParent()
{
	auto contextParent = m_Context->GetTreeParent();
	if (contextParent)
		AddUsingInternal(contextParent.get());
}

void UsingCache::RemoveParentUsing()
{
	auto contextParent = m_Context->GetTreeParent();
	if (!contextParent)
		return;
	m_ParentIsHidden = true;
	SizeT pos = weak_find(m_Usings, contextParent.get());
	if (pos >= m_Usings.size())
		return;
	if (contextParent->CurrHasUsingCache())
		contextParent->GetUsingCache()->DelIncoming(this);
	m_Usings.erase(m_Usings.begin() + pos);
	SetDirty();
}

UInt32 UsingCache::GetNrUsings() const
{
	UpdateUsings();
	return m_Usings.size();
}
const TreeItem* UsingCache::GetUsing(UInt32 i) const
{
	assert(m_UsingUrls.empty());
	MG_PRECONDITION(i < m_Usings.size());
	return lock_raw(m_Usings[i]);
}


void UsingCache::AddIncoming(UsingCache* incoming) const
{
	m_Incoming.push_back(incoming);
}

void UsingCache::DelIncoming(UsingCache* incoming) const
{
	vector_erase(m_Incoming, incoming);
}

#if defined(MG_DEBUG)
	namespace {
		std::atomic<UInt32> sd_UsingCacheRecursionCount = 0;
	}
#endif 

void UsingCache::CheckSearchSpace(const TreeItem* nameSpace) const
{
	if (!nameSpace)
		return;
	TreeItemContextHandle tich(nameSpace, "CheckSeachSpace");

#if defined(MG_DEBUG)
	StaticMtIncrementalLock<sd_UsingCacheRecursionCount> gLock;
	dms_assert(sd_UsingCacheRecursionCount <= sd_NrInstances);
#endif 

	if (nameSpace == m_Context)
		m_Context->throwItemError("AddUsing would result in a circular reference");

	// don't trigger unneccesary UpdateCache

	const TreeItemCPtrArray& nameSpaceUsings = nameSpace->GetUsingCache()->m_Usings;
	TreeItemCPtrArray::const_iterator i = nameSpaceUsings.end();
	TreeItemCPtrArray::const_iterator b = nameSpaceUsings.begin();
	while (i != b)
		CheckSearchSpace(lock_raw(*--i));
}


void UsingCache::ClearUsings(bool keepParent)
{
	UInt32 nrKeep = (keepParent && m_Context->GetTreeParent() && !m_ParentIsHidden)
		? 1
		: 0;
	dms_assert(m_Usings.begin()+nrKeep <= m_Usings.end());

	usings_iterator b = m_Usings.begin()+nrKeep; // don't clear parent?
	usings_iterator e = m_Usings.end();
	const_usings_iterator i = b;

	// m_Usings entries are non-owning weak refs. During teardown a used namespace may already be gone
	// (expired weak -> lock_raw null) or be mid-destruction without a cache; in either case there is no
	// incoming registration of `this` left to remove, so skip it (don't deref null / don't create a cache).
	while (i!=e)
		if (const TreeItem* ns = lock_raw(*i++))
			if (ns->CurrHasUsingCache())
				ns->GetUsingCache()->DelIncoming(this);

	if (nrKeep)
		m_Usings.erase(b, e);
	else
		vector_clear(m_Usings);

	vector_clear(m_UsingUrls);
	SetDirty();
}

bool UsingCache::AddUsingInternal(const TreeItem* nameSpace) const
{
	dms_assert(nameSpace);

	SizeT prevPos = weak_find(m_Usings, nameSpace);
	if (prevPos >= m_Usings.size())
	{
		// TODO: test that the search space of the nameSpace does not include this
		CheckSearchSpace(nameSpace);
		nameSpace->GetUsingCache()->AddIncoming(const_cast<UsingCache*>(this));
	}
	else
	{
		if (prevPos == m_Usings.size() -1)
			return false;
		m_Usings.erase(m_Usings.begin() + prevPos);
	}
	m_Usings.emplace_back(make_weak(nameSpace));
	return true;
}

void UsingCache::AddUsing(const TreeItem* nameSpace)
{
	if (AddUsingInternal(nameSpace))
		SetDirty();
}

void UsingCache::AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace)
{
	UInt32 nrToAdd = lastNameSpace - firstNameSpace;
	dms_assert(nrToAdd); // guaranteed by calling TreeItem::AddUsings

	// look for overlapping range between end of current namespaces and begin of additional ones
	usings_iterator end = m_Usings.end();
	usings_iterator pos = m_Usings.begin();
	while (pos != end && lock_raw(*pos) != *firstNameSpace) ++pos; // find *firstNameSpace by locked identity (requires nrToAdd>0)

	assert(pos <= end);
	if (UInt32(end - pos) <= nrToAdd) // overlapping range not possible
	{
		const TreeItem** i = firstNameSpace;
		while (pos != end)
			if (*i++ != lock_raw(*pos++))
				goto doAdd;
		firstNameSpace = i; // skip starting range that is equal to end range of m_Usings
	}

doAdd:
	while (firstNameSpace != lastNameSpace)
		AddUsing(*firstNameSpace++);
}

void UsingCache::AddUsingUrl(TokenID urlToken)
{
	SizeT pos = vector_find(m_UsingUrls, urlToken);

	if (pos < m_UsingUrls.size() )
	{
		if (pos == m_UsingUrls.size()-1)
			return;
		m_UsingUrls.erase(m_UsingUrls.begin()+pos);
	}
	m_UsingUrls.push_back(urlToken);
	SetDirty();
}

bool UsingCache::AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd, SizeT nrSkippedFromEnd)
{
	dms_assert(urlsEnd);
	dms_assert(urlsBegin != urlsEnd);
	CharPtr lastUrlStart = urlsEnd;

	while (lastUrlStart != urlsBegin && *--lastUrlStart != ';') ;
	if (*lastUrlStart ==';') ++lastUrlStart;

	TokenID urlToken = GetTokenID_mt(lastUrlStart, urlsEnd);
	bool canSkip = 
			nrSkippedFromEnd < m_UsingUrls.size() 
		&&	*(m_UsingUrls.end()-(nrSkippedFromEnd+1)) == urlToken;

	if (urlsBegin != lastUrlStart)
		canSkip &= !AddUsingUrls(urlsBegin, lastUrlStart-1, canSkip ? ++nrSkippedFromEnd : 0);

	if (!canSkip)
		AddUsingUrl(urlToken);
	return canSkip;
}

void UsingCache::AddUsingUrls(CharPtr urlBegin, CharPtr urlEnd)
{
	dms_assert(urlBegin);
	dms_assert(urlEnd);
	dms_assert(urlBegin != urlEnd); // PRECONDITION, don't call this when there is nothing to add anyway.
	AddUsingUrls(urlBegin, urlEnd, 0);
}

void UsingCache::UpdateUsings() const
{
	if (m_UsingUrls.empty())
		return;

	for (auto i = m_UsingUrls.begin(), e = m_UsingUrls.end(); i!=e; ++i)
	{
		TokenID url = *i;
		auto ns = FindNamespace(url, true); // 'using' url resolution: definition scope reachable via hidden parent
	   	if (!ns)
			throwErrorF("UsingCache", "Cannot find reference in Using = \"{}\"\n{}"
			,	GetTokenStr(url).c_str()
			,	m_Context->GetSourceName().c_str()
			);
		AddUsingInternal(ns.get());
	}
	vector_clear(m_UsingUrls);
}

TreeItemCPtrArray MergeArrays(const TreeItemCPtrArray& tmpSrc, const TreeItemCPtrArray& extra)
{
	TreeItemCPtrArray result;
	result.clear();
	result.resize(tmpSrc.size() + extra.size()); // default-constructed (empty) weak entries
	result.erase(
		std::set_union(
			tmpSrc.begin(), tmpSrc.end(),
			extra.begin(),  extra.end(),
			result.begin(),
			CompareLtWeakItemId()
		),
		result.end()
	);
	MG_DEBUGCODE(dms_assert(TestOrder(result.begin(), result.end())); );
	return result;
}

void MergeCacheIntoArray(const UsingCache* uc, TreeItemCPtrArray& tmp1)
{
	tmp1 = MergeArrays(tmp1, uc->m_SortedItemCache);
}

void UsingCache::Update(const TreeItem* item)
{
	dms_assert(item);
	item->GetUsingCache()->UpdateCache();
	dms_assert(item->GetUsingCache()->IsReady());
}

#if defined(MG_DEBUG)
std::atomic<UInt32> sd_UpdateCacheTmpLockCount = 0;
#endif

void UsingCache::UpdateCache() const
{
	assert(m_CacheState != CacheStateType::BUSY);

	if (!IsDirty())
		return;

	MG_SIGNAL_ON_UPDATEMETAINFO

	MG_DEBUG_DATA_CODE(md_PrevState = m_CacheState; )

	// Trigger UpdateMetaInfo with GetNrSubItems 
	// in order to find all items, m_Usings, etc.
	m_Context->UpdateMetaInfo();

	m_CacheState = CacheStateType::READY;
	tmp_swapper<CacheStateType> lockCacheStateAsBusy(m_CacheState, CacheStateType::BUSY);

	UpdateUsings();
	UInt32 nrUsings = m_Usings.size();
	for (UInt32 i = nrUsings; i--; )
		if (const TreeItem* u = lock_raw(m_Usings[i])) // never assume a weak using entry is still alive
			Update(u);

#if defined(MG_DEBUG)
	// Warm the referred-item namespace chain HERE, in this UpdateMetaInfo-permitted
	// context, BEFORE installing the no-update guard below. The namespace-merge walk
	// further down calls GetReferredItem(), which triggers a fresh UpdateMetaInfo on any
	// not-yet-resolved referred namespace; under the guard that would assert. Normally
	// the chain is already warm (the configuration was resolved as a side effect of
	// computing some item), so this is a no-op; it matters only when a UsingCache is
	// built COLD -- e.g. the definition-time @checkfunctions audit runs the checker before
	// the configuration is otherwise resolved.
	for (SharedTreeItem warmRef = make_shared_tree(m_Context, existing_obj{}); warmRef; warmRef = warmRef->GetReferredItem())
		warmRef->UpdateMetaInfo();

	MG_LOCKER_NO_UPDATEMETAINFO
	assert(sd_UpdateCacheTmpLockCount == 0);
	StaticMtIncrementalLock<sd_UpdateCacheTmpLockCount> useTmp;
#endif

	vector_clear(m_SortedItemCache);

	TreeItemCPtrArray tmpSubItems, tmpNameSpace;

	SharedTreeItem refItem = make_shared_tree(m_Context, existing_obj{}); // TODO ownership: was raw const TreeItem*; held as shared so the GetReferredItem() chain outlives each loop iteration
	while (true) {
		assert(refItem->m_State.GetProgress() >= ProgressState::MetaInfo || (refItem->m_State.GetFailType() != FailType::None));

		auto nrSubItems = m_Context->CountNrSubItems();
		tmpSubItems.clear();
		tmpSubItems.reserve(nrSubItems);
		const TreeItem* subItem = refItem->_GetFirstSubItem(); // avoid UpdateMetaInfo
		while (subItem)
		{
			tmpSubItems.emplace_back(make_weak(subItem));
			subItem = subItem->GetNextItem();
		}
		std::sort(tmpSubItems.begin(), tmpSubItems.end(), CompareLtWeakItemId());
		MG_DEBUGCODE(dms_assert(TestOrder(tmpSubItems.begin(), tmpSubItems.end())); )
		tmpNameSpace = MergeArrays(tmpNameSpace, tmpSubItems);
		refItem = refItem->GetReferredItem();
		if (!refItem)
			break;
	}

	MG_DEBUGCODE( dms_assert(TestOrder(tmpNameSpace.begin(), tmpNameSpace.end())); )

	for (UInt32 i = nrUsings; i--; )
		if (const TreeItem* u = lock_raw(m_Usings[i])) // never assume a weak using entry is still alive
			MergeCacheIntoArray(u->GetUsingCache(), tmpNameSpace);

	// sorted and no doubles in m_SortedItemCache?
	MG_DEBUGCODE( dms_assert(TestOrder(tmpNameSpace.begin(), tmpNameSpace.end())); )
	dms_assert(m_CacheState == CacheStateType::BUSY);
	dms_assert(m_SortedItemCache.size() == 0);

	m_SortedItemCache.insert(m_SortedItemCache.begin(), tmpNameSpace.begin(), tmpNameSpace.end());
}

auto UsingCache::FindNamespace(TokenID url, bool mayResolveViaHiddenParent) const -> SharedTreeItem
{
	UInt32 n = m_Usings.size();
	SharedStr urlAsString = SharedStr(url);
	if (mayResolveViaHiddenParent && !urlAsString.empty() && *urlAsString.begin() == '/')
	{
		// absolute 'using' urls resolve from the root, independent of the usings list;
		// required for strict function-instance scopes whose usings hold no parent to
		// route through
		if (!m_Context->GetTreeParent() && m_Context->IsCacheItem())
			return SessionData::Curr()->GetConfigRoot()->FindItem(urlAsString); // context ref of instantiated template in cache
		const TreeItem* root = m_Context;
		while (auto parent = root->GetTreeParent())
			root = parent.get();
		if (!root->IsCacheItem())
			return root->FindItem(urlAsString);
	}
	if (!n)
	{
		if (m_ParentIsHidden && mayResolveViaHiddenParent)
		{
			// strict function scope: relative 'using' urls still resolve against the
			// (hidden) parent, i.e. the definition scope
			if (auto contextParent = m_Context->GetTreeParent())
				return contextParent->FindItem(urlAsString);
		}
		if (!m_Context->GetTreeParent() && !m_Context->IsCacheItem())
			return {};
		if (m_Context->GetTreeParent())
			return {}; // strict scope with hidden parent: plain identifiers do not fall back
		// we look for context ref of instantiated template in cache
		dms_assert(url.GetStr().c_str()[0] == '/');
		return SessionData::Curr()->GetConfigRoot()->FindItem(urlAsString);
	}
	while (n--)
	{
		const TreeItem* u = lock_raw(m_Usings[n]); // never assume a weak using entry is still alive
		if (!u)
			continue;
		auto foundItem = u->FindItem(urlAsString); // TODO return 0 if firstName found somewhere
		if (foundItem)
			return foundItem;
	}
	if (m_ParentIsHidden && mayResolveViaHiddenParent)
	{
		// strict function scope: relative 'using' urls still resolve against the
		// (hidden) parent, i.e. the definition scope
		if (auto contextParent = m_Context->GetTreeParent())
			return contextParent->FindItem(urlAsString);
	}
	return {};
}

auto UsingCache::FindItem(TokenID itemID) const -> SharedTreeItem
{
	if (m_CacheState == CacheStateType::BUSY)
	{
		// Get it the old way, look in m_Context, and then in all m_usings in reverse order
		dms_assert(m_Context);
		auto ti = m_Context->GetConstSubTreeItemByID(itemID);
		if (ti) 
			return ti;
		return FindNamespace(itemID, false); // BUSY-window identifier lookup: strict scopes must not fall back to the hidden parent
	}

 	UpdateCache();
	dms_assert(IsReady());

	MG_DEBUGCODE( dms_assert(TestOrder(m_SortedItemCache.begin(), m_SortedItemCache.end())); )

	CompareLtWeakItemId cmp;
	auto result = std::lower_bound(m_SortedItemCache.begin(), m_SortedItemCache.end(), itemID, cmp);
	if (result == m_SortedItemCache.end() || cmp(itemID, *result))
		return {};
	return SharedTreeItem(result->lock()); // lock the weak entry to an owning SharedTreeItem
}

void UsingCache::OnItemAdded(const TreeItem* child)
{
	if (!IsReady())
		return;

	CompareLtWeakItemId cmp;
	auto ip = std::lower_bound(m_SortedItemCache.begin(), m_SortedItemCache.end(), child, cmp);
	if (ip == m_SortedItemCache.end() || cmp(child, *ip))
 		m_SortedItemCache.insert(ip, make_weak(child));
	else // name already in cache, pointed at by ip.
	{
		if (auto cachedItem = ip->lock()) // never assume a weak cache entry is still alive; an expired entry is simply overwritten
		{
			dms_assert(cachedItem->GetID() == child->GetID());
			if (cachedItem.get() == child) // as same item; no problem, maybe inserted though other incoming route
				return;
			if (child->GetTreeParent().get() != m_Context) // would have been new
			{
				if (cachedItem->GetTreeParent().get() == m_Context)  // *ip has best rights
					return;

				// child was toegevoegd in 1 der usings; kijk of het niet overruled wordt door *ip
				UInt32 n = m_Usings.size();
				SharedTreeItem foundItem;
				while (n-- && !foundItem)
					if (auto u = m_Usings[n].lock()) // never assume a weak using entry is still alive
						foundItem = u->GetUsingCache()->FindItem(child->GetID());
				assert(!foundItem || foundItem.get() == cachedItem.get() || foundItem.get() == child);
				if (foundItem && foundItem.get() != child)
					return; // best rights for existing *ip
				// null foundItem (all usings expired): child is the only live candidate -> overwrite
			}
		}
		*ip = make_weak(child); // overwrite
	}

	MG_DEBUGCODE( dms_assert(TestOrder(m_SortedItemCache.begin(), m_SortedItemCache.end())); )

	for (auto i = m_Incoming.begin(), e = m_Incoming.end(); i!=e; ++i)
		(*i)->OnItemAdded(child);
}

void UsingCache::SetDirty()
{
	if (m_CacheState == CacheStateType::DIRTY)
		return;

	assert(m_CacheState != CacheStateType::BUSY);
	vector_clear(m_SortedItemCache);

	auto
		i = m_Incoming.begin(),
		e = m_Incoming.end();
	while (i!=e)
		(*i++)->SetDirty();

	m_CacheState = CacheStateType::DIRTY;
}
