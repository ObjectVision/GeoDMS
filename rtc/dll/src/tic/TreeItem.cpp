// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem: the configuration-tree node -- tree topology, item state and
// update flow, storage binding and interest-count management.

#include "TreeItem.h"
#include "TreeItemFunctionSpec.h"
//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcInterface.h"
#include "mci/ValueClass.h"
#include "mci/ValueComposition.h"
#include "act/ActorLock.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "act/Waiter.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/PropDef.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "utl/scoped_exit.h"
#include "utl/SourceLocation.h"
#include "xct/DmsException.h"

#include "LispList.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLockContainers.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataLocks.h"
#include "LispTreeType.h"
#include "OperationContext.h"
#include "OperGroups.h"
#include "PropFuncs.h"
#include "SessionData.h"
#include "SupplCache.h"
#include "StateChangeNotification.h"
#include "TreeItemClass.h"
#include "TreeItemSet.h"
#include "TreeItemUtils.h"
#include "TicInterface.h"
#include "TicPropDefConst.h"
#include "TreeItemProps.h"
#include "TreeItemContextHandle.h"
#include "TreeItemInternal.h"
#include "UsingCache.h"
#include "stg/MemoryMappedDataStorageManager.h"

#include <unordered_set>



// #undef MG_DEBUG_DATA // DEBUG MEMORY ALLOCS AND SETS md_FullName

auto TreeItem::GetOrCreateConfigProperties() const -> ConfigProperties&
{
	if (!m_ConfigProperties)
	{
		assert(!IsCacheItem()); // ConfigProperties is config-only and is never created on cache items
		m_ConfigProperties = std::make_unique<ConfigProperties>();
	}
	return *m_ConfigProperties;
}

const SharedStr& TreeItem::GetExprMember() const noexcept
{
	static const SharedStr s_emptyExpr;
	return m_ConfigProperties ? m_ConfigProperties->mc_Expr : s_emptyExpr;
}

const AbstrCalculatorRef& TreeItem::GetCalculatorMember() const noexcept
{
	static const AbstrCalculatorRef s_empty;
	return m_ConfigProperties ? m_ConfigProperties->mc_Calculator : s_empty;
}

const AbstrCalculatorRef& TreeItem::GetIntegrityCheckerMember() const noexcept
{
	static const AbstrCalculatorRef s_empty;
	return m_ConfigProperties ? m_ConfigProperties->mc_IntegrityChecker : s_empty;
}

const AbstrCalculatorRef& TreeItem::GetSizeExpectationMember() const noexcept
{
	static const AbstrCalculatorRef s_empty;
	return m_ConfigProperties ? m_ConfigProperties->mc_SizeExpectation : s_empty;
}

const AbstrCalculatorRef& TreeItem::GetSizeUpperboundMember() const noexcept
{
	static const AbstrCalculatorRef s_empty;
	return m_ConfigProperties ? m_ConfigProperties->mc_SizeUpperbound : s_empty;
}

void TreeItem::ResetCalculatorMember() const
{
	if (m_ConfigProperties)
		m_ConfigProperties->mc_Calculator.reset();
}

void TreeItem::ResetIntegrityCheckerMember() const
{
	if (m_ConfigProperties)
		m_ConfigProperties->mc_IntegrityChecker.reset();
}

bool TreeItem::IsEditable() const
{
	if (!GetExprMember().empty())
		return false;
	if (auto& calc = GetCalculatorMember(); calc && !calc->IsDataBlock())
		return false;

	return !IsCurrLoadable() || IsCurrStorable();
}

bool TreeItem::GetIsInstantiated() const
{
	return GetTSF(TSF_DataInMem); 
}

void TreeItem::SetIsInstantiated() const
{
	SetTSF(TSF_DataInMem); 
}


//----------------------------------------------------------------------
// RTTI
//----------------------------------------------------------------------

IMPL_DYNC_TREEITEMCLASS(TreeItem, "TreeItem")

//----------------------------------------------------------------------
// void TreeItem::GetOrCreateSupplCache()
//----------------------------------------------------------------------

SupplCache* TreeItem::GetOrCreateSupplCache() const
{
	assert(IsMetaThread());
	if (!HasSupplCache())
		m_SupplCache = std::make_unique<SupplCache>();
	return m_SupplCache.get();
}

//----------------------------------------------------------------------
// UpdateMetaInfoDetectionLock
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

THREAD_LOCAL UInt32 sd_UpdateMetaInfoProtect = 0;

UpdateMetaInfoDetectionLock::UpdateMetaInfoDetectionLock()
{
	++sd_UpdateMetaInfoProtect;
}

UpdateMetaInfoDetectionLock::~UpdateMetaInfoDetectionLock()
{
	--sd_UpdateMetaInfoProtect;
}

bool UpdateMetaInfoDetectionLock::IsLocked()
{
	return sd_UpdateMetaInfoProtect != 0;
}

#endif

//----------------------------------------------------------------------
// ctor/dtor Functions
//----------------------------------------------------------------------

std::atomic<UInt32> TreeItem::s_NotifyChangeLockCount = 0;
UInt32 TreeItem::s_MakeEndoLockCount     = 0;
UInt32 TreeItem::s_ConfigReadLockCount   = 0;

#if defined(MG_DEBUG_DATA)

namespace {

	// Debug leak registry: RAW identity keys (non-owning). Must be raw, not shared_ptr: items self-register
	// via insert(this) in the TreeItem ctor (before any shared_ptr owns them, so shared_from_this is unusable)
	// and self-deregister via erase(this) in the dtor, and the registry must NOT keep items alive.
	struct TreeItemRegistryType : std::set<const TreeItem*> { std::mutex critical_section; };
	static_ptr<TreeItemRegistryType> s_TreeItems;
	UInt32                      s_nrTreeItemAdmLocks = 0;
	TreeItemAdmLock             s_treeItemAdm;

}	// anonymous namespace


static void ReportDataItem(const AbstrDataItem* di)
{
	assert(di);
	auto ado = di->GetDataObj();
	assert(ado);

	reportF(MsgCategory::memory, SeverityTypeID::ST_MinorTrace, "RefCnt={}; InterestCnt={}; KE={}; #DataLocks={}, Name={}",
		di->weak_from_this().use_count(),
		di->GetInterestCount(),
		di->GetKeepDataState(),
		di->GetDataObjLockCount(),
		di->GetFullName().c_str()
	);
}

TIC_CALL void TreeItemWithMemReport()
{
	if (!s_TreeItems)
		return;

	DBG_START("TreeItem", "ReportMem", true);
	for (auto itemPtr: *s_TreeItems)
	{
		auto di = AsDynamicDataItem(itemPtr);
		if (!di)
			continue;
		auto ado = di->m_DataObject.get();
		if (!ado)
			continue;
		ReportDataItem(di);
	}
}

#include "dbg/DebugReporter.h"
//static auto treeItemWithMemReporter = MakeDebugCaller(TreeItemWithMemReport);

#endif //defined(MG_DEBUG_DATA)


//----------------------------------------------------------------------
// class  : TreeItemAdmLock
//----------------------------------------------------------------------
#if defined(MG_DEBUG_DATA)

TreeItemAdmLock::TreeItemAdmLock()
{
	if (s_nrTreeItemAdmLocks++)
		return;

	dms_assert(!s_TreeItems);
	s_TreeItems.assign( new TreeItemRegistryType );
}

TreeItemAdmLock::~TreeItemAdmLock()
{
	if (--s_nrTreeItemAdmLocks)
		return;

	assert(s_TreeItems);
	if (!g_IsTerminating)
		Report();

//	assert(!s_TreeItems->size());
	s_TreeItems.reset();
}

void TreeItemAdmLock::Report()
{
	dms_assert(s_TreeItems);
	auto lock = std::scoped_lock(s_TreeItems->critical_section);

	SizeT n = s_TreeItems->size();
	if(n)
	{
		reportF_without_cancellation_check(MsgCategory::memory, SeverityTypeID::ST_Error, "MemoryLeak of {} TreeItems. See EventLog for details.", n);

		auto i = s_TreeItems->begin();
		auto e = s_TreeItems->end();
		while (i!=e)
		{
			const TreeItem* ti = *i++;
			reportF_without_cancellation_check(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, "MemoryLeak: {} ({},{}) {}",
				ti->GetDynamicClass()->GetName(),
				ti->weak_from_this().use_count(),
				ti->IsCacheItem(),
				ti->GetFullName().c_str());
		}
	}
}

#endif //defined(MG_DEBUG_DATA)

TreeItem::TreeItem ()
	:	m_ID(TokenID::GetEmptyID())
{
#if defined(MG_DEBUG_DATA)
	auto lock = std::scoped_lock(s_TreeItems->critical_section);
	s_TreeItems->insert(this);
#endif
}

void RemoveStoredPropValues(TreeItem* item); // defined in CopyTreeContext.cpp

TreeItem::~TreeItem ()
{
	DisableStorage();

	MG_LOCKER_NO_UPDATEMETAINFO

	if (IsFunctionItem())
		TreeItem_EraseFunctionSpec(this);

	NotifyStateChange(this, NC_Deleting);

	// An item may now be destroyed while consumers still hold interest in it: supplier interest is NON-owning
	// (weak for std suppliers), so it does not keep its target alive, and the holders decrement only if the
	// target is still live (they no-op on this dying item). Hence this item's own m_InterestCount can be > 0
	// here. But we MUST undo our OWN supplier interest before vanishing -- otherwise the interest we placed on
	// our suppliers leaks and our s_SupplTreeInterest[this] entry dangles. (StopInterest, the usual undo, only
	// runs when our interest reaches 0, which never happened for an item destroyed with residual interest.)
	if (DoesHaveSupplInterest())
	{
		garbage_can supplGarbage = StopSupplInterest(); // releases our suppliers' interest; destructs here
	}

	SetKeepDataState(false); // StringDC en NumbDC cache items hebben ook KeepInterest

	// Symmetric to ~AbstrDataItem: an item may be destroyed while still of interest (consumer interest is
	// non-owning/weak), so m_InterestCount can be > 0 here while StopInterest never ran. Release our interest
	// accounting (our s_SessionUsageCounter share + the interest we placed on our suppliers) by mirroring the
	// final 1->0 transition. KeepData was dropped just above, so the guard is purely on residual interest count.
	// For an AbstrDataItem this already ran in ~AbstrDataItem (count is 0 here -> no-op); this covers plain
	// TreeItems and AbstrUnits, whose StopInterest IS TreeItem::StopInterest (reachable from ~TreeItem).
	if (GetInterestCount())
	{
		m_InterestCount = 0;
		garbage_can interestGarbage = StopInterest();
	}

	// Tear down our UsingCache BEFORE releasing sub-items. A child cache registers itself in its
	// parent's m_Incoming (UsingCache::AddParent); the symmetric deregistration in a child's
	// ~UsingCache walks its WEAK m_Usings parent entry, which has already expired once the parent's
	// shared refcount hit 0 (we are inside the parent's ~TreeItem). That skips DelIncoming and would
	// leave a DANGLING child UsingCache* in our m_Incoming, which our own ~UsingCache then derefs
	// (SetDirty) -> UAF. Destroying our cache first detaches us from every still-live child's m_Usings
	// (and from used sibling/ancestor namespaces) top-down, so no child ever references a dead parent
	// cache and no dead child is left in any m_Incoming. reset() is a no-op when the cache is absent.
	m_UsingCache.reset();

	// Parent owns its sub-items (downward std::shared_ptr): drop each child now.
	// ReleaseSubItem splices the first child out (m_FirstSub advances to the next sibling) and then
	// drops the owning ptr; looping on m_FirstSub tears down the sibling chain iteratively (no stack
	// growth per sibling), while each child's own subtree recurses via its dtor (bounded by depth).
	while (_GetFirstSubItem())
		ReleaseSubItem(_GetFirstSubItem());

	if (!mc_RefItem.expired())
		SetReferredItem(nullptr);

	// NB: !HasInterest() is no longer asserted -- residual non-owning interest may remain (see above).
	dms_assert( !m_State.Get(actor_flag_set::AF_SupplInterest) ); // guaranteed: undone by StopSupplInterest above

	// m_Parent is a non-owning weak back-pointer, already cleared by the owning parent's ReleaseSubItem.

	if (GetTSF(TSF_HasStoredProps))
		RemoveStoredPropValues(this);

#if defined(MG_DEBUG_DATA)
	auto lock = std::scoped_lock(s_TreeItems->critical_section);
	dms_assert(s_TreeItems->find(this) != s_TreeItems->end());
	s_TreeItems->erase(this);
#endif
}

