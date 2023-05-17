#include "TicPCH.h"
#pragma hdrstop

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "act/ActorLock.h"
#include "act/ActorVisitor.h"
#include "act/InterestRetainContext.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "act/UpdateMark.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/SingleLinkedList.inc"
#include "mci/PropDef.h"
#include "ser/VectorStream.h"
#include "set/VectorFunc.h"
#include "stg/AbstrStorageManager.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "utl/IncrementalLock.h"
#include "utl/MySPrintF.h"
#include "utl/SplitPath.h"
#include "utl/scoped_exit.h"
#include "utl/Swapper.h"
#include "xct/DmsException.h"

#include "LispList.h"

#include "LockLevels.h"

#include "AbstrCalculator.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLockContainers.h"
#include "DataStoreManagerCaller.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataItemClass.h"
#include "DataLocks.h"
#include "DataStoreManager.h"
#include "FreeDataManager.h"
#include "LispTreeType.h"
#include "OperationContext.ipp"
#include "Operator.h"
#include "OperGroups.h"
#include "PropFuncs.h"
#include "ExprRewrite.h"
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

#include <stdarg.h>
#include <future>


using TreeItemInterestPtr = InterestPtr<const TreeItem*>;

#include "cs_lock_map.h"
using treeitem_lock_map = cs_lock_map<SharedTreeItem>;

// #undef MG_DEBUG_DATA // DEBUG MEMORY ALLOCS AND SETS md_FullName

bool TreeItem::IsEditable() const
{
	if (HasCalculator())
		if (!mc_Calculator || !mc_Calculator->IsDataBlock())
			return false;

	return ((!IsLoadable()) || IsStorable()); 
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
	if (!HasSupplCache())
		m_SupplCache.assign( new SupplCache );
	return m_SupplCache;
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


#if defined(MG_DEBUG)

/* REMOVE
SizeT d_DcKnownCount = 0;

SizeT TreeItem::GetDcKnownCount() { return d_DcKnownCount; }

#endif //defined(MG_DEBUG)

void TreeItem::SetDcKnown  () const
{
//	dms_assert(!IsDcKnown());
	SetTSF  (TSF_DSM_CrKnown);
	MG_DEBUGCODE( ++d_DcKnownCount; )
}

void TreeItem::ClearDcKnown() const
{
//	dms_assert( IsDcKnown());
	ClearTSF(TSF_DSM_CrKnown);
	MG_DEBUGCODE( --d_DcKnownCount; )
}
*/
#endif //defined(MG_DEBUG)

#if defined(MG_DEBUG_DATA)

namespace {

	static_ptr<TreeItemSetType> s_TreeItems;
	UInt32                      s_nrTreeItemAdmLocks = 0;
	TreeItemAdmLock             s_treeItemAdm;

}	// anonymous namespace


void ReportDataItem(const AbstrDataItem* di)
{
	std::size_t c = di->GetDataObj()->GetNrBytesNow(false);
	if (c<1000) return;
	MGD_TRACE(("RC=%d; IC=%d; KE=%d; DL=%d, Nr=%Iu, Nm=%s",
		di->GetRefCount(),
		di->GetInterestCount(),
		di->GetKeepDataState(),
		di->GetDataObjLockCount(),
		c,
		di->GetFullName().c_str()
	));
}

void TreeItemWithMemReport()
{
	if (!s_TreeItems)
		return;

	DBG_START("TreeItem", "ReportMem", true);
	for (auto itemPtr: *s_TreeItems)
	{
		auto di = AsDynamicDataItem(itemPtr);
		if (!di)
			continue;
		auto ado = di->m_DataObject;
		if (!ado)
			continue;
			ReportDataItem(di);
	}
}

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
	s_TreeItems.assign( new TreeItemSetType );
}

TreeItemAdmLock::~TreeItemAdmLock()
{
	if (--s_nrTreeItemAdmLocks)
		return;

	dms_assert(s_TreeItems);
	Report();

	dms_assert(!s_TreeItems->size());
	s_TreeItems.reset();
}

