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
#include "ptr/PersistentSharedObj.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedPtr.h"
#include "ptr/SharedStr.h"
#include "set/Token.h"

#include "MetaInfo.h"
#include "OperArgPolicy.h"

#include "TreeItemFlags.h"

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
using ItemTree = single_linked_tree<TreeItem>;

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
using SharedTreeItemInterestPtr = InterestPtr<SharedPtr<const TreeItem> >;

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
struct TreeItem : Actor, ItemTree
{
	using base_type = Actor ;

	friend Object* CreateFunc<TreeItem>();

	// BEGIN integrated members of impl::treeitem_production_task
	// Counts in-flight or queued production/update (negiative) or usage(positive) tasks for this item.
	mutable std::atomic<LONG> m_ItemCount = 0;
	// Weak backref to the OperationContext that is producing this item.
	mutable std::weak_ptr<OperationContext> m_Producer;
	// END   integrated members of impl::treeitem_production_task

protected: // ctor / dtor
	TIC_CALL TreeItem ();
	TIC_CALL ~TreeItem ();
	friend struct OwningPtr<TreeItem>;

public:
//	ctor / dtor

	// Initialize ID and link this item into parent's subitems (pre: GetParent()==0).
	TIC_CALL void InitTreeItem(TreeItem* parent, TokenID id); // Set name and adds this to parent as child PRECONDITION: GetParent() == 0;

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
//	         CharPtr _GetExpr() const  { return mc_Expr.c_str(); }
    SharedStr _GetExprStr() const { return mc_Expr; }
    void _SetExpr(WeakStr str) { mc_Expr = str; }

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
	bool HasSupplCache() const { return m_SupplCache; }
	const SupplCache* GetSupplCache() const { dms_assert(m_SupplCache); return m_SupplCache; }
	TIC_CALL SupplCache* GetOrCreateSupplCache() const;

// Dumping 

	// XML dump for diagnostics or config serialization; dumpSubTags toggles subtree traversal.
	TIC_CALL virtual void XML_Dump(OutStreamBase* out, bool dumpSubTags = true) const; // DumpDecl

//	storage

	// Configure storage manager and behavior (read-only, driver, options).
	TIC_CALL void SetStorageManager(CharPtr storageName, CharPtr storageType, StorageReadOnlySetting readOnly, CharPtr driver = nullptr, CharPtr options = nullptr);
	TIC_CALL bool HasStorageManager() const;
	TIC_CALL AbstrStorageManager* GetStorageManager(bool throwOnFailure = true) const;
          AbstrStorageManager* GetCurrStorageManager() const { return m_StorageManager; }
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
	TIC_CALL [[nodiscard]] const PersistentSharedObj* GetParent () const noexcept override;       // override PersistentSharedObj
          SharedTreeItem GetTreeParent   () const   { return m_Parent; }
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
	TIC_CALL TreeItem* CreateItemFromPath(CharPtr subItemNames, const Class* cls = 0);
	TIC_CALL TreeItem* CreateItem        (TokenID id,           const Class* cls = 0);

	// Special roots for config and cache trees.
	static TIC_CALL TreeItem* CreateConfigRoot(TokenID id);
	static TIC_CALL TreeItem* CreateCacheRoot();

	// Calculator presence; Impl may check a deeper condition than HasCalculator.
	TIC_CALL bool HasCalculator()   const noexcept;
	TIC_CALL bool HasCalculatorImpl() const noexcept;

	// Capability flags given current state and configuration.
	TIC_CALL bool IsLoadable()      const;
	TIC_CALL bool IsStorable()      const;

	TIC_CALL bool IsCurrLoadable()  const;
	TIC_CALL bool IsCurrStorable()  const;

	// Derivable if loadable or has calculator without config data.
	bool IsDerivable()     const { return IsLoadable() || HasCalculator() && !HasConfigData(); }
	TIC_CALL bool HasConfigData() const;
	TIC_CALL bool HasCurrConfigData() const;

	// Cache item predicates (without UpdateMetaInfo).
	bool IsPart()          const { return IsCacheItem() && GetTreeParent(); }    // doesn't call UpdateMetaInfo
	bool IsCacheRoot()     const { return IsCacheItem() && !GetTreeParent(); }   // doesn't call UpdateMetaInfo
	TIC_CALL bool IsEditable()      const;

//	implement AdoptableRefObject (ex virtual)
	TIC_CALL void DisableAutoDelete();
	TIC_CALL void EnableAutoDelete();
          bool IsAutoDeleteDisabled() const { return GetTSF(TSF_IsAutoDeleteDisabled); }
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
	TIC_CALL SharedPtr<TreeItem> Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const;
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
	TIC_CALL const TreeItem* GetCurrUltimateItem() const noexcept;
	TIC_CALL const TreeItem* GetCurrRangeItem() const  noexcept;
	TIC_CALL const TreeItem* GetUltimateItem() const  noexcept;
	TIC_CALL const TreeItem* GetCurrRefItem () const  noexcept;
	TIC_CALL const TreeItem* GetReferredItem() const  noexcept;
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