static void ResetAllKeepInterest(TreeItem* item)
{
	dms_assert(item);
	TreeItem* walker = item;
	do
	{
		walker->SetKeepDataState(false); 
		if (auto refItem = walker->mc_RefItem.lock())
			const_cast<TreeItem*>(refItem.get())->SetKeepDataState(false);
		walker = item->WalkCurrSubTree(walker);
	} while (walker);
}

#if defined(MG_DEBUG)
bool ExplainValue_IsClear();
#endif

// Recursively reset config-derived state (calculators, integrity checker, storage) over the subtree.
// This breaks calculator->supplier (and calculator->ancestor) reference cycles BEFORE the refcount
// teardown, so the owned subtree can actually collapse once its holder is released. (Pre-A this was
// bundled into EnableAutoDeleteImpl together with releasing the per-node auto-delete pin; the pin is gone.)
void TreeItem::ResetSubTreeConfigData()
{
	ResetCalculatorMember();
	ResetIntegrityCheckerMember();
	if (m_ConfigProperties) // #1218: may hold cross-branch supplier refs; break them before the refcount teardown
		m_ConfigProperties->mc_CheckGuardians.reset();
	if (!IsCacheItem())
		DisableStorage();
	for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		subItem->ResetSubTreeConfigData();
}

// Formerly removed the auto-delete pin. The pin is gone (ownership is downward: the parent owns its
// sub-items and roots are held by their SharedPtr owners). This now (a) breaks supplier cycles via
// ResetSubTreeConfigData so refcount teardown can complete, and (b) for a config root, releases the
// SessionData ownership, which drops the last owning ref and cascades destruction of the (now cycle-free)
// tree. For any owned item (parented/cache/endogenous) the holder dropping its SharedPtr (or the parent's
// ReleaseSubItem) is what frees it; here we only need the cycle-break.
// Wait for the shared session usages to be released, but never forever, and say what is holding
// them when they are not.
//
// The drain exists so the main thread does not begin teardown while workers still hold resources,
// and it used to be an unbounded exclusive acquire of s_SessionUsageCounter. That is where a GUI
// closed DURING a calculation parked for good (#1191): the window is gone, every worker is idle in
// the task pool, and the process sits on its memory until someone kills it -- 7.2 GB in the small
// reproduction, 148 GB for 7 hours in the case that started that report.
//
// The count is not held by running workers alone: TreeItem::StartInterest takes a shared usage per
// item of interest and StopInterest releases it, so an item still of interest at teardown holds the
// count above zero with nothing left to bring it down -- the residual interest of the t611/t810
// teardown hangs. Waiting longer cannot help there.
//
// So the wait is bounded and reports: each slice names what is still outstanding, and after the
// last one the teardown continues anyway. That is a deliberate trade: proceeding while a usage is
// outstanding risks touching an item a worker still holds, but the alternative -- an invisible
// process holding its memory forever -- is what users actually suffer, and the report is what
// identifies the leaking holder for the follow-up fix.
static void DrainSessionUsageOrReport(TreeItem* configRoot)
{
	const UInt32 sliceMSec = 10000;
	const UInt32 nrSlices = 6;

	for (UInt32 slice = 0; slice != nrSlices; ++slice)
	{
		if (s_SessionUsageCounter.try_lock_for(sliceMSec))
		{
			s_SessionUsageCounter.unlock();
			return;
		}
		reportF(MsgCategory::other, SeverityTypeID::ST_Warning
			, "Closing down: {} session usage(s) still outstanding after {} seconds"
			, s_SessionUsageCounter.shared_use_count(), (slice + 1) * (sliceMSec / 1000)
		);
	}

	// Name the items that are still of interest: they are the holders that kept the count up, and
	// without this the next occurrence is again a process that just never exits.
	SizeT nrReported = 0, nrOfInterest = 0;
	for (TreeItem* walker = configRoot; walker; walker = configRoot->WalkCurrSubTree(walker))
	{
		if (!walker->GetInterestCount())
			continue;
		++nrOfInterest;
		if (nrReported++ < 10)
			reportF(MsgCategory::other, SeverityTypeID::ST_Warning
				, "Closing down: [[{}]] is still of interest ({}x)"
				, walker->GetFullName(), walker->GetInterestCount()
			);
	}
	reportF(MsgCategory::other, SeverityTypeID::ST_Warning
		, "Closing down: continuing teardown with {} item(s) of interest in the configuration tree;"
		  " see issue #1191. Report this configuration and what it was doing when it was closed."
		, nrOfInterest
	);
}

void TreeItem::EnableAutoDelete() // does not call UpdateMetaInfo
{
	bool isConfigRoot = !(IsCacheItem() || IsEndogenous() || GetTreeParent());

	if (isConfigRoot)
	{
		// Gracefully end worker threads before tearing down the config tree. Mark the session as
		// cancelling so in-flight workers cancel (releasing the shared ownership of their inputs and the
		// mutable ownership of what they produce), then drain by taking s_SessionUsageCounter exclusively:
		// this makes any new try_lock_shared fail (the designed cancellation trigger, see ItemLocks.cpp)
		// and waits until every worker has released its shared usage. Without it the main thread could
		// begin teardown / static-component destruction while workers still hold resources -> leak (the
		// timing-dependent leak the removed auto-delete pin used to mask). The wait is bounded and
		// reports what it is waiting for; see DrainSessionUsageOrReport above for why (#1191).
		if (auto sd = SessionData::Curr())
			sd->SetCancelling();
		DrainSessionUsageOrReport(this);

		dbg_assert(ExplainValue_IsClear());
		assert(!SessionData::Curr() || !SessionData::Curr()->GetConfigRoot() || SessionData::Curr()->GetConfigRoot().get() == this);
		ResetAllKeepInterest(this); // bring interestCount to 0 and DataInMem to DiskCache before teardown
	}

	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;

	ResetSubTreeConfigData(); // break supplier cycles so the tree can collapse by refcount

	if (isConfigRoot)
		SessionData::ReleaseIt(this); // drop SessionData's owning ref -> cascade destroys the tree
	// NB: do not touch `this` after ReleaseIt on a config root -- that drop may have been the last owning
	// ref, in which case the root TreeItem is already destroyed here.
}

// MTA, 16-08-2004:
// there are some strong correlations between flags that need to be checked, and fased out (remove redundant flags).
//
// (semi) invariants: 
//
// TSF_IsCacheItem == actor_flag_set::AF_IsPassor if CacheRoot (subitems are also CacheItems but not passors)
// TSF_IsCacheItem => TSF_IsEndogenous

void TreeItem::SetIsCacheItem() // does not call UpdateMetaInfo
{
	assert(IsEndogenous());
	assert(!GetTreeParent()); // only call on root
	if (IsCacheItem())
		return;

	TreeItem* walker = this;
	do {
		walker->m_StatusFlags.Set(TSF_IsCacheItem);
		walker = WalkCurrSubTree(walker);
	} while(walker);
}

// Copy the template/cache/passor/keep-data flags down from parent into this freshly-linked child.
void TreeItem::InheritParentState(TreeItem* parent)
{
	m_StatusFlags.Set( parent->m_StatusFlags.GetBits(TSF_InTemplate | TSF_IsCacheItem | TSF_InHidden) );

	// special processing
	if (parent->IsPassor())
		SetPassor();
	if (parent->GetKeepDataState())
		SetKeepDataState(true);
	if (parent->GetLazyCalculatedState())
		SetLazyCalculatedState(true);
	if (parent->GetFreeDataState())
		SetFreeDataState(true);
	if (parent->GetStoreDataState())
		SetStoreDataState(true);
}

void InitTreeItem(TreeItem* parent, SharedMutableTreeItem subItem, TokenID id)
{
	assert(subItem);
	TreeItem* self = subItem.get();

	assert(self->m_State.GetProgress() < ProgressState::MetaInfo);
	if (id) CheckTreeItemName( id.GetStrLock().c_str() );
	self->m_ID = id;

	assert(!self->_GetFirstSubItem()); // not allowed since the FullName of sub items would be corrupted

	assert(IsMetaThread());
	if (TreeItem::s_MakeEndoLockCount)
		self->SetTSF(TSF_IsEndogenous);
	if (parent)
	{
		MG_LOCKER_NO_UPDATEMETAINFO

		parent->AddItem(std::move(subItem)); // transfers shared ownership; `self` stays valid (parent owns it)
		self->InheritParentState(parent);
		NotifyStateChange(parent, NC_NewSubItem);
	}
#if defined(MG_DEBUG_DATA)
	self->md_FullName = self->GetFullName();
	if (parent)
		self->md_FullName = parent->md_FullName + '/' + self->GetName().c_str();
#endif
}

//----------------------------------------------------------------------
// NameTreeReg
//----------------------------------------------------------------------

auto NameTreeReg_GetParentAndBranchID(CharPtrRange subItemNames) -> name_pair_t
{
	auto ptr = subItemNames.second;
	while (ptr != subItemNames.first)
	{
		if (*--ptr == DELIMITER_CHAR)
			return { CharPtrRange(subItemNames.first, ptr), CharPtrRange(ptr+1, subItemNames.second) };
	}
	return { CharPtrRange(subItemNames.first, ptr), subItemNames };
}

//----------------------------------------------------------------------
// Parent & Name Functions
//----------------------------------------------------------------------

TokenID TreeItem::GetID () const
{
	assert(m_ID || !(!m_Parent.expired())); // All SubItems must have a name
	return m_ID;
}

//----------------------------------------------------------------------
// Containment Functions
//----------------------------------------------------------------------

bool TreeItem::HasSubItems() const  noexcept
{
	if (_GetFirstSubItem())
		return true;
	if (IsDataItem(this))
		return false;
	UpdateMetaInfo();
	return _GetFirstSubItem();
}

UInt32 TreeItem::CountNrSubItems() const noexcept
{
	UpdateMetaInfo();
	UInt32 result = 0;
	const TreeItem* iter = _GetFirstSubItem();
	while (iter)
	{
		++result;
		iter = iter->GetNextItem();
	}
	return result;
}

UInt32 TreeItem::_CountNrSubItems ()  noexcept
{
	UInt32 result = 0;
	const TreeItem* iter = _GetFirstSubItem();
	while (iter)
	{
		++result;
		iter = iter->GetNextItem();
	}
	return result;
}

const TreeItem* TreeItem::GetFirstSubItem() const noexcept
{
	UpdateMetaInfoIfNotAlready();
	return _GetFirstSubItem();
}

const TreeItem* TreeItem::GetCurrFirstSubItem() const  noexcept
{
	assert(m_State.GetProgress() >= ProgressState::MetaInfo || WasFailed());
	return _GetFirstSubItem();
}

// Inlined sub-item list operations (was single_linked_tree<TreeItem>; see std-ptr-migration-plan.md §11).
// Ownership is downward via std::shared_ptr: the parent owns m_FirstSub and each node owns m_Next.
void TreeItem::AddSub(SharedMutableTreeItem subItem)
{
	dms_assert(subItem);
	dms_assert(!subItem->m_Next); // a fresh node is not yet linked to a sibling
	SharedMutableTreeItem* slot = &m_FirstSub;
	while (*slot) slot = &((*slot)->m_Next);
	*slot = std::move(subItem); // append, transferring ownership into the list
}

auto TreeItem::ExtractSub(TreeItem* subItem) -> SharedMutableTreeItem
{
	dms_assert(m_FirstSub); // subItem was once added, so this must have a firstSub
	SharedMutableTreeItem* slot = &m_FirstSub;
	while (slot->get() != subItem) { dms_assert(*slot); slot = &((*slot)->m_Next); }
	// splice the node out: take its owning ptr, then reconnect predecessor to successor.
	SharedMutableTreeItem extracted = std::move(*slot);     // *slot now empty; extracted owns node (+ its m_Next chain)
	*slot = std::move(extracted->m_Next);                   // predecessor -> successor; extracted->m_Next cleared
	return extracted;                                       // owns only `subItem` (and its own subtree); m_Next null
}

