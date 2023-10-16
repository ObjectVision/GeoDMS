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
#pragma once

#if !defined(__TREEITEM_H)
#define __TREEITEM_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "TicBase.h"

#include "act/Actor.h"
#include "act/Any.h"
#include "mci/SingleLinkedList.h"
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
using TreeItemList = single_linked_list<TreeItem>;

#if defined(MG_DEBUG)

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

using SharedTreeItemInterestPtr = InterestPtr<SharedPtr<const TreeItem> >;

//----------------------------------------------------------------------
// NameTreeReg
//----------------------------------------------------------------------

typedef std::pair<CharPtrRange, CharPtrRange> name_pair_t;
TIC_CALL auto NameTreeReg_GetParentAndBranchID(CharPtrRange subItemNames)->name_pair_t;

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

TIC_CALL const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID);

//----------------------------------------------------------------------
// class  : TreeItem
//----------------------------------------------------------------------

struct TreeItem : Actor, TreeItemList
{
	using base_type = Actor ;

	friend Object* CreateFunc<TreeItem>();

	// BEGIN integrated members of impl::treeitem_production_task
	mutable std::atomic<LONG> m_ItemCount = 0;
	mutable std::weak_ptr<OperationContext> m_Producer;
	// END   integrated members of impl::treeitem_production_task

protected:
	TIC_CALL TreeItem ();
	TIC_CALL ~TreeItem ();

public:
//	ctor / dtor

	TIC_CALL void InitTreeItem(TreeItem* parent, TokenID id); // Set name and adds this to parent as child PRECONDITION: GetParent() == 0;

//	Meta Info
	TIC_CALL void SetDescr(WeakStr description);
	TIC_CALL void SetExpr (WeakStr expression);

	TIC_CALL TokenID GetID        () const override;

	TIC_CALL virtual SharedStr GetDescr() const;
	TIC_CALL SharedStr _GetDescr() const;
	TIC_CALL SharedStr GetDisplayName() const;

	TIC_CALL SharedStr GetExpr() const;
//	         CharPtr _GetExpr() const  { return mc_Expr.c_str(); }
			 SharedStr _GetExprStr() const { return mc_Expr; }
			 void _SetExpr(WeakStr str) { mc_Expr = str; }

// Namespaces

	TIC_CALL void AddUsing (const TreeItem* );
	TIC_CALL void AddUsings(const TreeItem** firstNameSpace, const TreeItem** lastNameSpace);
	TIC_CALL void AddUsingUrls(CharPtr urlsBegin, CharPtr urlsEnd);
	TIC_CALL void AddUsingUrl (TokenID );

	TIC_CALL void ClearNamespaceUsage();
	TIC_CALL UInt32 GetNrNamespaceUsages() const ;
	TIC_CALL const TreeItem* GetNamespaceUsage(UInt32 i) const;

//	Suppliers

	bool HasSupplCache() const { return m_SupplCache; }
	const SupplCache* GetSupplCache() const { dms_assert(m_SupplCache); return m_SupplCache; }
	SupplCache* GetOrCreateSupplCache() const;

// Dumping 

	TIC_CALL virtual void XML_Dump(OutStreamBase* out) const; // DumpDecl

//	storage

	TIC_CALL void SetStorageManager(CharPtr storageName, CharPtr storageType, bool readOnly, CharPtr driver = nullptr, CharPtr options = nullptr);
	TIC_CALL bool HasStorageManager() const;
	TIC_CALL AbstrStorageManager* GetStorageManager(bool throwOnFailure = true) const;
	         AbstrStorageManager* GetCurrStorageManager() const { return m_StorageManager; }
	TIC_CALL void DisableStorage(bool disableStorage=true); // don't use storage
	         bool IsDisabledStorage() const { return GetTSF(TSF_DisabledStorage); }
	TIC_CALL bool IsDataReadable()    const;

//	Containment

	TIC_CALL UInt32  CountNrSubItems () const; // calls UpdateMetaInfo
	TIC_CALL UInt32 _CountNrSubItems ();       // doesn't call UpdateMetaInfo

