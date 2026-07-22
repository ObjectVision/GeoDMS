// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once // MSVC fast include guard
#endif

#if !defined(__TREEITEM_H)
#define __TREEITEM_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"

#include "act/Actor.h"
#include "act/any.h"
#include "act/garbage_can.h"
#include "mci/SingleLinkedTree.h"
#include "ptr/InterestHolders.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedTreePtr.h"
#include "ptr/SharedStr.h"
#include "ptr/WeakPtr.h"
#include "set/Token.h"

#include "MetaInfo.h"
#include "OperArgPolicy.h"

#include "TreeItemFlags.h"
#include <act/ActorEnums.h>
#include <act/ActorVisitor.h>
#include <act/SupplierVisitFlag.h>
#include <cpc/Types.h>
#include <dbg/Check.h>
#include <geo/CharPtrRange.h>
#include <mci/Class.h>
#include <mci/Object.h>
#include <mci/PropDef.h>
#include <xml/XMLOut.h>
#include <LispRef.h>
/*
#include "AbstrDataItem.h"
#include "DataLocks.h"
#include "OperGroups.h"
#include "TreeItemClass.h"
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
*/
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
TIC_CALL auto NameTreeReg_GetParentAndBranchID(CharPtrRange subItemNames)->name_pair_t;

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
	TIC_CALL TreeItem ();
	friend struct OwningPtr<TreeItem>;

public:
	// Public dtor: required so std::shared_ptr's deleter can destroy the object from namespace scope
	// (Object.h SharedCreateFunc, shared_tree_ptr's newly_obj ctor). Construction stays factory-only.
	TIC_CALL ~TreeItem ();

//	ctor / dtor

	// Initialization happens through the free function InitTreeItem(parent, subItem, id) below,
	// which transfers shared ownership of subItem into parent's sub-item list (pre: GetParent()==0).

//	Meta Info
	// User-visible description and expression (configuration-time).
	TIC_CALL void SetDescr(WeakStr description);
	TIC_CALL void SetExpr (WeakStr expression);

	// Identification
	TIC_CALL TokenID GetID        () const override;

	// Description getters; GetDisplayName may prefer a localized/pretty form of the name.
	TIC_CALL virtual SharedStr GetDescr() const;
	TIC_CALL SharedStr _GetDescr() const;
	TIC_CALL SharedStr GetDisplayName() const;

	// Expression (config) getter/setter; _GetExprStr returns the raw stored expression.
	TIC_CALL SharedStr GetExpr() const;
    SharedStr _GetExprStr() const { return GetExprMember(); }
    void _SetExpr(WeakStr str) { if (m_ConfigProperties || !str.empty()) GetOrCreateConfigProperties().mc_Expr = str; }

// Namespaces

	// Track used namespaces and URLs to enable unqualified name resolution.
	TIC_CALL void AddUsing (const TreeItem* );
	TIC_CALL void AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace);
	TIC_CALL void AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd);
	TIC_CALL void AddUsingUrl (TokenID );

	TIC_CALL void ClearNamespaceUsage();
	TIC_CALL UInt32 GetNrNamespaceUsages() const ;
	TIC_CALL const TreeItem* GetNamespaceUsage(UInt32 i) const;

//	Suppliers

	// Suppliers cache (configured and implied dependencies).
	bool HasSupplCache() const { return bool(m_SupplCache); }
	const SupplCache* GetSupplCache() const { dms_assert(m_SupplCache); return m_SupplCache.get(); }
	TIC_CALL SupplCache* GetOrCreateSupplCache() const;

// Dumping 

	// XML dump for diagnostics or config serialization; dumpSubTags toggles subtree traversal.
	TIC_CALL virtual void XML_Dump(OutStreamBase* out, bool dumpSubTags = true) const; // DumpDecl
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
	TIC_CALL bool IsDataReadable()    const;