void TreeItemAdmLock::Report()
{
	dms_assert(s_TreeItems);
	auto lock = std::scoped_lock(s_TreeItems->critical_section);

	SizeT n = s_TreeItems->size();
	if(n)
	{
		reportF_without_cancellation_check(SeverityTypeID::ST_Error, "MemoryLeak of %d TreeItems. See EventLog for details.", n);

		TreeItemSetType::iterator i = s_TreeItems->begin();
		TreeItemSetType::iterator e = s_TreeItems->end();
		while (i!=e)
		{
			const TreeItem* ti = *i++;
			reportF_without_cancellation_check(SeverityTypeID::ST_MajorTrace, "MemoryLeak: %s (%d,%d) %s",
				ti->GetDynamicClass()->GetName(), 
				ti->GetRefCount(), 
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
//	dbg_assert(!IsDcKnown()); // this destruction implies that no DCs refer to it anymore 
//	dms_assert(!IsKnown()); // this destruction implies that no DCs refer to it anymore 
	DisableStorage();

	MG_LOCKER_NO_UPDATEMETAINFO

	NotifyStateChange(this, NC_Deleting);

	SetKeepDataState(false); // StringDC en NumbDC cache items hebben ook KeepInterest

	dms_assert(_GetFirstSubItem() == 0);

	if (mc_RefItem)
		SetReferredItem(nullptr);

	dms_assert(!HasInterest());
	dms_assert( !m_State.Get(actor_flag_set::AF_SupplInterest) );

	if (m_Parent)
		const_cast<TreeItem*>(m_Parent.get_ptr())->RemoveItem(this);

	if (GetTSF(TSF_HasStoredProps))
		RemoveStoredPropValues(this);

#if defined(MG_DEBUG_DATA)
	auto lock = std::scoped_lock(s_TreeItems->critical_section);
	dms_assert(s_TreeItems->find(this) != s_TreeItems->end());
	s_TreeItems->erase(this);
#endif
}

void ResetAllKeepInterest(TreeItem* item)
{
	dms_assert(item);
	TreeItem* walker = item;
	do
	{
		walker->SetKeepDataState(false); 
		if (walker->mc_RefItem)
			const_cast<TreeItem*>(walker->mc_RefItem.get_ptr())->SetKeepDataState(false); 
		walker = item->WalkCurrSubTree(walker);
	} while (walker);
}

void TreeItem::DisableAutoDelete() // does not call UpdateMetaInfo
{
	if (IsAutoDeleteDisabled())
		return;

	SharedPtr<TreeItem> subItem = _GetFirstSubItem(); 
	while (subItem)
	{
		subItem->DisableAutoDelete();
		subItem = subItem->GetNextItem();
	}

	SetTSF(TSF_IsAutoDeleteDisabled, true); // call inherited
	IncRef();
}

void TreeItem::EnableAutoDeleteImpl() // does not call UpdateMetaInfo
{
	DBG_START("TreeItem", "EnableAutoDeleteImpl", false); //  DEBUG Access violation

	DBG_TRACE(("Item: %s", GetFullName().c_str()));      //  DEBUG Access violation

	dms_assert(IsAutoDeleteDisabled());

	if (!IsCacheItem())
		DisableStorage();

	SharedPtr<TreeItem> subItem = _GetFirstSubItem(); 
	while (subItem)
	{
		if (subItem->IsAutoDeleteDisabled() )
		{
			#if defined(MG_DEBUG_DATA)
				auto_flag_recursion_lock<ASFD_SetAutoDeleteLock> reentryLock(subItem->Actor::m_State);
			#endif
			subItem->EnableAutoDeleteImpl();
		}
		subItem = subItem->GetNextItem(); // this line may cause the destruction of the old subItem
	}

	SetTSF(TSF_IsAutoDeleteDisabled, false); // call inherited
	Release();
}

#if defined(MG_DEBUG)
bool ExplainValue_IsClear();
#endif

void TreeItem::EnableAutoDeleteRootImpl() // does not call UpdateMetaInfo
{
	DBG_START("TreeItem", "EnableAutoDeleteRootImpl", true);

	dbg_assert(ExplainValue_IsClear());


	dms_assert(!SessionData::Curr() || !SessionData::Curr()->GetConfigRoot() || SessionData::Curr()->GetConfigRoot() == this );

	DBG_TRACE(("START ResetAllKeepIterest(this)"));
	ResetAllKeepInterest(this);              // neccesary to bring interestCount to 0 and DataInMem to DiskCache         

	StaticMtIncrementalLock<TreeItem::s_NotifyChangeLockCount> dontNotify;

	SafeFileWriterArray sfwa;
	EnableAutoDeleteImpl(); // this may be destroyed

	DBG_TRACE(("START SessionData::ReleaseIt(this)"));
	SessionData::ReleaseIt(this);

	DBG_TRACE(("START sfwa.Commit()"));
	sfwa.Commit();
}

void TreeItem::EnableAutoDelete() // does not call UpdateMetaInfo
{
	if (! IsAutoDeleteDisabled())
		return;

	#if defined(MG_DEBUG_DATA)
		dms_assert(!Actor::m_State.Get(ASFD_SetAutoDeleteLock)); 
		Actor::m_State.Set(ASFD_SetAutoDeleteLock);
	#endif

	bool isConfigRoot = !(IsCacheItem() || IsEndogenous() || GetTreeParent());

	mc_Calculator.reset();
	mc_IntegrityChecker.reset();

	if (isConfigRoot) // we have a configRoot: close all handles to it
		EnableAutoDeleteRootImpl();
	else
		EnableAutoDeleteImpl();
	// this may be deleted here or after the keepAliveForReentryLock destruction
}

// MTA, 16-08-2004:
// there are some strong correlations between flags that need to be checked, and fased out (remove redundant flags).
//
// (semi) invariants: 
//
// TSF_IsCacheItem == actor_flag_set::AF_IsPassor if CacheRoot (subitems are also CacheItems but not passors)
// TSF_IsCacheItem => TSF_AutoDeleteDisabled
// TSF_IsCacheItem => TSF_IsEndogenous

void TreeItem::SetIsCacheItem() // does not call UpdateMetaInfo
{
	dms_assert(IsEndogenous());
	dms_assert(IsPassor());
	dms_assert(! IsCacheItem());
	dms_assert(! IsAutoDeleteDisabled());
	dms_assert(! IsCacheItem()); // only call once
	dms_assert(! GetTreeParent()); // only call on root

	DisableAutoDelete();

	TreeItem* walker = this;
	do {
		walker->m_StatusFlags.Set(TSF_IsCacheItem);
		walker = WalkCurrSubTree(walker);
	} while(walker);
}

void TreeItem::InitTreeItem(TreeItem* parent, TokenID id)
{
	dms_assert(m_State.GetProgress() < PS_MetaInfo);
	if (id) CheckTreeItemName( id.GetStr().c_str() );
	m_ID = id;

	assert(!_GetFirstSubItem()); // not allowed since the FullName of sub items would be corrupted

	assert(IsMetaThread());
	if (s_MakeEndoLockCount)
		SetTSF(TSF_IsEndogenous);
	if (parent) 
	{
		MG_LOCKER_NO_UPDATEMETAINFO

		// inherit some TreeItem State Flags.
		if (parent->IsAutoDeleteDisabled())
			DisableAutoDelete();
		parent->AddItem(this);
		m_StatusFlags.Set( parent->m_StatusFlags.GetBits(TSF_InTemplate | TSF_IsCacheItem | TSF_InHidden) );

		// special processing
		if (parent->IsPassor())
			SetPassor();
		if (parent->GetKeepDataState())
			SetKeepDataState(true);
		if (parent->GetFreeDataState())
			SetFreeDataState(true); 
		if (parent->GetStoreDataState())
			SetStoreDataState(true);
		NotifyStateChange(parent, NC_NewSubItem);
	}
#if defined(MG_DEBUG_DATA)
	md_FullName = GetFullName();
	if (parent)
		md_FullName = parent->md_FullName + '/' + GetName().c_str();
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
	dms_assert(m_Parent.is_null() || m_ID); // All SubItems must have a name
	return m_ID;
}

//----------------------------------------------------------------------
// Containment Functions
//----------------------------------------------------------------------

bool TreeItem::HasSubItems   () const
{
	if (_GetFirstSubItem())
		return true;
	if (IsDataItem(this))
		return false;
	UpdateMetaInfo();
	return _GetFirstSubItem();
}

UInt32 TreeItem::CountNrSubItems () const
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

UInt32 TreeItem::_CountNrSubItems ()
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

const TreeItem* TreeItem::GetFirstSubItem() const
{
	UpdateMetaInfo();
	return _GetFirstSubItem();
}

const TreeItem* TreeItem::GetCurrFirstSubItem() const
{
	assert(m_State.GetProgress() >= PS_MetaInfo);
	return _GetFirstSubItem();
}

void TreeItem::AddItem(TreeItem* child)
{
	dms_assert(child);
	dms_assert(!child->m_Parent);

	dms_assert(!GetSubTreeItemByID(child->GetID()));

	dms_assert(!child->GetInterestCount());
	child->m_Parent = this;
	dms_assert(child->IsAutoDeleteDisabled() == IsAutoDeleteDisabled() );

	AddSub(child);

	if (m_UsingCache)         m_UsingCache->OnItemAdded(child);
}

void TreeItem::RemoveItem(TreeItem* child)
{
	MGD_PRECONDITION(child);
	dms_assert(child->m_Parent == this);

	if (m_UsingCache) m_UsingCache->OnItemRemoved(child);

	DelSub(child);

	SharedTreeItem thisHolder;
	bool mustDisconnectInterest;
	{
		leveled_std_section::scoped_lock globalSectionLock(sg_CountSection);
		mustDisconnectInterest = child->m_InterestCount;
		thisHolder = std::move(child->m_Parent); // reduces ref count
	}
	if (mustDisconnectInterest)
		thisHolder->DecInterestCount();
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
s5 = GetTokenID_st("label"),
s6 = GetTokenID_st("palette"),
s7 = GetTokenID_st("VAT"),
s8 = GetTokenID_st("lokatie"),
s9 = GetTokenID_st("grens"),
s10 = GetTokenID_st("lijn");

bool IsGenericID(TokenID id)
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
			return SharedStr(tu->GetUnitClass()->GetValueType()->GetName().c_str());
	}

	if (IsGenericID(GetID()) && GetTreeParent())
		return GetTreeParent()->GetDisplayName() + " " + SharedStr(GetID());

	return SharedStr(GetID());
}

SharedStr TreeItem::GetExpr() const
{
	if (m_Parent)
		m_Parent->UpdateMetaInfo();
	return mc_Expr;
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
		throwItemErrorF("SetExpr(%s) not allowed since Calculator is set by parent", SingleQuote( exprStr.begin(), exprStr.send() ).c_str());
	}

	if (mc_Expr != expr)
	{
		AssertPropChangeRights("CalculationRule");

		mc_Expr = expr;

		Invalidate();

		NotifyStateChange(this, NC_PropValueChanged);
	}
}

void TreeItem::SetDC(const DataController* newDC, const TreeItem* newRefItem) const
{
	dms_assert(!InTemplate() || !newDC && !newRefItem);

	if (mc_DC == newDC && (!newRefItem || newRefItem == mc_RefItem))
		return;

	SharedTreeItem newRI;
	if (newDC)
		newRI = newDC->MakeResult();
	if (newRefItem)
		newRI = newRefItem;

	if (newRI == this)
	{
//		newDC = nullptr; // TODO G8: avoid construction of SourceDescr(...) at this phase
		newRI = nullptr;
	}
	InterestPtr<DataControllerRef> oldDC;
	SharedTreeItemInterestPtr oldRefItem;

	// TODO G8: re-evaluate for thread and exception safety: set up private and commit in nothrow critical section or lock-free OK.
	TreeItemInterestPtr interestCopy = GetInterestPtrOrNull();

	if (interestCopy)
	{
		if (newDC)
			newDC->IncInterestCount(); // can throw, then what happens with mc_Calculator

		if (mc_DC)
		{
			MG_DEBUGCODE(UInt32 dcIC = mc_DC->GetInterestCount());
			dbg_assert(dcIC >= 1);
			oldDC = std::move(mc_DC);
			dms_assert(!mc_DC);
			oldDC->DecInterestCount();
			dbg_assert(oldDC->GetInterestCount() == dcIC);
		}
		if (mc_RefItem)
		{
			MG_DEBUGCODE(UInt32 refIC = mc_RefItem->GetInterestCount());
			dbg_assert(refIC >= 1);
			oldRefItem = std::move(mc_RefItem);
			dms_assert(!mc_RefItem);
			oldRefItem->DecInterestCount();
			dbg_assert(oldRefItem->GetInterestCount() == refIC);
		}
	}
	mc_DC = std::move(newDC);
	SetReferredItem(newRI);
}

void TreeItem::SetCalculator(AbstrCalculatorRef pr) const
{
	dms_check_not_debugonly;
	dms_assert(IsMetaThread());

	if (pr == mc_Calculator)
		return;
	dms_assert(pr);
	mc_Calculator = std::move(pr);
}

SharedTreeItemInterestPtr TreeItem::GetInterestPtrOrNull() const 
{
	return static_cast<const TreeItem*>(Actor::GetInterestPtrOrNull().get_ptr()); 
}

bool TreeItem::HasCalculatorImpl() const
// if true this func guarantees that GetCalculator will return a non-null mc_Calculator
// true if not in template and (has expr or creator) or this is cache-result
// in which case mc_Calculator has already been set by DataController to a DC_BackPtr
{
	dbg_assert(IsPassor() || !m_Parent || m_Parent->CheckMetaInfoReady() || s_MakeEndoLockCount);
	if (mc_Calculator)
		return true;
	// items in templates never have calculators
	if (InTemplate())
		return false;
	if (!mc_Expr.empty())
		return true;
	if (IsUnit(this) && GetTSF(USF_HasConfigRange))
	{
		dms_assert(!IsCacheItem());
		dms_assert(!IsPassor());
		dms_assert(AsUnit(this)->HasVarRange());
		return true;
	}
	return false;
}

bool TreeItem::HasCalculator() const
// if true this func guarantees that GetCalculator will return a non-null mc_Calculator
// true if not in template and (has expr or creator) or this is cache-result
// in which case mc_Calculator has already been set by DataController to a DC_BackPtr
{
	dms_check_not_debugonly; 

	if (!IsPassor() && m_Parent && !m_Parent->Was(PS_MetaInfo))
		m_Parent->UpdateMetaInfo();

	return HasCalculatorImpl();
}

bool TreeItem::CanSubstituteByCalcSpec() const // TODO G8: Substitute away
{
	if (HasCalculator())
		return true;
	return false;
}

bool TreeItem::HasIntegrityChecker() const
{
	return !InTemplate() && integrityCheckPropDefPtr->HasNonDefaultValue(this);
}

AbstrCalculatorRef TreeItem::GetIntegrityChecker() const
{
	dms_assert(HasIntegrityChecker()); // Precondition
	if (!mc_IntegrityChecker)
	{
		SharedStr iCheckStr = integrityCheckPropDefPtr->GetValue(this);
		mc_IntegrityChecker = AbstrCalculator::ConstructFromStr(this, iCheckStr, CalcRole::Checker);
	}
	return mc_IntegrityChecker;
}

void TreeItem::AssertPropChangeRights(CharPtr changeWhat) const
{
	if ((! IsEndogenous()) || s_MakeEndoLockCount)
		return;
	if (! UpdateMarker::HasActiveChangeSource() )
		throwItemErrorF("Illegal attempt to change the %s of an endogenous item", changeWhat);
}

void TreeItem::AssertDataChangeRights(CharPtr changeWhat) const
{
	dms_check_not_debugonly;

	dms_assert(!IsCacheItem()); // PRECONDITION

	if (!HasConfigData())
		throwItemErrorF("Illegal attempt to change the %s of a calculatable item", changeWhat);
	if (IsStorable())
		return;
	if (IsLoadable()) // data is not derivable
		throwItemErrorF("Illegal attempt to change the %s of a loadable item", changeWhat);
	dms_assert(!IsDerivable()); // implied by !IsLoadable() && !HasCalculator() || HasConfigData, but MUTATING through HasCalculator
//	dms_assert(HasConfigSource()); // implied by !IsDerivable && !IsCacheItem(), but MUTATING through HasCalculator

//	AssertPropChangeRights(changeWhat);
	dms_assert(IsMainThread());

	if ((! IsEndogenous()) || s_MakeEndoLockCount)
		return;
	if (! UpdateMarker::HasActiveChangeSource() )
		reportF(SeverityTypeID::ST_Warning, "Changing the %s of endogenous item %s", changeWhat, GetSourceName().c_str());
}

SharedPtr<const AbstrCalculator> TreeItem::GetCalculator() const
{
	MakeCalculator();
	return mc_Calculator;
}

void ApplyCalculator(TreeItem* holder, const AbstrCalculator* ac)
{
	// TODO G8: Re-evaluate types here; going to variant and back looks contrived
	auto metaInfo = ac->GetMetaInfo();
	if (metaInfo.index() == 0)
	{
		std::get<MetaFuncCurry>(metaInfo).operator()(holder, ac);
		assert(ac->GetHolder()->GetIsInstantiated() || ac->GetHolder()->WasFailed(FR_MetaInfo));
	}
}

void TreeItem::MakeCalculator() const
{
	dms_check_not_debugonly;

	if (WasFailed(FR_Determine))
		return;

	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < PS_MetaInfo && !GetTreeParent()->WasFailed(FR_MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	dms_assert(!m_Parent || (m_Parent->m_State.GetProgress() >= PS_MetaInfo) || m_Parent->WasFailed(FR_MetaInfo));

	//	may only be called after HasCalculator (would) return(ed) true
//	dms_assert(!InTemplate() || (mc_Calculator && mc_Calculator->DelayDataControllerAccess()));
//	dms_assert(mc_Calculator || !mc_Expr.empty()); 
	if (mc_DC || GetIsInstantiated() || mc_Calculator)
		return;

	TreeItemContextHandle tich(this, "MakeCalculator"); FencedInterestRetainContext irc;

	if (m_State.Get(ASF_MakeCalculatorLock))
		return Fail(
			"Invalid recursion in GetCalculator detected.\n"
			"Check calculation rule of this item"
			, FR_Determine
		);
	auto_flag_recursion_lock<ASF_MakeCalculatorLock> lock(m_State);

	if (mc_Expr.empty() && (IsCacheItem() || !IsUnit(this))|| IsPassor())
		return;


	try {
		AbstrCalculatorRef newCalculator;
		if (!mc_Expr.empty())
			newCalculator = AbstrCalculator::ConstructFromStr(this, mc_Expr, CalcRole::Calculator);
		SetCalculator(newCalculator);
	}
	catch (...)
	{
		return CatchFail(FR_Determine);
	}
	if (WasFailed(FR_Determine))
		return;

}

void FailItemType(const TreeItem* self, const TreeItem* refItem)
{
	auto msg = mySSPrintF("ItemType %s is incompatible with the result of the calculation which is of type %s"
	,	self->GetDynamicObjClass()->GetName().c_str()
	,	refItem->GetDynamicObjClass()->GetName().c_str()
	);
	self->Fail(msg, FR_Determine);
}

bool TreeItem::_CheckResultObjType(const TreeItem* refItem) const
{
	assert(refItem);
	if (WasFailed(FR_Determine))
		return false;
	try {
		if (refItem->GetDynamicObjClass()->IsDerivedFrom(GetDynamicObjClass()) )
			return true;
		FailItemType(this, refItem);
	}
	catch (...)
	{
		if (!WasFailed(FR_Determine))
		{
			auto err = catchException(true);
			DoFail(err, FR_Determine);
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

const TreeItem* TreeItem::GetCurrRefItem() const
{
	dms_assert(Was(PS_MetaInfo) || IsPassor() || IsUnit(this) && AsUnit(this)->IsDefaultUnit());
	return mc_RefItem;
}

const TreeItem* TreeItem::GetReferredItem() const
{
//	UpdateMetaInfo();
	dms_assert(!SuspendTrigger::DidSuspend());
	if (m_Parent) 
		m_Parent->UpdateMetaInfo();

	if (!mc_RefItem && HasCalculator())
	{
		MakeCalculator();
//		if (!WasFailed(FR_Determine) && mc_Calculator)
//			if (mc_Calculator->DelayDataControllerAccess())
//				SetReferredItem(DetermineReferredItem(mc_Calculator));
	}

	return mc_RefItem;
}

const TreeItem* TreeItem::GetCurrUltimateItem() const
{
	return _GetCurrUltimateItem(this);
}

const TreeItem* TreeItem::GetCurrRangeItem() const
{
	return _GetCurrRangeItem(this);
}

const TreeItem* TreeItem::GetUltimateItem() const
{
	UpdateMetaInfo();
	return _GetUltimateItem(this);
}

const TreeItem* TreeItem::GetSourceItem() const
{
	const TreeItem* sourceItem = this;
	do
	{
		sourceItem = sourceItem->GetReferredItem();
	}
	while (sourceItem && sourceItem->IsCacheItem());
	dms_assert(sourceItem != this);
	return sourceItem;
}

const TreeItem* TreeItem::GetUltimateSourceItem() const
{
	const TreeItem* item = this;
	const TreeItem* source;
	while (source = item->GetSourceItem())
		item = source;
	dms_assert(item);
	return item;
}

const TreeItem* TreeItem::GetCurrSourceItem() const
{
	const TreeItem* sourceItem = this;
	do
	{
		sourceItem = sourceItem->mc_RefItem;
	}
	while (sourceItem && sourceItem->IsCacheItem());

	return sourceItem;
}

const TreeItem* TreeItem::GetCurrUltimateSourceItem() const
{
	const TreeItem* item = this;
	const TreeItem* source;
	while (source = item->GetCurrSourceItem())
		item = source;
	dms_assert(item);
	return item;
}

// ============ SetRefItem
struct OldRefDecrementer : SharedPtr<const Actor>
{
	~OldRefDecrementer() {
		if (has_ptr())
			get_ptr()->DecInterestCount();
	}
	using SharedPtr::operator=;
};

void TreeItem_RemoveInheritedSubItems(TreeItem* self)
{
	dms_assert(IsMetaThread());

	TreeItem* si = self->_GetFirstSubItem();
	while (si)
	{
		auto nsi = si->GetNextItem();
		if (si->IsInherited())
		{
			dms_assert(si->IsAutoDeleteDisabled());
			dms_assert(si->m_Parent == self);
			SharedPtr<TreeItem> ssi = si;
			si->RemoveFromConfig();
		}
		si = nsi;
	}
}

void TreeItem::SetReferredItem(const TreeItem* refItem) const
{
	dms_assert(IsMetaThread() || !refItem);

	dms_assert(!IsDataItem(this) || AsDataItem(this)->GetDataObjLockCount() <= 0); // DON'T MESS WITH SHARED-LOCKED ITEMS

	dms_assert(refItem != this);
	dms_assert(!refItem || !refItem->InTemplate());
	if (mc_RefItem == refItem)
		return;

	if (refItem && !_CheckResultObjType(refItem))
		refItem = nullptr;

//	if (mc_RefItem && mc_RefItem->IsCacheRoot() && _GetFirstSubItem()) // when called from destructor, all subitems were already destroyed
//		TreeItem_RemoveInheritedSubItems(const_cast<TreeItem*>(this)); // only allowed from MainThread()

	// remove the old interest
	OldRefDecrementer oldRefItemCounter;
	SharedPtr<const TreeItem> tmpRefItemHolder = refItem;
	TreeItemInterestPtr newRefItemCounter;

retry:
	if (m_InterestCount)
		newRefItemCounter = refItem; // calls IncInterestCount, which can throw, current interest might disappear concurrently
	{
		leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection); // check and swap or try again
		if (m_InterestCount) // still interested?
		{
			dms_assert(newRefItemCounter || !refItem); // Starting Interest only allowed from this main thread. 
			if (refItem && !newRefItemCounter) // situation has changed 
				goto retry;
			// point of certain return, prepare settlement upon destruction
			newRefItemCounter.release();
			oldRefItemCounter = mc_RefItem.get(); // decrement interest count upon destruction
		}
		else
			newRefItemCounter = nullptr; // decrease new interest if current interest has been released concurrently
		// everything is OK now to do a swap of responsibilities

		if (mc_RefItem && mc_RefItem != tmpRefItemHolder && mc_RefItem->m_BackRef == this)
			mc_RefItem->m_BackRef = nullptr;
		mc_RefItem = std::move(tmpRefItemHolder);
		if (mc_RefItem && mc_RefItem->IsCacheRoot() && !mc_RefItem->m_BackRef)
			mc_RefItem->m_BackRef = this;
	}

	dms_assert(!oldRefItemCounter || oldRefItemCounter->GetInterestCount()); // will retain interest up to destruction, now privately owned.

	if (!mc_RefItem)
		return;

	mc_RefItem->DetermineState();
	if (GetKeepDataState()) 
		const_cast<TreeItem*>(mc_RefItem.get_ptr())->SetKeepDataState(true); // LET OP: State is niet weggehaald bij vorige refItem (want er zijn misschien nog andere keepers)

	const UInt32 inheritedFlags = TSF_Depreciated | TSF_Categorical;
	m_StatusFlags.SetBits(inheritedFlags, mc_RefItem->m_StatusFlags.GetBits(inheritedFlags));
}

// ============ GetParent

const PersistentSharedObj* TreeItem::GetParent () const
{
	return GetTreeParent();
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
		dms_assert(!mc_Calculator);
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

void TreeItem::SetKeepDataState(bool value)
{ 
	if (GetTSF(TSF_KeepData) != value)
	{
		SetTSF(TSF_KeepData, value);
		for (TreeItem* subItem = _GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->SetKeepDataState(value);
		if (!value)
		{
			const TreeItem* uti = _GetHistoricUltimateItem(this);
			actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("TreeItem::SetKeepDataState") sg_ActorLockMap, uti); // datalockcount 1->0 or drop of interest is 
			uti->TryCleanupMem();
		}
	}
	if (value)
		if (mc_RefItem)
			const_cast<TreeItem*>(mc_RefItem.get())->SetKeepDataState(true);
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

const TreeItem* TreeItem::GetStorageParent (bool alsoForWrite) const
{
	if (GetTSF(TSF_InTemplate | TSF_IsCacheItem))
		return nullptr;
	const TreeItem* storageParent = this;
	do {
		if (storageParent->IsDisabledStorage())
			return nullptr;
		if (storageParent->HasStorageManager())
		{
			if (alsoForWrite && storageParent->GetStorageManager()->IsReadOnly())
				return nullptr;
			return storageParent;
		}
		storageParent = storageParent->m_Parent;
	} while (storageParent);
	return nullptr;
}

const TreeItem* TreeItem::GetCurrStorageParent(bool alsoForWrite) const
{
	if (GetTSF(TSF_InTemplate | TSF_IsCacheItem))
		return nullptr;
	const TreeItem* storageParent = this;
	do {
		if (storageParent->IsDisabledStorage())
			return nullptr;
		auto sm = storageParent->GetCurrStorageManager();
		if (sm)
		{
			if (alsoForWrite && sm->IsReadOnly())
				return nullptr;
			return storageParent;
		}
		storageParent = storageParent->m_Parent;
	} while (storageParent);
	return nullptr;
}

bool TreeItem::IsLoadable() const
{
	return (IsDataItem(this) || IsUnit(this)) 
		&&	GetStorageParent(false);
}

bool TreeItem::IsCurrLoadable() const
{
	dms_assert(!m_Parent || m_Parent->Was(PS_MetaInfo));
	return (IsDataItem(this) || IsUnit(this))
		&& GetCurrStorageParent(false);
}

bool TreeItem::IsStorable() const
{
	if (!IsDataItem(this) && !IsUnit(this)) 
		return false;
	const TreeItem* storageParent = GetStorageParent(true);
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
		self = self->GetTreeParent();
		dms_assert(self);
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
		m_UsingCache.assign(new UsingCache(this));
	return m_UsingCache;
}

const UsingCache* TreeItem::GetUsingCache() const
{
	if (!m_UsingCache)
		m_UsingCache.assign(new UsingCache(this));
	return m_UsingCache;
}

void TreeItem::RemoveFromConfig() const
{
	dms_assert(!IsCacheItem());
	auto self = const_cast<TreeItem*>(this);
	dms_assert(self);
	dms_assert(m_RefCount > 0); // Disabled Auto Delete results in at least one refCount
	SharedPtr<TreeItem> holder(self);
	dms_assert(m_RefCount > 1);
	self->EnableAutoDelete();
	dms_assert(m_RefCount > 0); // holder counts as well

	auto tp = GetTreeParent();
	if (!tp)
		return;

	// make this invisible and then exclude from parent to avoid finding them. This should be synchronized with getting new references, but it seems unlikely that this might become a realistic issue
	NotifyStateChange(this, NC_Deleting);
	const_cast<TreeItem*>(GetTreeParent())->RemoveItem(self);
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
	bool hasCalculator = HasCalculatorImpl();
	bool hasConfigData = HasConfigData();
	return isLoadable && !hasCalculator && !hasConfigData;
}

//----------------------------------------------------------------------
// TreeItem Find Functions
//----------------------------------------------------------------------

const TreeItem* TreeItem::GetConstSubTreeItemByID(TokenID subItemID) const
{
	dms_assert(this);

	if (!this) 
		return nullptr;

	const TreeItem* subItem = GetFirstSubItem(); // calls UpdateMetaInfo
	while (true)
	{
		if (!subItem)
		{
			if (mc_RefItem)
			{
				dms_assert(mc_RefItem != this);
				return mc_RefItem->GetConstSubTreeItemByID(subItemID);
			}
			return nullptr;
		}

		if	(subItem->GetID() == subItemID)
			return subItem;
		subItem = subItem->GetNextItem();
	}
}

const TreeItem* TreeItem::GetCurrSubTreeItemByID(TokenID subItemID) const
{
	dms_assert(this);

	if (!this)
		return nullptr;

	const TreeItem* subItem = GetCurrFirstSubItem(); // requires UpdateMetaInfo to have been called
	while (true)
	{
		if (!subItem)
		{
			if (mc_RefItem)
			{
				dms_assert(mc_RefItem != this);
				return mc_RefItem->GetCurrSubTreeItemByID(subItemID);
			}
			return nullptr;
		}

		if (subItem->GetID() == subItemID)
			return subItem;
		subItem = subItem->GetNextItem();
	}
}

TreeItem* TreeItem::GetSubTreeItemByID(TokenID subItemID) // does not UpdateMetaInfo
{
	if (!this) 
		return nullptr;

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
		if (ids.second.first != subItemNames.first || ids.second.size() && ids.second.first[0] == '.')
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
		if (ids.second.first != subItemNames.first || ids.second.size() && ids.second.first[0] == '.')
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

const TreeItem* TreeItem::GetCurrItem(CharPtrRange subItemNames) const
{
	dms_assert(this);
	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	if (ids.first.empty()) // subItemNames is an atomic token or parent = root
	{
		if (ids.second.first != subItemNames.first || ids.second.size() && ids.second.first[0] == '.')
			throwItemError("GetCurrItem is not allowed to look outside the accessible search context");
		return GetCurrSubTreeItemByID(GetTokenID(ids.second));
	}
	const TreeItem* parent = GetCurrItem(ids.first);
	return (parent) ? parent->GetCurrSubTreeItemByID(GetTokenID(ids.second)) : nullptr;
}


const TreeItem* TreeItem::FindItem(CharPtrRange subItemNames) const
{
	dms_assert(this);
	dms_assert(IsMetaThread());

	if (subItemNames.empty())
		return this;

	auto ids = NameTreeReg_GetParentAndBranchID(subItemNames);
	dms_assert(ids.first.first == subItemNames.first);
	dms_assert(ids.second.second == subItemNames.second);
	if (ids.second.first == subItemNames.first) // subItemNames is an atomic token
	{	
		dms_assert(!ids.second.empty());
		if (ids.second.first[0] == '.')
			return FollowDots(ids.second);

		UpdateMetaInfo();
		TokenID existingToken = GetExistingTokenID<mt_tag>(ids.second); //to be found token was already created if asserts hold
		if (!IsDefined(existingToken))
			return nullptr;
		return FindTreeItemByID(this, existingToken);
	}
	if (ids.first.empty()) // We start at root.
	{
		const TreeItem* configRoot = SessionData::Curr()->GetConfigRoot();
		if (configRoot)
		{
			configRoot->UpdateMetaInfo();
			return configRoot->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
		}
		return nullptr;
	}
	const TreeItem* parent = FindItem(ids.first);
	if (!parent)
		return nullptr;
	parent->UpdateMetaInfo();
	return parent->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
}

auto FollowBestDots(const TreeItem* self, CharPtrRange dots) noexcept -> BestItemRef
{
	dms_assert(self);
	dms_assert(dots.size());
	while (true)
	{
		if (*dots.first != '.')
			return { self, SharedStr(dots.first) };
		dots.first++;
		if (dots.first == dots.second)
			return { self, SharedStr(dots.first) };

		auto parent = self->GetTreeParent();
		if (!parent)
			return { self, SharedStr(dots.first - 1) };
		self = parent;
	}
}


auto TreeItem::FindBestItem(CharPtrRange subItemNames) const -> BestItemRef
{
	dms_assert(this);

	if (subItemNames.empty())
		return { this, {} };

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
				return { result, {} };
		}
		return { this, SharedStr(ids.second) };
	}

	if (ids.first.empty()) 
	{
		// We start at root, first characted was '/'
		dms_assert(subItemNames[0] == DELIMITER_CHAR);
		dms_assert(ids.second.first = subItemNames.first + 1);

		const TreeItem* configRoot = SessionData::Curr()->GetConfigRoot();
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
	dms_assert(!ids.first.empty());
	//dms_assert(!ids.second.empty()); Wrong if path contains '//' 

	auto parentRef = FindBestItem(ids.first);
	if (!parentRef.first)
		return { this, SharedStr(subItemNames) };

	if (!parentRef.second.empty())
		return { parentRef.first, parentRef.second + SharedStr(ids.first.second, ids.second.second) };

	parentRef.first->UpdateMetaInfo();
	auto result = parentRef.first->GetConstSubTreeItemByID(GetExistingTokenID(ids.second));
	if (!result)
		return { parentRef.first, SharedStr(ids.second) };
	return { result, {} };
}

const TreeItem* TreeItem::CheckObjCls(const Class* requiredClass) const
{
	dms_assert(requiredClass);
	if (!this)
		return 0; //requiredClass->throwItemError("Illegal cast of null pointer");
	const Class* thisClass = requiredClass->IsDataObjType()
			?	GetDynamicObjClass()
			:	GetDynamicClass();

	if	(!	thisClass->IsDerivedFrom(requiredClass))
		throwItemErrorF("Cannot cast to the requested type: %s", 
			requiredClass->GetName()
		);
	return this;
}

TreeItem* TreeItem::CheckCls(const Class* requiredClass)
{
	if (!this)
		return 0; //throwItemError("Illegal cast of null pointer");
	dms_assert(requiredClass);

	const Class* thisClass = requiredClass->IsDataObjType()
			?	GetCurrentObjClass()
			:	GetDynamicClass();

	if (!	thisClass->IsDerivedFrom(requiredClass))
		throwItemErrorF(
			"Cannot cast to the requested type: %s", 
			requiredClass->GetName()
		);
	return this;
}

const TreeItem* TreeItem::FollowDots(CharPtrRange dots) const
{
	dms_assert(this);
	dms_assert(dots.size());
	const TreeItem* result = this;
	while (true)
	{
		if (*dots.first++ != '.')
			throwItemError("FollowDots: '/' or '.' expected");
		if (dots.first == dots.second)
			return result;

		result = result->GetTreeParent();
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
		self->throwItemErrorF("CreateItem('%s') failed since it is already created as '%s'",
			requiredClass->GetName(), self->GetDynamicClass()->GetName());
	return self; 
}

TreeItem* CreateAndInitItem(TreeItem* self, TokenID id, const Class* requiredClass)
{
	dms_assert(requiredClass);

	OwningPtr<TreeItem> newSubItem = debug_cast<TreeItem*>(requiredClass->CreateObj());
	dms_assert(newSubItem);

	newSubItem->InitTreeItem(self, id);

	return newSubItem.release();
}

TreeItem* TreeItem::CreateItem(TokenID id, const Class* requiredClass)
{
	dms_assert(!requiredClass || requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));

	if (this)
	{
		if (!id)
			return CheckedAs(this, requiredClass);

		// find foundSubItem according to firstSubItemName
		TreeItem* foundSubItem = GetSubTreeItemByID(id);
		if (foundSubItem)
			return CheckedAs(foundSubItem, requiredClass);
	}

	// create something
	return CreateAndInitItem(this, id, (requiredClass) ? requiredClass : TreeItem::GetStaticClass());
}

TreeItem* TreeItem::CreateItemFromPath(CharPtr subItemNames, const Class* requiredClass)
{
	if (!requiredClass)
		requiredClass = TreeItem::GetStaticClass();

	dms_assert(requiredClass->IsDerivedFrom(TreeItem::GetStaticClass()));
	dms_assert(subItemNames);

	if (*subItemNames == 0) // all subItemNames are processed ??
		if (this)
			return CheckedAs(this, requiredClass);
		else
			return CreateAndInitItem(this, TokenID(), requiredClass);


	// parsing the subItemNames recursively by calling this method on subItemNames parts

	// OPTIMIZE: Reverse order to make use of parent token tables
	CharPtr   restSubItemNames; // new subItemNames after parse
	SharedStr firstSubItemName = splitPathBase(subItemNames, &restSubItemNames); // firstSubItemName of storage after parse
	bool      hasRestSubItems = (*restSubItemNames) != 0;

	if (firstSubItemName.empty() || firstSubItemName[0] == '.')
		// subItemNames started with a '/': traversing an absolute path is not allowed for locating a new object
		// traversing outside the specified namespace is not allowed for locating a new object
		throwItemErrorF("CreateItemFromPath(%s): Cannot create new items outside creation context", subItemNames);

	TokenID   firstSubItemID = GetTokenID_mt(firstSubItemName.c_str());
	dms_assert(!firstSubItemID.empty());
	TreeItem* foundSubItem   = 0;
	if (this)
		foundSubItem = GetSubTreeItemByID(firstSubItemID); // find foundSubItem according to firstSubItemName

	if (!foundSubItem) // create something
	{
		foundSubItem = CreateAndInitItem(this, firstSubItemID, (hasRestSubItems || !requiredClass) ? TreeItem::GetStaticClass() : requiredClass);
		if (!hasRestSubItems)
			return foundSubItem;
	}
	dms_assert(foundSubItem);
	return foundSubItem->CreateItemFromPath(restSubItemNames, requiredClass);
}

TreeItem* TreeItem::CreateConfigRoot(TokenID id) // static
{
	dms_assert(!s_MakeEndoLockCount);
	TreeItem* result = new TreeItem;
	result->InitTreeItem(nullptr, id);
	result->DisableAutoDelete();
	result->SetFreeDataState(true);
	return result;
}
TreeItem* TreeItem::CreateCacheRoot() // static
{
	dms_assert(s_MakeEndoLockCount);
	TreeItem* result = new TreeItem;
	result->InitTreeItem(nullptr, TokenID::GetEmptyID());
	result->SetPassor();
	return result;
}

bool HasOwnCalculatorNow(TreeItem* result)
{
	dms_assert(result);
	return (!result->mc_Expr.empty()) || (result->mc_Calculator && result->mc_Calculator->IsDataBlock());
}

TreeItem* TreeItem::Copy(TreeItem* dest, TokenID id, CopyTreeContext& copyContext) const
{
	const Class* cls = GetDynamicClass();

	dms_assert(dest || !id);
	bool isNew = (!dest) || (id && !dest->GetSubTreeItemByID(id));
	if (isNew && copyContext.DontCreateNew())
		return nullptr; 
	TreeItem* result = dest->CreateItem(id, cls);
	if (isNew && copyContext.MustMakePassor())
		result->SetPassor();

	dms_assert(result);
//	result->m_State.Clear(ASF_DataReadableDefined);

	bool mustCopyProps = true;
	bool dstIsRoot = (copyContext.m_DstRoot == nullptr);

	dms_assert(copyContext.m_SrcRoot);
	bool isArg = (copyContext.m_ArgList)
		&& (GetTreeParent() == copyContext.m_SrcRoot);

	if (dstIsRoot)
	{
		dms_assert(!isArg);
		dms_assert(dest == copyContext.m_DstContext || copyContext.m_DstContext == nullptr);
		copyContext.m_DstRoot = result;
		mustCopyProps = copyContext.MustCopyRoot();
	}

	if (copyContext.MustCopyExpr() && !isArg)
	{
		result->AssertPropChangeRights(USING_NAME);
		result->ClearNamespaceUsage();

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
				if (sns && sns != GetTreeParent() && !sns->DoesContain(this) )
				{
					if (nameSpaceBuffer.CurrPos())
						nameSpaceStream << ';';
					nameSpaceStream << copyContext.GetAbsOrRelNameID(sns, this, dest).GetStr().c_str();
					dms_assert(nameSpaceBuffer.GetData()[nameSpaceBuffer.CurrPos()-1] != ';');
				}
			}
			CharPtr dataBegin = nameSpaceBuffer.GetData();
			if (dataBegin)
			{
				dms_assert(nameSpaceBuffer.CurrPos());
				result->AddUsingUrls(dataBegin, dataBegin+nameSpaceBuffer.CurrPos());
			}
		}
	}
	if (InTemplate())
		result->mc_OrgItem = this;
	//	Now, call the virtual CopyProps func to let the derived class do some work
	if (mustCopyProps)
	{
		if (isArg || (copyContext.SetInheritFlag() && !HasOwnCalculatorNow(result)))
			result->SetTSF(TSF_InheritedRef);

		CopyProps(result, copyContext);

		//	Now, copy data if requested
		if (isArg)
		{
			dms_assert(!dest->InTemplate());
			dms_assert(copyContext.m_ArgList.IsRealList());

			result->SetCalculator( AbstrCalculator::ConstructFromLispRef(result, copyContext.m_ArgList.Left(), CalcRole::ArgCalc) );
			result->SetIsHidden(true);
			copyContext.m_ArgList = copyContext.m_ArgList.Right();
			return result; // don't copy subItems from this to result (take them from arg)
		}

		if (copyContext.MustUpdateMetaInfo())
			UpdateMetaInfo();
		if (isNew && copyContext.MergeProps())
			result->DisableStorage();

		if (!copyContext.InFenceOperator())
		{
			CopyPropsContext(result, this, copyContext.MinCpyMode(dstIsRoot), !copyContext.MergeProps()).Apply();
			if (!result->m_Location)
				result->m_Location = m_Location;

			if (!copyContext.MustCopyExpr())
			{
				// subItems van referees dmv case-parameter value of gewoon expr-ref. aangeroepen vanuit UpdateMetaInfo 
				// Case-Parameter := itemRef OF result of compound-expr  (met DC_Ptr)
				if (!result->mc_Calculator)
					result->SetCalculator(CreateCalculatorForTreeItem(result, this, copyContext));
			}
			else
			{
				if (mc_Calculator && mc_Calculator->IsDataBlock())
					result->SetCalculator(AbstrCalculator::ConstructFromDBT(AsDataItem(result), mc_Calculator));
			}
		}
	} // end if (!mustCopyProps)

	// Now, copy all sub-items
	if (!copyContext.DontCopySubItems())
		for (const TreeItem* subItem = GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
			subItem->Copy(result, subItem->GetID(), copyContext);

	// Now, copy from refItem; maybe more sub-items should be copied
	if (copyContext.CopyReferredItems())
	{
		const TreeItem* refItem = GetReferredItem();
		if (refItem)
			CopyTreeContext(result, refItem, "", DataCopyMode(copyContext.GetDCM()|DataCopyMode::DontCreateNew|DataCopyMode::NoRoot) ).Apply();
	}
	return result;
}

void TreeItem::Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const
{}

void TreeItem::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
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

	if (m_State.GetProgress() < PS_MetaInfo)
		m_State.SetProgress(PS_MetaInfo);
	dbg_assert(IsPassor() || !IsDataItem(this) || (!AsDataItem(this)->GetAbstrDomainUnit()) || AsDataItem(this)->GetAbstrDomainUnit()->CheckMetaInfoReadyOrPassor());
	dbg_assert(IsPassor() || !IsDataItem(this) || (!AsDataItem(this)->GetAbstrValuesUnit()) || AsDataItem(this)->GetAbstrValuesUnit()->CheckMetaInfoReadyOrPassor());
}

const bool MG_DEBUG_UPDATEMETAINFO = true;

void TreeItem::UpdateMetaInfoImpl() const
{
	dms_assert(!WasFailed(FR_MetaInfo));

	// begin of recursion protected area
	dms_check_not_debugonly;
	UpdateLock lock2(this, actor_flag_set::AF_UpdatingMetaInfo);

	UpdateSupplMetaInfo(); // Update Suppliers, calls MakeCalculator() -> mc_DC
	VisitSupplBoolImpl(this, SupplierVisitFlag::NamedSuppliers,
		[this](const Actor* supplier) -> bool
		{
			auto foundItem = dynamic_cast<const TreeItem*>(supplier);
			assert(foundItem);
			if (foundItem->GetTSF(TSF_Depreciated))
			{
				SharedTreeItem prevItem = foundItem, refItem = prevItem->GetCurrRefItem();
				MG_CHECK(refItem); // follows from TSF_Depreciated
				SharedTreeItem refRefItem = refItem->GetCurrRefItem();
				while (refRefItem) {
					prevItem = refItem;
					refItem = refRefItem;
					refRefItem = refItem->GetCurrRefItem();
				} 
				MG_CHECK(prevItem->GetID() != refItem->GetID());
				
				auto msg = mySSPrintF("'%s' refers by '%s' to '%s'\nReplace '%s' by '%s'."
				,	this->GetFullName()
				,	foundItem->GetFullName()
				,	prevItem->GetID()
				,	prevItem->GetID()
				,	refItem->GetID()
				);
				reportD(SeverityTypeID::ST_Warning, msg.AsRange());
			}
			return true;
		}
	);

	if (IsDataItem(this))
	{
		const AbstrDataItem* thisAdi = AsDataItem(this);

		// what is it?
		dbg_assert(thisAdi->GetAbstrDomainUnit()->CheckMetaInfoReadyOrPassor());
		dbg_assert(thisAdi->GetAbstrValuesUnit()->CheckMetaInfoReadyOrPassor());
		thisAdi->GetAbstrDomainUnit()->UpdateMetaInfo();
		thisAdi->GetAbstrValuesUnit()->UpdateMetaInfo();
	}

	if (mc_Calculator && !mc_Calculator->IsDataBlock())
		ApplyCalculator(const_cast<TreeItem*>(this), mc_Calculator);

//	UpdateDC();

	if (mc_DC && mc_DC->WasFailed())
		Fail(mc_DC);

	if (HasConfigData() && mc_Calculator && mc_Calculator->IsDataBlock())
		return;

	if (mc_RefItem)
	{
		mc_RefItem->UpdateMetaInfo();
		if (mc_RefItem->IsCacheRoot())
		{
			if (mc_DC && mc_DC->IsNew())
			{
				if (!mc_RefItem->GetTSF(TSF_HasPseudonym)) // can have another pseudonym
				{
					mc_RefItem->SetTSF(TSF_HasPseudonym);
#if defined(MG_DEBUG_DATA)
					mc_RefItem->md_FullName = md_FullName;
#endif
				}
				if (!GetFreeDataState() && !mc_DC->IsTransient())
					const_cast<TreeItem*>(mc_RefItem.get_ptr())->SetFreeDataState(false);
			}
			if (!this->IsCacheItem())
			{
				SharedTreeItem cacheItem = mc_RefItem;
				if (HasVisibleSubItems(cacheItem))
					CopyTreeContext(const_cast<TreeItem*>(this), cacheItem, "", DataCopyMode::NoRoot | DataCopyMode::MakeEndogenous | DataCopyMode::SetInheritFlag | DataCopyMode::MergeProps).Apply();
			}
		}
	}

//		if (GetStoreDataState() && refItem->IsCacheItem())
//			const_cast<TreeItem*>(refItem)->SetStoreDataState(true);
	if (IsCacheItem() || !IsDataReadable())
		return;

	dms_assert(!HasConfigData()); // implied by IsDataReadable
}	// end of recursion protected area

// ======================================================

namespace diagnostic_tests {
	bool TreeParenMetaInfoReadyOrFailed(const TreeItem* self)
	{
		return !self->GetTreeParent() || self->GetTreeParent()->Was(PS_MetaInfo) || self->GetTreeParent()->WasFailed(FR_MetaInfo);
	}

	bool DetermineStateWasCalled(const TreeItem* self)
	{
		return TreeParenMetaInfoReadyOrFailed(self)
			&& (self->m_LastGetStateTS == UpdateMarker::GetLastTS() || self->HasConfigData() || self->InTemplate() || self->IsPassor());
	}
}

MetaInfo TreeItem::GetCurrMetaInfo(metainfo_policy_flags mpf) const
{
	// suppliers have been scanned, thus mc_Calculator and m_SupplCache have been determined.
	dms_assert(diagnostic_tests::DetermineStateWasCalled(this));
	assert(IsMainThread());

	if (m_State.Get(ASF_GetCalcMetaInfo))
		throwItemError(
			"Invalid recursion in TreeItem::GetCurrMetaInfo() detected.\n"
			"Check calculation rule of this item"
		);
	auto_flag_recursion_lock<ASF_GetCalcMetaInfo> lock(m_State);

	if (HasCalculatorImpl())
	{
		//		if (IsCacheItem() && (!HasSupplCache() || GetSupplCache()->GetNrConfigured(this) == 0) )
		const AbstrCalculator* calc = mc_Calculator;
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
		dms_assert(!sourceItem->IsCacheItem());
		return sourceItem->GetCheckedKeyExpr();
	}
	//	if (metaInfo.index() == 0 && IsUnit(this) && std::get<MetaFuncCurry>(metaInfo).fullLispExpr.EndP())
	//		return ExprList(AsUnit(this)->GetValueType()->GetID());
//	dms_assert(metaInfo.index() != 0);
	if (metaInfo.index() == 0)
		return {};
	return std::get<1>(metaInfo);
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
		if (srcItem->WasFailed(FR_MetaInfo))
			return {};
		return { std::get<SharedTreeItem>(metaInfo)->GetCheckedDC(), srcItem };
	}

	return { GetOrCreateDataController(GetKeyExprImpl()), {} };
}

auto TreeItem_CreateConvertedExpr(const TreeItem* self, const TreeItem* cacheItem, LispPtr expr) -> LispRef
{
	if (!self->CheckResultItem(cacheItem))
	{
		assert(self->WasFailed(FR_MetaInfo));
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

auto TreeItem_CreateCheckedExpr(LispPtr resultExpr, const TreeItem* self) -> LispRef
{
	dms_assert(self->HasIntegrityChecker());

	auto icCalc = self->GetIntegrityChecker();
	if (!icCalc)
	{
		self->Fail("Failed to construct IntegryCheck", FR_Validate);
		return resultExpr;
	}
	auto ic = GetAsLispRef(icCalc->GetMetaInfo());
	return ExprList(token::integrity_check, resultExpr, ic);
}

void TreeItem::UpdateDC() const
{
	if (mc_DC || mc_RefItem || WasFailed(FR_MetaInfo) || InTemplate() || IsCacheItem() || IsPassor())
		return;

	auto [resultDC, srcItem] = GetOrgDC();
	// required for Convert test and subItem moniking, empty for applicators non-calculatable or loadable items (such as some parents).
	if (resultDC && IsDataItem(this) && !resultDC->WasFailed(FR_MetaInfo))
	{
		if (SharedTreeItem cacheItem = resultDC->MakeResult())
		{
			auto keyExpr = TreeItem_CreateConvertedExpr(this, cacheItem, resultDC->GetLispRef());
			if (!keyExpr)
			{
				assert(WasFailed(FR_MetaInfo));
				return;
			}
			resultDC = GetOrCreateDataController(keyExpr);
		}
	}
	if (resultDC && HasIntegrityChecker())
	{
		LispRef resultExpr;
		if (resultDC)
			resultExpr = resultDC->GetLispRef();
		else
			resultExpr = CreateLispTree(this, false);
		resultDC = GetOrCreateDataController(TreeItem_CreateCheckedExpr(resultExpr, this));
	}
	SetDC(resultDC, srcItem);
}

auto TreeItem_GetCheckedDC_impl(const TreeItem* self) ->DataControllerRef
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
	if (mc_RefItem)
	{
		dms_assert(!mc_RefItem->IsCacheItem());
		return mc_RefItem->GetCheckedDC();
	}
	if (IsCurrLoadable() && !GetTSF(USF_HasConfigRange))
		return GetOrCreateDataController(CreateLispTree(this, false));
	return {};
}

LispRef TreeItem::GetCheckedKeyExpr() const
{
	auto dc = TreeItem_GetCheckedDC_impl(this);
	if (dc)
		return dc->GetLispRef();
	auto result = GetKeyExprImpl();

	if (!result.EndP())
		return result;

	dms_assert(!IsCacheItem());
	if (IsDataItem(this) && AsDataItem(this)->HasDataObj() && !IsLoadable())
	{
		auto adi = AsDataItem(this);
		auto valueList = AsDataItem(this)->GetDataObj()->GetValuesAsKeyArgs(adi->GetAbstrValuesUnit()->GetCheckedKeyExpr());
		if (adi->HasVoidDomainGuarantee())
		{
			dms_assert(valueList.IsRealList());
			dms_assert(valueList.Right().EndP());
			return valueList.Left();
		}
		return LispRef(
			LispRef(token::union_data)
			, LispRef(adi->GetAbstrDomainUnit()->GetCheckedKeyExpr()
				, valueList
			)
		);
	}
	// required for Convert test and subItem moniking, empty for applicators non-calculatable or loadable items (such as some parents).
	result = CreateLispTree(this, false);
	if (HasIntegrityChecker())
		result = TreeItem_CreateCheckedExpr(result, this);
	return result;
}

#if defined(MG_DEBUG)
bool TreeItem::CheckMetaInfoReady() const
{
	return m_State.GetProgress()>=PS_MetaInfo || WasFailed(FR_MetaInfo);
}

bool TreeItem::CheckMetaInfoReadyOrPassor() const
{
	return IsPassor() || CheckMetaInfoReady();
}

#endif
const TreeItem* TreeItem::GetBackRef() const 
{
//	dms_assert(IsMainThread());
// TODO: SYNC on backref
	return m_BackRef;
}

void TreeItem::UpdateMetaInfoImpl2() const
{
	dbg_assert(IsMetaThread());

	if (m_LastGetStateTS >= UpdateMarker::LastTS())
		if ((m_State.GetProgress()>=PS_MetaInfo) || WasFailed(FR_MetaInfo)) // reset by DetermineState when supplier was invalidated
			return;

	if(m_State.IsDeterminingState() || m_State.IsUpdatingMetaInfo() || m_State.Get(ASF_MakeCalculatorLock) )
	{
		throwItemError(
			"Invalid recursion in UpdateMetaInfo detected.\n"
			"Check calculation rule and other referring properties of this item and/or its SubItems"
		);
	}
	dms_assert(IsPassor() || !SuspendTrigger::DidSuspend());

	FencedInterestRetainContext retainLocalInterestUntilThisDies;

	// DetermineState() -> DoInvalidate() could reset TSF_MetaInfoReady
	if (IsPassor())
	{
		if (!WasFailed())  // Passors can fail due to PrepareDataUsage that inherits failure from DataControiller
			SetMetaInfoReady();
		return;
	}

	if (GetTreeParent())
		GetTreeParent()->UpdateMetaInfo();

	try {
		DetermineState();

		if ((m_State.GetProgress()>=PS_MetaInfo) || WasFailed(FR_MetaInfo)) // reset by DetermineState when supplier was invalidated
			return;

		MG_SIGNAL_ON_UPDATEMETAINFO

		DBG_START("TreeItem", "UpdateMetaInfo", MG_DEBUG_UPDATEMETAINFO && false);
		DBG_TRACE(("fullname = %s", GetFullName().c_str()));

		TreeItemContextHandle tdc(this, "UpdateMetaInfo");

		dms_assert(m_LastChangeTS || IsPassor()); // PRECONDITION for SetProgress, guaranteed by IsDeterminingState() || IsPassor() || DetermineState()

		StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;

		dms_assert(!WasFailed(FR_MetaInfo));
		UpdateMetaInfoImpl(); // recursion protected part of UpdateMetaInfo

		SetMetaInfoReady();
		if (WasFailed(FR_MetaInfo))
			return;

		// Update Meta Info according to storage manager
		const TreeItem* storageParent = GetStorageParent(false);
		if (storageParent)
			storageParent->GetStorageManager()->UpdateTree(storageParent, const_cast<TreeItem*>(this));

		// validate units with refObject if it wasn't copied by the parent

//		if	(mc_RefItem && !GetTSF(TSF_InheritedRef) && !mc_Expr.empty())
//			Unify(mc_RefItem);
//		NotifyStateChange(this, NC2_MetaReady);
	}
	catch (...)
	{
		// don't try again
		dms_assert(m_State.GetProgress() <= PS_MetaInfo);
		m_State.SetProgress(PS_MetaInfo);
		CatchFail(FR_MetaInfo);
	}
	dms_assert(m_State.GetProgress() >= PS_MetaInfo);
}

void TreeItem::UpdateMetaInfo() const
{
	auto contextForReportingPurposes = TreeItemContextHandle(this, "UpdateMetaInfo");

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

ActorVisitState TreeItem::SuspendibleUpdate(ProgressState ps) const
{
//	dms_assert((m_State.GetProgress()>=PS_Committed) || (ps < PS_DataReady) || GetInterestCount() || !IsDataItem(this));
	dms_assert(ps == PS_Committed); // TODO: clean-up if this holds

	UpdateMetaInfo();
	auto remainingStackSpace = RemainingStackSpace();
	if (remainingStackSpace <= 327680)
	{
		// just use async to start a new thread.
		auto future = std::async([this, ps]()->ActorVisitState
			{
				SetMetaThreadID();
				assert(IsMetaThread());
				return this->base_type::SuspendibleUpdate(ps);
			}
		);
		ActorVisitState result = future.get();
		SetMetaThreadID();
		assert(IsMetaThread());
		return result;
	}
	return base_type::SuspendibleUpdate(ps);
}

ActorVisitState TreeItem::DoUpdate(ProgressState ps)
{
	DBG_START("TreeItem", "DoUpdate", MG_DEBUG_UPDATEMETAINFO && false);
	DBG_TRACE(("fullname = %s", GetFullName().c_str()));
	dms_assert(ps > PS_MetaInfo);

	dms_assert(m_State.GetProgress() >= PS_MetaInfo); //UpdateMetaInfo();

	dms_assert(m_State.GetProgress()>=PS_MetaInfo);

	TreeItemContextHandle tich(this, "Update");

	if (!IsDataItem(this) && !IsUnit(this))
		SetIsInstantiated();

	if ( InTemplate() )
		goto exitReady;

	if (ps < PS_Validated)
		goto exitReady;

	if (m_State.GetProgress() < PS_Validated) 
	{
		if (HasIntegrityChecker())
		{
			try
			{
				TreeItemContextHandle tich2(this, "IntegrityCheck Evaluation");

				auto iCheckerPtr = GetIntegrityChecker();
				dms_assert(iCheckerPtr);

				//InterestPtr<SharedPtr<const AbstrCalculator>> iChecker = iCheckerPtr;
				if (WasFailed(FR_Validate))
					return AVS_SuspendedOrFailed;

				auto result = CalcResult(iCheckerPtr, DataArray<Bool>::GetStaticClass()); // @@@SCHEDULE

				if (SuspendTrigger::DidSuspend())
					return AVS_SuspendedOrFailed;

				dms_assert(result && result->GetInterestCount());

				DataReadLockContainer c;                                                  // @@@USE
				if (!result->GetOld() || !c.Add(AsDataItem(result->GetOld()), DrlType::Suspendible))
				{
					if (SuspendTrigger::DidSuspend())
						return AVS_SuspendedOrFailed;
					dms_assert(result->WasFailed(FR_Data));
					if (result->WasFailed(FR_Data))
						Fail(result.get_ptr());
					return AVS_SuspendedOrFailed;
				}
				SizeT nrFailures = AsDataItem(result->GetOld())->CountValues<Bool>(false);
				if (nrFailures)
				{
					//	throwError(ICHECK_NAME, "%s", iChecker->GetExpr().c_str()); // will be caught by SuspendibleUpdate who will Fail this.
					Fail(ICHECK_NAME ": " + AsString(nrFailures) + " element(s) fail the test of " + iCheckerPtr->GetExpr(), FR_Validate); // will be caught by SuspendibleUpdate who will Fail this.
					dms_assert(WasFailed(FR_Validate));
					return AVS_Ready;
				}
			}
			catch (...)
			{
				auto err = catchException(false);
				DoFail(err, FR_Validate);
				return AVS_Ready;
			}
		}
		SetProgress(PS_Validated);
	}

	if (ps >= PS_Committed && m_State.GetProgress() < PS_Committed)
	{
		//	TODO, Uitzoeken wanneer Commit overbodig is (zoals indien in vorige sessie reeds gedaan).
		//	Voorlopig antwoord: wanneer er geen changes zijn, is er geen cause voor invalidatatie en mag DoUpdate helemaal niet aangeroepen worden.
		//	probleem: Als export file is weggegooid, moet de DoUpdate dan opnieuw worden uitgevoerd? 
		//	probleem: verschillende ItemCommits kunnen dezelfde export timestampen.
		bool result = CommitDataChanges(); // @@@SCHEDULE AND USE

		if (SuspendTrigger::DidSuspend())
			return AVS_SuspendedOrFailed;
		dms_assert(result || WasFailed(FR_Committed));

		SetProgress(PS_Committed);

		if (!result) 
		{
			dms_assert(WasFailed(FR_Committed));
			return AVS_SuspendedOrFailed;
		}
	}

exitReady:
	if (!MustApplyImpl())
		StopSupplInterest(); // Commit has only interest on this; validate has its own interest path

	return AVS_Ready;
}

//#include "dbg/SeverityType.h" // DEBUG, REMOVE

void TreeItem::SetProgress(ProgressState ps) const
{
	ProgressState oldProgress = m_State.GetProgress();
	Actor::SetProgress(ps); // changes m_State to US_Valid

	if (ps > PS_MetaInfo && oldProgress < ps)
	{
		dms_assert(ps >= PS_Validated);
		NotifyStateChange(this, ps == PS_Validated ? NC2_Validated : NC2_Committed);
	}
}

const TreeItem* TreeItem::GetFirstVisibleSubItem() const
{
	dms_assert(this);

	const TreeItem* subItem = GetFirstSubItem(); // calls UpdateMetaInfo
	if (subItem || !mc_RefItem)
		return subItem;
	return mc_RefItem->GetFirstVisibleSubItem();
}


const TreeItem* TreeItem::GetNextVisibleItem() const
{
	dms_assert(this);
	dms_assert(m_Parent);

	const TreeItem* nextItem = GetNextItem();
	if (nextItem)
		return nextItem;
	nextItem = m_Parent->mc_RefItem;
	dms_assert(nextItem != m_Parent);
	if (!nextItem)
		return nullptr;
	return nextItem->GetFirstVisibleSubItem();
}

const TreeItem* TreeItem::WalkConstSubTree(const TreeItem* curr) const // this acts as subTreeRoot
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
		curr = curr->GetTreeParent();
	}
	return nullptr;
}

struct StackFrame
{
	const TreeItem*   m_Base;
	const StackFrame* m_Caller;
};

bool TreeItem::VisitConstVisibleSubTree(const ActorVisitor& visitor, const StackFrame* prev) const
{
//	Invite(visitor);

	StackFrame frame = { this, prev };
	for  (; prev; prev = prev->m_Caller)
		if (prev->m_Base == this)
				return true;

	// go to subItems of refItem, if any
	const TreeItem* curr = this;
	do {
		const TreeItem* refItem = GetReferredItem();
		if (refItem)
			if (!refItem->VisitConstVisibleSubTree(visitor, &frame))
				return false;
		if (curr != this)
			if (!visitor(curr))
				return false;
	} while (curr = WalkConstSubTree(curr));
	return true;
}

inline TreeItem* TreeItem::WalkNext(TreeItem* curr) // this acts as subTreeRoot
{
	while (curr && curr != this)
	{
		TreeItem* next = curr->GetNextItem();
		if (next)
			return next;
		curr = const_cast<TreeItem*>(curr->GetTreeParent());
	}
	return nullptr;
}

// don't call UpdateMetaInfo for when you are in destructor / stop / nothrow land.
TreeItem* TreeItem::WalkCurrSubTree(TreeItem* curr) // this acts as subTreeRoot
{
	dms_assert(curr);
	if (curr->_HasSubItems())
		return curr->_GetFirstSubItem();
	return WalkNext(curr);
}

ActorVisitState TreeItem::VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const
{
	dms_assert(!SuspendTrigger::DidSuspend()); // precondition

	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < PS_MetaInfo && !GetTreeParent()->WasFailed(FR_MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	dms_assert(!GetTreeParent() || GetTreeParent()->m_State.GetProgress() >= PS_MetaInfo || GetTreeParent()->WasFailed(FR_MetaInfo)); // precondition

	// =============== Parent
	if (Test(svf, SupplierVisitFlag::Parent) && GetTreeParent())
	{
		if (visitor(GetTreeParent()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}

	if (InTemplate())
		return AVS_Ready;

	// =============== TemplateOrg

	if (Test(svf,  SupplierVisitFlag::TemplateOrg) && mc_OrgItem)
	{
		if (visitor(mc_OrgItem) == AVS_SuspendedOrFailed)
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
			if (dialogDataItem && visitor(dialogDataItem) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend()); // precondition

	// =============== look for explicit suppliers

	if (Test(svf, SupplierVisitFlag::ExplicitSuppliers) && HasSupplCache())
	{
		UInt32 n = GetSupplCache()->GetNrConfigured(this); // only ConfigSuppliers, Implied suppliers come after this, Calculator & StorageManager have added them
		for  (UInt32 i = 0; i < n; ++i)
		{
			const Actor* supplier = GetSupplCache()->begin(this)[i];
			dms_assert(!SuspendTrigger::DidSuspend()); // precondition
			 if (visitor(supplier) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;

			dms_assert(!SuspendTrigger::DidSuspend()); // precondition
			auto supplTI = debug_cast<const TreeItem*>(supplier); // all configured suppliers are TreeItems; all implied suppliers are AbstrCalculators
			if (supplTI->VisitConstVisibleSubTree(visitor) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
		}
	}

	dms_assert(!SuspendTrigger::DidSuspend()); // precondition
	// Ask ParseResult for suppliers

//	dms_assert(!Test(svf, SupplierVisitFlag::Calculator));
	/* REMOVE
		if (Test(svf, SupplierVisitFlag::Calculator)) // already done by StartInterest
			if (visitor(GetCalculator()) == AVS_SuspendedOrFailed)
				return AVS_SuspendedOrFailed;
	*/

	// =============== m_Calculator related
	if (Test(svf, SupplierVisitFlag::DetermineCalc))
		MakeCalculator(); // sets mc_Calculator, mc_DC, and mc_RefItem;

	if (mc_Calculator && Test(svf, SupplierVisitFlag::NamedSuppliers))
		if (mc_Calculator->VisitSuppliers(svf, visitor) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;

	if (Test(svf, SupplierVisitFlag::DetermineCalc))
		UpdateDC();

	if (mc_DC)
	{
		if (Test(svf, SupplierVisitFlag::DataController))
			if (visitor.Visit(mc_DC) != AVS_Ready)
				return AVS_SuspendedOrFailed;
		if (Test(svf, SupplierVisitFlag::SourceData))
		{
			const TreeItem* sourceItem = GetSourceItem();
			dms_assert(!sourceItem || sourceItem != this);
			if (visitor.Visit(sourceItem) != AVS_Ready)
				return AVS_SuspendedOrFailed;
		}
	}
	if (mc_RefItem)
	{
		if (Test(svf, SupplierVisitFlag::SourceData))
			if (visitor.Visit(mc_RefItem) != AVS_Ready)
				return AVS_SuspendedOrFailed;
	}

	if (Test(svf, SupplierVisitFlag::Checker) && HasIntegrityChecker())
	{
		auto ic = GetIntegrityChecker();
		auto dc = MakeResult(ic);
		if (visitor.Visit(dc) != AVS_Ready)
			return AVS_SuspendedOrFailed;
	}
	// =============== StorageManager of parent

	const TreeItem* storageParent = GetStorageParent(false);

	// implied DC for indirect storageNames and sqlString prop are set by VisitNextSupplier
	if (storageParent)
	{
		auto sm = storageParent->GetStorageManager(false);
		if (sm && sm->VisitSuppliers(svf, visitor, storageParent, this) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}

//	dms_assert(m_StorageManager || !HasStorageManager()); // Has -> GetStorageParent(false) returns this -> GetStorageManager was called, which could collect Implied Suppliers

	// =============== IntegrityChecker

	if (Test(svf, SupplierVisitFlag::Checker) && HasIntegrityChecker())
	{
		auto icResult = MakeResult(GetIntegrityChecker());
		if (visitor(icResult) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
		if (visitor(icResult->GetOld()) == AVS_SuspendedOrFailed)
			return AVS_SuspendedOrFailed;
	}
	return base_type::VisitSuppliers(svf, visitor);
}

void TreeItem::DoInvalidate() const
{
	dms_assert(!IsCacheItem());

	// m_State Has already been set to US_Invalidated before DoInvalidate gets called 
	if (m_SupplCache)
		m_SupplCache->Reset();

	SetReferredItem(nullptr);

//	CalcDestroyer destroyer(this, !GetTSF(TSF_InheritedRef)  && (!HasConfigData() || !IsDataItem(this)) && (!mc_Calculator || !mc_Calculator->IsDataBlock()));

	if (IsCacheItem())
		const_cast<TreeItem*>(this)->DropValue();

	m_State.Clear(ASF_WasLoaded);
	m_StatusFlags.Clear(TSF_DataInMem);

	mc_IntegrityChecker.reset();

	if (mc_DC)
	{
		OldRefDecrementer oldDcInterestCounter;
		DataControllerRef oldDC;
		{
			leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection); // check and swap or try again
			if (m_InterestCount) // still interested?
			{
				// point of certain return, prepare settlement upon destruction
				oldDcInterestCounter = mc_DC.get(); // decrement interest count upon destruction
			}
			oldDC = std::move(mc_DC);
			dms_assert(!mc_DC);
		}
	}
	if (!mc_Expr.empty())
		mc_Calculator.reset();

	Actor::DoInvalidate(); // StartSupplInterest, which might recollect mc_Calculator, mc_IntegrityChecker, mc_RefItem, ClearFail
	// =============== invalidate Parts (of cache items)
	NotifyStateChange(this, NC2_Invalidated);

	dms_assert(DoesHaveSupplInterest() || !GetInterestCount() || IsPassor());
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
	SetProgressAt(PS_MetaInfo, UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE(mySSPrintF("SetDataChanged(%s)", GetFullName().c_str()).c_str()) ) );  // new data not validated nor committed
}

garbage_t TreeItem::DropValue()
{
	MG_LOCKER_NO_UPDATEMETAINFO

	garbage_t garbageCan;
	ClearData(garbageCan); // Resets m_SegsPtr (DoClearData) and resets TSF_DataInMem
	return garbageCan;
}

TimeStamp TreeItem::DetermineLastSupplierChange(ErrMsgPtr& failReason, FailType& ft) const // noexcept
{
	assert(IsMetaThread());
	if (GetTreeParent() && GetTreeParent()->m_State.GetProgress() < PS_MetaInfo && !GetTreeParent()->WasFailed(FR_MetaInfo))
		GetTreeParent()->UpdateMetaInfo();
	// postcondition of UpdateMetaInfo
	assert(!GetTreeParent() || GetTreeParent()->m_State.GetProgress() >= PS_MetaInfo || GetTreeParent()->WasFailed(FR_MetaInfo)); 

	TimeStamp lastChangeTS = 0; // DataStoreManager::GetCachedConfigSourceTS(this);
	if (!lastChangeTS
#if defined(MG_ITEMLEVEL)
		|| m_ItemLevel == item_level_type(0)
#endif
		)
		lastChangeTS = Actor::DetermineLastSupplierChange(failReason, ft);

	// Track changes in authentic sources
//REMOVE	// make sure not to pass changes because item was already last
	if ((ft == FR_None) && IsDataReadable() && !WasFailed(FR_Determine))
	{
		try {
			dms_assert(!IsCacheItem());
			const TreeItem* storageParent = GetStorageParent(false); if (!storageParent) goto exit;
			dms_assert(storageParent);

			AbstrStorageManager* sm = storageParent->GetStorageManager(); if (!sm) goto exit;
			dms_assert(sm);

			FileDateTime lastFileChange = sm->GetCachedChangeDateTime( storageParent, GetRelativeName(storageParent).c_str() );

			if (lastFileChange)
			{
//				MakeMax(lastChangeTS, DSM::Curr()->DetermineExternalChange(lastFileChange) );
				assert( UpdateMarker::CheckTS(lastChangeTS) );
			}
		}
		catch (...)
		{
			failReason = catchException(false);
			ft         = FR_Determine;
		}
	}
exit:
	return lastChangeTS;
}

SharedStr TreeItem::GetSourceName() const
{
	SharedStr inhSN = base_type::GetSourceName();

	if (!GetConfigFileLineNr())
		return inhSN;

	return mySSPrintF("%s(%u,%u): %s"
	,	ConvertDmsFileNameAlways(GetConfigFileName())
	,	GetConfigFileLineNr()
	,	GetConfigFileColNr()
	,	inhSN
	);
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
			si->DoFail(msg, ft);
			si = si->GetNextItem();
		}
	}
	NotifyStateChange(this, NotificationCodeFromProblem(ft));
	return true;
}

//----------------------------------------------------------------------
// Read / Write Data
//----------------------------------------------------------------------

bool TreeItem::ReadItem(const StorageReadHandle& srh) // TODO: Make this a method of StorageReadHandle
{
	MG_DEBUGCODE( dms_assert( CheckMetaInfoReady() ); )

	dms_assert(! GetCurrRefItem() ); // caller must take care of only calling ReadItem for UltimateItems

	MG_SIGNAL_ON_UPDATEMETAINFO

	auto keepInterest = GetInterestPtrOrCancel();

	if (WasFailed(FR_Data))
		return false;
	if (IsDataReady(this))
		return true;

	const TreeItem* storageParent = GetCurrStorageParent(false);
	if (!storageParent)
		return false;
	
	TreeItemContextHandle checkPtr(this, "TreeItem::ReadItem");

	if (SuspendTrigger::MustSuspend())
		return false;

	try
	{
		reportF(SeverityTypeID::ST_MajorTrace, "Read %s(%s)"
		,		storageParent->GetStorageManager()->GetNameStr().c_str()
		,		GetFullName().c_str()
		);	

		if (srh.Read())
			return true;
		else if (!SuspendTrigger::DidSuspend())
			throwItemError("DoReadItem returned Failure");
		dms_assert(GetInterestCount());
	} 
	catch (...)
	{
 		dms_assert(!HasCurrConfigData());

		if (!WasFailed(FR_Data)) {
			auto err = catchException(true);
			err->TellExtraF("while reading data from %s"
			,	DMS_TreeItem_GetAssociatedFilename(this)
			);
			DoFail(err, FR_Data);
		}
		DropValue();
	}
	return false;
}

bool TreeItem::DoReadItem(StorageMetaInfo* smi)
{
	return false;
}

bool TreeItem::DoWriteItem(StorageMetaInfoPtr&&) const
{
	// can return false because of suspension or failure
	// caller must check state and suspend trigger to find out
	if (HasCalculator())
	{
		const AbstrCalculator* apr = GetCalculator();
		if (!apr)
		{
			dms_assert(IsUnit(this));
			return true;
		}
		auto result = CalcResult(apr,GetDynamicObjClass());
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
	dms_assert(m_State.GetProgress() >= PS_MetaInfo || IsPassor() || WasFailed(FR_Data));
	if (UpdateMarker::PrepareDataInvalidatorLock::IsLocked())
		drlFlags = DrlType(UInt32(drlFlags) & ~UInt32(DrlType::UpdateMask));

	dms_assert(!IsTemplate()); // formation of FuncDC's should prevent args to be calculated that fail to meet this precondition
	dms_assert(IsMetaThread() || !(UInt32(drlFlags) & UInt32(DrlType::UpdateMask)));

	if ((UInt32(drlFlags) & UInt32(DrlType::Certain)) && !SuspendTrigger::BlockerBase::IsBlocked())
	{
		SuspendTrigger::FencedBlocker lockSuspend;
		auto result = PrepareDataUsageImpl(drlFlags);
		dms_assert(result || WasFailed());
		return result;
	}

	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult
	auto result = PrepareDataUsageImpl(drlFlags);
	dms_assert(result || SuspendTrigger::DidSuspend() || WasFailed());
	return result;
}

enum class how_to_proceed { nothing, data_ready, failed, suspended, suspended_or_failed}; // return_suspended_or_failed, return_OK };

how_to_proceed PrepareDataCalc(SharedPtr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	dms_assert(!SuspendTrigger::DidSuspend() && !self->WasFailed(FR_Determine)); // Postcondition when CreateResultingTreeItem returns a result

//				FutureData dc = GetDC(GetCalculator());
//	self->UpdateDC();
	FutureData dc = self->GetCheckedDC();
//	dms_assert(dc || self->GetCurrRefItem());
//	if (!dc) dc = GetDC(self->GetCalculator()); // TODO G8: unwind recursion
//	dms_assert(dc);
	//				const AbstrCalculator* apr = GetCalculator();
	//				dms_assert(apr); // guaranteed by HasCalculator
	//				dms_assert(!SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns a result
	dms_check(self->GetInterestCount());

	//				auto result = CalcResult(apr, GetDynamicObjClass());
	if (dc)
	{
		auto dc2 = dc->CalcResult();
		dms_assert(dc2 || SuspendTrigger::DidSuspend() || dc->WasFailed(FR_Data));
		if (dc->WasFailed()) //  && !WasFailed())
		{
			self->Fail(dc.get_ptr());
		}
		if (self->WasFailed(FR_Data))
			return how_to_proceed::failed;
		if (!dc->GetOld())
		{
			dms_assert(SuspendTrigger::DidSuspend()); // Postcondition when CreateResultingTreeItem returns no result, yet hasn't failed
			return how_to_proceed::suspended;
		}
		if (SuspendTrigger::DidSuspend())
			return how_to_proceed::suspended;
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

	if (dc && dc->GetOld() != self && !dc->GetOld()->IsCacheItem()) // could be config item that can be read from external source
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
		dms_assert(!self->WasFailed(FR_Data));
		return how_to_proceed::data_ready;
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // PrepareDataUsage loads from cache if possible
	return how_to_proceed::nothing;
}

how_to_proceed PrepareDataRead(SharedPtr<const TreeItem> self, const TreeItem* refItem, DrlType drlFlags)
{
	MG_DEBUGCODE(dms_assert(!refItem->HasCalculatorImpl())); // implied by IsDataReadable
	dms_assert(!refItem->IsCacheItem());        // how else to derive data

	auto supplResult = VisitSupplBoolImpl(self, SupplierVisitFlag::Calc, [self, drlFlags](auto a) -> bool
		{
			auto t = dynamic_cast<const TreeItem*>(a);
			if (t && !t->PrepareDataUsage(drlFlags))
			{
				if (t->WasFailed(FR_Data))
					self->Fail(t);
				return false;
			}
			dms_assert(!SuspendTrigger::DidSuspend());
			return true;
		}
	);
	if (supplResult == AVS_SuspendedOrFailed)
	{
		if (SuspendTrigger::DidSuspend())	
			return how_to_proceed::suspended;
		else
		{
			assert(self->WasFailed(FR_Data));
			return how_to_proceed::failed;
		}
	}
	//				if (UpdateSuppliers(PS_Committed) == AVS_SuspendedOrFailed)
	//					goto suspended_or_failed;

	assert(!SuspendTrigger::DidSuspend());

	if (IsDataItem(refItem))
	{
		const AbstrUnit* adu = AsDataItem(refItem)->GetAbstrDomainUnit();
		if (!adu->PrepareDataUsageImpl(drlFlags)) // make sure that the cardinality can be calculated
		{
			if (!adu->WasFailed(FR_Data))
				return how_to_proceed::suspended;
			self->Fail(adu);
			return how_to_proceed::failed;
		}
	}
	dms_assert(!CheckCalculatingOrReady(refItem)); // was tested before and nothing could have started the calculation

	const TreeItem* storageParent = refItem->GetStorageParent(false); dms_assert(storageParent);
	const AbstrStorageManager* sm = storageParent->GetStorageManager(); dms_assert(sm);
	StorageMetaInfoPtr readInfo = sm->GetMetaInfo(storageParent, const_cast<TreeItem*>(refItem), StorageAction::read);
	if (readInfo)
	{
		auto readInfoPtr = std::make_shared<StorageMetaInfoPtr>(std::move(readInfo));
		assert(!readInfo);
		dms_assert(!CheckCalculatingOrReady(refItem));
		auto rtc = std::make_shared<OperationContext>();
		self->m_ReadAssets.emplace<decltype(rtc)>(rtc);

		FutureSuppliers emptyFutureSupplierSet; // TODO G8: Let readInfoPtr provide required suppliers, such as GridStorageMetaInfo->m_VIP->m_GridDomain (as its range is required in further processing
		rtc->ScheduleItemWriter(MG_SOURCE_INFO_CODE("TreeItem::PrepareDataUsageImpl for Readable data") const_cast<TreeItem*>(refItem),
			[storageParent, self, readInfoPtr](Explain::Context* context)
			{
				auto onExit = make_scoped_exit([self]() { self->m_ReadAssets.Clear(); });
				dms_assert(readInfoPtr);
				dms_assert(*readInfoPtr);
				(*readInfoPtr)->OnPreLock();
				StorageReadHandle sHandle(std::move(*readInfoPtr)); // locks storage manager
				dms_assert(!*readInfoPtr);
				sHandle.FocusItem()->ReadItem(sHandle); // Read Item
			}
			, emptyFutureSupplierSet
				, false
				, nullptr
				);
		readInfoPtr.reset();
		assert(!readInfo);
		assert(CheckCalculatingOrReady(refItem) || refItem->WasFailed(FR_Data) || SuspendTrigger::DidSuspend());
		assert(self->GetInterestCount());
	}
	if (refItem->IsFailed())
	{
		if (refItem != self)
			self->Fail(refItem);
		return how_to_proceed::failed;
	}

	if (SuspendTrigger::DidSuspend())
		return how_to_proceed::suspended;

	dms_assert(CheckCalculatingOrReady(refItem));
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

	dms_assert(!SuspendTrigger::DidSuspend()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode, which hides SuspendTrigger::GetLastResult

	dms_assert(!(UInt32(drlType) &  UInt32(DrlType::Certain)) || SuspendTrigger::BlockerBase::IsBlocked()); // Callers responsibility

	// Checks State against suppliers if any changes occured after m_LastCheckedTS and Invalidates if any changes occured in any supplier

	UpdateMarker::ChangeSourceLock changeStamp(this, "PrepareDataUsage");

	dms_assert(IsPassor() || HasConfigData() || (m_State.GetProgress()>=PS_MetaInfo) || WasFailed(FR_MetaInfo)); // reset by DetermineState when supplier was invalidated

	const TreeItem* refItem = nullptr;

	treeitem_lock_map::ScopedTryLock localPreparedataLock(MG_SOURCE_INFO_CODE("TreeItem::PrepareDataUsageImpl ScopedTry") sg_PrepareDataUsageLockMap, this);
	if (!localPreparedataLock)
	{
		if (!WaitForReadyOrSuspendTrigger(this))
			goto suspended_or_failed;

		dms_assert(!SuspendTrigger::DidSuspend());
		dms_assert(!WasFailed(FR_Data));
		dms_assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(GetCurrUltimateItem()));
		goto data_ready;
	}

	if (m_State.IsDataFailed())  // may have been arranged in an alternative thread.
		goto failed_norefitem;
	refItem = GetCurrUltimateItem();
	
	dms_assert(refItem->IsPassor() || HasConfigData() || refItem->m_State.GetProgress() >= PS_MetaInfo || refItem->WasFailed(FR_MetaInfo));
	dms_assert(refItem->GetInterestCount() || !IsDataItem(this) || AsDataItem(refItem)->m_DataObject || drlType == DrlType::Certain); // interest consistency

	if (CheckCalculatingOrReady(refItem)) // quick route first
		goto data_ready; // may have been arranged in an alternative thread.
	if (IsDataItem(this))
	{
		auto avu = AbstrValuesUnit( AsDataItem(this) );
		if (avu && !avu->IsCacheItem())
		{
			if (!avu->PrepareDataUsage(drlFlags) || !avu->PrepareRange())
			{
				if (!SuspendTrigger::DidSuspend())
					Fail(avu);
				return false;
			}
		}
	}
	dms_assert(!SuspendTrigger::DidSuspend());

	try {
		while (true)
		{
			if (CheckCalculatingOrReady(refItem))
				goto data_ready_or_in_cache;
			//		_ProcessConfigData(true, false);
			//		Now try to actually get valid data
			if (refItem != this && refItem->IsFailed())
				Fail(refItem);
			if (WasFailed(FR_Data))
				goto failed;

			if ((drlType != DrlType::UpdateNever) && HasCalculator())
				switch (PrepareDataCalc(this, refItem, drlFlags))
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
				switch (PrepareDataRead(this, refItem, drlFlags))
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
			if (WasFailed(FR_Data))           goto failed;

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
		DoFail(err, FR_Data); 
		goto failed;
	}

data_ready:
	dms_assert(!SuspendTrigger::DidSuspend());
	dms_assert(!IsDataItem(this) || HasConfigData() || CheckCalculatingOrReady(refItem) || WasFailed(FR_Data));
	return SuspendTrigger::BlockerBase::IsBlocked() 
		|| IsPassor() 
		|| (m_State.GetTransState() >= actor_flag_set::AF_Committing) 
		|| SuspendibleUpdate(PS_Committed) 
		|| !SuspendTrigger::DidSuspend();

suspended_or_failed:
	dms_assert(drlType != DrlType::Certain || !SuspendTrigger::DidSuspend());
	dms_assert(SuspendTrigger::DidSuspend() || WasFailed()); // PRECONDITION THAT each suspend has been acted upon or we're on Certain mode
	if (SuspendTrigger::DidSuspend())
		goto suspended;

failed:
	dms_assert(WasFailed());
	dms_assert(!SuspendTrigger::DidSuspend());
	if (IsCalculatingOrReady(refItem))
		return true;
failed_norefitem:
	dms_assert(WasFailed());
	if (throwOnFail)
		ThrowFail();
	return false;

suspended:
	assert(drlType != DrlType::Certain);
	assert(SuspendTrigger::DidSuspend());
	return false;

nodata:
	Fail("No calculation rule or storage manager was specified and no specific primary data was provided", FR_Data);
	goto failed;
}


bool TreeItem::PrepareData() const
{
	return PrepareDataUsage(DrlType::Suspendible) && WaitForReadyOrSuspendTrigger(GetCurrUltimateItem());
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

bool HasCfsStorage(const TreeItem* obj)
{
	const TreeItem* storageHolder = obj->GetStorageParent(false);

	return storageHolder && !stricmp(storageHolder->GetStorageManager()->GetClsName().c_str(), "cfs");
}

bool TreeItem::HasConfigData() const
{
	if (IsCacheItem())
		return false;
	if (!IsDataItem(this) && !IsUnit(this))
		return false;

	if (mc_Calculator)  // DC_Ptr: false
		return mc_Calculator->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!mc_Expr.empty())
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

	if (mc_Calculator)  // DC_Ptr: false
		return mc_Calculator->IsDataBlock(); // DC_Ptr: false, ExprCalculator: false;
	if (!mc_Expr.empty())
		return false;
	if (GetCurrStorageParent(false) != nullptr)
		return false;
	if (IsUnit(this))
		return AsUnit(this)->HasTiledRangeData();
	if (IsDataItem(this))
		return AsDataItem(this)->m_DataObject != nullptr;
	return false;
}

bool TreeItem::CommitDataChanges() const
{
	dms_assert(m_State.GetProgress() >= PS_MetaInfo);
	if	(	(m_State.GetProgress() < PS_Committed)
		&&  IsStorable()
		&&	!IsDataReadable()
		&&	!IsFailed()
	)
	{
		MG_DEBUGCODE( dms_assert(! Actor::m_State.Get(ASF_WasLoaded) ); )

		const TreeItem* storageHolder = GetStorageParent(true);
		bool            hasCalculator = HasCalculator();
		dms_assert(storageHolder); // guaranteed by IsStorable();
		const AbstrStorageManager* sm = storageHolder->GetStorageManager();
		dms_assert(sm); // guaranteed by IsStorable();

		if (hasCalculator || IsDataReady(this) && !GetCurrRangeItem()->WasFailed(FR_Committed))
		{
			DBG_START("TreeItem", "CommitDataChanges", false);
			DBG_TRACE(("self = %s", GetSourceName().c_str()));

			auto interestHolder = GetInterestPtrOrNull();
			dms_assert(interestHolder); // Commit is Called from DoUpdate
			if (	(IsCalculatingOrReady(GetCurrRangeItem()) || PrepareDataUsage(DrlType::Suspendible))
				&&	WaitForReadyOrSuspendTrigger(GetCurrRangeItem()) 
				&& !GetCurrRangeItem()->WasFailed(FR_Committed)
				&&	DoWriteItem(sm->GetMetaInfo(storageHolder, const_cast<TreeItem*>(this), StorageAction::write))
			)
			// DoReadDataItem also for calling CreateResultingTreeItem(true, false) for TreeItems without storage ??
					SetProgress(PS_Committed);
			else
			{
				// can have failed just because PrepareDataUsage suspended or failed; 
//				dms_assert(!TriggerOperator::GetLastResult() || GetInterestCount() > 1); // suspension only possible when not called from DecInterestCount
				// must check suspend trigger to find out
				if (SuspendTrigger::DidSuspend())
				{
					if (GetInterestCount() < 2)
						ReportSuspension();
				}
				else if (!WasFailed(FR_Committed))
					Fail(
						mySSPrintF("Unable to commit data to storage %s", 
							DMS_TreeItem_GetAssociatedFilename(this)
						)
					,	FR_Committed
					);
				dms_assert(SuspendTrigger::DidSuspend() || WasFailed(FR_Committed));
				if (SuspendTrigger::DidSuspend())
					return false; // suspended or failed, try again later
			}
		}
	}

	const TreeItem* uti = _GetHistoricUltimateItem(this);
	actor_section_lock_map::ScopedLock specificSectionLock(MG_SOURCE_INFO_CODE("TreeItem::CommitDataChanges") sg_ActorLockMap, uti);
	uti->TryCleanupMem();
	return true;
}

bool PartOfInterestImpl(const TreeItem* self)
{
	while (self)
	{
		if (self->GetInterestCount())
			return true;
		self = self->GetTreeParent();
	}
	return false;
}

bool TreeItem::PartOfInterest() const
{ 
	if (GetInterestCount())
		return true;
	if (!IsCacheItem())
		return false;

	return PartOfInterestImpl(GetTreeParent());
}

garbage_t TreeItem::TryCleanupMem() const
{
	if (IsCacheItem() && !IsCacheRoot())
		return {};

//	return {}; // DEBUG

	leveled_std_section::scoped_lock globalDataLockCountLock(sg_CountSection);

	garbage_t garbage;
	TryCleanupMemImpl(garbage);
	return garbage;
}

bool TreeItem::TryCleanupMemImpl(garbage_t& garbageCan) const
{
	if (PartOfInterestOrKeep())
		return false;

	if (m_ItemCount < 0)
		return false;

	ClearTSF(TSF_DataInMem);
	if (!IsUnit(this))
		ClearData(garbageCan);

	if (IsCacheItem())
		for (const TreeItem* subTI = _GetFirstSubItem(); subTI; subTI = subTI->GetNextItem())
			subTI->TryCleanupMemImpl(garbageCan);

	return true;
}

void TreeItem::ClearData(garbage_t&) const
{}

//----------------------------------------------------------------------
// Dumping to OutStreamBase
//----------------------------------------------------------------------

#include "xml/xmlOut.h"
#include "xml/xmlTreeOut.h"
#include <time.h>


void TreeItem::XML_Dump(OutStreamBase* xmlOutStr) const
{ 
	// write #include <filename> if configStore defined
	if (xmlOutStr->GetLevel() > 0)
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
				IncludeFileSave(this, dirName.c_str(), DSM::GetSafeFileWriterArray(this));
			return;
		}
	}

	// Copy of code from Object because xmlElem must live after subItems
	SharedStr tagName = SharedStr((xmlOutStr->GetSyntaxType() != OutStreamBase::ST_DMS) ? GetXmlClassName().c_str() : GetSignature().c_str());

	XML_OutElement xmlElem(*xmlOutStr, tagName.c_str(), GetName().c_str(), true);

	xmlOutStr->DumpPropList(this);
	xmlOutStr->DumpSubTags(this);
	// end of Copy

	if (IsDataItem(this))
	{
		bool isDataBlock = mc_Calculator && mc_Calculator->IsDataBlock();
		if (isDataBlock || HasConfigData())
		{
			xmlOutStr->DumpSubTagDelim();
			if (isDataBlock)
				*xmlOutStr << mc_Calculator->GetExpr().c_str();
			else
			{
				TreeItemInterestPtr holder(this);
				XML_DumpData(xmlOutStr);
			}
		}
	}

	// check if any non endogenous subitems exist
	const TreeItem* subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	while(true)
	{
		if (!subItem)
			goto afterSubItems;
		if (!subItem->IsEndogenous())
			break; // found one
		subItem = subItem->GetNextItem();
	}

	// output all non endogenous subitems
	xmlElem.SetHasSubItems();
	xmlOutStr->BeginSubItems();

	subItem = _GetFirstSubItem(); // we don't want UpdateMetaInfo
	while (subItem)
	{
		if (!subItem->IsEndogenous())
			subItem->XML_Dump(xmlOutStr);
		subItem = subItem->GetNextItem();
	}
	xmlOutStr->EndSubItems();

afterSubItems:
	if (m_StorageManager) 
		m_StorageManager->CloseStorage();
}


//----------------------------------------------------------------------
// TreeItem SetStorageManger Functions
//----------------------------------------------------------------------

//#include "stg/StorageInterface.h"

void TreeItem::SetStorageManager(AbstrStorageManager* storageManager)
{
	if (m_StorageManager == storageManager)
		return;
	if (!GetTreeParent())
		throwItemErrorF(
			"StorageManager '%s' on root item is not allowed;\n"
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
		dms_assert(HasStorageManager()); // prcondition: GetStorageManager may only be called when HasStorageManager() returns true
			dms_assert(!IsCacheItem());        // implied by HasStorageManager()
			dms_assert(!InTemplate());         // implied by HasStorageManager()
			dms_assert(!IsDisabledStorage());  // implied by HasStorageManager()
			dms_assert( storageNamePropDefPtr->HasNonDefaultValue(this));

		SharedStr storageName = TreeItemPropertyValue(this, storageNamePropDefPtr);
		auto sm = AbstrStorageManager::Construct(this
		,	storageName
		,	storageTypePropDefPtr    ->GetValue(this)
		,	storageReadOnlyPropDefPtr->GetValue(this)
		,	throwOnFailure
		);

		dms_assert(sm || !throwOnFailure); // guaranteed by AbstrStorageManager::Construct
		if (sm)
			const_cast<TreeItem*>(this)->SetStorageManager(sm); // resets m_DisabledStorage
	}
	dms_assert(m_StorageManager || !throwOnFailure);
	return m_StorageManager;
}

void TreeItem::SetStorageManager(CharPtr storageName, CharPtr storageType, bool readOnly, CharPtr driver, CharPtr options)
{
	DBG_START("TreeItem", "SetStorageManager", false);
	storageNamePropDefPtr->SetValue(this, SharedStr(storageName) );
	storageTypePropDefPtr->SetValue(this, storageType ? GetTokenID_mt(storageType) : TokenID::GetEmptyID() );
	storageReadOnlyPropDefPtr->SetValue(this, readOnly);
	if (driver != nullptr)
		storageDriverPropDefPtr->SetValue(this, SharedStr(driver));
	if (options != nullptr)
		storageOptionsPropDefPtr->SetValue(this, SharedStr(options));
	SetStorageManager(nullptr);
//	m_State.Clear(ASF_DataReadableDefined);
	Invalidate();
}

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

const TreeItem* FindTreeItemByID(const TreeItem* searchLoc, TokenID subItemID)
{
	dms_assert(searchLoc);
	dms_assert(!subItemID.empty());
	dms_assert(GetTokenStr(subItemID).c_str()[0] != '.');

	while (searchLoc) {
		if (searchLoc->m_UsingCache)
			return searchLoc->m_UsingCache->FindItem(subItemID);

		const TreeItem* foundSubItem = searchLoc->GetConstSubTreeItemByID(subItemID);
		if (foundSubItem)
			return foundSubItem;

		searchLoc = searchLoc->GetTreeParent();
	};
	return nullptr;
}

//----------------------------------------------------------------------
//	InterestCount management
//----------------------------------------------------------------------

#include "DataArray.h"

//mc_IntegrityCheckTiles

#if defined(MG_DEBUG_DATASTORELOCK)
UInt32 sd_ItemInterestCounter = 0;
#endif

void TreeItem::StartInterest() const
{
	dms_assert(!std::uncaught_exceptions());
	if (!s_SessionUsageCounter.try_lock_shared())
	{
		dms_assert(OperationContext::CancelableFrame::CurrActive());
		DSM::CancelOrThrow(this);
	}
	auto unlockDsmUsageCounter = make_releasable_scoped_exit([]() { s_SessionUsageCounter.unlock_shared(); });
	UpdateMetaInfo();
	dms_assert(GetInterestCount() == 0);

	SharedPtr<const TreeItem> refItem = GetReferredItem();

	SharedActorInterestPtr    calcHolder = mc_DC.get_ptr();
	SharedTreeItemInterestPtr refItemHolder = refItem;
	SharedTreeItemInterestPtr parentHolder = GetTreeParent(); //  IsCacheItem() ? GetTreeParent() : nullptr;

	Actor::StartInterest();

	const TreeItem* storageParent = GetStorageParent(false);
	if (storageParent)
	{
		auto undoActorInterest = make_releasable_scoped_exit([this]() { this->Actor::StopInterest();; });
		storageParent->GetStorageManager()->StartInterest(storageParent, this);

		// nothrow from here
		undoActorInterest.release();
	}
	// nothrow from here, avoid rollbacks and release the InterestHolders without releasing the interest
	parentHolder.release();
	refItemHolder.release();
	calcHolder.release();
	unlockDsmUsageCounter.release();
#if defined(MG_DEBUG_DATASTORELOCK)
	++sd_ItemInterestCounter;
#endif
}

garbage_t TreeItem::StopInterest() const noexcept
{
	const TreeItem* storageParent = GetCurrStorageParent(false);
	if (storageParent)
	{
		auto sm = storageParent->GetCurrStorageManager();
		if (sm)
			sm->StopInterest(storageParent, this);
	}
	auto garbage = Actor::StopInterest();

	if (GetTreeParent())
		garbage |= GetTreeParent()->DecInterestCount();
	if (mc_DC)
		garbage |= mc_DC->DecInterestCount();

	if (mc_RefItem)
		garbage |= mc_RefItem->DecInterestCount();
	else
		garbage |= TryCleanupMem();

	try {
		m_ReadAssets.Clear();
	}
	catch(...)
	{
		reportF(SeverityTypeID::ST_Warning, "%s uncaught exception ", GetSourceName());
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

	dms_assert(OperationContext::CancelableFrame::CurrActive());
	DSM::CancelOrThrow(this);
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
	dms_assert(IsInWriteLock(this) || IsMainThread() || (IsUnit(this) && !IsCacheItem()));
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

void DMS_CONV CompareOutput(ClientHandle clientHandle, const Byte* data, streamsize_t size)
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

void TreeItem::SetLocation(SourceLocation* loc)
{
	m_Location = loc;
}

const SourceLocation* TreeItem::GetLocation() const
{
	if (m_Location)
		return m_Location;
	return Actor::GetLocation();
}

SharedStr TreeItem::GetConfigFileName() const
{
	if (m_Location)
		return m_Location->m_ConfigFileDescr->GetFileName();
	if (m_Parent)
		return m_Parent->GetConfigFileName();
	return SharedStr();
}

UInt32  TreeItem::GetConfigFileLineNr() const
{
	return (m_Location)
		? m_Location->m_ConfigFileLineNr
		: (m_Parent)
		? m_Parent->GetConfigFileLineNr()
		: 0;
}

UInt32  TreeItem::GetConfigFileColNr() const
{
	return (m_Location)
		? m_Location->m_ConfigFileColNr
		: (m_Parent)
		? m_Parent->GetConfigFileColNr()
		: 0;
}