void TreeItem::Reorder(TreeItem** first, TreeItem** last)
{
	// Recover the owning shared_ptrs from the current list, keyed by raw pointer, then re-append
	// in the requested order. (GraphicContainer::SaveOrder passes the children's raw pointers.)
	std::vector<SharedMutableTreeItem> owned;
	while (m_FirstSub)
	{
		SharedMutableTreeItem node = std::move(m_FirstSub);
		m_FirstSub = std::move(node->m_Next); // advance, clearing node->m_Next
		owned.push_back(std::move(node));
	}
	for (; first != last; ++first)
	{
		auto i = std::find_if(owned.begin(), owned.end(),
			[&](const SharedMutableTreeItem& p) { return p.get() == *first; });
		dms_assert(i != owned.end() && *i);
		AddSub(std::move(*i));
	}
}

void TreeItem::AddItem(SharedMutableTreeItem child)
{
	assert(child);
	assert(child->m_Parent.expired());

	assert(!GetSubTreeItemByID(child->GetID()));

	assert(!child->GetInterestCount());

	TreeItem* childRaw = child.get();
	childRaw->m_Parent = make_weak_tree(this);   // non-owning weak back-pointer (set before the move empties `child`)
	AddSub(std::move(child));     // transfer ownership into the sub-item list

	if (m_UsingCache)         m_UsingCache->OnItemAdded(childRaw);
}

void TreeItem::ReleaseSubItem(TreeItem* subItem) // detach a sub-item and release the parent's ownership of it
{
	SharedMutableTreeItem owned = ExtractSub(subItem); // unlink and take the owning ptr out of the list
	subItem->m_Parent.reset();                       // clear the weak back-pointer before dropping ownership
	// `owned` drops here: destroys subItem when this was the last owner (its dtor tears down its own subtree).
}

void TreeItem::RemoveItem(TreeItem* child)
{
	MGD_PRECONDITION(child);
	dms_assert(child->m_Parent.lock().get() == this);

	if (m_UsingCache) m_UsingCache->OnItemRemoved(child);

	bool mustDisconnectInterest;
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);
		mustDisconnectInterest = child->m_InterestCount;
	}
	ReleaseSubItem(child); // unlink, clear weak m_Parent, release the parent's ownership (may destroy child)
	if (mustDisconnectInterest)
		DecInterestCount(); // 'this' is the parent that carried the child's interest
}

//----------------------------------------------------------------------
// Meta Info Qyery Functions
//----------------------------------------------------------------------

SharedStr TreeItem::GetDescr() const
{
	UpdateMetaInfo();
	return _GetDescr();
}

SharedStr TreeItem::_GetDescr() const
{
	return TreeItemPropertyValue(this, descrPropDefPtr);
}

static TokenID
s1 = GetTokenID_st("GridData"),
s2 = GetTokenID_st("PaletteData"),
s3 = GetTokenID_st("UnionData"),
s4 = GetTokenID_st("code"),
s5 = GetTokenID_st("Label"),
s6 = GetTokenID_st("Palette"),
s7 = GetTokenID_st("VAT"),
s8 = GetTokenID_st("lokatie"),
s9 = GetTokenID_st("grens"),
s10 = GetTokenID_st("lijn");

static bool IsGenericID(TokenID id)
{

	return id == s1 
		|| id == s2 
		|| id == s3 
		|| id == s4 
		|| id == s5 
		|| id == s6 
		|| id == s7 
		|| id == s8 
		|| id == s9 
		|| id == s10;
}

#include "UnitClass.h"

SharedStr TreeItem::GetDisplayName() const
{
	SharedStr result = TreeItemPropertyValue(this, labelPropDefPtr);
	if (!result.empty())
		return result;

	if (IsUnit(this))
	{
		const AbstrUnit* tu = AsUnit(this);
		if (tu->IsDefaultUnit())
			return SharedStr(tu->GetUnitClass()->GetValueType()->GetName().c_str() MG_DEBUG_ALLOCATOR_SRC("TreeItem::GetDisplayName"));
	}

	if (IsGenericID(GetID()) && GetTreeParent())
		return GetTreeParent()->GetDisplayName() + " " + SharedStr(GetID());

	return SharedStr(GetID());
}

SharedStr TreeItem::GetExpr() const
{
	if (auto parent = m_Parent.lock())
		parent->UpdateMetaInfo();
	return GetExprMember();
}

void TreeItem::SetDescr(WeakStr description)
{
	descrPropDefPtr->SetValue(this, description);
}

void TreeItem::SetExpr(WeakStr expr)
{
	if (IsInInherited())
	{
		SharedStr exprStr(expr);
		throwItemErrorF("SetExpr({}) not allowed since Calculator is set by parent", SingleQuote( exprStr.begin(), exprStr.send() ).c_str());
	}

	if (GetExprMember() != expr)
	{
		AssertPropChangeRights("CalculationRule");

		GetOrCreateConfigProperties().mc_Expr = expr;

		Invalidate();

		NotifyStateChange(this, NC_PropValueChanged);
	}
}

void TreeItem::SetDC(DataControllerRef newDC, const TreeItem* newRefItem) const
{
	dms_assert(!InTemplate() || (!newDC && !newRefItem));

	if (mc_DC == newDC && (!newRefItem || newRefItem == mc_RefItem.lock().get()))
		return;

	SharedTreeItem newRI;
	if (newDC)
	{
		// #795 in the meta phase: MakeResult below runs the operator's CreateResult, and a diagnostic
		// raised from there has no item to name yet -- the back reference that lets a result name
		// itself is only installed by the SetReferredItem below, once that result exists. Hand the DC
		// the item whose calculation this is, the same name that CallCalcResultImpl adopts in the data
		// phase and under the same rule: fill an empty slot only, so a calculation keeps the name it
		// started with, and let the back reference take over as soon as there is one.
		if (!newDC->GetOriginItem())
			newDC->SetOriginItem(make_shared_tree(this, existing_obj{}));
		newRI = newDC->MakeResult();
	}
	if (newRefItem)
		newRI = make_shared_tree(newRefItem, existing_obj{});

	if (newRI.get() == this)
	{
//		newDC = nullptr; // TODO G8: avoid construction of SourceDescr(...) at this phase
		newRI = nullptr;
	}
	InterestPtr<DataControllerRef> oldDC;

	// TODO G8: re-evaluate for thread and exception safety: set up private and commit in nothrow critical section or lock-free OK.
	auto interestCopy = GetInterestPtrOrNull();

	if (interestCopy)
	{
		if (newDC)
			newDC->IncInterestCount(); // can throw, then what happens with mc_Calculator

		if (mc_DC)
		{
			MG_DEBUGCODE(auto dcIC = mc_DC->GetInterestCount());
			dbg_assert(dcIC >= 1);
			oldDC = std::move(mc_DC);
			dms_assert(!mc_DC);
			oldDC->DecInterestCount();
			dbg_assert(oldDC->GetInterestCount() == dcIC);
		}
		// NB: the old referred-item interest is NO LONGER released here. mc_RefItem is now a weak_tree_ptr
		// (non-owning), so `oldRefItem = std::move(mc_RefItem)` could neither clear it (std::move of a weak
		// binds InterestPtr's std::weak_ptr ctor, which locks without moving -> assert(!mc_RefItem) tripped)
		// nor was it correct: SetReferredItem(newRI.get()) below already snapshots the old mc_RefItem into an
		// OldRefDecrementer (lock_ptr) and decrements its interest on settlement. Doing it here too
		// double-decremented the old referred item's interest. Let SetReferredItem own that responsibility.
	}
	mc_DC = std::move(newDC);
	SetReferredItem(newRI.get());
	if (mc_DC && mc_DC->m_State.Get(actor_flag_set::AF_IntegrityChecked))
		m_State.Set(actor_flag_set::AF_IntegrityChecked);
}

void TreeItem::SetCalculator(AbstrCalculatorRef pr) const
{
	dms_check_not_debugonly;
	dms_assert(IsMetaThread());

	if (pr == GetCalculatorMember())
		return;
	dms_assert(pr);
	GetOrCreateConfigProperties().mc_Calculator = std::move(pr);
}

SharedTreeItemInterestPtr TreeItem::GetInterestPtrOrNull() const
{
	// TreeItem is now an Actor (no longer a SharedActor), so we cannot route through
	// Actor::GetInterestPtrOrNull (which dynamic_casts to SharedActor). Build the interest ptr directly
	// from this std-owned TreeItem, mirroring the base's lock + manual-increment + already_incremented flow
	// to stay deadlock-free (IncInterestCount would re-take sg_CountSection).
	leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
	if (!m_InterestCount)
		return {};

	auto result = make_shared_tree(this, existing_obj{});
	++m_InterestCount;
	return SharedTreeItemInterestPtr(std::move(result), already_incremented_tag{});
}

bool TreeItem::HasCalculatorImpl() const  noexcept
// if true this func guarantees that GetCalculator will return a non-null mc_Calculator:
// not in template and (configured calculator, non-empty expr, or unit with a configured range)
{
	dbg_assert(IsPassor() || m_Parent.expired() || m_Parent.lock()->CheckMetaInfoReady() || s_MakeEndoLockCount);
	if (GetCalculatorMember())
		return true;
	// items in templates never have calculators
	if (InTemplate())
		return false;
	if (!GetExprMember().empty())
		return true;
	if (IsUnit(this) && GetTSF(USF_HasConfigRange))
	{
		assert(AsUnit(this)->HasVarRange());
		return true;
	}
	return false;
}

bool TreeItem::HasCalculator() const noexcept
// if true this func guarantees that GetCalculator will return a non-null mc_Calculator:
// not in template and (configured calculator, non-empty expr, or unit with a configured range)
{
	dms_check_not_debugonly; 

	if (!IsPassor())
		if (auto parent = m_Parent.lock(); parent && !parent->Was(ProgressState::MetaInfo))
			parent->UpdateMetaInfo();

	return HasCalculatorImpl();
}

bool TreeItem::CanSubstituteByCalcSpec() const noexcept // TODO G8: Substitute away
{
	if (HasCalculator())
		return true;
	return false;
}

bool TreeItem::HasIntegrityChecker() const
{
	return !InTemplate() && integrityCheckPropDefPtr->HasNonDefaultValue(this);
}

auto TreeItem::GetIntegrityChecker() const -> AbstrCalculatorRef
{
	assert(HasIntegrityChecker()); // Precondition
	auto& cfg = GetOrCreateConfigProperties();
	if (!cfg.mc_IntegrityChecker)
	{
		SharedStr iCheckStr = integrityCheckPropDefPtr->GetValue(this);
		cfg.mc_IntegrityChecker = AbstrCalculator::ConstructFromStr(this, iCheckStr, CalcRole::Checker);
	}
	return cfg.mc_IntegrityChecker;
}

bool TreeItem::HasSizeExpectation() const
{
	return GetSizeExpectationMember() || sizeExpectationPropDefPtr->HasNonDefaultValue(this);
}

auto TreeItem::GetSizeExpectation() const -> AbstrCalculatorRef
{
	assert(HasSizeExpectation()); // Precondition
	auto& cfg = GetOrCreateConfigProperties();
	if (!cfg.mc_SizeExpectation)
	{
		SharedStr ruleStr = sizeExpectationPropDefPtr->GetValue(this);
		cfg.mc_SizeExpectation = AbstrCalculator::ConstructFromStr(this, ruleStr, CalcRole::Checker);
	}
	return cfg.mc_SizeExpectation;
}

bool TreeItem::HasSizeUpperbound() const
{
	return GetSizeUpperboundMember() || sizeUpperboundPropDefPtr->HasNonDefaultValue(this);
}

auto TreeItem::GetSizeUpperbound() const -> AbstrCalculatorRef
{
	assert(HasSizeUpperbound()); // Precondition
	auto& cfg = GetOrCreateConfigProperties();
	if (!cfg.mc_SizeUpperbound)
	{
		SharedStr ruleStr = sizeUpperboundPropDefPtr->GetValue(this);
		cfg.mc_SizeUpperbound = AbstrCalculator::ConstructFromStr(this, ruleStr, CalcRole::Checker);
	}
	return cfg.mc_SizeUpperbound;
}

void TreeItem::AssertPropChangeRights(CharPtr changeWhat) const
{
	if ((! IsEndogenous()) || s_MakeEndoLockCount)
		return;
	if (! UpdateMarker::HasActiveChangeSource() )
		throwItemErrorF("Illegal attempt to change the {} of an endogenous item", changeWhat);
}