//	Containment

	// Subitem counts; CountNrSubItems may call UpdateMetaInfo, while _CountNrSubItems will not.
	TIC_CALL UInt32  CountNrSubItems () const noexcept; // calls UpdateMetaInfo
	TIC_CALL UInt32 _CountNrSubItems () noexcept;       // doesn't call UpdateMetaInfo

	TIC_CALL bool              HasSubItems   () const noexcept;                            // calls UpdateMetaInfo
	TIC_CALL bool              _HasSubItems  ()  noexcept { return _GetFirstSubItem(); }    // doesn't call UpdateMetaInfo

	// Inlined single-linked sub-item list (was the single_linked_tree<TreeItem> base). Raw links for now;
	// these become std::shared_ptr in the ownership migration (see doc/development/std-ptr-migration-plan.md).
	      TreeItem* _GetFirstSubItem()       noexcept { return m_FirstSub.get(); }
	const TreeItem* _GetFirstSubItem() const noexcept { return m_FirstSub.get(); }
	      TreeItem* GetNextItem()            noexcept { return m_Next.get(); }
	const TreeItem* GetNextItem()      const noexcept { return m_Next.get(); }
	TIC_CALL void Reorder(TreeItem** first, TreeItem** last); // exported: shv GraphicContainer::SaveOrder calls it

	// GetFirstSubItem may return nullptr; Curr variants do not trigger UpdateMetaInfo.
	TIC_CALL const TreeItem*   GetFirstSubItem() const  noexcept;
	TIC_CALL const TreeItem*   GetCurrFirstSubItem() const  noexcept;
	TIC_CALL const TreeItem*   GetFirstVisibleSubItem() const  noexcept;
	TIC_CALL const TreeItem*   GetNextVisibleItem() const  noexcept;

	// Walkers for subtree traversal (const and non-const); Visit* supports visitor pattern.
	TIC_CALL const TreeItem*   WalkConstSubTree(const TreeItem* curr) const  noexcept; // this acts as subTreeRoot
	TIC_CALL auto              VisitConstVisibleSubTree(const ActorVisitor& visitor) const -> ActorVisitState;
	TIC_CALL TreeItem*         WalkCurrSubTree(TreeItem* curr) noexcept;              // this acts as subTreeRoot
	TIC_CALL TreeItem*         WalkNext(TreeItem* curr)  noexcept;                    // this acts as subTreeRoot

	// Inheritance flags: an inherited ref item or in inherited subtree.
	bool IsInherited() const { return GetTSF(TSF_InheritedRef); }
	bool IsInInherited() const { if (IsInherited()) return true; auto tp = GetTreeParent(); return tp && tp->IsInherited(); }

	// Parents

	// Parent access (PersistentSharedObj override) and storage parent resolution (for R/W).
	TIC_CALL [[nodiscard]] const PersistentObject* GetParent () const noexcept override;       // override PersistentSharedObj
          SharedTreeItem GetTreeParent   () const   { return m_Parent.lock(); } // safe weak->shared upgrade (parent owns child; m_Parent is non-owning)
	TIC_CALL SharedTreeItem GetStorageParent(bool alsoForWrite) const;
	TIC_CALL SharedTreeItem GetCurrStorageParent(bool alsoForWrite) const;