	TIC_CALL void SetKeepDataState(bool value);
          bool GetKeepDataState () const { return GetTSF(TSF_KeepData); }

	TIC_CALL void SetLazyCalculatedState(bool value);
          bool GetLazyCalculatedState() const { return GetTSF(TSF_LazyCalculated); }

			bool HasRepetitiveUsers() const { return GetTSF(TSF_KeepData|TSF_THA_Keep); }
	TIC_CALL void SetStoreDataState(bool value);
          bool GetStoreDataState() const { return GetTSF(TSF_StoreData); }
	TIC_CALL void SetFreeDataState(bool value);
          bool GetFreeDataState() const { return GetTSF(TSF_FreeData); }
	bool IsTemplate() const { return GetTSF(TSF_IsTemplate); }
	bool InTemplate() const { return GetTSF(TSF_InTemplate); }
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

//protected: // new callback functions
	// Hooks for storage read/write and data (clear/copy/signature/result checks).
	TIC_CALL virtual bool DoReadItem(StorageMetaInfoPtr smi); friend struct StorageReadHandle;
	TIC_CALL virtual bool DoWriteItem(StorageMetaInfoPtr&& smiHolder) const;
	TIC_CALL virtual void ClearData(garbage_can&) const;
	TIC_CALL virtual void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const;
	TIC_CALL virtual SharedStr GetSignature() const;
	TIC_CALL virtual bool CheckResultItem(const TreeItem* refItem) const;

//	override Actor callbacks
	// Update/invalidate hooks from Actor; handle suspended updates via SuspendibleUpdate.
	TIC_CALL ActorVisitState DoUpdate(ProgressState ps) override;
	TIC_CALL void DoInvalidate  () const override;

	// Determine last supplier change for caching and invalidation decisions.
	TIC_CALL TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const /*noexcept*/ override;

private:
	bool _CheckResultObjType(const TreeItem* refItem) const;

public:
	// Update that can suspend; returns appropriate visit state to scheduler.
	TIC_CALL ActorVisitState SuspendibleUpdate(ProgressState ps) const override;

// InterestCount management
	// Interest drives resource lifetime; “KeepDataState” maintains data aside from interest count.
	TIC_CALL bool PartOfInterest() const;
	bool   HasInterest     () const { return GetInterestCount() || GetKeepDataState(); }
	bool   PartOfInterestOrKeep() const { return PartOfInterest() || GetKeepDataState(); }

	// Namespace “using” cache accessors.
	bool CurrHasUsingCache() const { return m_UsingCache;  }
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
	void EnableAutoDeleteImpl(); // does not call UpdateMetaInfo
	void EnableAutoDeleteRootImpl(); // does not call UpdateMetaInfo
	void MakeCalculator() const noexcept;
	void UpdateMetaInfoImpl() const;
	TIC_CALL void SetReferredItem(const TreeItem* refItem) const;
	const TreeItem* DetermineReferredItem(const AbstrCalculator* ac) const;

	// Subtree mutation; preconditions enforced by assertions.
	void AddItem   (TreeItem* child); // PRECONDITION: child->GetParent()==0;
	void RemoveItem(TreeItem* child); // PRECONDITION: child->GetParent()==this

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
	TIC_CALL auto GetBackRef() const -> const TreeItem*;
	TIC_CALL auto GetFullCfgName() const -> SharedStr override;
//private: // TODO G8: encapsulate

	// Identification token; assumed cheap-copy and stable.
	TokenID                        m_ID;

	// Only used by CacheRoots to refer back to a source/root.
	mutable const TreeItem*        m_BackRef = nullptr; // only used by CacheRoots

	// Subitems manage insertion in a non-refcounted set; child holds counted-ref to parent.
	SharedTreeItem                 m_Parent;   // ro-access, counted-ref

	// Configuration-time expression associated with this item.
	mutable SharedStr              mc_Expr;


	// optional pointers to various services
	mutable OwningPtr<SupplCache>  m_SupplCache;
	mutable OwningPtr<UsingCache>  m_UsingCache;
	mutable AbstrStorageManagerRef m_StorageManager; 
	mutable rtc::any::Any          m_ReadAssets; friend struct OperationContext;

public: // TODO G8: encapsulate and move config attr (aka mc_ ) into a separate ConfigTreeItem class

	// Status flags for visibility, template state, storage/data retention, etc.
	mutable treeitem_flag_set      m_StatusFlags;

	// Pluggable behavior: calculators, integrity checkers, size estimators, and DC.
	mutable AbstrCalculatorRef     mc_Calculator, mc_IntegrityChecker, mc_SizeEstimator;
	mutable DataControllerRef      mc_DC;
	mutable SharedPtr<const TreeItem> mc_RefItem, mc_OrgItem;

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

using SharedTreeItem = SharedPtr<const TreeItem>;
/*
Utility to handle integrity check failures; iCheckerResult is expected from a checker calculator.
checkStringGenerator delayed-evaluates error strings to avoid overhead when not needed.
*/
TIC_CALL bool IntegrityCheckFailure(const TreeItem* self, const AbstrDataItem* iCheckerResult, std::function<SharedStr()> checkStringGenerator);

#endif // __TREEITEM_H