void TreeItem::AssertDataChangeRights(CharPtr changeWhat) const
{
	dms_check_not_debugonly;

	dms_assert(!IsCacheItem()); // PRECONDITION

	if (!HasConfigData())
		throwItemErrorF("Illegal attempt to change the {} of a calculatable item", changeWhat);
	if (IsStorable())
		return;
	if (IsLoadable()) // data is not derivable
		throwItemErrorF("Illegal attempt to change the {} of a loadable item", changeWhat);
	dms_assert(!IsDerivable()); // implied by !IsLoadable() && !HasCalculator() || HasConfigData, but MUTATING through HasCalculator
//	dms_assert(HasConfigSource()); // implied by !IsDerivable && !IsCacheItem(), but MUTATING through HasCalculator

//	AssertPropChangeRights(changeWhat);
	dms_assert(IsMetaThread());

	if ((! IsEndogenous()) || s_MakeEndoLockCount)
		return;
	if (! UpdateMarker::HasActiveChangeSource() )
		reportF(SeverityTypeID::ST_Warning, "Changing the {} of endogenous item {}", changeWhat, GetSourceName().c_str());
}

SharedPtr<const AbstrCalculator> TreeItem::GetCalculator() const
{
	MakeCalculator();
	return GetCalculatorMember();
}

void ApplyCalculator(TreeItem* holder, const AbstrCalculator* ac)
{
	// TODO G8: Re-evaluate types here; going to variant and back looks contrived
	auto metaInfo = ac->GetMetaInfo();
	if (metaInfo.index() == 0)
	{
		std::get<MetaFuncCurry>(metaInfo).operator()(holder, ac);
		assert(ac->GetHolder()->GetIsInstantiated() || ac->GetHolder()->WasFailed(FailType::MetaInfo));
	}
}

void TreeItem::MakeCalculator() const noexcept
{
	dms_check_not_debugonly;

	if (WasFailed(FailType::Determine))
		return;

	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < ProgressState::MetaInfo && !GetTreeParent()->WasFailed(FailType::MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	dms_assert(m_Parent.expired() || (m_Parent.lock()->m_State.GetProgress() >= ProgressState::MetaInfo) || m_Parent.lock()->WasFailed(FailType::MetaInfo));

	//	may only be called after HasCalculator (would) return(ed) true
//	dms_assert(!InTemplate() || (mc_Calculator && mc_Calculator->DelayDataControllerAccess()));
//	dms_assert(mc_Calculator || !mc_Expr.empty()); 
	if (mc_DC || GetIsInstantiated() || GetCalculatorMember())
		return;

	TreeItemContextHandle tich(this, "MakeCalculator"); FencedInterestRetainContext irc("MakeCalculator");

	if (m_State.Get(ASF_MakeCalculatorLock))
		return Fail(
			"Invalid Recursion in GetCalculator detected.\n"
			"Check calculation rule of this item"
			, FailType::Determine
		);
	auto_flag_recursion_lock<ASF_MakeCalculatorLock> lock(m_State);

	if ((GetExprMember().empty() && (IsCacheItem() || !IsUnit(this)))|| IsPassor())
		return;


	try {
		AbstrCalculatorRef newCalculator;
		if (!GetExprMember().empty())
			newCalculator = AbstrCalculator::ConstructFromStr(this, GetExprMember(), CalcRole::Calculator);
		SetCalculator(newCalculator);
	}
	catch (...)
	{
		return CatchFail(FailType::Determine);
	}
	if (WasFailed(FailType::Determine))
		return;
}

static void ReportItemType(const TreeItem* self, const TreeItem* refItem)
{
	auto msg = mySSPrintF("{}: ItemType {} is incompatible with the result of the calculation which is of type {}"
		, self->GetFullName().c_str()
		, self->GetDynamicObjClass()->GetName().c_str()
		, refItem->GetDynamicObjClass()->GetName().c_str()
	);

	reportF(SeverityTypeID::ST_Warning, msg.c_str());
}

static void FailItemType(const TreeItem* self, const TreeItem* refItem)
{
	auto msg = mySSPrintF("ItemType {} is incompatible with the result of the calculation which is of type {}"
	,	self->GetDynamicObjClass()->GetName().c_str()
	,	refItem->GetDynamicObjClass()->GetName().c_str()
	);
	self->Fail(msg, FailType::Determine);
}

// Arc, polygon and multipoint attributes all share the same DataItemClass (their value type is the
// common sequence-class, see UnitClass::GetValueType), so a declared composition that disagrees with
// the computed one passes the ItemType check unnoticed - e.g. an attribute declared (poly) but filled
// by points2sequence (which yields arc). Warn so users can make the configuration explicit about the
// intended composition (points2sequence for arc, points2polygon for poly); this is slated to become an
// error in a future GeoDms major version (issue #1038).
static void ReportResultCompositionDeprecation(const TreeItem* self, const AbstrDataItem* selfDi, const AbstrDataItem* refDi)
{
	auto msg = mySSPrintF(
		"{}: Depreciated: the declared ValueComposition '{}' differs from the '{}' of the calculation result.\n"
		"Make the configuration explicit about the intended composition "
		"(use points2sequence for arc and points2polygon for poly). "
		"This will become an error in a future GeoDms major version."
	,	self->GetFullName().c_str()
	,	GetValueCompositionID(selfDi->GetValueComposition()).AsSharedStr().c_str()
	,	GetValueCompositionID(refDi->GetValueComposition()).AsSharedStr().c_str()
	);
	reportD(SeverityTypeID::ST_Warning, msg.c_str());
}

bool TreeItem::_CheckResultObjType(const TreeItem* refItem) const
{
	assert(refItem);
	if (WasFailed(FailType::Determine))
		return false;
	try {
		if (IsDataItem(refItem) && !IsDataItem(this))
			ReportItemType(this, refItem);
		if (IsUnit(refItem) && !IsUnit(this))
			ReportItemType(this, refItem);

		if (refItem->GetDynamicObjClass()->IsDerivedFrom(GetDynamicObjClass()) )
		{
			// same DynamicObjClass but a different (arc/poly/multipoint) ValueComposition: deprecated, warn (#1038)
			if (IsDataItem(this) && IsDataItem(refItem))
			{
				auto selfDi = AsDataItem(this);
				auto refDi  = AsDataItem(refItem);
				auto refVC  = refDi->GetValueComposition();
				if (selfDi->GetValueComposition() != refVC)
				{
					ReportResultCompositionDeprecation(this, selfDi, refDi);
					// Adopt the computed composition: silences a second visit (dedup) and propagates the
					// actual composition to consumers of this item.
					const_cast<AbstrDataItem*>(selfDi)->SetValueComposition(refVC);
				}
			}
			return true;
		}
		FailItemType(this, refItem);
	}
	catch (...)
	{
		if (!WasFailed(FailType::Determine))
		{
			auto err = catchException(true);
			DoFailCaller(err, FailType::Determine);
		}
	}
	return false;
}

bool TreeItem::CheckResultItem(const TreeItem* refItem) const
{
	assert(refItem);
	return _CheckResultObjType(refItem);
}

// ============ GetRefItem

auto TreeItem::GetCurrRefItem() const noexcept -> std::shared_ptr<const TreeItem>
{
//	assert(Was(ProgressState::MetaInfo) || WasFailed() || IsPassor() || IsUnit(this) && AsUnit(this)->IsDefaultUnit());
	return mc_RefItem.lock();
}

auto TreeItem::GetReferredItem() const  noexcept -> std::shared_ptr<const TreeItem>
{
	assert(!SuspendTrigger::DidSuspend());
	if (auto parent = m_Parent.lock())
		parent->UpdateMetaInfo();

	if (!!mc_RefItem.expired() && HasCalculator())
		MakeCalculator();
				
	return mc_RefItem.lock();
}

auto TreeItem::GetCurrUltimateItem() const noexcept -> std::shared_ptr<const TreeItem>
{
	return _GetCurrUltimateItem(this);
}

auto TreeItem::GetCurrRangeItem() const noexcept -> std::shared_ptr<const TreeItem>
{
	return _GetCurrRangeItem(this);
}

auto TreeItem::GetUltimateItem() const noexcept -> std::shared_ptr<const TreeItem>
{
	UpdateMetaInfo();
	return _GetUltimateItem(this);
}

const TreeItem* TreeItem::GetSourceItem() const  noexcept
{
	const TreeItem* sourceItem = this;
	do
	{
		sourceItem = sourceItem->GetReferredItem().get();
	}
	while (sourceItem && sourceItem->IsCacheItem());
	assert(sourceItem != this);
	return sourceItem;
}

const TreeItem* TreeItem::GetUltimateSourceItem() const noexcept
{
	const TreeItem* item = this;
	const TreeItem* source;
	while ((source = item->GetSourceItem()))
		item = source;
	assert(item);
	return item;
}

const TreeItem* TreeItem::GetCurrSourceItem() const noexcept
{
	const TreeItem* sourceItem = this;
	do
	{
		sourceItem = sourceItem->mc_RefItem.lock().get();
	}
	while (sourceItem && sourceItem->IsCacheItem());

	return sourceItem;
}

const TreeItem* TreeItem::GetCurrUltimateSourceItem() const noexcept
{
	const TreeItem* item = this;
	const TreeItem* source;
	while ((source = item->GetCurrSourceItem()))
		item = source;
	dms_assert(item);
	return item;
}

// ============ SetRefItem
// Holds the OLD referred TreeItem (now std-owned via shared_tree_ptr) and decrements its interest count
// on destruction -- deferred settlement so the swap below can't drop interest mid-flight.
struct OldRefDecrementer
{
	std::shared_ptr<const TreeItem> m_Item;
	OldRefDecrementer& operator=(std::shared_ptr<const TreeItem> item) { m_Item = std::move(item); return *this; }
	explicit operator bool() const { return bool(m_Item); }
	const TreeItem* operator->() const { return m_Item.get(); }
	~OldRefDecrementer() {
		if (m_Item)
			m_Item->DecInterestCount();
	}
};

// Same deferred-settlement pattern for the OLD DataController (which stays an intrusive SharedActor).
struct OldDcInterestDecrementer : SharedPtr<const DataController>
{
	using SharedPtr::operator=;
	~OldDcInterestDecrementer() {
		if (has_ptr())
			get()->DecInterestCount();
	}
};

void TreeItem::SetReferredItem(const TreeItem* refItem) const
{
	assert(IsMetaThread() || !refItem);

	assert(!IsDataItem(this) || AsDataItem(this)->GetDataObjLockCount() <= 0); // DON'T MESS WITH SHARED-LOCKED ITEMS

	assert(refItem != this);
	assert(!refItem || !refItem->InTemplate());
	if (mc_RefItem.lock().get() == refItem)
		return;

#if defined(MG_DEBUG_INTERESTSOURCE_LOGGING)
	if (m_State.Get(actor_flag_set::AFD_PivotElem))
	{
		auto curr = refItem;
		while (curr) // propagate PivotElem to the ultimate item
		{
			curr->m_State.Set(actor_flag_set::AFD_PivotElem);
			curr = curr->mc_RefItem.lock().get();
		}
	}
	if (refItem && refItem->m_State.Get(actor_flag_set::AFD_PivotElem))
		m_State.Set(actor_flag_set::AFD_PivotElem);

#endif

	if (refItem && !_CheckResultObjType(refItem))
		refItem = nullptr;

	if (refItem && IsDataItem(this) && !IsCacheItem() && !refItem->IsCacheItem())
	{
		assert(IsDataItem(refItem));
		if (auto sp = GetStorageParent(true))
		{
			auto sm = sp->GetStorageManager();
			assert(sm);
			if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
			{
				// hack to get a cache item instead of a config-item in order to connect the DaraArray to the mmd storage
				auto keyExpr = ExprList(token::convert, refItem->GetCheckedKeyExpr(), AsDataItem(refItem)->GetValuesUnitOrThrow()->GetCheckedKeyExpr());
				SetDC(GetOrCreateDataController(keyExpr));
				return;
			}
		}
	}

	// remove the old interest
	OldRefDecrementer oldRefItemCounter;
	auto tmpRefItemHolder = MakeSharedFromBorrowedObjectPtr(refItem);
	TreeItemInterestPtr newRefItemCounter;

retry:
	if (m_InterestCount)
		newRefItemCounter = refItem; // calls IncInterestCount, which can throw, current interest might disappear concurrently
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection); // check and swap or try again
		if (m_InterestCount) // still interested?
		{
			assert(newRefItemCounter || !refItem); // Starting Interest only allowed from this main thread. 
			if (refItem && !newRefItemCounter) // situation had changed anyway
				goto retry;
			// point of certain return, prepare settlement upon destruction
			newRefItemCounter.release();
			oldRefItemCounter = mc_RefItem.lock(); // owning snapshot of the old ref; decrement interest count upon destruction
		}
		else
			newRefItemCounter = nullptr; // decrease new interest if current interest has been released concurrently
		// everything is OK now to do a swap of responsibilities

		auto oldRefItem = mc_RefItem.lock();
		if (oldRefItem && oldRefItem != tmpRefItemHolder && oldRefItem->m_BackRef.lock().get() == this)
			oldRefItem->m_BackRef.reset();
		mc_RefItem = std::move(tmpRefItemHolder);
		if (auto newRefItem = mc_RefItem.lock(); newRefItem && newRefItem->IsCacheItem() && newRefItem->m_BackRef.expired())
			newRefItem->m_BackRef = make_weak_tree(this);
	}

	assert(!oldRefItemCounter || oldRefItemCounter->GetInterestCount()); // will retain interest up to destruction, now privately owned.

	auto newRefItem = mc_RefItem.lock();
	if (!newRefItem)
		return;

	newRefItem->DetermineState();
	if (GetKeepDataState())
		const_cast<TreeItem*>(newRefItem.get())->SetKeepDataState(true); // LET OP: State is niet weggehaald bij vorige refItem (want er zijn misschien nog andere keepers)
	if (GetLazyCalculatedState())
		const_cast<TreeItem*>(newRefItem.get())->SetLazyCalculatedState(true); // LET OP: State is niet weggehaald bij vorige refItem (want er zijn misschien nog andere keepers)

	const UInt32 inheritedFlags = TSF_Depreciated | TSF_Categorical;
	m_StatusFlags.SetBits(inheritedFlags, newRefItem->m_StatusFlags.GetBits(inheritedFlags));
}

