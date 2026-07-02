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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "UsingCache.h"

#include "dbg/DebugCast.h"
#include "geo/StringBounds.h"
#include "mci/register.h"
#include "utl/IncrementalLock.h"
#include "utl/Swapper.h"

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
struct CompareLtWeakItemId
{
	bool operator ()(const weak_ti& a, const weak_ti& b) const { return lock_raw(a)->GetID() <  lock_raw(b)->GetID(); }
	bool operator ()(const weak_ti& a, TokenID bID)       const { return lock_raw(a)->GetID() <  bID; }
	bool operator ()(TokenID aID, const weak_ti& b)       const { return aID <  lock_raw(b)->GetID(); }
	bool operator ()(const weak_ti& a, const TreeItem* b) const { return lock_raw(a)->GetID() <  b->GetID(); }
	bool operator ()(const TreeItem* a, const weak_ti& b) const { return a->GetID() <  lock_raw(b)->GetID(); }
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
	if (b==e) return true;

	TokenID lastID = lock_raw(*b)->GetID();
	while (++b!=e)
	{
		dms_assert(lastID <  lock_raw(*b)->GetID() );
		lastID = lock_raw(*b)->GetID();
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
	UInt32 nrKeep = (keepParent && m_Context->GetTreeParent())
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
		auto ns = FindNamespace(url);
	   	if (!ns)
			throwErrorF("UsingCache", "Cannot find reference in Using = \"%s\"\n%s"
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

auto UsingCache::FindNamespace(TokenID url) const -> SharedTreeItem
{
	UInt32 n = m_Usings.size();
	SharedStr urlAsString = SharedStr(url);
	if (!n)
	{
		if (!m_Context->GetTreeParent() && !m_Context->IsCacheItem())
			return {};
		// we look for context ref of instantiated template in cache
		dms_assert(!m_Context->GetTreeParent());
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
		return FindNamespace(itemID);
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
		dms_assert(lock_raw(*ip)->GetID() == child->GetID());
		if (lock_raw(*ip) == child) // as same item; no problem, maybe inserted though other incoming route
			return;
		if (child->GetTreeParent().get() != m_Context) // would have been new
		{
			if (lock_raw(*ip)->GetTreeParent().get() == m_Context)  // *ip has best rights
				return;

			// child was toegevoegd in 1 der usings; kijk of het niet overruled wordt door *ip
			UInt32 n = m_Usings.size();
			SharedTreeItem foundItem;
			while (n-- && !foundItem)
				foundItem = lock_raw(m_Usings[n])->GetUsingCache()->FindItem(child->GetID());
			assert(foundItem && (foundItem.get() == lock_raw(*ip) || foundItem.get() == child));
			if (foundItem.get() != child)
				return; // best rights for existing *ip
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
