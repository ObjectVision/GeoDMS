// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem: the configuration-tree node — tree topology, item state and
// update flow, storage binding and interest-count management.

#include "TreeItem.h"
#include "TreeItemFunctionSpec.h"
//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "RtcInterface.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
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
#include "UsingCache.h"
#include "stg/MemoryMappedDataStorageManager.h"

#include "set/StaticQuickAssoc.h"

#include <bitset>
#include <unordered_set>

// user-defined function items: declared parameter count + designated result sub-item
// + optional per-parameter function-signature exemplars, kept in a side-assoc keyed by
// the function definition item (set by the config parser, read at call dispatch;
// erased in ~TreeItem).
namespace {
	struct FunctionSpecData
	{
		UInt32 nrParams = 0;
		TokenID resultName;
		std::vector<std::tuple<UInt32, std::weak_ptr<const TreeItem>, std::vector<TokenID>>> paramSigs; // (param index, signature exemplar, type-application args)
		// the `= {}` on the members below keeps `FunctionSpecData{ nrParams, resultName, {} }`
		// out of -Wmissing-field-initializers, like the bool members further down
		std::vector<std::pair<UInt32, std::weak_ptr<const TreeItem>>> paramTypeExemplars = {}; // K11a by-example: (param index, UNIT exemplar whose declared members type the parameter)
		std::vector<std::tuple<UInt32, TokenID, TokenID, bool>> genericParams = {}; // (param index, type variable, constraint, isDomainVar)
		std::vector<std::pair<TokenID, TokenID>> typeVars = {}; // the declaration's own ordered <var: constraint> list (WP4.1)
		std::vector<UInt32> metaRefParams = {}; // 'item x' parameters: bound as raw item references (sourceDescr), not calculation keys
		bool hasRestParam = false;      // '...x' rest parameter (always the LAST param): binds ONE OR MORE trailing arguments
		bool definitionChecked = false; // WP3.4: body scope/shape validated once
		bool isVariantSet = false;      // §5.7: a function that dispatches to variant sub-functions by argument type
		bool signatureOnly = false;     // 'alias = function<...>(...) -> ...;' — a signature-only function item (declared type, no body)
		bool resultIsFunction = false;  // §5.10: '-> function' / '-> sigAlias' — the result is function-valued
		std::weak_ptr<const TreeItem> resultSig = {}; // the '-> sigAlias<...>' result-signature exemplar, if any (else expired)
		std::vector<TokenID> resultSigTypeArgs = {};  // the result signature's type-application args
	};
	bool IsDefaultValue(const FunctionSpecData& v) { return v.nrParams == 0 && !v.resultName && v.paramSigs.empty() && v.genericParams.empty() && v.typeVars.empty() && v.metaRefParams.empty() && !v.hasRestParam && !v.definitionChecked && !v.isVariantSet && !v.signatureOnly && !v.resultIsFunction && v.resultSig.expired() && v.resultSigTypeArgs.empty(); }
	static_quick_assoc<const TreeItem*, FunctionSpecData> s_FunctionSpecAssoc;

	static TokenID t_gcAny          = GetTokenID_st("any");
	static TokenID t_gcNumerics     = GetTokenID_st("numerics");
	static TokenID t_gcIntegers     = GetTokenID_st("integers");
	static TokenID t_gcFloats       = GetTokenID_st("floats");
	static TokenID t_gcUInts        = GetTokenID_st("uints");
	static TokenID t_gcUnsignedInts = GetTokenID_st("unsigned_ints");
	static TokenID t_gcSInts        = GetTokenID_st("sints");
	static TokenID t_gcSignedInts   = GetTokenID_st("signed_ints");
	static TokenID t_gcDomains      = GetTokenID_st("domains");
	static TokenID t_gcPoints       = GetTokenID_st("points");
	static TokenID t_gcDomainPoints = GetTokenID_st("domain_points");
	static TokenID t_gcSignedDomainPoints = GetTokenID_st("signed_domain_points");
	static TokenID t_gcUnsignedDomainPoints = GetTokenID_st("unsigned_domain_points");
}

TIC_CALL bool IsKnownGenericConstraint(TokenID constraintName)
{
	return constraintName == t_gcAny
		|| constraintName == t_gcNumerics
		|| constraintName == t_gcIntegers
		|| constraintName == t_gcFloats
		|| constraintName == t_gcUInts
		|| constraintName == t_gcUnsignedInts
		|| constraintName == t_gcSInts
		|| constraintName == t_gcSignedInts
		|| constraintName == t_gcDomains
		|| constraintName == t_gcPoints
		|| constraintName == t_gcDomainPoints
		|| constraintName == t_gcSignedDomainPoints
		|| constraintName == t_gcUnsignedDomainPoints;
}

bool MatchesGenericConstraint(const ValueClass* vc, TokenID constraintName)
{
	if (!vc)
		return false;
	if (constraintName == t_gcAny)      return true;
	if (constraintName == t_gcNumerics) return vc->IsNumeric();
	if (constraintName == t_gcIntegers) return vc->IsIntegral();
	if (constraintName == t_gcFloats)   return vc->IsNumeric() && !vc->IsIntegral();
	if (constraintName == t_gcUInts || constraintName == t_gcUnsignedInts)
		return vc->IsIntegral() && !vc->IsSigned();
	if (constraintName == t_gcSInts || constraintName == t_gcSignedInts)
		return vc->IsIntegral() && vc->IsSigned();
	if (constraintName == t_gcDomains)  return vc->IsCountable();
	if (constraintName == t_gcPoints)   return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single;
	if (constraintName == t_gcDomainPoints)
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable();
	if (constraintName == t_gcSignedDomainPoints)
		// domain_points restricted to SIGNED coordinates (spoint, ipoint): only those
		// can carry a negative cell offset. Signedness lives on the coordinate type,
		// not on the point value class — is_signed<Point<T>> is false for every T —
		// so consult the scalar class, exactly as IsCountable() does for integrality.
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable()
			&& vc->GetScalarClass() && vc->GetScalarClass()->IsSigned();
	if (constraintName == t_gcUnsignedDomainPoints)
		// the complement within domain_points (wpoint, upoint): see the note above
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable()
			&& vc->GetScalarClass() && !vc->GetScalarClass()->IsSigned();
	return false;
}

using TreeItemInterestPtr = InterestPtr<const TreeItem*>;

#include "cs_lock_map.h"
using treeitem_lock_map = cs_lock_map<const TreeItem*>; // raw identity key (transient, non-owning), like actor_section_lock_map / data_flags_lock_map

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
		s_FunctionSpecAssoc.erase(this);

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
void TreeItem::EnableAutoDelete() // does not call UpdateMetaInfo
{
	bool isConfigRoot = !(IsCacheItem() || IsEndogenous() || GetTreeParent());

	if (isConfigRoot)
	{
		// Gracefully end worker threads before tearing down the config tree. Mark the session as
		// cancelling so in-flight workers cancel (releasing the shared ownership of their inputs and the
		// mutable ownership of what they produce), then drain by taking s_SessionUsageCounter exclusively:
		// this makes any new try_lock_shared fail (the designed cancellation trigger, see ItemLocks.cpp)
		// and blocks until every worker has released its shared usage. Without it the main thread could
		// begin teardown / static-component destruction while workers still hold resources -> leak (the
		// timing-dependent leak the removed auto-delete pin used to mask).
		if (auto sd = SessionData::Curr())
			sd->SetCancelling();
		{ leveled_counted_section::scoped_lock drainWorkers(s_SessionUsageCounter); }

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
	if (id) CheckTreeItemName( id.GetStr().c_str() );
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
		newRI = newDC->MakeResult();
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

static void ApplyCalculator(TreeItem* holder, const AbstrCalculator* ac)
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

TIC_CALL void TreeItem_SetFunctionSpec(const TreeItem* functionItem, UInt32 nrParams, TokenID resultName)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc.assoc(functionItem, FunctionSpecData{ nrParams, resultName, {} });
}

TIC_CALL void TreeItem_AddFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* signatureExemplar, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(signatureExemplar && signatureExemplar->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].paramSigs.emplace_back(paramIndex, signatureExemplar->weak_from_this(), std::move(typeArgs));
}

SharedTreeItem TreeItem_GetFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& paramSig : specPtr->paramSigs)
			if (std::get<0>(paramSig) == paramIndex)
				return SharedTreeItem(std::get<1>(paramSig).lock());
	return {};
}

// K11a by-example: a 'p: exemplar' parameter whose exemplar is a UNIT — the
// exemplar's declared sub-items serve as the parameter's member block for the
// definition-time checker (the parse-time clone carries only the class).
TIC_CALL void TreeItem_AddFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* exemplar)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(exemplar);
	s_FunctionSpecAssoc[functionItem].paramTypeExemplars.emplace_back(paramIndex, exemplar->weak_from_this());
}

SharedTreeItem TreeItem_GetFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& pe : specPtr->paramTypeExemplars)
			if (pe.first == paramIndex)
				return SharedTreeItem(pe.second.lock());
	return {};
}

TIC_CALL void TreeItem_SetFunctionRestParam(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].hasRestParam = true;
}

TIC_CALL bool TreeItem_HasFunctionRestParam(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->hasRestParam;
}

TIC_CALL void TreeItem_AddFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].metaRefParams.push_back(paramIndex);
}

bool TreeItem_IsFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (UInt32 idx : specPtr->metaRefParams)
			if (idx == paramIndex)
				return true;
	return false;
}

const std::vector<TokenID>* TreeItem_GetFunctionParamSigTypeArgs(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& paramSig : specPtr->paramSigs)
			if (std::get<0>(paramSig) == paramIndex && !std::get<2>(paramSig).empty())
				return &std::get<2>(paramSig);
	return nullptr;
}

TIC_CALL void TreeItem_SetFunctionTypeVars(const TreeItem* functionItem, std::vector<std::pair<TokenID, TokenID>> typeVars)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].typeVars = std::move(typeVars);
}

const std::vector<std::pair<TokenID, TokenID>>* TreeItem_GetFunctionTypeVars(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr && !specPtr->typeVars.empty())
		return &specPtr->typeVars;
	return nullptr;
}

TIC_CALL void TreeItem_SetFunctionSignatureOnly(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].signatureOnly = true;
}

bool TreeItem_IsFunctionSignatureOnly(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->signatureOnly;
}

// §5.10: record a function-valued result. `resultSigExemplar` (the '-> sigAlias<...>' exemplar,
// may be null for a bare '-> function') + its type-application args enable faithful rendering.
TIC_CALL void TreeItem_SetFunctionResultSig(const TreeItem* functionItem, bool resultIsFunction, const TreeItem* resultSigExemplar, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.resultIsFunction = resultIsFunction;
	if (resultSigExemplar)
		spec.resultSig = resultSigExemplar->weak_from_this();
	spec.resultSigTypeArgs = std::move(typeArgs);
}

bool TreeItem_IsFunctionResultFunction(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->resultIsFunction;
}

SharedTreeItem TreeItem_GetFunctionResultSig(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		return SharedTreeItem(specPtr->resultSig.lock());
	return {};
}

const std::vector<TokenID>* TreeItem_GetFunctionResultSigTypeArgs(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr && !specPtr->resultSigTypeArgs.empty())
		return &specPtr->resultSigTypeArgs;
	return nullptr;
}

// ===================================== §5.7 v2: variant specificity / disjointness

namespace {

	using VariantParamSet = std::bitset<UInt32(ValueClassID::VT_Count)>;

	// the set of value classes a variant parameter accepts, over the CLOSED value-class
	// universe: a generic values-variable -> its constraint's subset; a concrete
	// script-named class -> singleton; anything else (plain items, composite types,
	// function-typed parameters, item-spec units) -> everything ("soft" wildcard).
	// Token-based only: safe at parse time (no meta machinery).
	VariantParamSet VariantParamMatchSet(const TreeItem* variant, UInt32 paramIndex, const TreeItem* param, bool* isHard)
	{
		VariantParamSet s;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(variant, seqNr++, &idx, &var, &cons, &isDom))
			if (idx == paramIndex && !isDom)
			{
				for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count); ++v)
					if (auto vc = ValueClass::FindByValueClassID(ValueClassID(v)))
						if (MatchesGenericConstraint(vc, cons))
							s.set(v);
				if (isHard) *isHard = true;
				return s;
			}
		const ValueClass* vc = nullptr;
		if (IsDataItem(param))
			vc = ValueClass::FindByScriptName(AsDataItem(param)->ValuesUnitToken());
		else if (IsUnit(param))
			vc = AsUnit(param)->GetValueType();
		if (vc)
		{
			s.set(UInt32(vc->GetValueClassID()));
			if (isHard) *isHard = true;
			return s;
		}
		s.set(); // wildcard
		if (isHard) *isHard = false;
		return s;
	}

	struct VariantMatchInfo
	{
		const TreeItem*              variant = nullptr;
		std::vector<VariantParamSet> sets;
		bool                         allHard = true;
	};

	VariantMatchInfo GetVariantMatchInfo(const TreeItem* variant)
	{
		VariantMatchInfo r;
		r.variant = variant;
		UInt32 np = TreeItem_GetFunctionParamCount(variant);
		r.sets.reserve(np);
		const TreeItem* param = variant->_GetFirstSubItem();
		for (UInt32 i = 0; i != np && param; ++i, param = param->GetNextItem())
		{
			bool hard = false;
			r.sets.push_back(VariantParamMatchSet(variant, i, param, &hard));
			r.allHard = r.allHard && hard;
		}
		return r;
	}

	// -1: a strictly more specific than b; +1: b strictly more specific; 0: identical
	// coverage; 2: incomparable. Requires equal arity.
	int CompareVariantInfo(const VariantMatchInfo& a, const VariantMatchInfo& b)
	{
		bool aLEb = true, bLEa = true;
		for (SizeT i = 0; i != a.sets.size(); ++i)
		{
			if ((a.sets[i] & ~b.sets[i]).any()) aLEb = false;
			if ((b.sets[i] & ~a.sets[i]).any()) bLEa = false;
		}
		if (aLEb && bLEa) return 0;
		if (aLEb) return -1;
		if (bLEa) return +1;
		return 2;
	}

} // anonymous namespace

bool TreeItem_VariantMatches(const TreeItem* variant, const std::vector<const ValueClass*>& argVCs)
{
	UInt32 np = TreeItem_GetFunctionParamCount(variant);
	// a '...x' rest variant binds one-or-more trailing arguments through its LAST
	// param: it matches any argument count >= its declared param count
	bool hasRest = TreeItem_HasFunctionRestParam(variant);
	if (hasRest ? argVCs.size() < np : argVCs.size() != np)
		return false;
	auto info = GetVariantMatchInfo(variant);
	for (SizeT i = 0; i != argVCs.size(); ++i)
	{
		const auto& acceptSet = info.sets[std::min<SizeT>(i, info.sets.size() - 1)]; // rest tail: the last param's set
		if (!argVCs[i])
		{
			if (!acceptSet.all())
				return false; // a non-class argument (function value, literal) only matches a wildcard position
			continue;
		}
		if (!acceptSet.test(UInt32(argVCs[i]->GetValueClassID())))
			return false;
	}
	return true;
}

int TreeItem_CompareVariantSpecificity(const TreeItem* a, const TreeItem* b)
{
	auto ia = GetVariantMatchInfo(a), ib = GetVariantMatchInfo(b);
	// unequal declared arity (possible when a rest variant and a fixed/longer variant
	// both match one call): the variant with MORE declared params is more specific
	if (ia.sets.size() != ib.sets.size())
		return ia.sets.size() > ib.sets.size() ? -1 : +1;
	return CompareVariantInfo(ia, ib);
}