// Search Items by name

	// Name-based search; variants for current vs UpdateMetaInfo-based behavior.
	TIC_CALL SharedTreeItem   GetConstSubTreeItemByID(TokenID subItemName) const; // calls UpdateMetaInfo
	TIC_CALL SharedTreeItem   GetCurrSubTreeItemByID(TokenID subItemName) const;
	TIC_CALL       TreeItem*   GetSubTreeItemByID(TokenID subItemName);

	// Path-based resolution; BestItem attempts fuzzy or best-effort matching.
	TIC_CALL       TreeItem* GetItem     (CharPtrRange subItemNames);
	TIC_CALL       TreeItem* GetBestItem (CharPtrRange subItemNames);
	TIC_CALL SharedTreeItem GetCurrItem (CharPtrRange subItemNames) const; // doesn't call UpdateMetaInfo

	TIC_CALL SharedTreeItem FindItem    (CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	TIC_CALL BestItemRef FindBestItem(CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	auto FindAndVisitItem(CharPtrRange subItemNames, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>;  // directly referred persistent object.

	// Type checking helpers to verify runtime class before usage.
	TIC_CALL const TreeItem* CheckObjCls(const Class* requiredClass) const;
	TIC_CALL       TreeItem* CheckCls   (const Class* requiredClass);
	TIC_CALL const TreeItem* FollowDots(CharPtrRange dots) const;

	// Script-facing name in a context.
	TIC_CALL virtual auto GetScriptName(const TreeItem* context) const -> SharedStr;

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

	TIC_CALL bool IsCurrLoadable()  const;
	TIC_CALL bool IsCurrStorable()  const;

	// Derivable if loadable or has calculator without config data.
	bool IsDerivable()     const { return IsLoadable() || (HasCalculator() && !HasConfigData()); }
	TIC_CALL bool HasConfigData() const;
	TIC_CALL bool HasCurrConfigData() const;

	// Cache item predicates (without UpdateMetaInfo).
	bool IsPart()          const { return IsCacheItem() && GetTreeParent(); }    // doesn't call UpdateMetaInfo
	bool IsCacheRoot()     const { return IsCacheItem() && !GetTreeParent(); }   // doesn't call UpdateMetaInfo
	TIC_CALL bool IsEditable()      const;

	// Breaks supplier cycles over the subtree and, for a config root, releases it from its SessionData
	// (which cascades destruction). Ownership is downward; there is no longer an auto-delete pin.
	TIC_CALL void EnableAutoDelete();
	TIC_CALL void SetIsCacheItem();
          bool IsCacheItem() const { return GetTSF(TSF_IsCacheItem); }

//	Getting Data into or out of memory

	// Data lifecycle: prepare, commit, cleanup. Some may suspend via Actor mechanisms.
	TIC_CALL bool TryPrepareDataUsage() const; // called in idle time for items that will soon be visible, returns false when Suspended
	TIC_CALL bool CommitDataChanges() const;
	TIC_CALL garbage_can TryCleanupMem() const; // overridden by AbstrDataItem
	TIC_CALL garbage_can DropValue();
	TIC_CALL bool PrepareDataUsageImpl(DrlType drlType) const;
	TIC_CALL bool PrepareDataUsage(DrlType drlType) const;
	TIC_CALL virtual bool TryCleanupMemImpl(garbage_can& garbageCan) const; // overridden by AbstrDataItem
	TIC_CALL bool PrepareData() const;

//	Copying

	// Deep copy into dest with specified id and context; CopyProps customizable.
	TIC_CALL [[nodiscard]] SharedMutableTreeItem Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const;
	void UpdateMetaInfoImpl2() const; // sort of const
	TIC_CALL void UpdateMetaInfo() const noexcept override; // sort of const
	TIC_CALL void UpdateMetaInfoIfNotAlready() const noexcept;

//	override Actor callbacks

	// Progress reporting, failure handling, and permission assertions.
	TIC_CALL void SetProgress(ProgressState ps) const override;
	TIC_CALL bool DoFail(ErrMsgPtr msg, FailType ft) const override;
	TIC_CALL void AssertPropChangeRights(CharPtr changeWhat) const override;
	TIC_CALL void AssertDataChangeRights(CharPtr changeWhat) const override;

	// Visit suppliers with flags determining breadth/depth, implied/configured.
	TIC_CALL ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

//	calculator and reffered items

	// Calculator accessors and derivation chain navigation (source/ultimate items).
	TIC_CALL auto GetCalculator() const -> AbstrCalculatorRef;
	TIC_CALL const TreeItem* GetCurrSourceItem() const noexcept;
	TIC_CALL const TreeItem* GetSourceItem() const noexcept;
	TIC_CALL const TreeItem* GetUltimateSourceItem() const noexcept;
	TIC_CALL const TreeItem* GetCurrUltimateSourceItem() const noexcept;

	// Integrity checker and size estimator are specialized calculators.
	TIC_CALL bool HasIntegrityChecker() const;
	TIC_CALL auto GetIntegrityChecker() const -> AbstrCalculatorRef;

	TIC_CALL bool HasSizeEstimator() const;
	TIC_CALL auto GetSizeEstimator() const->AbstrCalculatorRef;

	// Referred/ultimate item helpers; “Curr” variants avoid UpdateMetaInfo.
	TIC_CALL auto GetCurrUltimateItem() const noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetCurrRangeItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetUltimateItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetCurrRefItem () const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL auto GetReferredItem() const  noexcept -> std::shared_ptr<const TreeItem>;
	TIC_CALL virtual void Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const;

//	TIC_CALL MetaInfo GetMetaInfo(metainfo_policy_flags mpf) const;
	TIC_CALL MetaInfo GetCurrMetaInfo(metainfo_policy_flags mpf) const;
	TIC_CALL LispRef GetBaseKeyExpr() const;
//	TIC_CALL LispRef GetOrgKeyExpr() const;
	TIC_CALL virtual LispRef GetKeyExprImpl() const;
	TIC_CALL auto GetOrgDC() const->std::pair<DataControllerRef, SharedTreeItem>;
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

	TIC_CALL void SetLazyCalculatedState(bool value);
          bool GetLazyCalculatedState() const { return GetTSF(TSF_LazyCalculated); }

	TIC_CALL void SetStoreDataState(bool value);
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
	TIC_CALL auto GetLocation() const -> const SourceLocation* override;
	TIC_CALL SharedStr GetConfigFileName  () const;
	TIC_CALL UInt32  GetConfigFileLineNr() const;
	TIC_CALL UInt32  GetConfigFileColNr () const;

	// Override Object to provide a more specific source name (e.g., composed from location).
	TIC_CALL SharedStr GetSourceName() const override; // override Object

//	StoredProp management
	// Track stored property associations; helpful for persistence backends.
	void AddPropAssoc(AbstrPropDef* propDef) const;
	void SubPropAssoc(AbstrPropDef* propDef) const;

	// DC and Calculator wiring; setting DC may adjust calculator and referred items.
	TIC_CALL void SetDC(DataControllerRef newDC, const TreeItem* newRefItem = nullptr) const;
	TIC_CALL void SetCalculator(AbstrCalculatorRef pr) const; // also called by DataController
	TIC_CALL SharedTreeItemInterestPtr GetInterestPtrOrNull() const;
	TIC_CALL SharedTreeItemInterestPtr GetInterestPtrOrCancel() const;
	std::weak_ptr<const Actor> weak_from_actor() const override { return weak_from_this(); } // std-managed: real weak for the supplier-interest list

//protected: // new callback functions
	// Hooks for storage read/write and data (clear/copy/signature/result checks).
	TIC_CALL virtual bool DoReadItem(StorageMetaInfoPtr smi); friend struct StorageReadHandle;
	TIC_CALL virtual bool DoWriteItem(StorageMetaInfoPtr&& smiHolder) const;
	TIC_CALL virtual void ClearDataObject(garbage_can&) const;
	TIC_CALL virtual void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const;
	TIC_CALL virtual SharedStr GetSignature() const;
	TIC_CALL virtual bool CheckResultItem(const TreeItem* refItem) const;

//	override Actor callbacks
	// Update/invalidate hooks from Actor; handle suspended updates via SuspendibleUpdate.
	TIC_CALL ActorVisitState DoUpdate() override;
	TIC_CALL void DoInvalidate  () const override;

	// Determine last supplier change for caching and invalidation decisions.
	TIC_CALL TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const /*noexcept*/ override;

private:
	bool _CheckResultObjType(const TreeItem* refItem) const;

public:
	// Update that can suspend; returns appropriate visit state to scheduler.
	TIC_CALL ActorVisitState SuspendibleUpdate() const override;

// InterestCount management
	// Interest drives resource lifetime; “KeepDataState” maintains data aside from interest count.
	TIC_CALL bool PartOfInterest() const;
	bool   HasInterest     () const { return GetInterestCount() || GetKeepDataState(); }
	bool   PartOfInterestOrKeep() const { return PartOfInterest() || GetKeepDataState(); }

	// Namespace “using” cache accessors.
	bool CurrHasUsingCache() const { return bool(m_UsingCache);  }
                UsingCache* GetUsingCache();
	TIC_CALL const UsingCache* GetUsingCache() const;

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
	friend TIC_CALL void InitTreeItem(TreeItem* parent, SharedMutableTreeItem subItem, TokenID id);

	// Storage IO entry points; ReadItem integrates with StorageReadHandle.
	bool ReadItem(StorageReadHandle&& srh);
	void SetStorageManager(AbstrStorageManager* sm);

	// Mark meta-info as ready after updates.
	void SetMetaInfoReady() const;

	// Template/visibility propagation helpers.
	void SetInHidden(bool value);
	void SetInTemplate();

	// Instantiation flag (eager template instantiation tracking).
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
	TIC_CALL auto GetBackRef() const -> SharedTreeItem; // owning snapshot of the weak back-ref (null if unset/expired)
	TIC_CALL auto GetFullCfgName() const -> SharedStr override;
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
		AbstrCalculatorRef mc_SizeEstimator;
	};
	mutable std::unique_ptr<ConfigProperties> m_ConfigProperties;

	// ConfigProperties accessors: read-side helpers are null-safe (return an empty value when the
	// ConfigProperties is absent); the create-side allocates on demand and is config-only.
	const ConfigProperties*                  GetConfigProperties() const noexcept { return m_ConfigProperties.get(); }
	TIC_CALL ConfigProperties&               GetOrCreateConfigProperties() const;
	TIC_CALL const SharedStr&          GetExprMember()             const noexcept;
	TIC_CALL const AbstrCalculatorRef& GetCalculatorMember()       const noexcept;
	TIC_CALL const AbstrCalculatorRef& GetIntegrityCheckerMember() const noexcept;
	TIC_CALL const AbstrCalculatorRef& GetSizeEstimatorMember()    const noexcept;
	TIC_CALL void ResetCalculatorMember()       const; // defined in .cpp where AbstrCalculator is complete
	TIC_CALL void ResetIntegrityCheckerMember() const;

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
TIC_CALL void InitTreeItem(TreeItem* parent, SharedMutableTreeItem subItem, TokenID id);

// Free function that allows self==nullptr (avoids UB from calling member on nullptr).
TIC_CALL auto TreeItem_CreateItem(TreeItem* self, TokenID id, const Class* cls = nullptr) -> SharedMutableTreeItem;
TIC_CALL auto TreeItem_CreateItemFromPath(TreeItem* self, CharPtr subItemNames, const Class* cls = nullptr) -> SharedMutableTreeItem;
TIC_CALL TreeItem* TreeItem_CheckCls(TreeItem* self, const Class* requiredClass);
TIC_CALL const TreeItem* TreeItem_CheckObjCls(const TreeItem* self, const Class* requiredClass);

using SharedTreeItem = std::shared_ptr<const TreeItem>;
/*
Utility to handle integrity check failures; iCheckerResult is expected from a checker calculator.
checkStringGenerator delayed-evaluates error strings to avoid overhead when not needed.
*/
TIC_CALL bool IntegrityCheckFailure(const TreeItem* self, const AbstrDataItem* iCheckerResult, std::function<SharedStr()> checkStringGenerator);

// user-defined function items (declared with the 'function' keyword): declared parameter
// count (= the number of leading sub-items that a call binds), the designated result
// sub-item, and optional per-parameter function-signature exemplars.
TIC_CALL void    TreeItem_SetFunctionSpec(const TreeItem* functionItem, UInt32 nrParams, TokenID resultName);
TIC_CALL UInt32  TreeItem_GetFunctionParamCount(const TreeItem* functionItem);
TIC_CALL TokenID TreeItem_GetFunctionResultName(const TreeItem* functionItem);
TIC_CALL void    TreeItem_AddFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* signatureExemplar, std::vector<TokenID> typeArgs = {});
// meta-reference parameters ('item x'): the argument binds as a raw item reference
// (sourceDescr key), like PropValue's item argument in a direct call — never as the
// argument's calculation/range key
TIC_CALL void    TreeItem_AddFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex);
TIC_CALL bool    TreeItem_IsFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex);
// '...x' rest parameter (always the LAST param): binds ONE OR MORE trailing arguments;
// in the body it may only be passed on as the trailing argument of a function call
TIC_CALL void    TreeItem_SetFunctionRestParam(const TreeItem* functionItem);
TIC_CALL bool    TreeItem_HasFunctionRestParam(const TreeItem* functionItem);
TIC_CALL auto    TreeItem_GetFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex) -> SharedTreeItem;
TIC_CALL const std::vector<TokenID>* TreeItem_GetFunctionParamSigTypeArgs(const TreeItem* functionItem, UInt32 paramIndex); // WP4.1: 'sig<V, D>' arguments
TIC_CALL void    TreeItem_SetFunctionTypeVars(const TreeItem* functionItem, std::vector<std::pair<TokenID, TokenID>> typeVars); // WP4.1: ordered <var: constraint> list
TIC_CALL const std::vector<std::pair<TokenID, TokenID>>* TreeItem_GetFunctionTypeVars(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionSignatureOnly(const TreeItem* functionItem); // 'alias = function<...>(...) -> ...;' — declared type, no body
TIC_CALL bool    TreeItem_IsFunctionSignatureOnly(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionResultSig(const TreeItem* functionItem, bool resultIsFunction, const TreeItem* resultSigExemplar, std::vector<TokenID> typeArgs = {}); // §5.10: function-valued result
TIC_CALL bool    TreeItem_IsFunctionResultFunction(const TreeItem* functionItem);
TIC_CALL auto    TreeItem_GetFunctionResultSig(const TreeItem* functionItem) -> SharedTreeItem;
TIC_CALL const std::vector<TokenID>* TreeItem_GetFunctionResultSigTypeArgs(const TreeItem* functionItem);
TIC_CALL void    TreeItem_CopyFunctionSpec(const TreeItem* dstFunctionItem, const TreeItem* srcFunctionItem);

// generic type variables on function parameters: function f<V: numerics>(... attribute<V> x ...)
class ValueClass;
TIC_CALL void    TreeItem_AddFunctionGenericParam(const TreeItem* functionItem, UInt32 paramIndex, TokenID varName, TokenID constraintName, bool isDomainVar = false);
TIC_CALL bool    TreeItem_GetFunctionGenericParam(const TreeItem* functionItem, UInt32 seqNr, UInt32* paramIndex, TokenID* varName, TokenID* constraintName, bool* isDomainVar = nullptr);
TIC_CALL bool    TreeItem_IsFunctionDefinitionChecked(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionDefinitionChecked(const TreeItem* functionItem);
TIC_CALL void    TreeItem_SetFunctionVariantSet(const TreeItem* functionItem);
TIC_CALL bool    TreeItem_IsFunctionVariantSet(const TreeItem* functionItem);
TIC_CALL bool    IsKnownGenericConstraint(TokenID constraintName);
TIC_CALL bool    MatchesGenericConstraint(const ValueClass* vc, TokenID constraintName);

// §5.7 v2: variant match sets over the closed value-class universe
TIC_CALL bool    TreeItem_VariantMatches(const TreeItem* variant, const std::vector<const ValueClass*>& argVCs);
TIC_CALL int     TreeItem_CompareVariantSpecificity(const TreeItem* a, const TreeItem* b); // -1/+1: strictly more specific side; 0: identical; 2: incomparable
TIC_CALL void    TreeItem_CheckVariantSetDisjointness(const TreeItem* setItem); // throws on identical/incomparable overlapping coverage

// enforce strict scoping on a function definition: name resolution from within stops at
// the item (own sub-items + explicit 'using' imports); relative import urls still
// resolve against the definition scope.
TIC_CALL void    TreeItem_MakeStrictScope(TreeItem* functionItem);

#endif // __TREEITEM_H