	TIC_CALL bool              HasSubItems   () const;                            // calls UpdateMetaInfo
	TIC_CALL bool              _HasSubItems  () { return _GetFirstSubItem(); }    // doesn't call UpdateMetaInfo

	TIC_CALL const TreeItem*   GetFirstSubItem() const;
	TIC_CALL const TreeItem*   GetCurrFirstSubItem() const;
	TIC_CALL const TreeItem*   GetFirstVisibleSubItem() const;
	TIC_CALL const TreeItem*   GetNextVisibleItem() const;

	TIC_CALL const TreeItem*   WalkConstSubTree(const TreeItem* curr) const; // this acts as subTreeRoot
	TIC_CALL bool              VisitConstVisibleSubTree(const ActorVisitor& visitor, const struct StackFrame* = 0) const;
	TIC_CALL TreeItem*         WalkCurrSubTree(TreeItem* curr);              // this acts as subTreeRoot
	TIC_CALL TreeItem*         WalkNext(TreeItem* curr);                     // this acts as subTreeRoot

	bool IsInherited() const { return GetTSF(TSF_InheritedRef); }
	bool IsInInherited() const { if (IsInherited()) return true; auto tp = GetTreeParent(); return tp && tp->IsInherited(); }

	// Parents

	TIC_CALL const PersistentSharedObj* GetParent () const override;       // override PersistentSharedObj
	         SharedTreeItem GetTreeParent   () const   { return m_Parent; }
	TIC_CALL SharedTreeItem GetStorageParent(bool alsoForWrite) const;
	TIC_CALL SharedTreeItem GetCurrStorageParent(bool alsoForWrite) const;

// Search Items by name

	TIC_CALL SharedTreeItem   GetConstSubTreeItemByID(TokenID subItemName) const; // calls UpdateMetaInfo
	TIC_CALL SharedTreeItem   GetCurrSubTreeItemByID(TokenID subItemName) const;
	TIC_CALL       TreeItem*   GetSubTreeItemByID(TokenID subItemName);

	TIC_CALL       TreeItem* GetItem     (CharPtrRange subItemNames);
	TIC_CALL       TreeItem* GetBestItem (CharPtrRange subItemNames);
	TIC_CALL SharedTreeItem GetCurrItem (CharPtrRange subItemNames) const; // doesn't call UpdateMetaInfo

