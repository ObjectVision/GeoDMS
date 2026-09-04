// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  TreeItem is the node type of the configuration tree: a named Actor with
 *  parent/sub-item links, the item's configuration state (calculator or
 *  expression, storage association, properties and metadata), and the
 *  interest-counting machinery that drives demand-driven calculation.
 *  Free functions at the end support function items and their generic
 *  parameter/result sub-items (template instantiation).
 */

#if !defined(__TIC_TREEITEM_H)
#define __TIC_TREEITEM_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"

#include "act/Actor.h"
#include "act/any.h"
#include "act/garbage_can.h"
#include "ptr/InterestHolders.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedStr.h"
#include "sym/Token.h"

#include "MetaInfo.h"
#include "OperArgPolicy.h"

#include "TreeItemFlags.h"
#include <act/ActorEnums.h>
#include <act/ActorVisitor.h>
#include <act/SupplierVisitFlag.h>
#include <cpc/Types.h>
#include <dbg/Diagnostics.h>
#include <vt/CharPtrRange.h>
#include <mci/Class.h>
#include <mci/Object.h>
#include <LispRef.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

class AbstrPropDef;  // was: <mci/PropDef.h>; only AbstrPropDef* is used here
// OutStreamBase (was: <xml/XMLOut.h>) is forward-declared in RtcBase.h
//----------------------------------------------------------------------
// class  : TreeItem Facets
//----------------------------------------------------------------------

/*
Forward declarations for serialization, storage, and traversal
Avoid including heavy headers here to keep compile times down.
*/

struct XML_OutElement;
struct SortedNameIndexArray;
struct CopyTreeContext;
class InpStreamBuff;
class OutStreamBuff;

struct OperationContext;
struct UsingCache;
struct SupplCache;
struct SourceLocation;
struct TreeItemCheckGuardians; // #1218: closure of applicable IntegrityChecks (TreeItem.cpp)

class AbstrCalculator;
// (single_linked_tree<TreeItem> is now inlined directly into TreeItem; see m_FirstSub/m_Next below)

#if defined(MG_DEBUG)
/*
UpdateMetaInfo detection guard:
- Used to assert that UpdateMetaInfo is not re-entrant in certain sensitive paths.
- In DEBUG, it locks a thread-local or process-wide flag to catch misuse.
*/
struct UpdateMetaInfoDetectionLock
{
	TIC_CALL UpdateMetaInfoDetectionLock();
	TIC_CALL ~UpdateMetaInfoDetectionLock();
	TIC_CALL static bool IsLocked();
};

#define MG_LOCKER_NO_UPDATEMETAINFO UpdateMetaInfoDetectionLock localLock##__LINE__;
#define MG_SIGNAL_ON_UPDATEMETAINFO dms_assert(!UpdateMetaInfoDetectionLock::IsLocked());

#else

#define MG_LOCKER_NO_UPDATEMETAINFO
#define MG_SIGNAL_ON_UPDATEMETAINFO

#endif

// Convenience interest pointer type to hold a TreeItem with interest counting semantics
using SharedTreeItemInterestPtr = InterestPtr<std::shared_ptr<const TreeItem> >;

//----------------------------------------------------------------------
// NameTreeReg
//----------------------------------------------------------------------

typedef std::pair<CharPtrRange, CharPtrRange> name_pair_t;
/*
Split subItemNames into parent path and branch id (e.g., "a/b/c" => ("a", "b/c")).
Assumes CharPtrRange is a non-owning range over a character buffer.
*/
auto NameTreeReg_GetParentAndBranchID(CharPtrRange subItemNames)->name_pair_t;

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

/*
Find subtree item by TokenID under a given root; does NOT throw.
Used by path resolution and calculators.
*/
TIC_CALL const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID);

//----------------------------------------------------------------------
// class  : TreeItem
//----------------------------------------------------------------------

/*
TreeItem
- Core node abstraction in the configuration/data tree with:
  - Naming (TokenID), hierarchy (parent/subitems), and namespace usages.
  - Storage integration (read/write), calculators (derivation), and data controller.
  - Interest management for lifetime and resource usage.
  - Meta-info update orchestration for lazy population/refresh.

Thread-safety:
- Some fields are mutable to support lazy evaluation in const context.
- m_ItemCount (atomic) and m_Producer (weak_ptr) are used for production/update tasks.
- Public methods marked noexcept should avoid throwing; many methods may suspend via Actor APIs.

Lifetime:
- Subitems manage insertion/removal via AddItem/RemoveItem.
- Parent is a SharedTreeItem to ensure safe upward traversal without immediate deletion of parents.
*/