TIC_CALL void TreeItem_CheckVariantSetDisjointness(const TreeItem* setItem)
{
	// definition-time (§5.7 v2): two variants whose acceptance sets overlap must be
	// specificity-ordered — identical or incomparable overlapping coverage is an
	// error now instead of a per-call ambiguity later. Pairs with a "soft" position
	// (unresolvable/wildcard type) are left to the call-time ambiguity guard.
	std::vector<VariantMatchInfo> infos;
	for (const TreeItem* v = setItem->_GetFirstSubItem(); v; v = v->GetNextItem())
		if (v->IsFunctionItem())
			infos.push_back(GetVariantMatchInfo(v));

	for (SizeT i = 0; i != infos.size(); ++i)
		for (SizeT j = i + 1; j != infos.size(); ++j)
		{
			const auto& a = infos[i]; const auto& b = infos[j];
			if (a.sets.size() != b.sets.size() || !a.allHard || !b.allHard)
				continue;
			bool overlap = true;
			for (SizeT k = 0; k != a.sets.size() && overlap; ++k)
				if (!(a.sets[k] & b.sets[k]).any())
					overlap = false;
			if (!overlap)
				continue;
			int cmp = CompareVariantInfo(a, b);
			if (cmp == 0)
				throwDmsErrF("variant set '{}': variants '{}' and '{}' accept identical argument types"
					, setItem->GetFullName().c_str(), a.variant->GetID().GetStr().c_str(), b.variant->GetID().GetStr().c_str());
			if (cmp == 2)
				throwDmsErrF("variant set '{}': variants '{}' and '{}' overlap without one being more specific than the other; split their parameter types"
					, setItem->GetFullName().c_str(), a.variant->GetID().GetStr().c_str(), b.variant->GetID().GetStr().c_str());
		}
}

void TreeItem_CopyFunctionSpec(const TreeItem* dstFunctionItem, const TreeItem* srcFunctionItem)
{
	assert(dstFunctionItem && dstFunctionItem->IsFunctionItem());
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(srcFunctionItem);
	if (specPtr)
		s_FunctionSpecAssoc.assoc(dstFunctionItem, *specPtr);
}

TIC_CALL void TreeItem_AddFunctionGenericParam(const TreeItem* functionItem, UInt32 paramIndex, TokenID varName, TokenID constraintName, bool isDomainVar)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].genericParams.emplace_back(paramIndex, varName, constraintName, isDomainVar);
}

bool TreeItem_GetFunctionGenericParam(const TreeItem* functionItem, UInt32 seqNr, UInt32* paramIndex, TokenID* varName, TokenID* constraintName, bool* isDomainVar)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (!specPtr || seqNr >= specPtr->genericParams.size())
		return false;
	const auto& genericParam = specPtr->genericParams[seqNr];
	if (paramIndex)     *paramIndex     = std::get<0>(genericParam);
	if (varName)        *varName        = std::get<1>(genericParam);
	if (constraintName) *constraintName = std::get<2>(genericParam);
	if (isDomainVar)    *isDomainVar    = std::get<3>(genericParam);
	return true;
}

bool TreeItem_IsFunctionDefinitionChecked(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->definitionChecked;
}

void TreeItem_SetFunctionDefinitionChecked(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].definitionChecked = true;
}

TIC_CALL void TreeItem_SetFunctionVariantSet(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].isVariantSet = true;
}

TIC_CALL bool TreeItem_IsFunctionVariantSet(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->isVariantSet;
}

TIC_CALL UInt32 TreeItem_GetFunctionParamCount(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr ? specPtr->nrParams : 0;
}

TokenID TreeItem_GetFunctionResultName(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr ? specPtr->resultName : TokenID::GetEmptyID();
}

TIC_CALL void TreeItem_MakeStrictScope(TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	functionItem->GetUsingCache()->RemoveParentUsing();
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
	if (m_UsingCache) m_UsingCache->ClearUsings(true);
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

//----------------------------------------------------------------------
// TreeItem Find Functions
//----------------------------------------------------------------------

SharedTreeItem TreeItem::GetConstSubTreeItemByID(TokenID subItemID) const
{
	const TreeItem* subItem = GetFirstSubItem(); // calls UpdateMetaInfo
	while (true)
	{
		if (!subItem)
		{
			if (auto refItem = mc_RefItem.lock())
			{
				assert(refItem.get() != this);
				return refItem->GetConstSubTreeItemByID(subItemID);
			}
			return {};
		}

		if	(subItem->GetID() == subItemID)
			return make_shared_tree(subItem, existing_obj{});
		subItem = subItem->GetNextItem();
	}
}

SharedTreeItem TreeItem::GetCurrSubTreeItemByID(TokenID subItemID) const
{
	auto subItem = GetCurrFirstSubItem(); // requires UpdateMetaInfo to have been called
	while (true)
	{
		if (!subItem)
		{
			if (auto refItem = mc_RefItem.lock())
			{
				assert(refItem.get() != this);
				return refItem->GetCurrSubTreeItemByID(subItemID);
			}
			return {};
		}

		if (subItem->GetID() == subItemID)
			return make_shared_tree(subItem, existing_obj{});
		subItem = subItem->GetNextItem();
	}
}

TreeItem* TreeItem::GetSubTreeItemByID(TokenID subItemID) // does not UpdateMetaInfo
{
	TreeItem* subItem = _GetFirstSubItem(); // doesn't call UpdateMetaInfo (non const)

	while (subItem && subItem->GetID() != subItemID)
		subItem = subItem->GetNextItem();

	return subItem;
}

TreeItem* TreeItem::GetItem(CharPtrRange subItemNames)
{
	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			return nullptr;
		return GetSubTreeItemByID(GetTokenID(ids.second));
	}
	TreeItem* parent = GetItem(ids.first);
	return (parent) ? parent->GetSubTreeItemByID(GetTokenID(ids.second)) : nullptr;
}

TreeItem* TreeItem::GetBestItem(CharPtrRange subItemNames)
{
	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			return nullptr;
		auto result = GetSubTreeItemByID(GetTokenID(ids.second));
		return result ? result : this;
	}
	TreeItem* parent = GetItem(ids.first);
	if (!parent)
		return nullptr;
	auto result = parent->GetSubTreeItemByID(GetTokenID(ids.second));
	return result ? result : parent;
}

SharedTreeItem TreeItem::GetCurrItem(CharPtrRange subItemNames) const
{
	if (subItemNames.empty())
		return make_shared_tree(this, existing_obj{});

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || (ids.second.size() && ids.second.first[0] == '.'))
			throwItemError("GetCurrItem is not allowed to look outside the accessible search context");
		return GetCurrSubTreeItemByID(GetTokenID(ids.second));
	}
	auto parent = GetCurrItem(ids.first);
	return parent ? parent->GetCurrSubTreeItemByID(GetTokenID(ids.second)) : SharedTreeItem{};
}


SharedTreeItem TreeItem::FindItem(CharPtrRange subItemNames) const
{
	assert(IsMetaThread());

	if (subItemNames.empty())
		return make_shared_tree(this, existing_obj{});

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	assert(ids.first.first == subItemNames.first);
	assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) // subItemNames is an atomic token
	{	
		assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
			return make_shared_tree(FollowDots(ids.second), existing_obj{});

		UpdateMetaInfoIfNotAlready();

		TokenID existingToken = GetExistingTokenID<mt_tag>(ids.second); //to be found token was already created if asserts hold
		if (!IsDefined(existingToken))
			return {};
		return make_shared_tree(FindTreeItemByID(this, existingToken), existing_obj{});
	}
	SharedTreeItem parent = {};
	if (ids.first.empty()) // We start at root.
	{
		MG_CHECK(!IsCacheItem());
		parent = make_shared_tree(static_cast<const TreeItem*>(GetRoot()), existing_obj{});
	}
	else
		parent = FindItem(ids.first);

	if (!parent)
		return {};
	parent->UpdateMetaInfoIfNotAlready();
//	if (parent->WasFailed(FailType::MetaInfo))
//		parent->ThrowFail();
	return parent->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
}

auto TreeItem::FindAndVisitItem(CharPtrRange subItemNames, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>  // directly referred persistent object.
{
	assert(IsMetaThread());
	assert(Test(svf, SupplierVisitFlag::ImplSuppliers));

	if (subItemNames.empty())
		return {};

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	assert(ids.first.first == subItemNames.first);
	assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) // subItemNames is an atomic token
	{
		assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
		{
			auto item = FollowDots(ids.second);
			if (visitor.Visit(item) == AVS_SuspendedOrFailed)
				return {};
			return make_shared_tree(item, existing_obj{});
		}

		UpdateMetaInfo();
		TokenID existingToken = GetExistingTokenID<mt_tag>(ids.second); //to be found token was already created if asserts hold
		if (!IsDefined(existingToken))
			return {};
		auto item = FindTreeItemByID(this, existingToken);
		if (visitor.Visit(item) == AVS_SuspendedOrFailed)
			return {};
		return make_shared_tree(item, existing_obj{});
	}
	SharedTreeItem parent;
	if (ids.first.empty()) // We start at root.
	{
		parent = SessionData::Curr()->GetConfigRoot();
		if (visitor.Visit(parent.get()) == AVS_SuspendedOrFailed)
			return {};
	}
	else
	{
		auto  optionalParent = FindAndVisitItem(ids.first, svf, visitor);
		if (!optionalParent)
			return {};

		parent = optionalParent.value();
	}
	if (!parent)
		return {};
	parent->UpdateMetaInfo();

	auto result = parent->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
	if (visitor.Visit(result.get()) == AVS_SuspendedOrFailed)
		return {};
	return result;
}

static auto FollowBestDots(const TreeItem* self, CharPtrRange dots) noexcept -> BestItemRef
{
	dms_assert(self);
	dms_assert(dots.size());
	while (true)
	{
		if (*dots.first != '.')
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };
		dots.first++;
		if (dots.first == dots.second)
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };

		auto parent = self->GetTreeParent();
		if (!parent)
			return { make_shared_tree(self, existing_obj{}), SharedStr(dots.first - 1 MG_DEBUG_ALLOCATOR_SRC("FollowBestDots")) };
		self = parent.get();
	}
}


auto TreeItem::FindBestItem(CharPtrRange subItemNames) const -> BestItemRef
{
	if (subItemNames.empty())
		return { make_shared_tree(this, existing_obj{}), {} };

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	dms_assert(ids.first.first == subItemNames.first);
	dms_assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) 
	{
		// subItemNames is an atomic token
		dms_assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
			return FollowBestDots(this, ids.second);
		UpdateMetaInfo();
		TokenID t = GetExistingTokenID(ids.second);
		if (IsDefined(t)) {
			auto result = FindTreeItemByID(this, t);
			if (result)
				return { make_shared_tree(result, existing_obj{}), {} };
		}
		return { make_shared_tree(this, existing_obj{}), SharedStr(ids.second) };
	}

	if (ids.first.empty()) 
	{
		// We start at root, first characted was '/'
		assert(subItemNames[0] == DELIMITER_CHAR);
		assert(ids.second.first = subItemNames.first + 1);

		auto configRoot = SessionData::Curr()->GetConfigRoot();
		if (configRoot)
		{
			configRoot->UpdateMetaInfo();
			auto t = GetExistingTokenID(ids.second);
			if (IsDefined(t))
			{
				auto result = configRoot->GetConstSubTreeItemByID(t);
				if (result)
					return { result, {} };
			}
		}
		return { configRoot, SharedStr(ids.second) };
	}

	// ===== we have first and second here, so we'd have to recurse and combine
	assert(!ids.first.empty());
	//dms_assert(!ids.second.empty()); Wrong if path contains '//' 

	auto parentRef = FindBestItem(ids.first);
	if (!parentRef.first)
		return { make_shared_tree(this, existing_obj{}), SharedStr(subItemNames) };

	if (!parentRef.second.empty())
		return { parentRef.first, parentRef.second + SharedStr(CharPtrRange(ids.first.second, ids.second.second)) };

	parentRef.first->UpdateMetaInfo();
	auto result = parentRef.first->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
	if (!result)
		return { parentRef.first, SharedStr(ids.second) };
	return { result, {} };
}

const TreeItem* TreeItem_CheckObjCls(const TreeItem* self, const Class* requiredClass)
{
	dms_assert(requiredClass);
	if (!self)
		return nullptr;
	const Class* thisClass = requiredClass->IsDataObjType()
			?	self->GetDynamicObjClass()
			:	self->GetDynamicClass();

	if	(!	thisClass->IsDerivedFrom(requiredClass))
		self->throwItemErrorF("Cannot cast to the requested type: {}", 
			requiredClass->GetName()
		);
	return self;
}

const TreeItem* TreeItem::CheckObjCls(const Class* requiredClass) const
{
	return TreeItem_CheckObjCls(this, requiredClass);
}

TreeItem* TreeItem_CheckCls(TreeItem* self, const Class* requiredClass)
{
	if (!self)
		return nullptr;
	dms_assert(requiredClass);

	const Class* thisClass = requiredClass->IsDataObjType()
			?	self->GetCurrentObjClass()
			:	self->GetDynamicClass();

	if (!	thisClass->IsDerivedFrom(requiredClass))
		self->throwItemErrorF(
			"Cannot cast to the requested type: {}", 
			requiredClass->GetName()
		);
	return self;
}

TreeItem* TreeItem::CheckCls(const Class* requiredClass)
{
	return TreeItem_CheckCls(this, requiredClass);
}

const TreeItem* TreeItem::FollowDots(CharPtrRange dots) const
{
	assert(dots.size());
	const TreeItem* result = this;
	while (true)
	{
		if (*dots.first++ != '.')
			throwItemError("FollowDots: '/' or '.' expected");
		if (dots.first == dots.second)
			return result;

		result = result->GetTreeParent().get();
		if (!result)
			throwItemError("FollowDots: relative pathname ascended above root");
	}
}

auto TreeItem::GetScriptName(const TreeItem* context) const -> SharedStr
{
	assert(*GetName().c_str());
	assert(context);
	assert(context->GetTreeParent());

	return context->GetTreeParent()->GetFindableName(this);
}

TreeItem* CheckedAs(TreeItem* self, const Class* requiredClass)
{
	// check on type of this and return
	if (requiredClass && !self->IsKindOf(requiredClass) )
		self->throwItemErrorF("CreateItem('{}') failed since it is already created as '{}'",
			requiredClass->GetName(), self->GetDynamicClass()->GetName());
	return self; 
}

auto CreateAndInitItem(TreeItem* self, TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	assert(requiredClass);

	// TreeItem-family classes are created through std::make_shared (CreateSharedObj); the raw
	// CreateObj() path remains as a fallback for any class without a shared creator registered.
	SharedMutableTreeItem newSubItem;
	if (requiredClass->HasSharedCreator())
		newSubItem = std::static_pointer_cast<TreeItem>(requiredClass->CreateSharedObj());
	else
		newSubItem = make_shared_tree(debug_cast<TreeItem*>(requiredClass->CreateObj()), newly_obj{});
	assert(newSubItem);

	// Pass a co-owning copy: InitTreeItem moves its copy into the parent's sub-item list (or drops it
	// for a root); `newSubItem` retains a share so the node survives and is returned to the caller.
	InitTreeItem(self, newSubItem, id);

	return newSubItem;
}