// ============ GetParent

[[nodiscard]] const PersistentObject* TreeItem::GetParent () const noexcept
{
	return GetTreeParent().get();
}

void TreeItem::SetInHidden(bool value)
{ 
	if (GetTSF(TSF_InHidden) != value)
	{
		SetTSF(TSF_InHidden, value);
		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetInHidden(value || subItem->GetTSF(TSF_IsHidden));
	}
}
void TreeItem::SetIsHidden(bool value)
{
	if (GetTSF(TSF_IsHidden) != value)
	{
		SetTSF(TSF_IsHidden, value);
		SetInHidden(
				value 
			|| (GetTreeParent() && GetTreeParent()->GetTSF(TSF_InHidden)));
	}
}

void TreeItem::SetInTemplate()
{ 
	if (!InTemplate())
	{
		SetTSF(TSF_InTemplate);
		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetInTemplate();
		dms_assert(!GetCalculatorMember());
		SetPassor();
	}
}

void TreeItem::SetIsTemplate()
{
	if (!IsTemplate())
	{
		SetTSF(TSF_IsTemplate);
		SetInTemplate();
	}
}

//----------------------------------------------------------------------
// user-defined function items ('function' keyword): template-like inert body
// plus a declared parameter count and a designated result sub-item, kept in a
// side-assoc (set by the config parser, read at call dispatch).
//----------------------------------------------------------------------

void TreeItem::SetIsFunction()
{
	SetIsTemplate(); // function bodies are inert like template bodies; evaluation happens per instantiation
	SetTSF(TSF_IsFunctionItem);
}


void TreeItem::SetKeepDataState(bool value)
{ 
	if (GetTSF(TSF_KeepData) != value)
	{
		SetTSF(TSF_KeepData, value);
		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetKeepDataState(value);
		if (!value)
		{
			// uti is empty when this item is mid-destruction (called from ~AbstrDataItem); the dtor frees its
			// memory anyway, so only clean up when the ultimate is still live.
			if (auto uti = _GetHistoricUltimateItem(this))
			{
				actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("TreeItem::SetKeepDataState") sg_ActorLockMap, uti.get()); // datalockcount 1->0 or drop of interest is
				uti->TryCleanupMem();
			}
		}
	}
	if (value)
		if (auto refItem = mc_RefItem.lock())
			const_cast<TreeItem*>(refItem.get())->SetKeepDataState(true);
}

void TreeItem::SetLazyCalculatedState(bool value)
{
	if (GetTSF(TSF_LazyCalculated) != value)
	{
		SetTSF(TSF_LazyCalculated, value);
		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetLazyCalculatedState(value);
	}
	if (value)
		if (auto refItem = mc_RefItem.lock())
			const_cast<TreeItem*>(refItem.get())->SetLazyCalculatedState(true);
}

void TreeItem::SetStoreDataState(bool value)
{ 
	if (GetStoreDataState() != value)
	{
		SetTSF(TSF_StoreData, value);

		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetStoreDataState(value);
	}
}

void TreeItem::SetFreeDataState(bool value)
{ 
	if (GetFreeDataState() != value)
	{
		SetTSF(TSF_FreeData, value);

		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetFreeDataState(value);
	}
}

SharedTreeItem TreeItem::GetStorageParent (bool alsoForWrite) const
{
	if (GetTSF(TSF_InTemplate | TSF_IsCacheItem))
		return {};
	const TreeItem* storageParent = this;
	do {
		if (storageParent->IsDisabledStorage())
			return {};
		if (storageParent->HasStorageManager())
		{
			if (alsoForWrite && storageParent->GetStorageManager()->IsReadOnly())
				return {};
			return MakeSharedFromBorrowedObjectPtr( storageParent );
		}
		storageParent = storageParent->m_Parent.lock().get();
	} while (storageParent);
	return {};
}

SharedTreeItem TreeItem::GetCurrStorageParent(bool alsoForWrite) const
{
	if (GetTSF(TSF_InTemplate | TSF_IsCacheItem))
		return {};
	const TreeItem* storageParent = this;
	do {
		if (storageParent->IsDisabledStorage())
			return {};
		auto sm = storageParent->GetCurrStorageManager();
		if (sm)
		{
			if (alsoForWrite && sm->IsReadOnly())
				return {};
			return MakeSharedFromBorrowedObjectPtr(storageParent);
		}
		storageParent = storageParent->m_Parent.lock().get();
	} while (storageParent);
	return {};
}

bool TreeItem::IsLoadable() const
{
	if (!IsDataItem(this) && !IsUnit(this)) 
		return false;
	auto sp = GetStorageParent(false);
	if (!sp)
		return false;
	auto sm = sp->GetStorageManager();
	if (sm && sm->IsWriteOnlyStorage())
		return false;
	return true;
}

/*
bool TreeItem::IsLoadableAndExists() const
{
	if (!IsDataItem(this) && !IsUnit(this))
		return false;
	auto sp = GetStorageParent(false);
	if (!sp)
		return false;

	auto sm = sp->GetStorageManager();
	assert(sm);

	return sm->DoCheckExistence(sp, this);
}
*/

bool TreeItem::IsCurrLoadable() const
{
	assert(m_Parent.expired() || m_Parent.lock()->Was(ProgressState::MetaInfo) || m_Parent.lock()->WasFailed());
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	auto sp = GetCurrStorageParent(false);
	if (!sp)
		return false;
	auto sm = sp->GetCurrStorageManager();
	assert(sm);

	return sm->DoCheckExistence(sp.get(), this);
}

bool TreeItem::IsStorable() const
{
	if (!IsDataItem(this) && !IsUnit(this)) 
		return false;
	auto storageParent = GetStorageParent(true);
	if (!storageParent || !storageParent->GetStorageManager()->IsWritable())
		return false;
	// see if any of the ancestors up to the storageParent has the storageReadOnly property
	const TreeItem* self = this;
	while (true) 
	{
		if (storageReadOnlyPropDefPtr->GetValue(self))
			return false;
		if (self == storageParent.get())
			return true;
		self = self->GetTreeParent().get();
		assert(self);
	}
}

bool TreeItem::IsCurrStorable() const
{
	if (!IsDataItem(this) && !IsUnit(this))
		return false;
	const TreeItem* storageParent = GetCurrStorageParent(true).get();
	if (!storageParent || !storageParent->GetStorageManager()->IsWritable())
		return false;
	// see if any of the ancestors up to the storageParent has the storageReadOnly property
	const TreeItem* self = this;
	while (true)
	{
		if (storageReadOnlyPropDefPtr->GetValue(self))
			return false;
		if (self == storageParent)
			return true;
		self = self->GetTreeParent().get();
		assert(self);
	}
}

void TreeItem::AddUsing(const TreeItem* nameSpace)
{
	GetUsingCache()->AddUsing(nameSpace);
}

void TreeItem::AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace)
{
	if (firstNameSpace != lastNameSpace)
		GetUsingCache()->AddUsings(firstNameSpace, lastNameSpace);
}

UsingCache* TreeItem::GetUsingCache()
{
	if (!m_UsingCache)
		m_UsingCache = std::make_unique<UsingCache>(this);
	return m_UsingCache.get();
}

const UsingCache* TreeItem::GetUsingCache() const
{
	if (!m_UsingCache)
		m_UsingCache = std::make_unique<UsingCache>(this);
	return m_UsingCache.get();
}

void TreeItem::RemoveFromConfig() const
{
	assert(!IsCacheItem());
	auto self = const_cast<TreeItem*>(this);
	assert(self);
	// Ownership is downward now: detaching from the owning parent releases (and, if this was the last
	// reference, destroys) the item. A config root has no parent and is released from its session.
	auto parent = GetTreeParent();
	if (parent)
		const_cast<TreeItem*>(parent.get())->RemoveItem(self);
	else
		self->EnableAutoDelete();
}

void TreeItem::AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd)
{
	GetUsingCache()->AddUsingUrls(urlsBegin, urlsEnd);
}

void TreeItem::AddUsingUrl(TokenID url)
{
	GetUsingCache()->AddUsingUrl(url);
}

void TreeItem::ClearNamespaceUsage()
{
	if (m_UsingCache)
		m_UsingCache->ClearUsings(true);
}

void TreeItem::ResetNamespaceUsage(bool includeImplicitParent, const TreeItem* definitionNamespace)
{
	// Re-seat the fixed usings of the cache we already have; never swap the cache object
	// itself. See UsingCache::ResetFixedUsings for why replacing it loses the parent
	// namespace of every sub-item that already registered as incoming.
	GetUsingCache()->ResetFixedUsings(includeImplicitParent, definitionNamespace);
}

UInt32 TreeItem::GetNrNamespaceUsages() const
{
	return m_UsingCache
		? m_UsingCache->GetNrUsings()
		: 0;
}

const TreeItem* TreeItem::GetNamespaceUsage(UInt32 i) const
{
	dms_assert(m_UsingCache);
	return m_UsingCache->GetUsing(i);
}

bool TreeItem::IsDataReadable() const
{
	bool isLoadable = IsLoadable();
	if (!isLoadable)
		return false;
	bool hasCalculator = HasCalculatorImpl();
	if (hasCalculator)
		return false;
	bool hasConfigData = HasConfigData();
	return !hasConfigData;
}


static bool HasOwnCalculatorNow(TreeItem* result)
{
	dms_assert(result);
	return (!result->GetExprMember().empty()) || (result->GetCalculatorMember() && result->GetCalculatorMember()->IsDataBlock());
}