struct TreeItem : Actor, std::enable_shared_from_this<TreeItem>
{
	using base_type = Actor;

	friend Object* CreateFunc<TreeItem>();

	// BEGIN integrated members of impl::treeitem_production_task
	// Counts in-flight or queued production/update (negiative) or usage(positive) tasks for this item.
	mutable std::atomic<LONG> m_ItemCount = 0;
	// Weak backref to the OperationContext that is producing this item.
	mutable std::weak_ptr<OperationContext> m_Producer;
	// END   integrated members of impl::treeitem_production_task

protected: // ctor
	TreeItem ();
	friend struct OwningPtr<TreeItem>;

public:
	// Public dtor: required so std::shared_ptr's deleter can destroy the object from namespace scope
	// (Object.h SharedCreateFunc, shared_tree_ptr's newly_obj ctor). Construction stays factory-only.
	~TreeItem ();

//	ctor / dtor

	// Initialization happens through the free function InitTreeItem(parent, subItem, id) below,
	// which transfers shared ownership of subItem into parent's sub-item list (pre: GetParent()==0).

//	Meta Info
	// User-visible description and expression (configuration-time).
	TIC_CALL void SetDescr(WeakStr description);
	TIC_CALL void SetExpr (WeakStr expression);

	// Identification
	TokenID GetNameID() const override;

	// Description getters; GetDisplayName may prefer a localized/pretty form of the name.
	virtual SharedStr GetDescr() const;
	SharedStr _GetDescr() const;
	TIC_CALL SharedStr GetDisplayName() const;

	// Expression (config) getter/setter; _GetExprStr returns the raw stored expression.
	TIC_CALL SharedStr GetExpr() const;
    SharedStr _GetExprStr() const { return GetExprMember(); }
    void _SetExpr(WeakStr str) { if (m_ConfigProperties || !str.empty()) GetOrCreateConfigProperties().mc_Expr = str; }

// Namespaces

	// Track used namespaces and URLs to enable unqualified name resolution.
	void AddUsing (const TreeItem* );
	void AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace);
	void AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd);
	TIC_CALL void AddUsingUrl (TokenID );

	void ClearNamespaceUsage();
	void ResetNamespaceUsage(bool includeImplicitParent, const TreeItem* definitionNamespace);
	UInt32 GetNrNamespaceUsages() const ;
	const TreeItem* GetNamespaceUsage(UInt32 i) const;

//	Suppliers

	// Suppliers cache (configured and implied dependencies).
	bool HasSupplCache() const { return bool(m_SupplCache); }
	const SupplCache* GetSupplCache() const { dms_assert(m_SupplCache); return m_SupplCache.get(); }
	TIC_CALL SupplCache* GetOrCreateSupplCache() const;

// Dumping 

	// XML dump for diagnostics or config serialization; dumpSubTags toggles subtree traversal.
	virtual void XML_Dump(OutStreamBase* out, bool dumpSubTags = true) const; // DumpDecl
private:
	// DMS-syntax serialization of a function item as a 'function name<tvs>(params) -> result'
	// declaration (rather than the generic 'container ...: IsTemplate' form). ST_DMS only.
	void XML_DumpFunctionDecl(OutStreamBase* out, bool notWritingDictionary) const;
public:

//	storage

	// Configure storage manager and behavior (read-only, driver, options).
	TIC_CALL void SetStorageManager(CharPtr storageName, CharPtr storageType, StorageReadOnlySetting readOnly, CharPtr driver = nullptr, CharPtr options = nullptr);
	TIC_CALL bool HasStorageManager() const;
	TIC_CALL AbstrStorageManager* GetStorageManager(bool throwOnFailure = true) const;
          AbstrStorageManager* GetCurrStorageManager() const { return m_StorageManager.get(); }
	// Disable storage to force in-memory or calculator-only operation.
	TIC_CALL void DisableStorage(bool disableStorage=true); // don't use storage
          bool IsDisabledStorage() const { return GetTSF(TSF_DisabledStorage); }
	bool IsDataReadable()    const;