auto TreeItem_CreateItem(TreeItem* self, TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	assert(!requiredClass || requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));

	if (self)
	{
		if (!id)
			return make_shared_tree(CheckedAs(self, requiredClass), existing_obj{}); // borrow an owning share of the existing item

		// find foundSubItem according to firstSubItemName
		TreeItem* foundSubItem = self->GetSubTreeItemByID(id);
		if (foundSubItem)
			return make_shared_tree(CheckedAs(foundSubItem, requiredClass), existing_obj{});
	}

	// create something
	return CreateAndInitItem(self, id, (requiredClass) ? requiredClass : TreeItem::GetStaticClass());
}

auto TreeItem::CreateItem(TokenID id, const Class* requiredClass) -> SharedMutableTreeItem
{
	return TreeItem_CreateItem(this, id, requiredClass);
}

auto TreeItem_CreateItemFromPath(TreeItem* self, CharPtr subItemNames, const Class* requiredClass) -> SharedMutableTreeItem
{
	if (!requiredClass)
		requiredClass = TreeItem::GetStaticClass();

	assert(requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));
	assert(subItemNames);

	if (*subItemNames == 0) // all subItemNames are processed ??
	{
		if (self)
			return make_shared_tree(CheckedAs(self, requiredClass), existing_obj{});
		else
			return CreateAndInitItem(self, TokenID(), requiredClass);
	}


	// parsing the subItemNames recursively by calling this method on subItemNames parts

	// OPTIMIZE: Reverse order to make use of parent token tables
	CharPtr   restSubItemNames; // new subItemNames after parse
	SharedStr firstSubItemName = splitPathBase(subItemNames, &restSubItemNames); // firstSubItemName of storage after parse
	bool      hasRestSubItems = (*restSubItemNames) != 0;

	if (firstSubItemName.empty() || firstSubItemName[0] == '.')
		// subItemNames started with a '/': traversing an absolute path is not allowed for locating a new object
		// traversing outside the specified namespace is not allowed for locating a new object
		throwItemErrorF(self, "CreateItemFromPath({}): Cannot create new items outside creation context", subItemNames);

	TokenID   firstSubItemID = GetTokenID_mt(firstSubItemName.c_str());
	dms_assert(!firstSubItemID.empty());
	TreeItem* foundSubItem   = nullptr;
	if (self)
		foundSubItem = self->GetSubTreeItemByID(firstSubItemID); // find foundSubItem according to firstSubItemName

	SharedMutableTreeItem createdHolder; // keeps a freshly-created item alive across the recursion when no parent owns it (self==nullptr)
	if (!foundSubItem) // create something
	{
		createdHolder = CreateAndInitItem(self, firstSubItemID, (hasRestSubItems || !requiredClass) ? TreeItem::GetStaticClass() : requiredClass);
		foundSubItem  = createdHolder.get();
		if (!hasRestSubItems)
			return createdHolder;
	}
	assert(foundSubItem);
	return TreeItem_CreateItemFromPath(foundSubItem, restSubItemNames, requiredClass);
}

auto TreeItem::CreateItemFromPath(CharPtr subItemNames, const Class* requiredClass) -> SharedMutableTreeItem
{
	return TreeItem_CreateItemFromPath(this, subItemNames, requiredClass);
}