SharedMutableTreeItem TreeItem::Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const
{
	const Class* cls = GetDynamicClass();

	assert(dest || !id);
	bool isNew = (!dest) || (id && !dest->GetSubTreeItemByID(id));
	if (isNew && copyContext.DontCreateNew())
		return {};
	auto result = dest->CreateItem(id, cls);
	if (isNew)
	{
		if (copyContext.MustMakePassor())
			result->SetPassor();
	}

	assert(result);
//	result->m_State.Clear(ASF_DataReadableDefined);

	bool mustCopyProps = true;
	bool dstIsRoot = (copyContext.m_DstRoot == nullptr);

	assert(copyContext.m_SrcRoot);
	bool isArg = (copyContext.m_ArgList)
		&& (GetTreeParent().get() == copyContext.m_SrcRoot);

	if (dstIsRoot)
	{
		assert(!isArg);
		assert(dest == copyContext.m_DstContext || copyContext.m_DstContext == nullptr);
		copyContext.m_DstRoot = result.get();
		mustCopyProps = copyContext.MustCopyRoot();
	}

	if (copyContext.MustCopyExpr() && !isArg)
	{
		result->AssertPropChangeRights(USING_NAME);
		bool srcIsFunction = IsFunctionItem();
		bool isFunctionInstantiation = srcIsFunction && dstIsRoot;

		// Build the correct fixed namespace base directly: an explicit function
		// instantiation has no call-site parent, while both function and template
		// instantiations search their definition namespace. Declared usings are added
		// afterwards and therefore have higher precedence.
		if (dstIsRoot)
			result->ResetNamespaceUsage(!isFunctionInstantiation, GetTreeParent().get());
		else
			result->ClearNamespaceUsage();

		UInt32 nrNameSpaces = GetNrNamespaceUsages();
		if (nrNameSpaces)
		{
			VectorOutStreamBuff nameSpaceBuffer;
			FormattedOutStream nameSpaceStream(&nameSpaceBuffer, FormattingFlags::None);

			//	Now, copy all namespaces.
			//	Note that namespaces may not be circular (requirement of FindItem)
			//	GetItem follows a relative or absolute path directly
			//	FindItem calls GetItem on this and throws in case of failure
			for (UInt32 i1 =0; i1 != nrNameSpaces; ++i1)
			{
				const TreeItem* sns = GetNamespaceUsage(i1);
				// the parent/ancestor skips exist because instances reach ancestors through
				// the injected definition-parent namespace; function imports are kept
				// verbatim (frozen absolute) -- a redundant entry is harmless
				if (sns && sns != GetTreeParent().get() && (srcIsFunction || !sns->DoesContain(this)))
				{
					if (nameSpaceBuffer.CurrPos())
						nameSpaceStream << ';';
					if (srcIsFunction)
						nameSpaceStream << sns->GetFullName(); // freeze imports absolute: resolved at the definition site
					else
						nameSpaceStream << copyContext.GetAbsOrRelNameID(sns, this, dest).GetStrLock().c_str();
					assert(nameSpaceBuffer.GetData()[nameSpaceBuffer.CurrPos()-1] != ';');
				}
			}
			CharPtr dataBegin = nameSpaceBuffer.GetData();
			if (dataBegin)
			{
				assert(nameSpaceBuffer.CurrPos());
				result->AddUsingUrls(dataBegin, dataBegin+nameSpaceBuffer.CurrPos());
			}
		}
	}
	if (InTemplate())
		result->mc_OrgItem = make_weak_tree(this);

	if (IsFunctionItem() && !dstIsRoot)
	{
		// a function definition copied as part of a larger subtree (e.g. inside an
		// instantiated template) stays a function with its declared specification
		result->SetIsFunction();
		TreeItem_CopyFunctionSpec(result.get(), this);
	}
	//	Now, call the virtual CopyProps func to let the derived class do some work

	assert(mustCopyProps || dstIsRoot || !copyContext.InFenceOperator());
	if (mustCopyProps)
	{
		if (isArg || (copyContext.SetInheritFlag() && !HasOwnCalculatorNow(result.get())))
			result->SetTSF(TSF_InheritedRef);

		if (GetTSF(TSF_Categorical))
			result->SetTSF(TSF_Categorical);

		CopyProps(result.get(), copyContext);

		//	Now, copy data if requested
		if (isArg)
		{
			assert(!dest->InTemplate());
			assert(copyContext.m_ArgList.IsRealList());
			assert(!copyContext.InFenceOperator());

			result->SetCalculator(AbstrCalculator::ConstructFromLispRef(result.get(), copyContext.m_ArgList.Left(), CalcRole::ArgCalc));
			result->SetIsHidden(true);
			assert(result.get() != copyContext.m_DstRoot);
			assert(copyContext.m_DstRoot != nullptr);
			copyContext.m_ArgList = copyContext.m_ArgList.Right();
			return result; // don't copy subItems from this to result (take them from arg)
		}
	}
	if (copyContext.MustUpdateMetaInfo())
		UpdateMetaInfo();
	if (mustCopyProps)
	{
		if (isNew && copyContext.MergeProps())
			result->DisableStorage();

		CopyPropsContext(result.get(), this, copyContext.MinCpyMode(dstIsRoot), !copyContext.MergeProps()).Apply();
		if (!result->m_Location)
			result->m_Location = this->m_Location;

		if (!copyContext.InFenceOperator())
		{

			if (!copyContext.MustCopyExpr())
			{
				// subItems van referees dmv case-parameter value of gewoon expr-ref. aangeroepen vanuit UpdateMetaInfo 
				// Case-Parameter := itemRef OF result of compound-expr  (met DC_Ptr)
				if (!result->GetCalculatorMember())
					result->SetCalculator(CreateCalculatorForTreeItem(result.get(), this, copyContext));
			}
			else
			{
				if (GetCalculatorMember() && GetCalculatorMember()->IsDataBlock())
					result->SetCalculator(AbstrCalculator::ConstructFromDBT(AsDataItem(result.get()), GetCalculatorMember().get()));
			}
		}
	}	// if (mustCopyProps)

	// Now, copy all sub-items
	if (!copyContext.DontCopySubItems())
	{
		for (const TreeItem* subItem = GetCurrFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			if (!copyContext.InFenceOperator() || !subItem->IsTemplate())
				subItem->Copy(result.get(), subItem->GetID(), copyContext); // copied item is owned by `result`; drop the returned co-owning temporary

		// Now, copy from refItem; maybe more sub-items should be copied
		if (copyContext.CopyReferredItems())
		{
			AnchestorStackGuard guard(copyContext, result, make_shared_tree(this, existing_obj{}));

			const TreeItem* refItem = GetCurrRefItem().get();
//			copyContext.m_Dcm = DataCopyMode(copyContext.GetDCM() | DataCopyMode::DontUpdateMetaInfo);
			//		if (refItem)
			//			CopyTreeContext(result, refItem, "", DataCopyMode(copyContext.GetDCM()|DataCopyMode::NoRoot) ).Apply();

			while (refItem)
			{
				if (auto resultRef = copyContext.FindAnchestor(refItem))
				{
					result->SetReferredItem(resultRef);
					break;
				}
				for (const TreeItem* subItem = refItem->GetCurrFirstSubItem(); subItem; subItem = subItem->GetNextItem())
				{
					if (!copyContext.InFenceOperator() || !subItem->IsTemplate())
					{
						auto subID = subItem->GetID();
						if (result->GetSubTreeItemByID(subID) == nullptr)
							subItem->Copy(result.get(), subItem->GetID(), copyContext); // copied item is owned by `result`; drop the returned co-owning temporary
					}
				}
				refItem = refItem->GetCurrRefItem().get();
			}

		}
	}
	return result;
}

void TreeItem::Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const
{}

void TreeItem::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	if (copyContext.InFenceOperator())
		return;

	result->SetTSF(TSF_HasConfigData, HasConfigData() );
}

SharedStr TreeItem::GetSignature() const
{
	static SharedStr containerStr("container");
	return containerStr;
}


const TreeItem* TreeItem::GetFirstVisibleSubItem() const noexcept
{
	const TreeItem* subItem = GetFirstSubItem(); // calls UpdateMetaInfo
	if (subItem)
		return subItem;
	if (auto refItem = mc_RefItem.lock())
		return refItem->GetFirstVisibleSubItem();
	return nullptr;
}


const TreeItem* TreeItem::GetNextVisibleItem() const noexcept
{
	const TreeItem* nextItem = GetNextItem();
	if (nextItem)
		return nextItem;
	auto parent = m_Parent.lock();
	if (!parent)
		return nullptr;
	nextItem = parent->mc_RefItem.lock().get();
	dms_assert(nextItem != parent.get());
	if (!nextItem)
		return nullptr;
	return nextItem->GetFirstVisibleSubItem();
}

const TreeItem* TreeItem::WalkConstSubTree(const TreeItem* curr) const noexcept // this acts as subTreeRoot
{
	if (!curr) 
		return this;
	if (curr->HasSubItems())
		return curr->GetFirstSubItem();
	while (curr && curr != this)
	{
		const TreeItem* next = curr->GetNextItem();
		if (next)
			return next;
		curr = curr->GetTreeParent().get();
	}
	return nullptr;
}

auto TreeItem_VisitConstVisibleSubTree(const TreeItem * self, const ActorVisitor& visitor, TreeItemSet& visitedItems) -> ActorVisitState
{
	auto [_1, isNewItem] = visitedItems.insert(self);
	if (!isNewItem)
		return ActorVisitState::AVS_Ready;

	// Iterative DFS. Each frame holds the current link in a TreeItem's
	// refItem-chain plus the next subItem to visit at that link, and a
	// per-frame name-dedup set (matching the local std::unordered_set in the
	// recursive form). Keeps stack usage O(1) in subtree depth.
	struct frame_t
	{
		const TreeItem* refChain;     // current link in the refItem-chain of the frame's owner
		const TreeItem* nextSubItem;  // next subItem to visit at refChain
		std::unordered_set<TokenID> visitedSubItemNames;
	};
	std::vector<frame_t> stack;
	stack.push_back({self, self->GetFirstSubItem(), {}});

	assert(!SuspendTrigger::DidSuspend());

	while (!stack.empty())
	{
		auto& f = stack.back();

		// Advance past empty refItem-chain links until we find a subItem or run out.
		while (!f.nextSubItem && f.refChain)
		{
			f.refChain = f.refChain->GetReferredItem().get();
			f.nextSubItem = f.refChain ? f.refChain->GetFirstSubItem() : nullptr;
		}

		if (!f.nextSubItem)
		{
			stack.pop_back();
			continue;
		}

		const TreeItem* subItem = f.nextSubItem;
		f.nextSubItem = subItem->GetNextItem();

		auto [_2, isNewSubItem] = f.visitedSubItemNames.insert(subItem->GetID());
		if (!isNewSubItem)
			continue;
		if (SuspendTrigger::DidSuspend())
			return ActorVisitState::AVS_SuspendedOrFailed;
		if (visitor(subItem) == ActorVisitState::AVS_SuspendedOrFailed)
			return ActorVisitState::AVS_SuspendedOrFailed;
		assert(!SuspendTrigger::DidSuspend());

		// Equivalent of TreeItem_VisitConstVisibleSubTree(subItem, visitor, visitedItems):
		// the recursive call's first action is the visited-items insert/early-return,
		// inlined here so we only push a frame when we'd actually descend.
		auto [_3, isNewSubTreeItem] = visitedItems.insert(subItem);
		if (isNewSubTreeItem)
			stack.push_back({subItem, subItem->GetFirstSubItem(), {}});
	}
	return ActorVisitState::AVS_Ready;
}

auto TreeItem::VisitConstVisibleSubTree(const ActorVisitor& visitor) const -> ActorVisitState
{
	TreeItemSet visitedSet;
	return TreeItem_VisitConstVisibleSubTree(this, visitor, visitedSet);
}

TreeItem* TreeItem::WalkNext(TreeItem* curr) noexcept // this acts as subTreeRoot
{
	while (curr && curr != this)
	{
		TreeItem* next = curr->GetNextItem();
		if (next)
			return next;
		curr = const_cast<TreeItem*>(curr->GetTreeParent().get());
	}
	return nullptr;
}

// don't call UpdateMetaInfo for when you are in destructor / stop / nothrow land.
TreeItem* TreeItem::WalkCurrSubTree(TreeItem* curr) noexcept // this acts as subTreeRoot
{
	assert(curr);
	if (curr->_HasSubItems())
		return curr->_GetFirstSubItem();
	return WalkNext(curr);
}