	TIC_CALL SharedTreeItem FindItem    (CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	TIC_CALL BestItemRef FindBestItem(CharPtrRange subItemNames) const; // calls UpdateMetaInfo
	auto FindAndVisitItem(CharPtrRange subItemNames, SupplierVisitFlag svf, const ActorVisitor& visitor) const->std::optional<SharedTreeItem>;  // directly referred persistent object.

	TIC_CALL const TreeItem* CheckObjCls(const Class* requiredClass) const;
	TIC_CALL       TreeItem* CheckCls   (const Class* requiredClass);
	TIC_CALL const TreeItem* FollowDots(CharPtrRange dots) const;

	TIC_CALL virtual auto GetScriptName(const TreeItem* context) const -> SharedStr;

// Creation

	TIC_CALL TreeItem* CreateItemFromPath(CharPtr subItemNames, const Class* cls = 0);
	TIC_CALL TreeItem* CreateItem        (TokenID id,           const Class* cls = 0);

	static TIC_CALL TreeItem* CreateConfigRoot(TokenID id);
	static TIC_CALL TreeItem* CreateCacheRoot();

	TIC_CALL bool HasCalculator()   const;
	TIC_CALL bool HasCalculatorImpl() const;

	TIC_CALL bool IsLoadable()      const;
	TIC_CALL bool IsStorable()      const;

	TIC_CALL bool IsCurrLoadable()      const;

	bool IsDerivable()     const { return IsLoadable() || HasCalculator() && !HasConfigData(); }
	TIC_CALL bool HasConfigData() const;
	TIC_CALL bool HasCurrConfigData() const;

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

	TIC_CALL bool TryPrepareDataUsage() const; // called in idle time for items that will soon be visible, returns false when Suspended
	TIC_CALL bool CommitDataChanges() const;
	TIC_CALL garbage_t TryCleanupMem() const; // overridden by AbstrDataItem
	TIC_CALL garbage_t DropValue();
	TIC_CALL bool PrepareDataUsageImpl(DrlType drlType) const;
	TIC_CALL bool PrepareDataUsage(DrlType drlType) const;
	TIC_CALL virtual bool TryCleanupMemImpl(garbage_t& garbageCan) const; // overridden by AbstrDataItem
	TIC_CALL bool PrepareData() const;

//	Copying

	TIC_CALL TreeItem* Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const;
	void UpdateMetaInfoImpl2() const; // sort of const
	TIC_CALL void UpdateMetaInfo() const override; // sort of const

//	override Actor callbacks

	TIC_CALL void SetProgress(ProgressState ps) const override;
	TIC_CALL bool DoFail(ErrMsgPtr msg, FailType ft) const override;
	TIC_CALL void AssertPropChangeRights(CharPtr changeWhat) const override;
	TIC_CALL void AssertDataChangeRights(CharPtr changeWhat) const override;

	TIC_CALL ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;

//	calculator and reffered items

	TIC_CALL auto GetCalculator() const -> AbstrCalculatorRef;
	TIC_CALL const TreeItem* GetCurrSourceItem() const;
	TIC_CALL const TreeItem* GetSourceItem() const;
	TIC_CALL const TreeItem* GetUltimateSourceItem() const;
	TIC_CALL const TreeItem* GetCurrUltimateSourceItem() const;

	TIC_CALL bool HasIntegrityChecker() const;
	TIC_CALL auto GetIntegrityChecker() const -> AbstrCalculatorRef;

	TIC_CALL const TreeItem* GetCurrUltimateItem() const;
	TIC_CALL const TreeItem* GetCurrRangeItem() const;
	TIC_CALL const TreeItem* GetUltimateItem() const;
	TIC_CALL const TreeItem* GetCurrRefItem () const;
	TIC_CALL const TreeItem* GetReferredItem() const;
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

	void SetDataChanged(); 

//	SourceLocation
	TIC_CALL void    SetLocation(SourceLocation* loc);
	TIC_CALL const SourceLocation* GetLocation() const override;
	TIC_CALL SharedStr GetConfigFileName  () const;
	TIC_CALL UInt32  GetConfigFileLineNr() const;
	TIC_CALL UInt32  GetConfigFileColNr () const;

	TIC_CALL SharedStr GetSourceName() const override; // override Object

//	StoredProp management
	void AddPropAssoc(AbstrPropDef* propDef) const;
	void SubPropAssoc(AbstrPropDef* propDef) const;

	TIC_CALL void SetDC(const DataController* keyExpr, const TreeItem* newRefItem = nullptr) const;
	TIC_CALL void SetCalculator(AbstrCalculatorRef pr) const; // also called by DataController
	TIC_CALL SharedTreeItemInterestPtr GetInterestPtrOrNull() const;
	TIC_CALL SharedTreeItemInterestPtr GetInterestPtrOrCancel() const;

//protected: // new callback functions
	TIC_CALL virtual bool DoReadItem(StorageMetaInfoPtr smi); friend struct StorageReadHandle;
	TIC_CALL virtual bool DoWriteItem(StorageMetaInfoPtr&& smiHolder) const;
	TIC_CALL virtual void ClearData(garbage_t&) const;
	TIC_CALL virtual void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const;
	TIC_CALL virtual SharedStr GetSignature() const;
	TIC_CALL virtual bool CheckResultItem(const TreeItem* refItem) const;

//	override Actor callbacks
	TIC_CALL ActorVisitState DoUpdate(ProgressState ps) override;
	TIC_CALL void DoInvalidate  () const override;

	TIC_CALL TimeStamp DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const /*noexcept*/ override;

private:
	bool _CheckResultObjType(const TreeItem* refItem) const;

public:
	TIC_CALL ActorVisitState SuspendibleUpdate(ProgressState ps) const override;

// InterestCount management
	TIC_CALL bool PartOfInterest() const;
	bool   HasInterest     () const { return GetInterestCount() || GetKeepDataState(); }
	bool   PartOfInterestOrKeep() const { return PartOfInterest() || GetKeepDataState(); }

	bool CurrHasUsingCache() const { return m_UsingCache;  }
	               UsingCache* GetUsingCache();
	TIC_CALL const UsingCache* GetUsingCache() const;

	TIC_CALL void RemoveFromConfig() const; 

protected:
	void StartInterest() const override;
	garbage_t StopInterest () const noexcept override;

private:
public: // TODO G8: Re-encapsulate
	void EnableAutoDeleteImpl(); // does not call UpdateMetaInfo
	void EnableAutoDeleteRootImpl(); // does not call UpdateMetaInfo
	void MakeCalculator() const;
	void UpdateMetaInfoImpl() const;
	TIC_CALL void SetReferredItem(const TreeItem* refItem) const;
	const TreeItem* DetermineReferredItem(const AbstrCalculator* ac) const;

	void AddItem   (TreeItem* child); // PRECONDITION: child->GetParent()==0;
	void RemoveItem(TreeItem* child); // PRECONDITION: child->GetParent()==this

	bool ReadItem(StorageReadHandle&& srh);
	void SetStorageManager(AbstrStorageManager* sm);

	void SetMetaInfoReady() const;

	void SetInHidden(bool value);
	void SetInTemplate();

	TIC_CALL bool GetIsInstantiated() const;
	TIC_CALL void SetIsInstantiated() const;

	bool CanSubstituteByCalcSpec() const;
public:
	void LoadBlobBuffer (const BlobBuffer& rs);
	void StoreBlobBuffer(      BlobBuffer& rs) const;

	virtual void LoadBlobStream (const InpStreamBuff*);
	virtual void StoreBlobStream(      OutStreamBuff*) const;

	bool CheckBlobBuffer(const BlobBuffer& rs) const;
//	bool LoadBlobIfAny() const;

	// data members
	TIC_CALL static std::atomic<UInt32> s_NotifyChangeLockCount;
	TIC_CALL static UInt32 s_MakeEndoLockCount;
	TIC_CALL static UInt32 s_ConfigReadLockCount;

#if defined(MG_DEBUG)
	TIC_CALL bool CheckMetaInfoReady() const;
	TIC_CALL bool CheckMetaInfoReadyOrPassor() const;
#endif

#if defined(MG_DEBUG_DATA)
public:
	mutable SharedStr md_FullName;
#endif // MG_DEBUG_DATA


	TIC_CALL const TreeItem* GetBackRef() const;

//private: // TODO G8: encapsulate

	TokenID                        m_ID;
	mutable const TreeItem*        m_BackRef = nullptr; // only used by CacheRoots

	// subItems insert/erase themselves from the non-refcounted subItems set and have a counted-ref to their parent
	SharedTreeItem                 m_Parent;   // ro-access, counted-ref
	mutable SharedStr              mc_Expr;


	// optional pointers to various services
	mutable OwningPtr<SupplCache>  m_SupplCache;
	mutable OwningPtr<UsingCache>  m_UsingCache;
	mutable AbstrStorageManagerRef m_StorageManager; 
	mutable rtc::any::Any          m_ReadAssets; friend struct OperationContext;

protected: friend struct CalcDestroyer;
public: // TODO G8: encapsulate and move config attr (aka mc_ ) into a separate ConfigTreeItem class

	mutable treeitem_flag_set      m_StatusFlags;

	mutable AbstrCalculatorRef     mc_Calculator, mc_IntegrityChecker;
	mutable DataControllerRef      mc_DC;
	mutable SharedPtr<const TreeItem> mc_RefItem, mc_OrgItem;

private:
	SharedPtr<SourceLocation>      m_Location;

	DECL_RTTI(TIC_CALL, TreeItemClass)

	friend struct UsingCache;
	friend class  AbstrStorageManager;
	friend struct DataController;
	friend struct DataWriteLock;
	friend struct DmsSpiritProduct;
	friend struct InterestReporter;

	template<typename T> friend void boost::checked_delete(T* x) BOOST_NOEXCEPT;

	// Helper Functions
	friend TIC_CALL const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID);
};

using SharedTreeItem = SharedPtr<const TreeItem>;

#endif // __TREEITEM_H