//	Containment

	// Subitem counts; CountNrSubItems may call UpdateMetaInfo, while _CountNrSubItems will not.
	TIC_CALL UInt32  CountNrSubItems () const noexcept; // calls UpdateMetaInfo
	UInt32 _CountNrSubItems () noexcept;       // doesn't call UpdateMetaInfo

	TIC_CALL bool              HasSubItems   () const noexcept;                            // calls UpdateMetaInfo
	bool              _HasSubItems  ()  noexcept { return _GetFirstSubItem(); }    // doesn't call UpdateMetaInfo

	// Inlined single-linked sub-item list (was the single_linked_tree<TreeItem> base). Raw links for now;
	// these become std::shared_ptr in the ownership migration (see doc/development/std-ptr-migration-plan.md).
	      TreeItem* _GetFirstSubItem()       noexcept { return m_FirstSub.get(); }
	const TreeItem* _GetFirstSubItem() const noexcept { return m_FirstSub.get(); }
	      TreeItem* GetNextItem()            noexcept { return m_Next.get(); }
	const TreeItem* GetNextItem()      const noexcept { return m_Next.get(); }
	TIC_CALL void Reorder(TreeItem** first, TreeItem** last); // exported: shv GraphicContainer::SaveOrder calls it

	// GetFirstSubItem may return nullptr; Curr variants do not trigger UpdateMetaInfo.
	TIC_CALL const TreeItem*   GetFirstSubItem() const  noexcept;
	const TreeItem*   GetCurrFirstSubItem() const  noexcept;
	TIC_CALL const TreeItem*   GetFirstVisibleSubItem() const  noexcept;
	TIC_CALL const TreeItem*   GetNextVisibleItem() const  noexcept;

	// Walkers for subtree traversal (const and non-const); Visit* supports visitor pattern.
	TIC_CALL const TreeItem*   WalkConstSubTree(const TreeItem* curr) const  noexcept; // this acts as subTreeRoot
	auto              VisitConstVisibleSubTree(const ActorVisitor& visitor) const -> ActorVisitState;
	TIC_CALL TreeItem*         WalkCurrSubTree(TreeItem* curr) noexcept;              // this acts as subTreeRoot
	TIC_CALL TreeItem*         WalkNext(TreeItem* curr)  noexcept;                    // this acts as subTreeRoot

	// Inheritance flags: an inherited ref item or in inherited subtree.
	bool IsInherited() const { return GetTSF(TSF_InheritedRef); }
	bool IsInInherited() const { if (IsInherited()) return true; auto tp = GetTreeParent(); return tp && tp->IsInherited(); }
	// an endogenous shadow of a sub-item of the referred cache root, see TSF_MergedFromRefItem (#1245)
	bool IsMergedFromRefItem() const { return GetTSF(TSF_MergedFromRefItem); }

	// Parents

	// Parent access (PersistentObject override) and storage parent resolution (for R/W).
	[[nodiscard]] const PersistentObject* GetParent () const noexcept override;       // override PersistentObject
          SharedTreeItem GetTreeParent   () const   { return m_Parent.lock(); } // safe weak->shared upgrade (parent owns child; m_Parent is non-owning)
	TIC_CALL SharedTreeItem GetStorageParent(bool alsoForWrite) const;
	SharedTreeItem GetCurrStorageParent(bool alsoForWrite) const;