ActorVisitState TreeItem::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	assert(!SuspendTrigger::DidSuspend()); // precondition

	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < ProgressState::MetaInfo && !GetTreeParent()->WasFailed(FailType::MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	dms_assert(!GetTreeParent() || GetTreeParent()->m_State.GetProgress() >= ProgressState::MetaInfo || GetTreeParent()->WasFailed(FailType::MetaInfo)); // precondition

	// =============== Parent
	if (Test(svf, SupplierVisitFlag::Parent) && GetTreeParent())
	{
		if (visitor(GetTreeParent().get()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}

	if (InTemplate())
		return AVS_Ready;

	// =============== TemplateOrg

	if (Test(svf,  SupplierVisitFlag::TemplateOrg))
	{
		if (auto orgItem = mc_OrgItem.lock())
			if (visitor(orgItem.get()) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
	}

	// =============== implicit suppliers from indirected properties

	if (Test(svf, SupplierVisitFlag::ImplSuppliers))
		if (VisitImplSupplFromIndirectProps(svf, visitor, this) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

	// =============== CDF and DIALOGDATA reference

	if (Test(svf, SupplierVisitFlag::CDF))
	{
		auto cdfItem = GetCdfAttr(this);
		if (cdfItem && visitor(cdfItem) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}
	if (Test(svf, SupplierVisitFlag::DIALOGDATA))
	{
		auto dialogData = dialogDataPropDefPtr->GetValue(this);
		if (!dialogData.empty())
		{
			auto dialogDataItem = ResolveItemPath(dialogData.AsRange());
			if (dialogDataItem && visitor(dialogDataItem.get()) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
	}
	assert(!SuspendTrigger::DidSuspend()); // precondition

	// =============== look for explicit suppliers

	if (Test(svf, SupplierVisitFlag::ExplicitSuppliers) && HasSupplCache())
	{
		auto n = GetSupplCache()->GetNrConfigured(this); // only ConfigSuppliers, Implied suppliers come after this, Calculator & StorageManager have added them
		for  (decltype(n) i = 0; i < n; ++i)
		{
			auto supplier = GetSupplCache()->begin(this)[i];
			assert(!SuspendTrigger::DidSuspend()); // precondition
			if (!supplier)
				continue;
			 if (visitor(supplier.get()) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;

			assert(!SuspendTrigger::DidSuspend()); // precondition
			auto supplTI = debug_cast<const TreeItem*>(supplier.get()); // all configured suppliers are TreeItems; all implied suppliers are AbstrCalculators
			if (supplTI->VisitConstVisibleSubTree(visitor) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
	}

	assert(!SuspendTrigger::DidSuspend()); // precondition
	// Ask ParseResult for suppliers

	// =============== m_Calculator related
	if (Test(svf, SupplierVisitFlag::DetermineCalc))
		MakeCalculator(); // sets mc_Calculator, mc_DC, and mc_RefItem;

	if (GetCalculatorMember() && Test(svf, SupplierVisitFlag::NamedSuppliers))
	{
		if (GetCalculatorMember()->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}

	// Not while this item is DETERMINING its state. UpdateDC builds mc_DC, and building it folds
	// the applicable IntegrityChecks into its key expression (#1180/#1218) -- a walk over this
	// item's ancestors AND its ExplicitSuppliers, which has to resolve configured supplier names.
	// DetermineState runs BEFORE any of that is available: it is the change-timestamp scan, ahead
	// of UpdateSupplMetaInfo, so the supplier names of this item's ancestors and suppliers need
	// not resolve yet. Deferring the build to the meta-info phase puts it after
	// Actor::UpdateMetaInfo's UpdateSupplMetaInfo(), where they do. A DC that already exists is
	// still visited below, so a later DetermineState pass sees it as before.
	if (Test(svf, SupplierVisitFlag::DetermineCalc) && !m_State.IsDeterminingState())
		UpdateDC();

	if (mc_DC)
	{
		if (Test(svf, SupplierVisitFlag::DataController))
			if (visitor.Visit(mc_DC.get()) != AVS_Ready)
				return AVS_SuspendedOrFailed;
		if (Test(svf, SupplierVisitFlag::SourceData))
		{
			const TreeItem* sourceItem = GetSourceItem();
			assert(!sourceItem || sourceItem != this);
			if (visitor.Visit(sourceItem) != AVS_Ready)
				return AVS_SuspendedOrFailed;
		}
	}
	if (auto refItem = mc_RefItem.lock())
	{
		if (Test(svf, SupplierVisitFlag::SourceData))
			if (visitor(refItem.get()) != AVS_Ready)
				return AVS_SuspendedOrFailed;
	}

	// =============== StorageManager of parent

	auto storageParent = GetStorageParent(false);

	// implied DC for indirect storageNames and sqlString prop are set by VisitNextSupplier
	if (storageParent)
	{
		if (auto sm = storageParent->GetStorageManager(false))
			if (sm->VisitSuppliers(svf, visitor, storageParent.get(), this) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
	}

	// =============== IntegrityChecker

	// #1180: an IntegrityCheck guards everything below the item carrying it, so the checks of
	// this item AND of all its ancestors are suppliers here: using a nested item has to evaluate
	// them, not only requesting the ancestor itself. A failing ancestor check thereby fails its
	// descendants, which is what makes a guard on a storage holder or a sub-table root mean
	// anything for the items read through it.
	//
	// An ancestor check that reaches into its own subtree closes a cycle this way and is reported
	// as a circular dependency. It already was one whenever the ancestor itself was requested, so
	// it could never be relied upon; the same holds for the condition elements of an indirect
	// expression. Restricting units defined OUTSIDE the subtree -- what the .mmd dictionary guards
	// of #1154 do -- needs no reference into it and so never closes a cycle.
	if (Test(svf, SupplierVisitFlag::Checker))
	{
		const TreeItem* guardian = this;
		SharedTreeItem guardianHolder; // keeps the ancestor alive while its checker is visited
		while (guardian)
		{
			if (guardian->HasIntegrityChecker())
			{
				auto ic = guardian->GetIntegrityChecker();

				// #1197: a check reached as a SUPPLIER is instantiated here, and that is the path a
				// generated check takes -- the restriction an .mmd dictionary carries (#1195), which
				// the modeller never wrote and cannot see. Measured: without this the failure reads
				// "eq Error: Cannot find operator for these arguments" against the item being read,
				// with nothing naming the check. The other site, in UpdateDC, covers a check folded
				// into the item's own DataController; both are needed.
				auto buildContext = MakeLCH([guardian]() -> SharedStr
					{
						return mySSPrintF("while building the IntegrityCheck {}", TreeItem_IntegrityCheckText(guardian));
					}
				);

				if (ic->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
					return AVS_SuspendedOrFailed;

				auto dc = MakeResult(ic.get());
				if (dc->WasFailed(FailType::MetaInfo))
				{
					Fail(dc.get());
		//			return AVS_SuspendedOrFailed;
				}
				if (visitor.Visit(dc.get()) != AVS_Ready)
					return AVS_SuspendedOrFailed;
				if (visitor.Visit(dc->GetOld()) == AVS_SuspendedOrFailed)
					return AVS_SuspendedOrFailed;
			}
			guardianHolder = guardian->GetTreeParent();
			guardian = guardianHolder.get();
		}
	}

	return base_type::VisitSuppliers(svf, visitor);
}


void TreeItem_RemoveDC(const TreeItem* self)
{
	assert(self);
	assert(!self->IsCacheItem());
	assert(IsMetaThread());

	if (!self->mc_DC)
		return;

	OldDcInterestDecrementer oldDcInterestCounter;
	DataControllerRef oldDC;
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection); // check and swap or try again
		if (self->GetInterestCount()) // still interested?
		{
			// point of certain return, prepare settlement upon destruction
			oldDcInterestCounter = self->mc_DC.get(); // decrement interest count upon destruction
		}
		oldDC = std::move(self->mc_DC);
		assert(!self->mc_DC);
	}
}

void TreeItem::DoInvalidate() const
{
	assert(!IsCacheItem());
	assert(IsMetaThread());

	// m_State Has already been set to US_Invalidated before DoInvalidate gets called 
	if (m_SupplCache)
		m_SupplCache->Reset();

	SetReferredItem(nullptr);

	if (IsCacheItem())
		const_cast<TreeItem*>(this)->DropValue();

	if (m_ReadAssets.has_value())
		m_ReadAssets.Clear();
	ClearTSF(TSF_ReadAssetsInterestScoped); // keep the interest-scoped marker in sync with m_ReadAssets

	m_StatusFlags.Clear(TSF_DataInMem);

	ResetIntegrityCheckerMember();
	if (m_ConfigProperties) // #1218: derived from the same config state as the checker and the SupplCache reset above
		m_ConfigProperties->mc_CheckGuardians.reset();

	TreeItem_RemoveDC(this);
	if (!GetExprMember().empty())
	{
		ResetCalculatorMember();
		for (auto subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
		{
			if (subItem->GetCalculatorMember() && subItem->GetCalculatorMember()->IsDcPtr()) // reflection of composite result component?
			{
				subItem->ResetCalculatorMember();
				TreeItem_RemoveDC(subItem);
			}
		}
	}

	Actor::DoInvalidate(); // StartSupplInterest, which might recollect mc_Calculator, mc_IntegrityChecker, mc_RefItem, ClearFail
	// =============== invalidate Parts (of cache items)
	NotifyStateChange(this, NC2_Invalidated);


	dms_assert(DoesHaveSupplInterest() || !GetInterestCount() || IsPassor() || WasFailed(FailType::Data));
}

void TreeItem::SetDataChanged()
{
	if (IsDataItem(this))
		ClearTSF(DSF_ValuesChecked);

//	m_State.Clear(ASF_DataReadable);
//	m_State.Set  (ASF_DataReadableDefined);

#if defined(MG_DEBUG_TS_SOURCE)
	UpdateMarker::ChangeSourceLock::CheckActivation(m_LastChangeTS, "SetDataChanged");
#endif
	SetProgressAt(ProgressState::MetaInfo, UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE(mySSPrintF("SetDataChanged({})", GetFullName().c_str()).c_str()) ) );  // new data not validated nor committed
}

garbage_can TreeItem::DropValue()
{
	MG_LOCKER_NO_UPDATEMETAINFO

	garbage_can garbageCan;
	ClearDataObject(garbageCan); // Resets m_SegsPtr (DoClearData) and resets TSF_DataInMem
	return garbageCan;
}

TimeStamp TreeItem::DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const // noexcept
{
	assert(IsMetaThread());
	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < ProgressState::MetaInfo && !GetTreeParent()->WasFailed(FailType::MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	// postcondition of UpdateMetaInfo
	assert(!GetTreeParent() || GetTreeParent()->m_State.GetProgress() >= ProgressState::MetaInfo || GetTreeParent()->WasFailed(FailType::MetaInfo)); 

	TimeStamp lastChangeTS = 0; // DataStoreManager::GetCachedConfigSourceTS(this);
	if (!lastChangeTS
#if defined(MG_ITEMLEVEL)
		|| m_ItemLevel == item_level_type(0)
#endif
		)
		lastChangeTS = Actor::DetermineLastSupplierChange(failReason, ft);

	// Track changes in authentic sources
//REMOVE	// make sure not to pass changes because item was already last
	if ((ft == FailType::None) && IsDataReadable() && !WasFailed(FailType::Determine))
	{
		try {
			assert(!IsCacheItem());
			auto storageParent = GetStorageParent(false); if (!storageParent) goto exit;
			assert(storageParent);

			AbstrStorageManager* sm = storageParent->GetStorageManager(); if (!sm) goto exit;
			assert(sm);

			FileDateTime lastFileChange = sm->GetCachedChangeDateTime( storageParent.get(), GetRelativeName(storageParent.get()).c_str() );

			if (lastFileChange)
			{
//				MakeMax(lastChangeTS, SessionData::Curr()->DetermineExternalChange(lastFileChange) );
				assert( UpdateMarker::CheckTS(lastChangeTS) );
			}
		}
		catch (...)
		{
			failReason = catchException(false);
			ft         = FailType::Determine;
		}
	}
exit:
	return lastChangeTS;
}

SharedStr TreeItem::GetSourceName() const
{
	SharedStr inhSN = base_type::GetSourceName();

//	if (!GetConfigFileLineNr())
	return inhSN;
/*
	return mySSPrintF("{}({},{}): {}"
	,	ConvertDmsFileNameAlways(GetConfigFileName())
	,	GetConfigFileLineNr()
	,	GetConfigFileColNr()
	,	inhSN
	);
*/
}

bool TreeItem::DoFail(ErrMsgPtr msg, FailType ft) const
{
	if (!IsCacheItem())
		msg->TellWhere(this);

	if (!Actor::DoFail(msg, ft))
		return false;

	if (IsCacheItem()) {
		auto si = _GetFirstSubItem();
		while (si)
		{
			si->DoFailCaller(msg, ft);
			si = si->GetNextItem();
		}
	}
	NotifyStateChange(this, NotificationCodeFromProblem(ft));
	return true;
}

//----------------------------------------------------------------------
// TreeItem SetStorageManger Functions
//----------------------------------------------------------------------

void TreeItem::SetStorageManager(AbstrStorageManager* storageManager)
{
	if (m_StorageManager == storageManager)
		return;
	if (!GetTreeParent())
		throwItemErrorF(
			"StorageManager '{}' on root item is not allowed;\n"
			"move StorageName property to the relevant subItems",
			storageManager->GetName()
		);
	m_StorageManager = storageManager;
	ClearTSF(TSF_DisabledStorage);
}

bool TreeItem::HasStorageManager() const 
{ 
	return m_StorageManager 
		||	(	!IsCacheItem()
			&&	!InTemplate()
			&&	!IsDisabledStorage() 
			&&	storageNamePropDefPtr->HasNonDefaultValue(this)
			);
}

AbstrStorageManager* TreeItem::GetStorageManager(bool throwOnFailure) const
{ 
	if (!m_StorageManager)
	{
		assert(IsMetaThread());

		if (m_State.Get(ASF_GetStorageManagerLock))
		{
			throwItemError(
				"Invalid Recursion detected in GetStorageManager.\n"
				"Check the storage definition rule and other referring properties of this item and/or its SubItems"
			);
		}
		auto_flag_recursion_lock< ASF_GetStorageManagerLock, true> lockit(m_State);

		assert(HasStorageManager()); // prcondition: GetStorageManager may only be called when HasStorageManager() returns true
		assert(!IsCacheItem());        // implied by HasStorageManager()
		assert(!InTemplate());         // implied by HasStorageManager()
		assert(!IsDisabledStorage());  // implied by HasStorageManager()
		assert(storageNamePropDefPtr->HasNonDefaultValue(this));

		SharedStr storageName = TreeItemPropertyValue(this, storageNamePropDefPtr);
		auto sm = AbstrStorageManager::Construct(this
			, storageName
			, storageTypePropDefPtr->GetValue(this)
			, storageReadOnlyPropDefPtr->HasNonDefaultValue(this) 
				? storageReadOnlyPropDefPtr->GetValue(this) ? StorageReadOnlySetting::ReadOnly : StorageReadOnlySetting::ReadWrite
				: StorageReadOnlySetting::Default
			, throwOnFailure
		);

		assert(sm || !throwOnFailure); // guaranteed by AbstrStorageManager::Construct
		if (sm)
			const_cast<TreeItem*>(this)->SetStorageManager(sm.get()); // resets m_DisabledStorage
	}
	assert(m_StorageManager || !throwOnFailure);
	return m_StorageManager.get();
}

void TreeItem::SetStorageManager(CharPtr storageName, CharPtr storageType, StorageReadOnlySetting readOnly, CharPtr driver, CharPtr options)
{
	DBG_START("TreeItem", "SetStorageManager", false);
	storageNamePropDefPtr->SetValue(this, SharedStr(storageName MG_DEBUG_ALLOCATOR_SRC("TreeItem::SetStorageManager storageName")) );
	storageTypePropDefPtr->SetValue(this, storageType ? GetTokenID_mt(storageType) : TokenID::GetEmptyID() );
	if (readOnly != StorageReadOnlySetting::Default)
		storageReadOnlyPropDefPtr->SetValue(this, readOnly == StorageReadOnlySetting::ReadOnly);
	else if (storageReadOnlyPropDefPtr->HasNonDefaultValue(this))
		storageReadOnlyPropDefPtr->RemoveValue(this);

	if (driver != nullptr)
		storageDriverPropDefPtr->SetValue(this, SharedStr(driver MG_DEBUG_ALLOCATOR_SRC("TreeItem::SetStorageManager driver")));
	if (options != nullptr)
		storageOptionsPropDefPtr->SetValue(this, SharedStr(options MG_DEBUG_ALLOCATOR_SRC("TreeItem::SetStorageManager options")));
	SetStorageManager(nullptr);

	Invalidate();
}

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID)
{
	assert(searchLoc);
	assert(!subItemID.empty());
	assert(GetTokenStrLock(subItemID).c_str()[0] != '.');

	while (searchLoc) {
		if (searchLoc->m_UsingCache)
			return searchLoc->m_UsingCache->FindItem(subItemID).get();

		auto foundSubItem = searchLoc->GetConstSubTreeItemByID(subItemID);
		if (foundSubItem)
			return foundSubItem.get();

		searchLoc = searchLoc->GetTreeParent().get();
	};
	return nullptr;
}

//----------------------------------------------------------------------
//	InterestCount management
//----------------------------------------------------------------------

//mc_IntegrityCheckTiles

#if defined(MG_DEBUG_DATASTORELOCK)
UInt32 sd_ItemInterestCounter = 0;
#endif

void TreeItem::StartInterest() const
{
	assert(!std::uncaught_exceptions());
	if (!s_SessionUsageCounter.try_lock_shared())
	{
		assert(CancelableFrame::CurrActive());
		CancelOrThrow(this);
	}
	auto unlockSessionUsageCounter = make_releasable_scoped_exit([]() { s_SessionUsageCounter.unlock_shared(); });
	UpdateMetaInfo();
	dms_assert(GetInterestCount() == 0);

	SharedTreeItemInterestPtr parentHolder = GetTreeParent();

	Actor::StartInterest(); // -> StartSupplInterest() -> VisitSuppl -> UpdateDC -. SetReferredItem

	auto undoActorInterest = make_releasable_scoped_exit([this]() { this->Actor::StopInterest();; });

	SharedTreeItem            refItem = mc_RefItem.lock(); // lock the weak back-ref to an owning ptr for the duration
	SharedActorInterestPtr    calcHolder = mc_DC.get_ptr();
	SharedTreeItemInterestPtr refItemHolder = refItem;

	auto storageParent = GetStorageParent(false);
	if (storageParent)
	{
		if (auto nmsm = dynamic_cast<NonmappableStorageManager*>(storageParent->GetStorageManager()))
			nmsm->StartInterest(storageParent.get(), this);

	}

	// nothrow from here
	undoActorInterest.release();

	// nothrow from here, avoid rollbacks and release the InterestHolders without releasing the interest
	parentHolder.release();
	refItemHolder.release();
	calcHolder.release();
	unlockSessionUsageCounter.release();
#if defined(MG_DEBUG_DATASTORELOCK)
	++sd_ItemInterestCounter;
#endif
}

garbage_can TreeItem::StopInterest() const noexcept
{
	auto storageParent = GetCurrStorageParent(false);
	if (storageParent)
		if (auto nmsm = dynamic_cast<NonmappableStorageManager*>(storageParent->GetStorageManager()))
			nmsm->StopInterest(storageParent.get(), this);

	auto garbage = Actor::StopInterest();

	if (GetTreeParent())
		garbage |= GetTreeParent()->DecInterestCount();
	if (mc_DC)
		garbage |= mc_DC->DecInterestCount();

	if (auto refItem = mc_RefItem.lock())
		garbage |= refItem->DecInterestCount();
	else
		garbage |= TryCleanupMem();

	// Release an INTEREST-SCOPED m_ReadAssets payload -- a parked read OperationContext (PrepareDataRead) or a
	// PhaseContainer phase_resource -- now that this item is out of interest, so an abandoned scheduled read/phase
	// does not survive to deadlock the config-root teardown drain (StartInterest is a precondition of the read, see
	// PrepareDataUsageImpl; PhaseContainer re-installs its phase_resource in PreCalcUpdate). The TSF flag (set at the
	// store sites) distinguishes these from PERSISTENT operator calc-metainfo (DiscrAlloc htp_meta, Overlay info,
	// ...) that MUST be kept across interest cycles for recalc -- and lets StopInterest (tic) release clc/geo-defined
	// payloads without naming their types. Move into the garbage_can (deferred): the payload's destruction may
	// recurse into StopInterest on its kept arg-suppliers, so it must not run inside this StopInterest. Safe even if
	// a read is mid-run: while scheduled the parked ref is the OC's only strong owner (releasing it cancels the
	// abandoned read); while running the worker holds its own strong ref (see StealOneTask), so this is a ref drop.
	if (GetTSF(TSF_ReadAssetsInterestScoped))
	{
		garbage |= std::move(m_ReadAssets);
		ClearTSF(TSF_ReadAssetsInterestScoped);
	}

	s_SessionUsageCounter.unlock_shared();

#if defined(MG_DEBUG_DATASTORELOCK)
	--sd_ItemInterestCounter;
#endif
	return garbage;
}

SharedTreeItemInterestPtr TreeItem::GetInterestPtrOrCancel() const
{
	auto result = GetInterestPtrOrNull();
	if (result)
		return result;

	assert(CancelableFrame::CurrActive());
	CancelOrThrow(this);
}


#if defined(MG_DEBUG)

void TreeItem::CheckFlagInvariants() const
{
//	dms_assert( IsDcKnown() || !IsKnown() );
//	dms_assert( IsDataItem(this) || !IsFnKnown());
}

#endif

// ============== BLOB ====================

void TreeItem::LoadBlobBuffer (const BlobBuffer& rs)
{
	dms_assert(IsCacheRoot());
//	dms_assert(IsReadLocked(this));
	MemoInpStreamBuff impBuff(rs.begin(), rs.end());
 	LoadBlobStream(&impBuff);
	for (auto si = _GetFirstSubItem(); si; si = GetNextItem())
		si->LoadBlobStream(&impBuff);
}

void TreeItem::LoadBlobStream (const InpStreamBuff*)
{
}
/*
void TreeItem::StoreBlobBuffer(BlobBuffer& rs) const
{
	dms_assert(IsInWriteLock(this) || IsMetaThread() || (IsUnit(this) && !IsCacheItem()));
	VectorOutStreamBuff os;

	StoreBlobStream(&os);
	for (auto si = GetFirstSubItem(); si; si = GetNextItem())
		si->StreamBlobStream(&impBuff);

	rs = BlobBuffer(os.GetData(), os.GetData() + os.CurrPos());
}
*/
void TreeItem::StoreBlobStream(OutStreamBuff*) const
{
	throwIllegalAbstract(MG_POS, this, "StoreBlobStream"); 
}

struct CompareOutputRecord
{
	CharPtr m_First, m_Last;
	bool m_Result;

	SizeT size() const { return m_Last - m_First; }
	bool  OK  () const { return m_Result && !size(); }
};

static void DMS_CONV CompareOutput(ClientHandle clientHandle, const Byte* data, streamsize_t size)
{
	CompareOutputRecord* cr = reinterpret_cast<CompareOutputRecord*>(clientHandle);
	if (cr->m_Result)
	{
		if	(size <= cr->size() && std::equal(data, data + size, cr->m_First))
			cr->m_First += size;
		else
			cr->m_Result = false;		
	}
}

bool TreeItem::CheckBlobBuffer(const BlobBuffer& rs) const
{
	CompareOutputRecord cr = { rs.begin(), rs.end(), true };

	CallbackOutStreamBuff os(reinterpret_cast<void*>(&cr), CompareOutput);
	StoreBlobStream(&os);
	return cr.OK();
}

//----------------------------------------------------------------------
//	impl SourceLocation related member funcs of TreeItem
//----------------------------------------------------------------------

void TreeItem::SetLocation(const SourceLocation* loc)
{
	m_Location = MakeSharedForNewlyCreatedObject( loc );
}

const SourceLocation* TreeItem::GetLocation() const
{
	if (m_Location)
		return m_Location.get();
	return base_type::GetLocation();
}

SharedStr TreeItem::GetConfigFileName() const
{
	if (m_Location)
		return m_Location->m_ConfigFileDescr->GetFileName();
	if (auto parent = m_Parent.lock())
		return parent->GetConfigFileName();
	return SharedStr();
}

UInt32  TreeItem::GetConfigFileLineNr() const
{
	if (m_Location)
		return m_Location->m_ConfigFileLineNr;
	if (auto parent = m_Parent.lock())
		return parent->GetConfigFileLineNr();
	return 0;
}

UInt32  TreeItem::GetConfigFileColNr() const
{
	if (m_Location)
		return m_Location->m_ConfigFileColNr;
	if (auto parent = m_Parent.lock())
		return parent->GetConfigFileColNr();
	return 0;
}