SharedMutableTreeItem TreeItem::CreateConfigRoot(TokenID id) // static
{
	dms_assert(!s_MakeEndoLockCount);
	SharedMutableTreeItem result = make_shared_tree(new TreeItem, newly_obj{}); // sole owner from birth (fresh std control block, wires shared_from_this)
	InitTreeItem(nullptr, result, id);
	result->SetFreeDataState(true);
	return result;
}
SharedMutableTreeItem TreeItem::CreateCacheRoot() // static
{
	dms_assert(s_MakeEndoLockCount);
	SharedMutableTreeItem result = make_shared_tree(new TreeItem, newly_obj{}); // sole owner from birth (fresh std control block, wires shared_from_this)
	InitTreeItem(nullptr, result, TokenID::GetEmptyID());
	result->SetPassor();
	return result;
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
		result->ClearNamespaceUsage();

		// function scoping (§4.6, revised 2026-07-13: lexical definition scope with
		// call-site isolation): an instantiated (or copied) function scope sees its own
		// sub-items (bound arguments, locals, result), the function's explicitly
		// imported namespaces (frozen to absolute paths below), and the DEFINITION
		// parent, injected as an absolute namespace like template instances get —
		// while the implicit tree-parent namespace (= call-site or copy-site scope) is
		// removed below, so call-site names stay invisible.
		bool srcIsFunction = IsFunctionItem();

		UInt32 nrNameSpaces = GetNrNamespaceUsages();
		bool addParentAsNamespace = dstIsRoot && GetTreeParent();
		if (nrNameSpaces || addParentAsNamespace)
		{
			VectorOutStreamBuff nameSpaceBuffer;
			FormattedOutStream nameSpaceStream(&nameSpaceBuffer, FormattingFlags::None);

			if (addParentAsNamespace)
				nameSpaceStream << GetTreeParent()->GetFullName();

			//	Now, copy all namespaces.
			//	Note that namespaces may not be circular (requirement of FindItem)
			//	GetItem follows a relative or absolute path directly
			//	FindItem calls GetItem on this and throws in case of failure
			for (UInt32 i1 =0; i1 != nrNameSpaces; ++i1)
			{
				const TreeItem* sns = GetNamespaceUsage(i1);
				// the parent/ancestor skips exist because instances reach ancestors through
				// the injected definition-parent namespace; function imports are kept
				// verbatim (frozen absolute) — a redundant entry is harmless
				if (sns && (srcIsFunction || (sns != GetTreeParent().get() && !sns->DoesContain(this))))
				{
					if (nameSpaceBuffer.CurrPos())
						nameSpaceStream << ';';
					if (srcIsFunction)
						nameSpaceStream << sns->GetFullName(); // freeze imports absolute: resolved at the definition site
					else
						nameSpaceStream << copyContext.GetAbsOrRelNameID(sns, this, dest).GetStr().c_str();
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
		if (srcIsFunction)
			result->GetUsingCache()->RemoveParentUsing(); // also forces the cache into existence: name resolution delegates-and-stops here
	}
	if (InTemplate())
		result->mc_OrgItem = make_weak_tree(this);

	if (IsFunctionItem() && !dstIsRoot)
	{
		// a function definition copied as part of a larger subtree (e.g. inside an
		// instantiated template) stays a function: flag, declared spec, strict scope
		result->SetIsFunction();
		TreeItem_CopyFunctionSpec(result.get(), this);
		result->GetUsingCache()->RemoveParentUsing();
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

//----------------------------------------------------------------------
// implement Actor callback functions
//----------------------------------------------------------------------

void TreeItem::SetMetaInfoReady() const 
{ 
	dms_assert(m_LastChangeTS || IsPassor()); // PRECONDITION for SetProgress

	if (m_State.GetProgress() < ProgressState::MetaInfo)
		m_State.SetProgress(ProgressState::MetaInfo);
	dbg_assert(IsPassor() || !IsDataItem(this) || (!AsDataItem(this)->GetAbstrDomainUnit()) || AsDataItem(this)->GetAbstrDomainUnit()->CheckMetaInfoReadyOrPassor());
	dbg_assert(IsPassor() || !IsDataItem(this) || (!AsDataItem(this)->GetAbstrValuesUnit()) || AsDataItem(this)->GetAbstrValuesUnit()->CheckMetaInfoReadyOrPassor());
}

const bool MG_DEBUG_UPDATEMETAINFO = true;

void TreeItem::UpdateMetaInfoImpl() const
{
	assert(!WasFailed(FailType::MetaInfo));

	Actor::UpdateMetaInfo(); // calls UpdateMetaInfo for all xxx Suppliers.

	if (HasIntegrityChecker())
		m_State.Set(actor_flag_set::AF_IntegrityChecked);

	GetPhaseNumber();

	VisitSupplBoolImpl(this, SupplierVisitFlag::NamedSuppliers,
		[this](const Actor* supplier) -> bool
		{
			auto foundItem = dynamic_cast<const TreeItem*>(supplier);
			assert(foundItem);
			if (foundItem->GetTSF(TSF_Depreciated))
			{
				SharedTreeItem prevItem = make_shared_tree(foundItem, existing_obj{}), refItem(prevItem->GetCurrRefItem());
				MG_CHECK(refItem); // follows from TSF_Depreciated
				SharedTreeItem refRefItem(refItem->GetCurrRefItem());
				while (refRefItem) {
					prevItem = refItem;
					refItem = refRefItem;
					refRefItem = refItem->GetCurrRefItem();
				}
				MG_CHECK(prevItem->GetID() != refItem->GetID());
				
				auto msg = mySSPrintF("'{}' refers by '{}' to '{}'\nReplace '{}' by '{}'."
				,	this->GetFullName()
				,	foundItem->GetFullName()
				,	prevItem->GetID()
				,	prevItem->GetID()
				,	refItem->GetID()
				);
				if (DMS_GetMajorVersionNumber() < 20)
					reportD(SeverityTypeID::ST_Warning, msg.AsRange());
				else
					throwItemError(msg);
			}
			return true;
		}
	);

	if (IsDataItem(this))
	{
		const AbstrDataItem* thisAdi = AsDataItem(this);

		// what is it?
		auto adu = thisAdi->GetDomainUnitOrThrow(); // owning, non-null or reported item error
		auto avu = thisAdi->GetValuesUnitOrThrow();
		dbg_assert(adu->CheckMetaInfoReadyOrPassor());
		dbg_assert(avu->CheckMetaInfoReadyOrPassor());
		adu->UpdateMetaInfo();
		avu->UpdateMetaInfo();
	}

	if (GetCalculatorMember() && !GetCalculatorMember()->IsDataBlock())
		ApplyCalculator(const_cast<TreeItem*>(this), GetCalculatorMember().get());

//	UpdateDC();

	if (mc_DC)
	{
		if (mc_DC->WasFailed())
			Fail(mc_DC.get());
	}

	if (HasConfigData() && GetCalculatorMember() && GetCalculatorMember()->IsDataBlock())
		return;

	if (auto refItem = mc_RefItem.lock())
	{
		refItem->UpdateMetaInfo();
		if (refItem->IsCacheRoot())
		{
			if (mc_DC && mc_DC->IsNew())
			{
				if (!refItem->GetTSF(TSF_HasPseudonym)) // can have another pseudonym
				{
					refItem->SetTSF(TSF_HasPseudonym);
#if defined(MG_DEBUG_DATA)
					refItem->md_FullName = md_FullName;
#endif
				}
				if (!GetFreeDataState() && !mc_DC->IsTransient())
					const_cast<TreeItem*>(refItem.get())->SetFreeDataState(false);
			}
			if (!this->IsCacheItem())
			{
				if (HasVisibleSubItems(refItem.get()))
					CopyTreeContext(const_cast<TreeItem*>(this), refItem.get(), "", DataCopyMode::NoRoot | DataCopyMode::MakeEndogenous | DataCopyMode::SetInheritFlag | DataCopyMode::MergeProps).Apply();
			}
		}
	}

//		if (GetStoreDataState() && refItem->IsCacheItem())
//			const_cast<TreeItem*>(refItem)->SetStoreDataState(true);
	if (IsCacheItem() || !IsDataReadable())
		return;

	assert(!HasConfigData()); // implied by IsDataReadable
}	// end of recursion protected area

// ======================================================

namespace diagnostic_tests {
	static bool TreeParenMetaInfoReadyOrFailed(const TreeItem* self)
	{
		return !self->GetTreeParent() || self->GetTreeParent()->Was(ProgressState::MetaInfo) || self->GetTreeParent()->WasFailed(FailType::MetaInfo);
	}

	[[maybe_unused]] static bool DetermineStateWasCalled(const TreeItem* self)
	{
		return TreeParenMetaInfoReadyOrFailed(self)
			&& (self->m_LastGetStateTS == UpdateMarker::GetLastTS() || self->HasConfigData() || self->InTemplate() || self->IsPassor());
	}
}

MetaInfo TreeItem::GetCurrMetaInfo(metainfo_policy_flags mpf) const
{
	// suppliers have been scanned, thus mc_Calculator and m_SupplCache have been determined.
//	assert(diagnostic_tests::DetermineStateWasCalled(this));
	assert(IsMetaThread());

	if (m_State.Get(ASF_GetCalcMetaInfo))
		throwItemError(
			"Invalid Recursion in TreeItem::GetCurrMetaInfo() detected.\n"
			"Check calculation rule of this item"
		);
	auto_flag_recursion_lock<ASF_GetCalcMetaInfo> lock(m_State);

	if (HasCalculatorImpl())
	{
		//		if (IsCacheItem() && (!HasSupplCache() || GetSupplCache()->GetNrConfigured(this) == 0) )
		const AbstrCalculator* calc = GetCalculatorMember().get();
		if (!calc)
		{
//			dms_assert(IsUnit(this)); // follows from CanSubstituteByCalcSpec()
			return LispRef{}; // let Unit::GetMetaInfo finish this
		}

		auto metaInfo = calc->GetMetaInfo();
		return metaInfo;
	}

	if (mpf & metainfo_policy_flags::recursive_check)
		throwItemError("Invalid Recursion in integrityCheck Evaluation");

	dms_assert(!IsCacheItem());

	if (mpf & metainfo_policy_flags::subst_never)
		return MetaFuncCurry{ .fullLispExpr = CreateLispTree(this, true) }; // should this result in a SymcDC to itself ? No, present this tree only in GetKeyExpr

	if (IsCurrLoadable())
		//		return CreateLispTree(this, false); // will result in a SymbDC
		//	if (IsUnit(this) || IsDerivable())
		return MetaFuncCurry{ .fullLispExpr = CreateLispTree(this, false) };

	return MetaFuncCurry{}; // not as variant 2, as that would create an infinite recursion from GetOrgDC
}

LispRef TreeItem::GetBaseKeyExpr() const
{
	auto metaInfo = GetCurrMetaInfo({});
	if (metaInfo.index() == 2)
	{
		auto& sourceItem = std::get<SharedTreeItem>(metaInfo);
		assert(!sourceItem->IsCacheItem());
		if (sourceItem.get() == this) // avoid direct self reference
			throwItemError("Invalid self reference");
		if (sourceItem.get() != this)
		{
			thread_local std::unordered_set<const TreeItem*> s_VisitingItems;
			if (!s_VisitingItems.insert(this).second)
				throwItemError("Circular key expression reference");
			auto guard = make_scoped_exit([this] { s_VisitingItems.erase(this); });
			return sourceItem->GetCheckedKeyExpr();
		}
	}
	//	if (metaInfo.index() == 0 && IsUnit(this) && std::get<MetaFuncCurry>(metaInfo).fullLispExpr.EndP())
	//		return ExprList(AsUnit(this)->GetValueType()->GetID());
//	dms_assert(metaInfo.index() != 0);
	if (metaInfo.index() == 0)
		return {};
	return std::get<LispRef>(metaInfo);
}

LispRef TreeItem::GetKeyExprImpl() const
{
	return GetBaseKeyExpr();
}

auto TreeItem::GetOrgDC() const -> std::pair<DataControllerRef, SharedTreeItem>
{
	auto metaInfo = GetCurrMetaInfo(metainfo_policy_flags::is_root_expr);
	if (metaInfo.index() == 0 && !std::get<0>(metaInfo).fullLispExpr.EndP())
		return{};
//	else 

	if (metaInfo.index() == 2)
	{
		auto srcItem = std::get<SharedTreeItem>(metaInfo);
		dms_assert(!srcItem->IsCacheItem()); // if it refers to a cache sub-item, it should have found the endogenous shadow copy in the config tree
		if (srcItem->WasFailed(FailType::MetaInfo))
			return {};
		return { std::get<SharedTreeItem>(metaInfo)->GetCheckedDC(), srcItem };
	}

	return { GetOrCreateDataController(GetKeyExprImpl()), {} };
}

static auto TreeItem_CreateConvertedExpr(const TreeItem* self, const TreeItem* cacheItem, LispPtr expr) -> LispRef
{
	if (!self->CheckResultItem(cacheItem))
	{
		assert(self->WasFailed(FailType::MetaInfo));
		return {};
	}
	auto dataItemSelf = AsDataItem(self);
	auto cacheDataItem = AsDataItem(cacheItem);

	// just check domain (again?)
	MG_CHECK(dataItemSelf->GetAbstrDomainUnit()->UnifyDomain(cacheDataItem->GetAbstrDomainUnit(), "", "", UnifyMode(UM_AllowDefaultLeft))); // GUARANTEED BY CheckResultItem

	// just check values unit (again?)
	const AbstrUnit* avu = dataItemSelf->GetAbstrValuesUnit();
	const AbstrUnit* svu = cacheDataItem->GetAbstrValuesUnit();
	MG_CHECK( avu->UnifyValues(svu, "", "", UnifyMode(UM_AllowDefaultLeft)) ); // GUARANTEED BY CheckResultItem

	// ===== this -> convert(this, formalValuesUnit) if result was DefaultUnit or allowably different.
	if ((svu->GetCurrRangeItem() != avu->GetCurrRangeItem()) && !avu->IsDefaultUnit())
	{
		auto valuesExpr = avu->GetCheckedKeyExpr();
		return slConvertedLispExpr(expr, valuesExpr);
	}
	return expr;
}

// #1180: an IntegrityCheck guards everything below the item carrying it, so the checks of this
// item AND of its ancestors are folded here, each as a condition wrapping the result expression.
// The fold keeps integrity checking inside the DataController graph: a condition is an operator
// argument, so it carries interest and is scheduled by an OperationContext, ordered against the
// primary data by CheckOperator -- the validate phase of DoUpdate then merely inspects the
// already-computed verdict (see there) instead of calculating out-of-band (#1181).
//
// Well-foundedness: a checker referencing an item inside its holder's subtree is refused as a
// circular dependency when its metainfo is built (the DoesContain gate in
// AbstrCalculator::SubstituteExpr), so a foldable check can only reference items OUTSIDE its
// holder's subtree, and the checked expressions of those can never fold this check again.
// The condition's LispRef is one and the same for every descendant, so its DataController is
// shared and the check's calculation runs once, not once per descendant.
static bool TreeItem_HasIntegrityCheckerInclAncestors(const TreeItem* self)
{
	SharedTreeItem holder; // keeps the ancestor alive while it is inspected
	for (auto guardian = self; guardian; holder = guardian->GetTreeParent(), guardian = holder.get())
		if (guardian->HasIntegrityChecker())
			return true;
	return false;
}

// Guarding an expression that already enforces the same check adds nothing: evaluating it
// evaluates the contained node, which fails on the same condition. That is the common shape
// under a checked ancestor -- an item's expression references a sibling whose own key
// expression the fold already guarded -- and without skipping, every item AND every reference
// between them carries its own copy, so one check on a root container multiplies over the
// whole configuration.
//
// The skip is decided from resultExprDC's memoized set of implied conditions (#1182), which is
// exact at any depth: it replaces a depth-bounded containment search that re-scanned the
// substituted tree on every fold and missed redundancy below its bound. Conditions are
// interned, so membership is a pointer-ordered set probe. Conditions are compared per conjunct,
// so a nearer ancestor's "a && b" also discharges an outer ancestor's "a"; the enforced set is
// seeded from the expression and extended with the guards this fold adds on top of it, which
// have no DataController of their own yet.
static auto TreeItem_CreateCheckedExpr(LispPtr resultExpr, const DataController* resultExprDC, const TreeItem* self) -> LispRef
{
	dms_assert(TreeItem_HasIntegrityCheckerInclAncestors(self));
	assert(!resultExprDC || resultExprDC->GetLispRef() == resultExpr);

	check_set enforced;
	if (resultExprDC)
		if (auto implied = resultExprDC->GetImpliedChecks())
			enforced = implied->thing;

	LispRef result = resultExpr;
	SharedTreeItem holder; // keeps the ancestor alive while its checker is folded
	for (auto guardian = self; guardian; holder = guardian->GetTreeParent(), guardian = holder.get())
	{
		if (!guardian->HasIntegrityChecker())
			continue;

		auto icCalc = guardian->GetIntegrityChecker();
		if (!icCalc)
		{
			self->Fail("Failed to construct IntegryCheck", FailType::Validate);
			return resultExpr;
		}

		auto contextForReportingPurposes = TreeItemContextHandle(guardian, "Create IntegrityCheck");

		auto ic = GetAsLispRef(icCalc->GetMetaInfo());
		if (ic.EndP())
		{
			self->Fail("Failed to construct IntegryCheck", FailType::Validate);
			return resultExpr;
		}
		if (AreCheckAtomsImplied(enforced, ic))
			continue;
		InsertCheckAtoms(enforced, ic);
		result = ExprList(token::integrity_check, result, ic);
	}
	return result;
}

void TreeItem::UpdateDC() const
{
	if (mc_DC || !mc_RefItem.expired() || WasFailed(FailType::MetaInfo) || InTemplate() || IsCacheItem() || IsPassor())
		return;

	auto [resultDC, srcItem] = GetOrgDC();
	// required for Convert test and subItem moniking, empty for applicators non-calculatable or loadable items (such as some parents).
	if (resultDC && IsDataItem(this) && !resultDC->WasFailed(FailType::MetaInfo))
	{
		if (SharedTreeItem cacheItem = resultDC->MakeResult())
		{
			auto keyExpr = TreeItem_CreateConvertedExpr(this, cacheItem.get(), resultDC->GetLispRef());
			if (!keyExpr)
			{
				assert(WasFailed(FailType::MetaInfo));
				return;
			}
			resultDC = GetOrCreateDataController(keyExpr);
		}
	}
	if (resultDC && TreeItem_HasIntegrityCheckerInclAncestors(this))
	{
		LispRef resultExpr = resultDC->GetLispRef();
		resultDC = GetOrCreateDataController(TreeItem_CreateCheckedExpr(resultExpr, resultDC.get(), this));
	}
	SetDC(resultDC, srcItem.get());
}

static auto TreeItem_GetCheckedDC_impl(const TreeItem* self) ->DataControllerRef
{
	assert(self);
	self->UpdateDC();
	return self->mc_DC;
}

auto TreeItem::GetCheckedDC() const->DataControllerRef
{
	auto resultDC = TreeItem_GetCheckedDC_impl(this);
	if (resultDC)
		return resultDC;
	if (auto refItem = mc_RefItem.lock())
	{
		assert(!refItem->IsCacheItem());
		return refItem->GetCheckedDC();
	}
	if (IsCurrLoadable() && !GetTSF(USF_HasConfigRange))
		return GetOrCreateDataController(CreateLispTree(this, false));
	return {};
}

LispRef TreeItem::GetCheckedKeyExpr() const
{
	auto dc = TreeItem_GetCheckedDC_impl(this);
	if (dc)
		return dc->GetLispRef(); // UpdateDC folded the checks of this item and of its ancestors into mc_DC (#1180)

	// Every other representation a consumer can receive is folded below, so that referencing
	// ANYTHING under a checked ancestor carries its guard -- whether the item's key comes from
	// its expression, from literal data, or from a plain reference. This is the packaging that
	// keeps integrity checking inside the DataController graph: the condition travels as an
	// operator argument, with interest, scheduled by an OperationContext (#1180, #1181).
	auto result = GetKeyExprImpl();
	if (result.EndP())
	{
		dms_assert(!IsCacheItem());
		if (IsDataItem(this) && AsDataItem(this)->HasDataObj() && !IsLoadable())
		{
			auto adi = AsDataItem(this);
			auto valueList = AsDataItem(this)->GetDataObj()->GetValuesAsKeyArgs(adi->GetAbstrValuesUnit()->GetCheckedKeyExpr());
			if (adi->HasVoidDomainGuarantee())
			{
				assert(valueList.IsRealList());
				assert(valueList.Right().EndP());
				result = valueList.Left();
			}
			else if (valueList.EndP())
				result = ExprList(token::const_
					,	ExprList(adi->GetAbstrValuesUnit()->GetValueType()->GetID()
						,	LispRef(Number(0))
						)
					,	adi->GetAbstrDomainUnit()->GetCheckedKeyExpr()
					);
			else
			{
				// one or more values, so we need a union
				assert(valueList.IsRealList());
				result = LispRef(
					LispRef(adi->m_StatusFlags.HasSortedValues() ? token::ordered_union_data : token::union_data)
					, LispRef(adi->GetAbstrDomainUnit()->GetCheckedKeyExpr()
						, valueList
					)
				);
			}
		}
		else
		{
			// required for Convert test and subItem moniking, empty for applicators non-calculatable or loadable items (such as some parents).
			this->DetermineState();
			result = CreateLispTree(this, false);
		}
	}
	if (TreeItem_HasIntegrityCheckerInclAncestors(this))
		result = TreeItem_CreateCheckedExpr(result, nullptr, this); // no DC on this path: implied set unknown, wrap all
	return result;
}

#if defined(MG_DEBUG)
bool TreeItem::CheckMetaInfoReady() const
{
	return m_State.GetProgress()>=ProgressState::MetaInfo || WasFailed(FailType::MetaInfo);
}

bool TreeItem::CheckMetaInfoReadyOrPassor() const
{
	return IsPassor() || CheckMetaInfoReady();
}

#endif

auto TreeItem::GetBackRef() const -> SharedTreeItem
{
//	dms_assert(IsMetaThread());
	return m_BackRef.lock();
}

auto TreeItem::GetFullCfgName() const -> SharedStr
{
	const TreeItem* cfgItem = this;
	SharedTreeItem backHolder; // owns the current back-ref target while walking
	while (cfgItem->IsCacheRoot())
	{
		backHolder = cfgItem->GetBackRef();
		if (!backHolder)
			break;
		cfgItem = backHolder.get();
	}
	return cfgItem->GetFullName();
}

void TreeItem::UpdateMetaInfoImpl2() const
{
	dbg_assert(IsMetaThread());

	if (m_LastGetStateTS >= UpdateMarker::LastTS())
		if ((m_State.GetProgress()>=ProgressState::MetaInfo) || WasFailed(FailType::MetaInfo)) // reset by DetermineState when supplier was invalidated
			return;

	try {
		if(m_State.IsDeterminingState() || m_State.IsUpdatingMetaInfo() || m_State.Get(ASF_MakeCalculatorLock) )
		{
			throwItemError(
				"Invalid Recursion in UpdateMetaInfo detected.\n"
				"Check calculation rule and other referring properties of this item and/or its Suppliers\n"
				"Suggestion: check context for ApplyMetaFunc calls that may scan a range of sub-items"
			);
		}
	//	dms_assert(IsPassor() || !SuspendTrigger::DidSuspend());

		FencedInterestRetainContext retainLocalInterestUntilThisDies("UpdateMetaInfo");

		// DetermineState() -> DoInvalidate() could reset TSF_MetaInfoReady
		if (IsPassor())
		{
			if (!WasFailed())  // Passors can fail due to PrepareDataUsage that inherits failure from DataControiller
				SetMetaInfoReady();
			return;
		}

		if (GetTreeParent())
			GetTreeParent()->UpdateMetaInfoIfNotAlready();
	
		if (HasStorageManager())
			GetStorageManager();

		DetermineState();
		if ((m_State.GetProgress()>=ProgressState::MetaInfo) || WasFailed(FailType::MetaInfo)) // reset by DetermineState when supplier was invalidated
			return;


		MG_SIGNAL_ON_UPDATEMETAINFO

		DBG_START("TreeItem", "UpdateMetaInfo", MG_DEBUG_UPDATEMETAINFO && false);
		DBG_TRACE(("fullname = {}", GetFullName().c_str()));

		TreeItemContextHandle tdc(this, "UpdateMetaInfo");
		auto waiter = Waiter(&tdc);

		assert(m_LastChangeTS || IsPassor()); // PRECONDITION for SetProgress, guaranteed by IsDeterminingState() || IsPassor() || DetermineState()

		StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
		UpdateMarker::ChangeSourceLock lock(this, "TreeItem::UpdateMetaInfoImpl");

		assert(!WasFailed(FailType::MetaInfo));

		// begin of recursion protected area
		{
			dms_check_not_debugonly;
			UpdateLock lock2(this, actor_flag_set::AF_UpdatingMetaInfo);

			UpdateMetaInfoImpl(); // recursion protected part of UpdateMetaInfo

			if (m_UsingCache)
				m_UsingCache->GetNrUsings();
		}
		SetMetaInfoReady();
		if (!WasFailed(FailType::MetaInfo))
		{

			// Update Meta Info according to storage manager
			auto storageParent = GetStorageParent(false);
			if (storageParent)
			{
				auto sm = storageParent->GetStorageManager();
				sm->UpdateTree(storageParent.get(), const_cast<TreeItem*>(this));
			}
			// validate units with refObject if it wasn't copied by the parent
		}
		ProcessMainThreadOpers();
	}
	catch (...)
	{
		// don't try again
		if (m_State.GetProgress() <= ProgressState::MetaInfo)
			m_State.SetProgress(ProgressState::MetaInfo);
		CatchFail(FailType::MetaInfo);
	}
	assert(m_State.GetProgress() >= ProgressState::MetaInfo);
}

#include <future>

void TreeItem::UpdateMetaInfo() const noexcept
{
	assert(IsMetaThread());
	auto remainingStackSpace = RemainingStackSpace();
	if (remainingStackSpace <= 327680)
	{
		// just use async to start a new thread.
		auto future = std::async([this] ()->void
			{
				SetMetaThreadID();
				assert(IsMetaThread());
				this->UpdateMetaInfoImpl2();
			}
		);
		future.get();
		SetMetaThreadID();
		assert(IsMetaThread());
	}
	else
		UpdateMetaInfoImpl2();
}

ActorVisitState TreeItem::SuspendibleUpdate() const
{
	UpdateMetaInfo();
	auto remainingStackSpace = RemainingStackSpace();
	if (remainingStackSpace <= 327680)
	{
		// just use async to start a new thread.
		auto future = std::async([this]()->ActorVisitState
			{
				SetMetaThreadID();
				assert(IsMetaThread());
				return this->base_type::SuspendibleUpdate();
			}
		);
		ActorVisitState result = future.get();
		SetMetaThreadID();
		assert(IsMetaThread());
		return result;
	}
	return base_type::SuspendibleUpdate();
}

void TreeItem::UpdateMetaInfoIfNotAlready() const noexcept
{
	if (m_State.IsDeterminingState() || m_State.IsUpdatingMetaInfo() || m_State.Get(ASF_MakeCalculatorLock))
		return;
	UpdateMetaInfo();
}

bool IntegrityCheckFailure(const TreeItem* self, const AbstrDataItem* iCheckerResult, std::function<SharedStr()> checkStringGenerator)
{
	SizeT nrFailures = iCheckerResult->CountValues<Bool>(false);
	if (!nrFailures)
		return false;
	SharedStr helperText = SingleQuote(checkStringGenerator().c_str());
	if (iCheckerResult->GetAbstrDomainUnit()->GetCount() == 1)
	{
		assert(nrFailures == 1);

		helperText += " is not true";
	}
	else
	{	
		auto failurePos = iCheckerResult->FindPos<Bool>(false, 0);
		SizeT oxfordComma = nrFailures >= 3;
		if (nrFailures > 1)
		{
			helperText = mySSPrintF("{} elements of {} are not true, at row {}"
				, nrFailures
				, helperText
				, failurePos
			);
			MakeMin(nrFailures, 4); // max 4 extra rows to report
			while (nrFailures-- > 0)
			{
				failurePos = iCheckerResult->FindPos<Bool>(false, failurePos + 1);
				if (!IsDefined(failurePos))
					break;

				CharPtr format = nrFailures ? ", {}" : oxfordComma ? ", and {}" : " and {}";
				helperText += mySSPrintF(format, failurePos);

			}
			if (IsDefined(failurePos))
				helperText += ", etc.";
		}
		else
		{
			helperText += mySSPrintF(" is not true at row {}", failurePos);
		}
	}

	// will be caught by SuspendibleUpdate who will Fail this.
	self->Fail(mySSPrintF("{} : {}", ICHECK_NAME, helperText), FailType::Validate); // will be caught by SuspendibleUpdate who will Fail this.

	assert(self->WasFailed(FailType::Validate));
	return true;
}

// #1180: validate self against its OWN IntegrityCheck only; the ancestors' verdicts arrive by
// validating the parent first -- which covers ITS ancestors the same way -- and inheriting a
// validate failure. This walks each chain once per update sweep, instead of every descendant
// re-walking all its ancestors: a parent whose own DoUpdate already ran answers from its progress
// and failure state, and a parent validated from here answers from the condition DataController,
// which the evaluation below leaves Validated (shared by identity with the #1180-folded
// conditions, deduplicated per #1182).
//
// Returns AVS_SuspendedOrFailed for SUSPENSION only; a verdict, either way, returns AVS_Ready
// and a failure is recorded on self (FailType::Validate). No progress is marked here: self's
// DoUpdate does that, and a parent validated on behalf of a descendant must not skip ahead of
// its own data phase.
static ActorVisitState TreeItem_ValidateIntegrity(const TreeItem* self)
{
	assert(self);

	if (SharedTreeItem parent = self->GetTreeParent(); parent
		&& !parent->IsPassor() && !parent->IsCacheItem() && !parent->InTemplate())
	{
		if (!parent->WasFailed(FailType::Validate) && parent->m_State.GetProgress() < ProgressState::Validated)
			if (TreeItem_ValidateIntegrity(parent.get()) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		if (parent->WasFailed(FailType::Validate))
		{
			self->Fail(parent.get());
			return AVS_Ready;
		}
	}

	if (!self->HasIntegrityChecker())
		return AVS_Ready;

	try
	{
		TreeItemContextHandle tich2(self, "IntegrityCheck Evaluation");

		auto iCheckerPtr = self->GetIntegrityChecker();
		assert(iCheckerPtr);

		auto iCheckerDC = MakeResult(iCheckerPtr.get());
		assert(iCheckerDC);
		if (iCheckerDC->WasFailed(FailType::Validate))
		{
			self->Fail(iCheckerDC.get());
			return AVS_Ready;
		}
		if (!iCheckerDC->Was(ProgressState::Validated))
		{
			// The verdict a folded check (#1180) computed during data preparation is NOT
			// guaranteed to still be resident here: once the wrapping IntegrityCheck operator
			// has consumed its condition argument, the argument interest is released and the
			// condition's DataController may be re-armed empty. Re-evaluation therefore goes
			// through CalledCalcHandle, which takes its own interest and schedules through an
			// OperationContext -- recomputing from whatever sub-results are still retained.
			iCheckerDC = CalledCalcHandle(iCheckerPtr.get(), DataArray<Bool>::GetStaticClass()); // @@@SCHEDULE

			if (SuspendTrigger::DidSuspend())
				return AVS_SuspendedOrFailed;

			// #1181 backstop, also in Release: primary data evaluated on behalf of an
			// integrity check must be under interest and scheduled -- CalledCalcHandle
			// guarantees both by construction, and this pins that contract where a bypass
			// (evaluating the checker without taking interest) would otherwise regress
			// silently, since the out-of-band answer is still the right verdict.
			MG_CHECK(iCheckerDC && iCheckerDC->GetInterestCount());

			DataReadLockContainer c;
			auto iCheckerFD = iCheckerDC->CallCalcResult(nullptr);// @@@USE
			if (!iCheckerFD)
			{
				if (SuspendTrigger::DidSuspend())
					return AVS_SuspendedOrFailed;
				assert(iCheckerDC->WasFailed(FailType::Data));
				self->Fail(iCheckerDC.get_ptr());
				assert(self->WasFailed());
				return AVS_Ready;
			}

			SharedDataItem iCheckerResult = make_shared_tree(AsDynamicDataItem(iCheckerDC->GetOld()), existing_obj{});
			if (iCheckerResult)
			{
				assert(iCheckerResult->GetInterestCount());

				std::shared_ptr<const TreeItem> adiCheckerResult = iCheckerResult->GetCurrUltimateItem();
				assert(adiCheckerResult->GetInterestCount());
				if (!WaitForReadyOrSuspendTrigger(adiCheckerResult.get()))
				{
					if (adiCheckerResult->WasFailed())
					{
						self->Fail(adiCheckerResult.get());
						return AVS_Ready;
					}
					assert(SuspendTrigger::DidSuspend());
					return AVS_SuspendedOrFailed;
				}
			}
			if (!iCheckerResult || !c.Add(iCheckerResult.get(), DrlType::Suspendible))
			{
				if (SuspendTrigger::DidSuspend())
					return AVS_SuspendedOrFailed;
				assert(iCheckerDC->WasFailed(FailType::Data) || !iCheckerResult || iCheckerResult->WasFailed(FailType::Data));
				if (iCheckerDC->WasFailed(FailType::Data))
					self->Fail(iCheckerDC.get_ptr());
				else if (iCheckerResult && iCheckerResult->WasFailed(FailType::Data))
					self->Fail(iCheckerResult.get());
				else
					self->Fail("Unknown error in IntegrityCheck: ", FailType::MetaInfo);
				assert(self->WasFailed());
				return AVS_Ready;
			}

			IntegrityCheckFailure(self, iCheckerResult.get(), [iCheckerPtr]() { return iCheckerPtr->GetExpr(); });
		}
	}
	catch (...)
	{
		auto err = catchException(false);
		self->DoFailCaller(err, FailType::Validate);
	}
	return AVS_Ready;
}

ActorVisitState TreeItem::DoUpdate()
{
	DBG_START("TreeItem", "DoUpdate", MG_DEBUG_UPDATEMETAINFO && false);
	DBG_TRACE(("fullname = {}", GetFullName().c_str()));

	assert(m_State.GetProgress() >= ProgressState::MetaInfo); //UpdateMetaInfo();

	assert(m_State.GetProgress()>=ProgressState::MetaInfo);

	TreeItemContextHandle tich(this, "Update");

	if (IsPassor() || IsCacheItem())
		return AVS_Ready;

	if (!IsDataItem(this) && !IsUnit(this))
		SetIsInstantiated();

	if ( InTemplate() )
		goto exitReady;

	if (auto dc = GetOrgDC().first)
		if (auto fc = dynamic_cast<const FuncDC*>(dc.get()))
			if (fc->m_OperatorGroup->GetNameID() == token::PhaseContainer)
				if (auto fd = dc->CallCalcResult())
					if (auto oc = fc->GetOperContext())
						if (oc->GetStatus() != task_status::none)
						{
							auto pcResult = oc->Join();
							if (pcResult != task_status::done)
							{
								if (pcResult == task_status::exception)
									Fail(oc->m_Result);
								return ActorVisitState::AVS_SuspendedOrFailed;
							}
						}

	if (m_State.GetProgress() < ProgressState::Validated)
	{
		if (WasFailed(FailType::Validate))
		{
			m_State.SetProgress(ProgressState::Validated);
			return AVS_SuspendedOrFailed;
		}
		// #1180: an IntegrityCheck guards everything below the item carrying it. The ancestral
		// verdicts arrive through the tree structure: the parent is validated first -- covering
		// ITS ancestors the same way -- and a validate-failed parent fails this item. Only this
		// item's OWN check is evaluated here, so a chain of ancestors is walked once per update
		// sweep instead of once per descendant; a parent whose own DoUpdate already ran answers
		// from its progress and failure state.
		// For an item with a DataController the checks were also folded into it as conditions
		// (TreeItem_CreateCheckedExpr), scheduled by an OperationContext with interest; the
		// evaluation below then finds the shared condition DataController ready and only inspects
		// the verdict. It still calculates for items without a DataController, i.e. when the item
		// itself is the request: a plain tree update demands no data, so the folded operator
		// never runs there and this is the only evaluator (measured, see #1182).
		if (TreeItem_ValidateIntegrity(this) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed; // suspended; not yet Validated, retried on resume
		if (WasFailed(FailType::Validate))
		{
			m_State.SetProgress(ProgressState::Validated);
			return AVS_Ready; // data remains viewable; commit is skipped
		}
		SetProgress(ProgressState::Validated);
	}

	if (m_State.GetProgress() < ProgressState::Committed)
	{
		//	TODO, Uitzoeken wanneer Commit overbodig is (zoals indien in vorige sessie reeds gedaan).
		//	Voorlopig antwoord: wanneer er geen changes zijn, is er geen cause voor invalidatatie en mag DoUpdate helemaal niet aangeroepen worden.
		//	probleem: Als export file is weggegooid, moet de DoUpdate dan opnieuw worden uitgevoerd? 
		//	probleem: verschillende ItemCommits kunnen dezelfde export timestampen.
		bool result = CommitDataChanges(); // @@@SCHEDULE AND USE

		if (SuspendTrigger::DidSuspend())
			return AVS_SuspendedOrFailed;

		assert(result || WasFailed(FailType::Committed));

		SetProgress(ProgressState::Committed);

		auto uti = _GetHistoricUltimateItem(this);
		actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("TreeItem::CommitDataChanges") sg_ActorLockMap, uti.get());
		uti->TryCleanupMem();

		if (!result) 
		{
			assert(WasFailed(FailType::Committed));
			return AVS_SuspendedOrFailed;
		}
	}

exitReady:
	assert(!IsCacheItem());
	assert(!IsPassor());
	if (!MustApplyImpl() && (IsPassor() || !IsCacheItem()))
		StopSupplInterest(); // Commit has only interest on this; validate has its own interest path

	return AVS_Ready;
}

void TreeItem::SetProgress(ProgressState ps) const
{
	ProgressState oldProgress = m_State.GetProgress();
	Actor::SetProgress(ps); // changes m_State to US_Valid

	assert(ps >= oldProgress);

	if (ps > ProgressState::MetaInfo && oldProgress < ps)
	{
		dms_assert(ps >= ProgressState::Validated);
		NotifyStateChange(this, ps == ProgressState::Validated ? NC2_Validated : NC2_Committed);
	}
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
			auto dialogDataItem = FindItem(dialogData.AsRange());
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

	if (Test(svf, SupplierVisitFlag::DetermineCalc))
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
// Read / Write Data
//----------------------------------------------------------------------

bool TreeItem::ReadItem(StorageReadHandle&& srh) // TODO: Make this a method of StorageReadHandle
{
	MG_DEBUGCODE( dms_assert( CheckMetaInfoReady() ); )

	assert(! GetCurrRefItem() ); // caller must take care of only calling ReadItem for UltimateItems

	MG_SIGNAL_ON_UPDATEMETAINFO

	auto keepInterest = GetInterestPtrOrCancel();

	if (WasFailed(FailType::Data))
		return false;
	if (IsDataReady(this))
		return true;

	const TreeItem* storageParent = GetCurrStorageParent(false).get();
	if (!storageParent)
		return false;
	
	TreeItemContextHandle checkPtr(this, "TreeItem::ReadItem");

	if (SuspendTrigger::MustSuspend())
		return false;

	try
	{
		auto progressMsg = mySSPrintF("Read {} from {}"
			, GetName()
			, storageParent->GetStorageManager()->GetNameStr()
		);

		reportD(MsgCategory::storage_read, SeverityTypeID::ST_MajorTrace, progressMsg.AsRange());

		if (srh.Read())
			return true;
		else if (!SuspendTrigger::DidSuspend())
			throwItemError("DoReadItem returned Failure");
		assert(GetInterestCount());
	} 
	catch (...)
	{
 		assert(!HasCurrConfigData());

		if (!WasFailed(FailType::Data)) {
			auto err = catchException(true);
			err->TellExtraF("while reading data from {}", DMS_TreeItem_GetAssociatedFilename(this));
			DoFailCaller(err, FailType::Data);
		}
	}
	return false;
}

bool TreeItem::DoReadItem(StorageMetaInfoPtr smi)
{
	return false;
}

bool TreeItem::DoWriteItem(StorageMetaInfoPtr&&) const
{
	// can return false because of suspension or failure
	// caller must check state and suspend trigger to find out
	if (HasCalculator())
	{
		auto apr = GetCalculator();
		if (!apr)
		{
			assert(IsUnit(this));
			return true;
		}
		auto result = CalledCalcHandle(apr.get(), GetDynamicObjClass());
		if (!result)
		{
			dms_assert(SuspendTrigger::DidSuspend() || WasFailed());
			return false;
		}
		if (result->IsFailed())
		{
			Fail(result.get_ptr());
			return false;
		}
		if (!result->GetOld())
		{
			dms_assert(SuspendTrigger::DidSuspend());
			return false;
		}
	}
	return true;
}

//=============================== ConcurrentMap (client is responsible for scoping and stack unwinding issues)


treeitem_lock_map sg_PrepareDataUsageLockMap("PrepareDataUsage");

bool TreeItem::PrepareDataUsage(DrlType drlFlags) const 
// returns false when 
//	- failed without data or 
//	- suspendend or 
//	- no calcrule etc and not a dataitem
//	doesn't suspend when drlType == DrlType::Certain, 
//	but can still fail, thus IsFailed() == true and return false
{
	dms_assert(m_State.GetProgress() >= ProgressState::MetaInfo || IsPassor() || WasFailed(FailType::Data));
	if (UpdateMarker::PrepareDataInvalidatorLock::IsLocked())
		drlFlags = DrlType(UInt32(drlFlags) & ~UInt32(DrlType::UpdateMask));

	dms_assert(!IsTemplate()); // formation of FuncDC's should prevent args to be calculated that fail to meet this precondition
	dms_assert(IsMetaThread() || !(UInt32(drlFlags) & UInt32(DrlType::UpdateMask)));

	if ((UInt32(drlFlags) & UInt32(DrlType::Certain)) && !SuspendTrigger::BlockerBase::IsBlocked())
	{
		SuspendTrigger::FencedBlocker lockSuspend("@TreeItem::PrepareDataUsage");
		auto result = PrepareDataUsageImpl(drlFlags);
		dms_assert(result || WasFailed());
		return result;
	}

	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult
	auto result = PrepareDataUsageImpl(drlFlags);
	dms_assert(result || SuspendTrigger::DidSuspend() || WasFailed());
	return result;
}

enum class how_to_proceed { nothing, data_ready, failed, suspended, suspended_or_failed}; // return_suspended_or_failed, return_OK ;

static how_to_proceed PrepareDataCalc(std::shared_ptr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	dms_assert(!SuspendTrigger::DidSuspend() && !self->WasFailed(FailType::Determine)); // Postcondition when CreateResultingTreeItem returns a result

//				FutureData dc = GetDC(GetCalculator());
//	self->UpdateDC();
	FutureData dc = self->GetCheckedDC();
//	dms_assert(dc || self->GetCurrRefItem());
//	if (!dc) dc = GetDC(self->GetCalculator()); // TODO G8: unwind recursion
//	dms_assert(dc);
	//				const AbstrCalculator* apr = GetCalculator();
	//				dms_assert(apr); // guaranteed by HasCalculator
	//				dms_assert(!SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns a result
	dms_check(self->HasInterest());

	if (dc)
	{
		auto dc2 = dc->CallCalcResult();
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;

		dms_assert(dc2 || SuspendTrigger::DidSuspend() || dc->WasFailed(FailType::Data));
		if (dc->WasFailed()) //  && !WasFailed())
		{
			self->StopSupplInterest();
			self->Fail(dc.get_ptr());
		}
		if (self->WasFailed(FailType::Data))
			return how_to_proceed::failed;
		if (!dc->GetOld())
		{
			dms_assert(SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns no result, yet hasn't failed
			return how_to_proceed::suspended;
		}
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;
		self->StopSupplInterest();
		dms_assert(dc2);
	}
	else
	{
		if (!refItem->IsCacheItem())
		{
			bool res = refItem->PrepareDataUsage(DrlType::Certain);
			if (refItem->IsFailed())
				self->Fail(refItem);
			if (!res)
				return how_to_proceed::suspended_or_failed;
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns a result

	if (dc && dc->GetOld() != self.get() && !dc->GetOld()->IsCacheItem()) // could be config item that can be read from external source
	{
		bool res = dc->GetOld()->PrepareDataUsageImpl(drlFlags);
		if (dc->GetOld()->IsFailed())
			self->Fail(dc->GetOld());

		if (!res)
			return how_to_proceed::suspended_or_failed;
	}
	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides MustSuspend
	if (CheckCalculatingOrReady(refItem))
	{
		dms_assert(!self->WasFailed(FailType::Data));
		return how_to_proceed::data_ready;
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // PrepareDataUsage loads from cache if possible
	return how_to_proceed::nothing;
}

static how_to_proceed PrepareDataRead(std::shared_ptr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	// PSEUDOCODE:
	// - Ensure refItem is data-readable and not a cache item.
	// - Prepare all suppliers (Calc group) of 'self'; on failure/suspend, propagate result.
	// - If refItem is a DataItem, ensure its domain unit (adu) can provide cardinality.
	// - Acquire storage manager and meta info to start a read OperationContext (oc).
	// - Before scheduling/starting the read oc:
	//     - If 'self' is a DataItem:
	//         - Fetch its domain unit (adu).
	//         - If the adu has a DataController, add its FutureData to futureSuppliers so the adu calculation
	//           is guaranteed to run before/with this read Operation.
	// - Track oc in self->m_ReadAssets to keep it alive.
	// - Return data_ready/suspended/failed based on status and triggers.

	MG_DEBUGCODE(dms_assert(!refItem->HasCalculatorImpl())); // implied by IsDataReadable
	dms_assert(!refItem->IsCacheItem());        // how else to derive data

	// Collect this read's future suppliers. Declared before the visit so the visited Calc-suppliers -- which for
	// a storage read include the storage manager's own data suppliers (e.g. strfiles FileName, via
	// StrFilesStorageManager::VisitSuppliers) -- are added as futures here. Each future keeps its supplier
	// calculated and of-interest from now, through GetMetaInfo below, and (once moved into the read
	// OperationContext) across the read. This closes the gap where a storage supplier's SupplInterest was dropped
	// on data-generation before a (re)read still needed it: a read OC keeps its suppliers of-interest, but only
	// the ones it is told about. Domain/values units are NOT visited under the Calc flag, so their futures are
	// still added explicitly below.
	FutureSuppliers futureSuppliers;

	auto supplResult = VisitSupplBoolImpl(self.get(), SupplierVisitFlag::Calc, [&futureSuppliers](auto a) -> bool
		{
			auto t = dynamic_cast<const TreeItem*>(a);
			if (t)
				if (auto dc = t->GetCheckedDC())
				{
					FutureData tmpFut = dc; // hold interest while obtaining the future (the DC might not yet have interest)
					if (auto fut = dc->CallCalcResult())
						futureSuppliers.emplace_back(std::move(fut));
					else if (SuspendTrigger::DidSuspend())
						return false;
				}
			return true;
		}
	);
	if (supplResult == AVS_SuspendedOrFailed)
	{
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;
		else
		{
			assert(self->WasFailed(FailType::Data));
			return how_to_proceed::failed;
		}
	}

	assert(!SuspendTrigger::DidSuspend());

	if (IsDataItem(refItem))
	{
		const AbstrUnit* adu = AsDataItem(refItem)->GetAbstrDomainUnit();
		if (!adu->PrepareDataUsageImpl(drlFlags)) // make sure that the cardinality can be calculated
		{
			if (!adu->WasFailed(FailType::Data))
				return how_to_proceed::suspended;
			self->Fail(adu);
			return how_to_proceed::failed;
		}
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // was tested before and nothing could have started the calculation

	auto storageParent = refItem->GetStorageParent(false); assert(storageParent);
	SharedPtr<AbstrStorageManager> sm = storageParent->GetStorageManager(); assert(sm);
	if (auto nmsm = MakeSharedFromBorrowedObjectPtr(dynamic_cast<NonmappableStorageManager*>(sm.get())))
		if (StorageMetaInfoPtr readInfo = nmsm->GetMetaInfo(storageParent.get(), const_cast<TreeItem*>(refItem), StorageAction::read))
		{
			// #933: resolve pre-lock prerequisites HERE (suspendable, on the requesting thread) so the
			// gated read payload performs only the locked read and never waits while holding the CS.
			readInfo->PrepareReadDataOrSuspend();
			if (SuspendTrigger::DidSuspend())
				return how_to_proceed::suspended;

			auto readInfoPtr = std::make_shared<std::atomic<StorageMetaInfoPtr>>(std::move(readInfo));
			assert(!CheckCalculatingOrReady(refItem));

			using OcPtr = std::atomic<std::shared_ptr<OperationContext>>;
			std::shared_ptr<OcPtr> ocPtrPtr = std::make_shared<OcPtr>();

			// futureSuppliers was started by the Calc-supplier visit above (storage data suppliers like FileName).
			// Domain/values units are visited under the DomainValues flag, not Calc, so add their futures here too.
			dms_check(refItem->GetInterestCount());
			if (IsDataItem(refItem))
			{
				if (auto refAdu = AsDataItem(refItem)->GetAbstrDomainUnit())
				{
					dms_check(refAdu->GetInterestCount());
					// If the domain unit has a DataController, make it a future supplier.
					if (auto aduDC = refAdu->GetCheckedDC())
					{
						FutureData tmpFut = aduDC; // SymbDC might not yet have interest
						auto fut = aduDC->CallCalcResult();
						if (fut)
							futureSuppliers.emplace_back(std::move(fut));
					}
				}
				if (auto refAvu = AsDataItem(refItem)->GetAbstrValuesUnit())
				{
					dms_check(refAvu->GetInterestCount());
					// If the values unit has a DataController, make it a future supplier.
					if (auto avuDC = refAvu->GetCheckedDC())
					{
						FutureData tmpFut = avuDC; // SymbDC might not yet have interest
						auto fut = avuDC->CallCalcResult();
						if (fut)
							futureSuppliers.emplace_back(std::move(fut));
					}
				}
			}

			*ocPtrPtr = OperationContext::CreateItemWriter(const_cast<TreeItem*>(refItem),
				[storageParent
				, ocWeakPtrPtr = std::weak_ptr<OcPtr>(ocPtrPtr)
				, readInfoPtr
				, nmsm](OperationContext* ocPtr, explain_context_ptr_t /*context*/)
				{
					auto onExit = make_scoped_exit([ocWeakPtrPtr]() {
						if (auto ocSharedPtrPtr = ocWeakPtrPtr.lock())
							*ocSharedPtrPtr = std::shared_ptr<OperationContext>();
					});
					assert(readInfoPtr);
					assert(readInfoPtr->load());
					auto smi = readInfoPtr->exchange(StorageMetaInfoPtr());
					assert(!readInfoPtr->load());
					if (ocPtr->m_StorageLockHeld) // #933: gate already acquired the CS; adopt it
					{
						ocPtr->m_StorageLockHeld = false; // ownership transfers to sHandle on the next line
						StorageReadHandle sHandle(nmsm.get(), std::move(smi), adopt_storage_lock);
						sHandle.FocusItem()->ReadItem(std::move(sHandle)); // Read Item
					}
					else // runDirect / inline path: no gate ran, acquire normally
					{
						StorageReadHandle sHandle(nmsm.get(), std::move(smi)); // locks storage manager
						sHandle.FocusItem()->ReadItem(std::move(sHandle)); // Read Item
					}
				}
				, std::move(futureSuppliers)
				, false
				, nmsm // #933: gate on this storage manager
			);

			if (auto loadedPtr = ocPtrPtr->load())
				if (loadedPtr->GetStatus() < task_status::running)
				{
					self->m_ReadAssets.emplace<std::shared_ptr<OcPtr>>(ocPtrPtr);
					self->SetTSF(TSF_ReadAssetsInterestScoped); // interest-scoped: StopInterest releases it when refItem goes out of interest
				}

			readInfoPtr.reset();
			assert(CheckCalculatingOrReady(refItem) || refItem->WasFailed(FailType::Data) || SuspendTrigger::DidSuspend());
			assert(self->GetInterestCount());
		}

	if (refItem->IsFailed())
	{
		if (refItem != self.get())
			self->Fail(refItem);
		return how_to_proceed::failed;
	}

	if (SuspendTrigger::DidSuspend())
		return how_to_proceed::suspended;

	if (!CheckCalculatingOrReady(refItem))
	{
		// #1152: no read was scheduled; a mappable storage manager (.mmd) serves data items by file
		// mapping in PrepareDataUsageImpl and unit ranges from the dictionary's Range subtag, so
		// reaching this point means the storage holds no primary data for this item, e.g. a
		// half-written .mmd whose dictionary lacks this unit's Range and that has no data folder for
		// it. Fail rather than report readiness: PrepareDataUsage's contract is that a true return
		// leaves the item calculating-or-ready or failed, and from a not-ready, not-failed state every
		// consumer breaks (ItemReadLock's WasFailed check, operators dereferencing GetTiledRangeData()).
		self->Fail(mySSPrintF("no primary data found in storage {} for {}"
			, sm->GetNameStr(), refItem->GetFullName()), FailType::Data);
		return how_to_proceed::failed;
	}
	return how_to_proceed::data_ready;
}

bool TreeItem::PrepareDataUsageImpl(DrlType drlFlags) const
// returns false when 
//	- failed without data or 
//	- suspendend or 
//	- no calcrule etc and not a dataitem
//	doesn't suspend when drlType == DrlType::Certain, 
//	but can still fail, thus IsFailed() == true and return false
{
	UpdateMetaInfo();
	bool throwOnFail = UInt32(drlFlags) &  UInt32(DrlType::ThrowOnFail);

	DrlType drlType = DrlType(UInt32(drlFlags) & UInt32(DrlType::UpdateMask));
	dms_assert(drlType <= DrlType::Certain);

	dms_assert(!IsTemplate()); // formation of FuncDC's should prevent args to be calculated that fail to meet this precondition

	assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult

	assert(!(UInt32(drlType) &  UInt32(DrlType::Certain)) || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility

	// Checks State against suppliers if any changes occured after m_LastCheckedTS and Invalidates if any changes occured in any supplier

	UpdateMarker::ChangeSourceLock changeStamp(this, "PrepareDataUsage");

	assert(IsPassor() || HasConfigData() || (m_State.GetProgress()>=ProgressState::MetaInfo) || WasFailed(FailType::MetaInfo)); // reset by DetermineState when supplier was invalidated

	const TreeItem* refItem = nullptr;

	treeitem_lock_map::ScopedTryLock localPreparedataLock(MG_SOURCE_INFO_CODE("TreeItem::PrepareDataUsageImpl ScopedTry") sg_PrepareDataUsageLockMap, this);
	if (!localPreparedataLock)
	{
		if (!WaitForReadyOrSuspendTrigger(this))
			goto suspended_or_failed;

		assert(!SuspendTrigger::DidSuspend());
		assert(!WasFailed(FailType::Data));
		assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(GetCurrUltimateItem().get()));
		goto data_ready;
	}

	if (m_State.IsDataFailed())  // may have been arranged in an alternative thread.
		goto failed_norefitem;
	refItem = GetCurrUltimateItem().get();

	assert(refItem->IsPassor() || HasConfigData() || refItem->m_State.GetProgress() >= ProgressState::MetaInfo || refItem->WasFailed(FailType::MetaInfo));
	assert(GetInterestCount() || !IsDataItem(this)); // interest consistency

	if (CheckCalculatingOrReady(refItem)) // quick route first
		goto data_ready; // may have been arranged in an alternative thread.
	if (IsDataItem(this))
	{
		auto avu = AbstrValuesUnit( AsDataItem(this) );
		if (avu && !avu->IsCacheItem())
		{
			if (!avu->PrepareDataUsage(drlFlags))
			{
				if (!SuspendTrigger::DidSuspend())
					Fail(avu);
				return false;
			}
		}
		if (!IsCacheItem())
		{
			if (auto sp = GetCurrStorageParent(false))
			{
				auto sm = sp->GetStorageManager();
				assert(sm);
				if (auto mmd = dynamic_cast<MmdStorageManager*>(sm))
				{
					bool mustWrite = HasCalculator() && !GetCalculator()->IsDataBlock();
					bool mustSkip = HasCalculator() && GetCalculator()->IsDataBlock();
					if ((!mustSkip && !mmd->IsOpen()) || (mustWrite && !mmd->IsOpenForWrite()))
					{
						auto parent = GetStorageParent(mustWrite);
						if (!parent)
							mustSkip = true;
						else
						{
							auto lock = AbstrStorageManager::lock_t(mmd->m_CriticalSection);
							auto smi = StorageMetaInfo(parent.get(), this);
							if (mustWrite)
								mmd->OpenForWrite(smi);
							else
								mmd->OpenForRead(smi);
						}
					}
					if (!mustSkip && !mustWrite)
					{
						auto fsn = sm->GetNameStr();
						auto rn = GetRelativeName(sp.get());
						if (rn.empty())
						{
							rn = "@main";
						}

						auto fn = DelimitedConcat(fsn, rn);
						if (!IsFileOrDirAccessible(fn))
						{
							DoFailCaller(std::make_shared<ErrMsg>("Data not found in .MMD storage folder"), FailType::Data);
							goto failed;
						}
						else
						{
							// fixes #1028?
							auto adu = AsDataItem(this)->GetAbstrDomainUnit();
							if (!adu->PrepareData())
							{
								if (adu->WasFailed())
									Fail(adu);	
								return false;
							}
							assert(adu->GetCount() != SizeT(-1));
							auto fh = OpenFileData(AsDataItem(this), avu ? avu->GetTiledRangeData().get() : nullptr, fn);
							if (!fh)
							{
								DoFailCaller(std::make_shared<ErrMsg>("Cannot open data in .MMD storage folder"), FailType::Data);
								goto failed;
							}
							AsDataItem(GetCurrUltimateItem())->m_DataObject.reset(fh.release()); // , !adi->IsPersistent(), true); // calls OpenFileData
							return true;
						}
					}
				}
			}
		}
	}
	assert(!SuspendTrigger::DidSuspend());

	try {
		while (true)
		{
			if (CheckCalculatingOrReady(refItem))
				goto data_ready_or_in_cache;
			//		_ProcessConfigData(true, false);
			//		Now try to actually get valid data
			if (refItem != this && refItem->IsFailed())
				Fail(refItem);
			if (WasFailed(FailType::Data))
				goto failed;

			if ((drlType != DrlType::UpdateNever) && HasCalculator())
				switch (PrepareDataCalc(make_shared_tree(this, existing_obj{}), refItem, drlFlags))
				{
				case how_to_proceed::nothing: break;
				case how_to_proceed::data_ready: goto data_ready;
				case how_to_proceed::failed: goto failed;
				case how_to_proceed::suspended:
					assert(SuspendTrigger::DidSuspend());
					goto suspended;
				case how_to_proceed::suspended_or_failed: goto suspended_or_failed;
				default: dms_assert(false);
				}

//			dms_assert(!refItem->DataAllocated());
			dms_assert(!SuspendTrigger::DidSuspend());

			if (refItem->IsDataReadable()) // could be this (no calculator)
				switch (PrepareDataRead(make_shared_tree(this, existing_obj{}), refItem, drlFlags))
				{
				case how_to_proceed::nothing: break;
				case how_to_proceed::data_ready: goto data_ready;
				case how_to_proceed::failed: goto failed;
				case how_to_proceed::suspended: 
					assert(SuspendTrigger::DidSuspend());
					goto suspended;
				default: dms_assert(false);
				}

			//* REMOVE, DEBUG, SOLVES: for_each(xx[SubItem(Combine(...), 'Nr_1')] )
			if (SuspendTrigger::DidSuspend()) goto suspended;
			if (WasFailed(FailType::Data))           goto failed;

			if (refItem->IsCacheItem() || HasCalculator())
			{
				if (drlType == DrlType::UpdateNever)
					return false;

				if (IsUnit(refItem))
					goto nodata;
			}
			else
			{
				if (IsUnit(refItem))
				{
					AsUnit(const_cast<TreeItem*>(refItem))->SetMaxRange();
					goto data_ready; // assume default range
				}
			}
			dms_assert(!IsUnit(refItem));
			if (IsDataItem(refItem))
				goto nodata;

			refItem->SetIsInstantiated();
			goto data_ready;
			//*/

		data_ready_or_in_cache:
			if (IsCalculatingOrReady(refItem))
				goto data_ready; // may have been arranged in an alternative thread.

			ItemReadLock lock(refItem);
			if (IsDataReady(refItem))
				goto data_ready;

		}
	}
	catch (...)
	{
		// REMOVE, TODO: Actor::Fail(const DmsException&) toevoegen in Actor.h en hier gebruiken.
		auto err = catchException(true);
		DoFailCaller(err, FailType::Data);
		goto failed;
	}

data_ready:
	assert(!SuspendTrigger::DidSuspend());
	assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(refItem) || WasFailed(FailType::Data));
	return SuspendTrigger::BlockerBase::IsBlocked() 
		|| IsPassor() 
		|| (m_State.GetTransState() >= actor_flag_set::AF_ValidatingAndCommitting) 
		|| SuspendibleUpdate() 
		|| !SuspendTrigger::DidSuspend();

suspended_or_failed:
	assert(drlType != DrlType::Certain || !SuspendTrigger::DidSuspend());
	assert(SuspendTrigger::DidSuspend() || WasFailed()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode
	if (SuspendTrigger::DidSuspend())
		goto suspended;

failed:
	assert(WasFailed());
	assert(!SuspendTrigger::DidSuspend());
	if (refItem && IsCalculatingOrReady(refItem))
		return true;

failed_norefitem:
	assert(WasFailed());
	if (throwOnFail)
		ThrowFail();
	return false;

suspended:
	assert(drlType != DrlType::Certain);
	assert(SuspendTrigger::DidSuspend());
	return false;

nodata:
	Fail("No calculation rule or storage manager was specified and no specific primary data was provided", FailType::Data);
	goto failed;
}


bool TreeItem::PrepareData() const
{
	assert(IsMetaThread());

	if (!PrepareDataUsage(DrlType::Suspendible))
		return false;
	auto ultItem = GetCurrUltimateItem();
	if (!WaitForReadyOrSuspendTrigger(ultItem.get()))
	{
		if (SuspendTrigger::DidSuspend())
			return false;
		assert(ultItem->WasFailed());
		if (ultItem.get() != this && ultItem->WasFailed())
			this->Fail(ultItem.get());
		return false;
	}
	return true;
}


// called in idle time for items that will soon be visible, returns false when Suspended, true when Failed
bool TreeItem::TryPrepareDataUsage() const
{
	if (!GetInterestCount())
		return true;
	try { 
		return PrepareDataUsage(DrlType::Suspendible) || WasFailed();
	}
	catch (const DmsException&)
	{
		return true;
	}
}

TIC_CALL void TreeItem::DisableStorage(bool disabledStorage) // does not call UpdateMetaInfo
{
	SetTSF(TSF_DisabledStorage, disabledStorage);
	if (m_StorageManager && disabledStorage)
	{
		m_StorageManager->DoNotCommitOnClose();
		m_StorageManager = nullptr;
	}
}

[[maybe_unused]] static bool HasCfsStorage(const TreeItem* obj)
{
	auto storageHolder = obj->GetStorageParent(false);

	return storageHolder && !stricmp(storageHolder->GetStorageManager()->GetClsName().c_str(), "cfs");
}

bool TreeItem::HasConfigData() const
{
	if (IsCacheItem())
		return false;
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	if (GetCalculatorMember())  // DC_Ptr: false
		return GetCalculatorMember()->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!GetExprMember().empty())
		return false;
	if (GetStorageParent(false) != nullptr)
		return false;
	if (IsUnit(this))
		return AsUnit(this)->HasTiledRangeData();
	if (IsDataItem(this))
		return AsDataItem(this)->m_DataObject != nullptr;
	return false;
}

bool TreeItem::HasCurrConfigData() const
{
	if (IsCacheItem())
		return false;
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	if (GetCalculatorMember())  // DC_Ptr: false
		return GetCalculatorMember()->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!GetExprMember().empty())
		return false;
	if (GetCurrStorageParent(false) != nullptr)
		return false;
	if (IsUnit(this))
		return AsUnit(this)->HasTiledRangeData();
	if (IsDataItem(this))
		return AsDataItem(this)->m_DataObject != nullptr;
	return false;
}

template <typename FailReasonFunc>
bool FinalizeFailure(const TreeItem* self, FailReasonFunc&& func)
{
	if (SuspendTrigger::DidSuspend())
	{
		if (self->GetInterestCount() < 2)
			ReportSuspension();
	}
	else
	{
		if (self->GetCurrRangeItem()->WasFailed(FailType::Committed))
			self->Fail(self->GetCurrRangeItem().get());

		if (!self->WasFailed(FailType::Committed))
			self->Fail(func(), FailType::Committed);
		assert(SuspendTrigger::DidSuspend() || self->WasFailed(FailType::Committed));
	}
	return false; // suspended or failed, try again later
}

bool TreeItem::CommitDataChanges() const
{
	assert(IsMetaThread());

	assert(m_State.GetProgress() >= ProgressState::MetaInfo);
	if (m_State.GetProgress() >= ProgressState::Committed)
		return true;
	if (!IsStorable())
		return true;
	if (IsDataReadable())
		return true;
	if (IsFailed())
		return false;

	auto storageHolder = GetStorageParent(true);
	assert(storageHolder); // guaranteed by IsStorable();

	bool hasCalculator = HasCalculator();
	if (!hasCalculator)
		return true;

	if (GetCurrRangeItem()->WasFailed(FailType::Committed))
	{
		Fail(GetCurrRangeItem().get());
		return false;
	}

	DBG_START("TreeItem", "CommitDataChanges", false);
	DBG_TRACE(("self = {}", GetSourceName().c_str()));

	auto interestHolder = GetInterestPtrOrNull();
	assert(interestHolder); // Commit is Called from DoUpdate

	auto sm = storageHolder->GetStorageManager();
	assert(sm); // guaranteed by IsStorable();

	if ((!IsCalculatingOrReady(GetCurrRangeItem().get()) && !PrepareDataUsage(DrlType::Suspendible)) || GetCurrRangeItem()->WasFailed(FailType::Committed))
		// can have failed just because PrepareDataUsage suspended or failed; 
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to start calculating data when trying to store it in {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	if (!WaitForReadyOrSuspendTrigger(GetCurrRangeItem().get()) || GetCurrRangeItem()->WasFailed(FailType::Committed))
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to complete calculating data when trying to store it in {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	assert(!SuspendTrigger::DidSuspend());

	auto mmd = dynamic_cast<MmdStorageManager*>(sm);
	if (mmd)
	{
		if (IsUnit(this))
		{
			AsUnit(this)->GetCount();
			mmd->UpdateDictionary(storageHolder.get()); // #1155: the dictionary emitted at OpenForWrite lacked this unit's Range
		}
		if (IsDataItem(this))
		{
			DataReadLock lock(AsDataItem(this)); // make sure data is calculated and stored
			// #1154: writing the data required the domain's range, so here -- and not at
			// OpenForWrite, where the dictionary was first emitted -- the extent of a domain
			// declared OUTSIDE this storage is finally readable and can be recorded.
			mmd->UpdateDictionary(storageHolder.get());
		}
		return true;
	}

	auto nmsm = dynamic_cast<NonmappableStorageManager*>(sm);
	MG_CHECK(nmsm); // mmd's have been handled above
	if (!nmsm)
		return true;

	if (!DoWriteItem(nmsm->GetMetaInfo(storageHolder.get(), const_cast<TreeItem*>(this), StorageAction::write))
		|| GetCurrRangeItem()->WasFailed(FailType::Committed))
		return FinalizeFailure(this, [this]() { return mySSPrintF("Unable to write data to storage {}", DMS_TreeItem_GetAssociatedFilename(this)); });

	return !WasFailed(FailType::Committed);
}

static bool PartOfInterestImpl(const TreeItem* self)
{
	while (self)
	{
		if (self->GetInterestCount())
			return true;
		self = self->GetTreeParent().get();
	}
	return false;
}

bool TreeItem::PartOfInterest() const
{ 
	if (GetInterestCount())
		return true;
	if (!IsCacheItem())
		return false;

	return PartOfInterestImpl(GetTreeParent().get());
}

garbage_can TreeItem::TryCleanupMem() const
{
	if (IsCacheItem() && !IsCacheRoot())
		return {};

//	return {}; // DEBUG

	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);

	garbage_can garbage;
	TryCleanupMemImpl(garbage);
	return garbage;
}

bool TreeItem::TryCleanupMemImpl(garbage_can& garbageCan) const
{
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemCount < 0)
		return false;

	if (IsDataItem(this))
		if (!AsDataItem(this)->HasVoidDomainGuarantee())
			ClearDataObject(garbageCan);

	if (IsCacheItem())
		for (const TreeItem* subTI = _GetFirstSubItem(); subTI; subTI = subTI->GetNextItem())
			subTI->TryCleanupMemImpl(garbageCan);

	return true;
}

void TreeItem::ClearDataObject(garbage_can&) const
{}

//----------------------------------------------------------------------
// Dumping to OutStreamBase
//----------------------------------------------------------------------

#include "xml/XMLOut.h"
#include "Xml/XmlTreeOut.h"
#include <time.h>

bool IsDumpingToFolder();

// =============================================================================
// DMS-syntax serialization of FUNCTION items as 'function name<tvs>(params) -> result'
// declarations (rather than the generic 'container ...: IsTemplate' form). All helpers
// hand-write raw text via operator<< onto the (stateful) OutStream_DMS: params must be
// ';'-separated (the attr machinery would emit ','), and the branch never calls
// WriteAttr/DumpPropList/DumpSubTags. Source fidelity: data-item types are built from the
// SOURCE tokens (ValuesUnitToken/DomainUnitToken), not the resolve-then-GetScriptName prop
// path. See doc/development/typed-hof-language-design.md and the plan.
// =============================================================================
namespace {

// Robust per-sub-item dump: a throw while rendering one child must not corrupt the enclosing
// brace nesting; emit an inline (parse-neutral) '//' comment and continue so EndSubItems runs.
void TreeItem_XML_DumpSubItemSafe(const TreeItem* subItem, OutStreamBase* out, bool notWritingDictionary)
{
	try
	{
		subItem->XML_Dump(out, notWritingDictionary);
	}
	catch (...)
	{
		auto err = catchException(false);
		out->NewLine();
		*out << "// ERROR dumping ";
		*out << SharedStr(subItem->GetName()).c_str();
		if (err)
		{
			SharedStr why(err->Why());
			std::string oneLine;
			for (CharPtr p = why.begin(), e = why.send(); p != e; ++p)
				oneLine += (*p == '\n' || *p == '\r') ? ' ' : *p;
			*out << ": ";
			*out << oneLine.c_str();
		}
		out->NewLine();
	}
}

void DMS_WriteTypeVars(OutStreamBase& out, const TreeItem* fn)
{
	auto tvs = TreeItem_GetFunctionTypeVars(fn);
	if (!tvs || tvs->empty())
		return;
	out << "<";
	bool first = true;
	for (const auto& tv : *tvs)
	{
		if (!first) out << ", ";
		first = false;
		out << SharedStr(tv.first).c_str();
		if (tv.second) { out << ": "; out << SharedStr(tv.second).c_str(); }
	}
	out << ">";
}

void DMS_WriteTypeArgs(OutStreamBase& out, const std::vector<TokenID>* typeArgs)
{
	if (!typeArgs || typeArgs->empty())
		return;
	out << "<";
	bool first = true;
	for (TokenID t : *typeArgs)
	{
		if (!first) out << ", ";
		first = false;
		out << SharedStr(t).c_str();
	}
	out << ">";
}

// The values-type PREFIX (no name, no domain): 'attribute<V>' / 'parameter<V>' /
// 'unit<vt>' / 'function' / 'container', from SOURCE tokens for data items.
void DMS_WriteValuesPrefix(OutStreamBase& out, const TreeItem* item)
{
	if (item->IsFunctionItem()) { out << "function"; return; }
	if (IsDataItem(item))
	{
		auto adi = AsDataItem(item);
		SharedStr vt(adi->ValuesUnitToken());
		// a void-domain item is a 'parameter<V>'; anything else is an 'attribute<V>'
		out << (adi->HasVoidDomainGuarantee() ? "parameter<" : "attribute<"); out << vt.c_str(); out << ">";
		return;
	}
	if (IsUnit(item)) { out << SharedStr(item->GetSignature()).c_str(); return; } // 'unit<vt>' — source-faithful
	out << "container";
}

// The domain SUFFIX ' (dTok[, comp])' — placed AFTER the item name (attribute grammar);
// nothing for a void/domain-less item or a non-data item. '.' (self) domains are omitted:
// they are the parser's default for a domain-less function param/result and re-derive on reload.
void DMS_WriteDomainSuffix(OutStreamBase& out, const TreeItem* item)
{
	if (!IsDataItem(item))
		return;
	auto adi = AsDataItem(item);
	if (adi->HasVoidDomainGuarantee())
		return; // void domain -> 'parameter<V>', no suffix
	auto vc = adi->GetValueComposition();
	SharedStr dt(adi->DomainUnitToken());
	bool selfDomain = (dt.ssize() == 1 && dt.begin()[0] == '.');
	if (selfDomain && vc == ValueComposition::Single)
		return; // '.' self-domain, Single -> implicit (the parser's default for a domain-less item)
	if (dt.empty())
	{
		if (vc == ValueComposition::Single)
		{
			// source token absent (e.g. some geometry/sequence attributes): fall back to the
			// resolved domain's script name so the domain is not silently dropped
			auto adu = adi->GetAbstrDomainUnit();
			if (!adu)
				return; // truly unresolved (an in-template generic domain) -> implicit
			dt = adu->GetScriptName(item);
			if (dt.empty())
				return;
		}
		else
			dt = SharedStr("."); // no domain token but a non-Single composition -> keep '.' so the composition survives
	}
	out << " ("; out << dt.c_str();
	if (vc != ValueComposition::Single)
	{
		SharedStr vcName(GetValueCompositionID(vc));
		if (!vcName.empty()) { out << ", "; out << vcName.c_str(); }
	}
	out << ")";
}

// A full typed item declaration '<values-prefix> name <domain-suffix>' (params, members).
void DMS_WriteTypedItem(OutStreamBase& out, const TreeItem* item)
{
	DMS_WriteValuesPrefix(out, item);
	out << " "; out << SharedStr(item->GetName()).c_str();
	DMS_WriteDomainSuffix(out, item);
}

void DMS_WriteFunctionParam(OutStreamBase& out, const TreeItem* fn, const TreeItem* param, UInt32 idx, UInt32 nrParams)
{
	SharedStr pname(param->GetName());
	if (idx + 1 == nrParams && TreeItem_HasFunctionRestParam(fn)) { out << "..."; out << pname.c_str(); return; }
	if (TreeItem_IsFunctionMetaRefParam(fn, idx)) { out << "item "; out << pname.c_str(); return; }
	if (auto sig = TreeItem_GetFunctionParamSignature(fn, idx))
	{
		out << pname.c_str(); out << ": ";
		out << SharedStr(sig->GetScriptName(fn)).c_str();
		DMS_WriteTypeArgs(out, TreeItem_GetFunctionParamSigTypeArgs(fn, idx));
		return;
	}
	DMS_WriteTypedItem(out, param); // '<values-prefix> name <domain-suffix>'
	// unit param carrying a member block: 'unit<uint32> Road { attribute<float64> flow; }'
	if (IsUnit(param) && param->_GetFirstSubItem())
	{
		out << " { ";
		for (const TreeItem* m = param->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			DMS_WriteTypedItem(out, m);
			if (IsDataItem(m) && !m->GetExpr().empty()) { out << " := "; out << SharedStr(m->GetExpr()).c_str(); }
			out << "; ";
		}
		out << "}";
	}
}

void DMS_WriteFunctionUsings(OutStreamBase& out, const TreeItem* fn)
{
	UInt32 n = fn->GetNrNamespaceUsages();
	for (UInt32 i = 0; i != n; ++i)
	{
		auto ns = fn->GetNamespaceUsage(i);
		if (ns && !ns->DoesContain(fn))
		{
			SharedStr nsName(ns->GetScriptName(fn));
			if (!nsName.empty()) { out << ", using = "; out << nsName.c_str(); }
		}
	}
}

void DMS_WriteResultType(OutStreamBase& out, const TreeItem* fn, const TreeItem* resultChild)
{
	if (TreeItem_IsFunctionResultFunction(fn) || (resultChild && resultChild->IsFunctionItem()))
	{
		if (auto rsig = TreeItem_GetFunctionResultSig(fn))
		{
			out << SharedStr(rsig->GetScriptName(fn)).c_str();
			DMS_WriteTypeArgs(out, TreeItem_GetFunctionResultSigTypeArgs(fn));
		}
		else
			out << "function";
		return;
	}
	if (resultChild) { DMS_WriteValuesPrefix(out, resultChild); DMS_WriteDomainSuffix(out, resultChild); return; } // '<prefix> (domain)'
	out << "container"; // defensive
}

} // anonymous namespace

void TreeItem::XML_DumpFunctionDecl(OutStreamBase* out, bool notWritingDictionary) const
{
	bool isVariantSet = TreeItem_IsFunctionVariantSet(this);
	bool isVariant    = GetTreeParent() && GetTreeParent()->IsFunctionItem() && TreeItem_IsFunctionVariantSet(GetTreeParent().get());
	bool isSigOnly    = TreeItem_IsFunctionSignatureOnly(this);
	SharedStr nameStr(GetName());
	bool mustDumpEndogenous = !IsDumpingToFolder();
	auto passes = [&](const TreeItem* s) {
		return (notWritingDictionary || !s->IsDisabledStorage())
			&& (mustDumpEndogenous || !s->IsEndogenous());
	};

	// --- variant SET: 'function name { variant v(...) ...; ... }'
	if (isVariantSet)
	{
		XML_OutElement elem(*out, "function", nameStr.c_str());
		elem.SetHasSubItems();
		out->BeginSubItems();
		for (const TreeItem* c = _GetFirstSubItem(); c; c = c->GetNextItem())
			if (passes(c))
				TreeItem_XML_DumpSubItemSafe(c, out, notWritingDictionary);
		out->EndSubItems();
		return;
	}

	UInt32 nrParams = TreeItem_GetFunctionParamCount(this);
	TokenID resultNameTok = TreeItem_GetFunctionResultName(this);
	const TreeItem* resultChild = nullptr;
	for (const TreeItem* c = _GetFirstSubItem(); c; c = c->GetNextItem())
		if (c->GetID() == resultNameTok) { resultChild = c; break; }

	// header: 'function name<tvs>(params), using = ns -> result'  (variants: keyword 'variant';
	// signature alias: 'name = function<tvs>(params) -> type;' — name precedes the keyword)
	XML_OutElement elem(*out,
		isSigOnly ? nameStr.c_str() : (isVariant ? "variant" : "function"),
		isSigOnly ? "= function" : nameStr.c_str());

	DMS_WriteTypeVars(*out, this);
	*out << "(";
	const TreeItem* p = _GetFirstSubItem();
	for (UInt32 i = 0; i != nrParams && p; ++i, p = p->GetNextItem())
	{
		if (i) *out << "; ";
		DMS_WriteFunctionParam(*out, this, p, i, nrParams);
	}
	*out << ")";
	DMS_WriteFunctionUsings(*out, this);
	*out << " -> ";
	DMS_WriteResultType(*out, this, resultChild);

	if (isSigOnly)
		return; // 'nuf = function<...>(...) -> type;' — no designation, no body; the dtor emits ';'

	if (resultChild) { *out << " := "; *out << SharedStr(resultChild->GetName()).c_str(); }

	// body block: all non-param children (the designated result child renders here too)
	const TreeItem* firstBody = _GetFirstSubItem();
	for (UInt32 k = nrParams; k && firstBody; --k) firstBody = firstBody->GetNextItem();
	const TreeItem* scan = firstBody;
	while (scan && !passes(scan)) scan = scan->GetNextItem();
	if (!scan)
		return; // no dumpable body item -> no block; dtor emits ';'
	elem.SetHasSubItems();
	out->BeginSubItems();
	for (const TreeItem* s = firstBody; s; s = s->GetNextItem())
		if (passes(s))
			TreeItem_XML_DumpSubItemSafe(s, out, notWritingDictionary);
	out->EndSubItems();
}

void TreeItem::XML_Dump(OutStreamBase* xmlOutStr, bool notWritingDictionary) const
{
	// write #include <filename> if configStore defined
	if (xmlOutStr->GetLevel() > 0 && IsDumpingToFolder())
	{
		SharedStr dirName = SharedStr( configStorePropDefPtr->GetValue(this) );
		if (!dirName.empty())
		{
			if (!*getFileNameExtension(dirName.c_str()))
			{
				if (xmlOutStr->GetSyntaxType() == OutStreamBase::ST_DMS)
					dirName += ".dms";
				else
					dirName += ".xml";
			}
			xmlOutStr->WriteInclude(dirName.c_str());
			if (xmlOutStr->HasFileName())
				IncludeFileSave(this, dirName.c_str());
			return;
		}
	}

	// A function item serializes as a 'function name<tvs>(params) -> result' declaration in
	// DMS syntax rather than the generic 'container ...: IsTemplate' form (which is misleading
	// and unpastable). Other stream types (XML/HTM detail pages) keep the generic rendering.
	if (xmlOutStr->GetSyntaxType() == OutStreamBase::ST_DMS && IsFunctionItem())
	{
		XML_DumpFunctionDecl(xmlOutStr, notWritingDictionary);
		return;
	}

	// Copy of code from Object because xmlElem must live after subItems
	SharedStr tagName = SharedStr((xmlOutStr->GetSyntaxType() != OutStreamBase::ST_DMS) ? SharedStr(GetXmlClassName()) : GetSignature());

	XML_OutElement xmlElem(*xmlOutStr, tagName.c_str(), GetName().c_str());

	xmlOutStr->DumpPropList(this);

	if (notWritingDictionary)
		xmlOutStr->DumpSubTags(this);
	else if (this == t_MmdDictionaryRoot)
	{
		// #1154: record what the stored bytes were written against for every unit declared
		// OUTSIDE this dictionary. Merged onto the read holder, #1180 folds these restrictions
		// into every sub-item read through it.
		auto restrictions = Mmd_SynthesizeExternalUnitRestrictions(this);
		if (!restrictions.empty())
			xmlOutStr->DumpSubTag(ICHECK_NAME, restrictions.c_str(), false);
	}
	// end of Copy

	if (IsDataItem(this))
	{
		bool isDataBlock = GetCalculatorMember() && GetCalculatorMember()->IsDataBlock();
		if (isDataBlock || HasConfigData())
		{
			xmlOutStr->DumpSubTagDelim();
			if (isDataBlock)
				*xmlOutStr << GetCalculatorMember()->GetExpr().c_str();
			else
			{
				TreeItemInterestPtr holder(this);
				XML_DumpData(xmlOutStr);
			}
		}
	}
	else if (IsUnit(this) && !notWritingDictionary)
	{
		auto au = AsUnit(this);
		if (au->HasVarRange() && IsCalculatingOrReady(au->GetCurrRangeItem().get()))
		{
			// when the dictionary is written at OpenForWrite time, the range of this unit may not have been
			// calculated yet (issue #1130: we can be inside PrepareDataUsage of this very unit);
			// then skip the Range subtag rather than tripping the IsCalculatingOrReady invariant in WaitReady
			TreeItemInterestPtr xholder(this);
			this->PrepareDataUsage(DrlType::Certain);

			xmlOutStr->DumpSubTag("Range", au->GetRangeAsStr(FormattingFlags::None).c_str(), false);
		}
	}

	// check if any non endogenous subitems exist
	const TreeItem* subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	if (!subItem)
		return;
	bool mustDumpEndogenousSubItems = !IsDumpingToFolder();
	while (true)
	{
		if (notWritingDictionary || !subItem->IsDisabledStorage()) // disabled storage items are not dumped in MMD dictionary
			if (mustDumpEndogenousSubItems || !subItem->IsEndogenous())
				break; // found one
		subItem = subItem->GetNextItem();
		if (!subItem)
			return; // no non endogenous subitems, so we don't dump subitems
	}

	// output all non endogenous subitems
	xmlElem.SetHasSubItems();
	xmlOutStr->BeginSubItems();

	subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	while (subItem)
	{
		if (notWritingDictionary || !subItem->IsDisabledStorage()) // disabled storage items are not dumped in MMD dictionary
			if (mustDumpEndogenousSubItems || !subItem->IsEndogenous())
				TreeItem_XML_DumpSubItemSafe(subItem, xmlOutStr, notWritingDictionary);
		subItem = subItem->GetNextItem();
	}
	xmlOutStr->EndSubItems();
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
	assert(GetTokenStr(subItemID).c_str()[0] != '.');

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


using template_set = std::set<SharedTreeItem>;

bool TreeItem_IsTemplateInstantiaton(const TreeItem* item)	
{
	assert(item);
	if (!item->HasCalculator())
		return false;

	auto calculator = item->GetCalculator();
	if (!calculator)
		return false;
	if (calculator->HasTemplSource())
		return true;
	return calculator->IsForEachTemplHolder();
}

auto TreeItem_GetTemplateSource(const TreeItem* item) -> SharedTreeItem
{
	assert(item);
	assert(item->HasCalculator());

	auto calculator = item->GetCalculator();
	assert(calculator);
	if (calculator->HasTemplSource())
		return make_shared_tree(calculator->GetTemplSource(), existing_obj{});
	// IsForEachTemplHolder() is true only when applyItem==nullptr,
	// but GetForEachTemplSource() asserts applyItem!=nullptr — mutually exclusive.
	// Skip holders that have not yet been instantiated.
	if (calculator->IsForEachTemplHolder())
		return {};
	return calculator->GetForEachTemplSource();
}

auto TreeItem_FindItem_impl(template_set& visitedSet, const TreeItem* searchLoc, TokenID id, const TreeItem* blockedSubItem = nullptr, bool findNextMode = false) -> SharedTreeItem
{
//	if (searchLoc->GetID() == id)
//		return searchLoc;
	if (TreeItem_IsTemplateInstantiaton(searchLoc))
	{
		if (auto templateSource = TreeItem_GetTemplateSource(searchLoc))
		{
			if (visitedSet.find(templateSource) != visitedSet.end())
				return {};

			visitedSet.insert(templateSource);
			const TreeItem* templItem = nullptr;
			while (true)
			{
				templItem = templateSource->WalkConstSubTree(templItem);
				if (!templItem)
					break;

				if (templItem->GetID() == id)
					return make_shared_tree(templItem, existing_obj{});
			}
		}
	}
	for (auto subItem = searchLoc->_GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
	{
		if (subItem == blockedSubItem)
			findNextMode = false; // start deep searching after this subItem
		else if (!findNextMode)
		{
			if (subItem->GetID() == id)
				return make_shared_tree(subItem, existing_obj{});

			if (auto result = TreeItem_FindItem_impl(visitedSet, subItem, id))
				return result;
		}
	}

	return {};
}

TIC_CALL auto TreeItem_FindItem(const TreeItem* searchLoc, TokenID id) -> SharedTreeItem
{
	if (!searchLoc || searchLoc->IsCacheItem())
		return {};
	bool findNextMode = searchLoc->GetID() == id;
	if (!findNextMode) // else we're to do the FindNext 
	{
		if (auto cache = searchLoc->m_UsingCache.get())
		{
			auto result = cache->FindItem(id);
			if (result)
				return result;
		}
	}
	
	template_set alreadyVisited;
	if (auto result = TreeItem_FindItem_impl(alreadyVisited, searchLoc, id, nullptr, false))
		return result;	

	while (auto parent = searchLoc->GetTreeParent().get())
	{
		if (!findNextMode && parent->GetID() == id)
			return make_shared_tree(parent, existing_obj{});

		if (auto result = TreeItem_FindItem_impl(alreadyVisited, parent, id, searchLoc, findNextMode))
			return result;
		searchLoc = parent;
	}
	return {};
}