// Search Items by name

	// Name-based search; variants for current vs UpdateMetaInfo-based behavior.
	TIC_CALL SharedTreeItem   GetConstSubTreeItemByID(TokenID subItemName) const; // calls UpdateMetaInfo
	TIC_CALL SharedTreeItem   GetCurrSubTreeItemByID(TokenID subItemName) const;
	TIC_CALL       TreeItem*   GetSubTreeItemByID(TokenID subItemName);

	// Path-based resolution; BestItem attempts fuzzy or best-effort matching.
	TIC_CALL       TreeItem* GetItem     (CharPtrRange subItemNames);
	      TreeItem* GetBestItem (CharPtrRange subItemNames);
	TIC_CALL SharedTreeItem GetCurrItem (CharPtrRange subItemNames) const; // doesn't call UpdateMetaInfo

	TIC_CALL SharedTreeItem ResolveItemPath(CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	TIC_CALL BestItemRef FindBestItem(CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	auto FindAndVisitItem(CharPtrRange subItemNames, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>;  // directly referred persistent object.

	// Type checking helpers to verify runtime class before usage.
	TIC_CALL const TreeItem* CheckObjCls(const Class* requiredClass) const;
	      TreeItem* CheckCls   (const Class* requiredClass);
	const TreeItem* FollowDots(CharPtrRange dots) const;

	// Script-facing name in a context.
	virtual auto GetScriptName(const TreeItem* context) const -> SharedStr;

// Creation

	// Dynamic creation of items based on path or explicit id and class.
	TIC_CALL auto CreateItemFromPath(CharPtr subItemNames, const Class* cls = 0) -> SharedMutableTreeItem;
	TIC_CALL auto CreateItem        (TokenID id,           const Class* cls = 0) -> SharedMutableTreeItem;

	// Special roots for config and cache trees. Both own the root from birth (no auto-delete pin),
	// so a parentless root is never exposed as a raw, unowned pointer.
	static TIC_CALL SharedMutableTreeItem CreateConfigRoot(TokenID id);
	static TIC_CALL SharedMutableTreeItem CreateCacheRoot();

	// Calculator presence; Impl may check a deeper condition than HasCalculator.
	TIC_CALL bool HasCalculator()   const noexcept;
	TIC_CALL bool HasCalculatorImpl() const noexcept;

	// Capability flags given current state and configuration.
	TIC_CALL bool IsLoadable()      const;
	TIC_CALL bool IsStorable()      const;

	bool IsCurrLoadable()  const;
	bool IsCurrStorable()  const;

	// Derivable if loadable or has calculator without config data.
	bool IsDerivable()     const { return IsLoadable() || (HasCalculator() && !HasConfigData()); }
	TIC_CALL bool HasConfigData() const;
	bool HasCurrConfigData() const;

	// Cache item predicates (without UpdateMetaInfo).
	bool IsPart()          const { return IsCacheItem() && GetTreeParent(); }    // doesn't call UpdateMetaInfo
	bool IsCacheRoot()     const { return IsCacheItem() && !GetTreeParent(); }   // doesn't call UpdateMetaInfo
	TIC_CALL bool IsEditable()      const;

	// Breaks supplier cycles over the subtree and, for a config root, releases it from its SessionData
	// (which cascades destruction). Ownership is downward; there is no longer an auto-delete pin.
	TIC_CALL void EnableAutoDelete();
	void SetIsCacheItem();
          bool IsCacheItem() const { return GetTSF(TSF_IsCacheItem); }

//	Getting Data into or out of memory

	// Data lifecycle: prepare, commit, cleanup. Some may suspend via Actor mechanisms.
	bool TryPrepareDataUsage() const; // called in idle time for items that will soon be visible, returns false when Suspended
	bool CommitDataChanges() const;
	garbage_can TryCleanupMem() const; // overridden by AbstrDataItem
	garbage_can DropValue();
	TIC_CALL bool PrepareDataUsageImpl(DrlType drlType) const;
	TIC_CALL bool PrepareDataUsage(DrlType drlType) const;
	virtual bool TryCleanupMemImpl(garbage_can& garbageCan) const; // overridden by AbstrDataItem
	TIC_CALL bool PrepareData() const;

//	Copying

	// Deep copy into dest with specified id and context; CopyProps customizable.
	TIC_CALL [[nodiscard]] SharedMutableTreeItem Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const;
	void UpdateMetaInfoImpl2() const; // sort of const
	void UpdateMetaInfo() const noexcept override; // sort of const
	void UpdateMetaInfoIfNotAlready() const noexcept;

//	override Actor callbacks

	// Progress reporting, failure handling, and permission assertions.
	void SetProgress(ProgressState ps) const override;
	bool DoFail(ErrMsgPtr msg, FailType ft) const override;
	void AssertPropChangeRights(CharPtr changeWhat) const override;
	void AssertDataChangeRights(CharPtr changeWhat) const override;

	// Visit suppliers with flags determining breadth/depth, implied/configured.
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

//	calculator and reffered items

	// Calculator accessors and derivation chain navigation (source/ultimate items).
	auto GetCalculator() const -> AbstrCalculatorRef;
	TIC_CALL const TreeItem* GetCurrSourceItem() const noexcept;
	TIC_CALL const TreeItem* GetSourceItem() const noexcept;
	TIC_CALL const TreeItem* GetUltimateSourceItem() const noexcept;
	const TreeItem* GetCurrUltimateSourceItem() const noexcept;

	// Integrity checker and size estimator are specialized calculators.
	bool HasIntegrityChecker() const;
	auto GetIntegrityChecker() const -> AbstrCalculatorRef;

	// Declared size knowledge (§4.6): the expectation is a point estimate, the upperbound a sound
	// bound that admission may reserve on. Both are cheap-to-evaluate calculation rules.
	bool HasSizeExpectation() const;
	auto GetSizeExpectation() const->AbstrCalculatorRef;
	bool HasSizeUpperbound() const;
	auto GetSizeUpperbound() const->AbstrCalculatorRef;

	// Referred/ultimate item helpers; “Curr” variants avoid UpdateMetaInfo.
	TIC_CALL auto GetCurrUltimateItem() const noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetCurrRangeItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetUltimateItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetCurrRefItem () const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetReferredItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	virtual void Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const;

//	TIC_CALL MetaInfo GetMetaInfo(metainfo_policy_flags mpf) const;
	MetaInfo GetCurrMetaInfo(metainfo_policy_flags mpf) const;
	LispRef GetBaseKeyExpr() const;
//	TIC_CALL LispRef GetOrgKeyExpr() const;
	virtual LispRef GetKeyExprImpl() const;
	auto GetOrgDC() const->std::pair<DataControllerRef, SharedTreeItem>;
	TIC_CALL LispRef GetCheckedKeyExpr() const;
	TIC_CALL auto GetCheckedDC() const -> DataControllerRef;
	TIC_CALL void UpdateDC() const;

#if defined(MG_DEBUG)
	TIC_CALL void CheckFlagInvariants() const;
#endif

	// Flag management helpers; enforce invariants after each mutation.
	void SetTSF(TreeItemStatusFlags sf) const 
	{ 
		m_StatusFlags.Set(sf); 
		CHECK_FLAG_INVARIANTS;
	}
	void ClearTSF(TreeItemStatusFlags sf) const 
	{ 
		m_StatusFlags.Clear(sf); 
		CHECK_FLAG_INVARIANTS;
	}
	void SetTSF(TreeItemStatusFlags sf, bool value) const 
	{ 
		m_StatusFlags.Set(sf, value); 
		CHECK_FLAG_INVARIANTS;
	}
	bool GetTSF(TreeItemStatusFlags sf) const { return m_StatusFlags.Get(sf); }

	// Visibility/template flags and data retention policies.
	TIC_CALL void SetIsHidden(bool v);
	TIC_CALL void SetIsTemplate();
	TIC_CALL void SetIsFunction();

	TIC_CALL void SetKeepDataState(bool value);
          bool GetKeepDataState () const { return GetTSF(TSF_KeepData); }

	void SetLazyCalculatedState(bool value);
          bool GetLazyCalculatedState() const { return GetTSF(TSF_LazyCalculated); }

	void SetStoreDataState(bool value);
          bool GetStoreDataState() const { return GetTSF(TSF_StoreData); }
	TIC_CALL void SetFreeDataState(bool value);
          bool GetFreeDataState() const { return GetTSF(TSF_FreeData); }
	bool IsTemplate() const { return GetTSF(TSF_IsTemplate); }
	bool InTemplate() const { return GetTSF(TSF_InTemplate); }
	bool IsFunctionItem() const { return GetTSF(TSF_IsFunctionItem); }
	bool IsEndogenous() const { return GetTSF(TSF_IsEndogenous); }

	// Mark data as changed to trigger downstream invalidation/updates.
	void SetDataChanged(); 

//	SourceLocation
	// Set and get source location info aiding error reporting and tooling.
	TIC_CALL void SetLocation(const SourceLocation* loc);
	auto GetLocation() const -> const SourceLocation* override;
	TIC_CALL SharedStr GetConfigFileName  () const;
	TIC_CALL UInt32  GetConfigFileLineNr() const;
	TIC_CALL UInt32  GetConfigFileColNr () const;

	// Override Object to provide a more specific source name (e.g., composed from location).
	SharedStr GetSourceName() const override; // override Object

//	StoredProp management
	// Track stored property associations; helpful for persistence backends.
	void AddPropAssoc(AbstrPropDef* propDef) const;
	void SubPropAssoc(AbstrPropDef* propDef) const;

	// DC and Calculator wiring; setting DC may adjust calculator and referred items.
	TIC_CALL void SetDC(DataControllerRef newDC, const TreeItem* newRefItem = nullptr) const;
	TIC_CALL void SetCalculator(AbstrCalculatorRef pr) const; // also called by DataController
	TIC_CALL SharedTreeItemInterestPtr GetInterestPtrOrNull() const;
	SharedTreeItemInterestPtr GetInterestPtrOrCancel() const;
	std::weak_ptr<const Actor> weak_from_actor() const override { return weak_from_this(); } // std-managed: real weak for the supplier-interest list

//protected: // new callback functions
	// Hooks for storage read/write and data (clear/copy/signature/result checks).
	virtual bool DoReadItem(StorageMetaInfoPtr smi); friend struct StorageReadHandle;
	virtual bool DoWriteItem(StorageMetaInfoPtr&& smiHolder) const;
	virtual void ClearDataObject(garbage_can&) const;
	virtual void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const;
	virtual SharedStr GetSignature() const;
	virtual bool CheckResultItem(const TreeItem* refItem) const;

//	override Actor callbacks
	// Update/invalidate hooks from Actor; handle suspended updates via SuspendibleUpdate.
	ActorVisitState DoUpdate() override;
	void DoInvalidate  () const override;

	// Determine last supplier change for caching and invalidation decisions.
	TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const /*noexcept*/ override;

private:
	bool _CheckResultObjType(const TreeItem* refItem) const;

public:
	// Update that can suspend; returns appropriate visit state to scheduler.
	ActorVisitState SuspendibleUpdate() const override;

// InterestCount management
	// Interest drives resource lifetime; “KeepDataState” maintains data aside from interest count.
	bool PartOfInterest() const;
	bool   HasInterest     () const { return GetInterestCount() || GetKeepDataState(); }
	bool   PartOfInterestOrKeep() const { return PartOfInterest() || GetKeepDataState(); }

	// Namespace “using” cache accessors.
	bool CurrHasUsingCache() const { return bool(m_UsingCache);  }
                UsingCache* GetUsingCache();
	const UsingCache* GetUsingCache() const;

	// Removal from config tree (detach and cleanup).
	TIC_CALL void RemoveFromConfig() const; 

protected:
	// Called when interest starts/stops; maintain resources accordingly.
	void StartInterest() const override;
	garbage_can StopInterest () const noexcept override;

private:
public: // TODO G8: Re-encapsulate
	// Internal helpers; consider moving to private when callers are refactored.
	void ResetSubTreeConfigData(); // recursive: reset calculators/integrity/storage to break supplier cycles before teardown
	void MakeCalculator() const noexcept;
	void UpdateMetaInfoImpl() const;
	TIC_CALL void SetReferredItem(const TreeItem* refItem) const;
	const TreeItem* DetermineReferredItem(const AbstrCalculator* ac) const;

	// Subtree mutation; preconditions enforced by assertions.
	void AddItem   (SharedMutableTreeItem child); // PRECONDITION: child->GetParent()==0; transfers ownership into the sub-item list
	void RemoveItem(TreeItem* child); // PRECONDITION: child->GetParent()==this
	void ReleaseSubItem(TreeItem* subItem); // detaches a sub-item and releases the parent's ownership of it
	void InheritParentState(TreeItem* parent); // copy template/cache/passor/keep-data flags down from parent (called by InitTreeItem)
	friend void InitTreeItem(TreeItem* parent, SharedMutableTreeItem subItem, TokenID id);

	// Storage IO entry points; ReadItem integrates with StorageReadHandle.
	bool ReadItem(StorageReadHandle&& srh);
	void SetStorageManager(AbstrStorageManager* sm);

	// Mark meta-info as ready after updates.
	void SetMetaInfoReady() const;

	// Template/visibility propagation helpers.
	void SetInHidden(bool value);
	void SetInTemplate();

	// Instantiation flag (eager template instantiation tracking).
	// Exported like its setter: PhaseContainer (in Clc) reads it to tell a phase's first
	// completion from a re-entry, see PhaseContainerOperator::CalcResult.
	TIC_CALL bool GetIsInstantiated() const;
	TIC_CALL void SetIsInstantiated() const;

	// Whether to substitute by calculator spec (performance/semantics shortcut).
	bool CanSubstituteByCalcSpec() const noexcept;
public:
	// Binary blob IO; override to customize (streams/buffers).
	void LoadBlobBuffer (const BlobBuffer& rs);
	void StoreBlobBuffer(      BlobBuffer& rs) const;

	virtual void LoadBlobStream (const InpStreamBuff*);
	virtual void StoreBlobStream(      OutStreamBuff*) const;

	bool CheckBlobBuffer(const BlobBuffer& rs) const;
//	bool LoadBlobIfAny() const;

	// data members
	// Global counters/locks used to defer notifications during batch operations.
	TIC_CALL static std::atomic<UInt32> s_NotifyChangeLockCount;
	TIC_CALL static UInt32 s_MakeEndoLockCount;
	TIC_CALL static UInt32 s_ConfigReadLockCount;

#if defined(MG_DEBUG)
	TIC_CALL bool CheckMetaInfoReady() const;
	TIC_CALL bool CheckMetaInfoReadyOrPassor() const;
#endif

#if defined(MG_DEBUG_DATA)
public:
	// For debugging: cached full name string for inspection.
	mutable SharedStr md_FullName;
#endif // MG_DEBUG_DATA


	// BackRef for special cache-root wiring; FullCfgName materialization.
	auto GetBackRef() const -> SharedTreeItem; // owning snapshot of the weak back-ref (null if unset/expired)
	auto GetFullCfgName() const -> SharedStr override;
//private: // TODO G8: encapsulate

	// Identification token; assumed cheap-copy and stable.
	TokenID                        m_ID;

	// Non-owning back-pointer, only used by CacheRoots to refer back to the config item that
	// (shared-)owns this cache item via its mc_RefItem. WeakPtr makes the non-owning intent
	// explicit (it is a typed raw pointer: the intrusive scheme has no liveness detection).
	// Safe to dereference because the owning config item clears this on disconnect
	// (SetReferredItem nulls the old refItem's m_BackRef; ~TreeItem calls SetReferredItem(nullptr)),
	// so it is always either valid or null and never dangles.
	mutable std::weak_ptr<const TreeItem> m_BackRef; // only used by CacheRoots

	// Subitems manage insertion in a non-refcounted set; child holds counted-ref to parent.
	// Non-owning weak back-pointer to the parent. Ownership is downward: the parent owns its
	// sub-items via the intrusive refcount (AddItem adopts a reference; ReleaseSubItem releases it).
	std::weak_ptr<const TreeItem>        m_Parent;   // ro-access, NON-owning (parent owns child)

	// Inlined sub-item links (was single_linked_tree<TreeItem>). Downward ownership is via
	// std::shared_ptr: the parent owns its first child (m_FirstSub) and each child owns its next
	// sibling (m_Next). Teardown is iterative over siblings (see ~TreeItem) so a long sibling
	// chain does not recurse the stack; depth recurses via the child dtor (bounded by tree depth).
	SharedMutableTreeItem          m_FirstSub; // owns first child (downward)
	SharedMutableTreeItem          m_Next;     // owns next sibling (downward)
	void AddSub(SharedMutableTreeItem subItem);          // append to the sub-item list (takes ownership)
	SharedMutableTreeItem ExtractSub(TreeItem* subItem); // unlink and return the extracted owning ptr (m_Next cleared)

	// optional pointers to various services
	mutable std::unique_ptr<SupplCache>  m_SupplCache;
	mutable std::unique_ptr<UsingCache>  m_UsingCache;
	mutable AbstrStorageManagerRef       m_StorageManager; 
	mutable rtc::any::Any                m_ReadAssets; friend struct OperationContext;

public: // TODO G8: encapsulate and move config attr (aka mc_ ) into a separate ConfigTreeItem class

	// Status flags for visibility, template state, storage/data retention, etc.
	mutable treeitem_flag_set      m_StatusFlags;

	// Config-only data: the calculation-rule expression plus the pluggable calculators.
	// Allocated lazily and only for configuration items; GetOrCreateConfigProperties() asserts the
	// owner is not a cache item. (Unrelated to HasConfigData(), which reports whether the item
	// holds authoritative primary data.)
	struct ConfigProperties
	{
		SharedStr          mc_Expr;             // configuration-time calculation rule
		AbstrCalculatorRef mc_Calculator;
		AbstrCalculatorRef mc_IntegrityChecker;
		AbstrCalculatorRef mc_SizeExpectation;
		AbstrCalculatorRef mc_SizeUpperbound;

		// #1218: memoized closure of the items whose IntegrityCheck applies here, along the
		// GetTreeParent and ExplicitSuppliers relations (TreeItem.cpp, TreeItem_GetCheckGuardians).
		// Meta-thread only, like mc_DC; reset with the other config-derived state (DoInvalidate,
		// ResetSubTreeConfigData -- it can hold cross-branch supplier references).
		std::shared_ptr<const TreeItemCheckGuardians> mc_CheckGuardians;
	};
	mutable std::unique_ptr<ConfigProperties> m_ConfigProperties;

	// ConfigProperties accessors: read-side helpers are null-safe (return an empty value when the
	// ConfigProperties is absent); the create-side allocates on demand and is config-only.
	const ConfigProperties*                  GetConfigProperties() const noexcept { return m_ConfigProperties.get(); }
	TIC_CALL ConfigProperties&               GetOrCreateConfigProperties() const;
	TIC_CALL const SharedStr&          GetExprMember()             const noexcept;
	TIC_CALL const AbstrCalculatorRef& GetCalculatorMember()       const noexcept;
	const AbstrCalculatorRef& GetIntegrityCheckerMember() const noexcept;
	const AbstrCalculatorRef& GetSizeExpectationMember()  const noexcept;
	const AbstrCalculatorRef& GetSizeUpperboundMember()   const noexcept;
	void ResetCalculatorMember()       const; // defined in .cpp where AbstrCalculator is complete
	void ResetIntegrityCheckerMember() const;

	// Pluggable behavior: data controller and referred/template-original items.
	mutable DataControllerRef      mc_DC;
	mutable std::weak_ptr<const TreeItem> mc_RefItem, mc_OrgItem;

private:
	// Optional source location tracking for diagnostics.
	SharedPtr<const SourceLocation> m_Location;

	DECL_RTTI(TIC_CALL, TreeItemClass)

	friend struct UsingCache;
	friend class  AbstrStorageManager;
	friend struct DataController;
	friend struct DataWriteLock;
	friend struct DmsSpiritProduct;
	friend struct InterestReporter;

	// Helper Functions
	friend TIC_CALL const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID);
};

// Initialize a freshly-created (make_shared'd) tree item: set its ID and transfer shared
// ownership of subItem into parent's sub-item list (parent==nullptr for a root). Free function
// so the caller's owning shared_ptr is moved in rather than recovered via shared_from_this.
void InitTreeItem(TreeItem* parent, SharedMutableTreeItem subItem, TokenID id);

// Free function that allows self==nullptr (avoids UB from calling member on nullptr).
auto TreeItem_CreateItem(TreeItem* self, TokenID id, const Class* cls = nullptr) -> SharedMutableTreeItem;
auto TreeItem_CreateItemFromPath(TreeItem* self, CharPtr subItemNames, const Class* cls = nullptr) -> SharedMutableTreeItem;
TIC_CALL TreeItem* TreeItem_CheckCls(TreeItem* self, const Class* requiredClass);
const TreeItem* TreeItem_CheckObjCls(const TreeItem* self, const Class* requiredClass);

using SharedTreeItem = std::shared_ptr<const TreeItem>;
/*
Utility to handle integrity check failures; iCheckerResult is expected from a checker calculator.
checkStringGenerator delayed-evaluates error strings to avoid overhead when not needed.
*/
TIC_CALL bool IntegrityCheckFailure(const TreeItem* self, const AbstrDataItem* iCheckerResult, std::function<SharedStr()> checkStringGenerator);

// The function-item specification API (the 'function' keyword machinery)
// lives in TreeItemFunctionSpec.h.

#endif // __TIC_TREEITEM_H
