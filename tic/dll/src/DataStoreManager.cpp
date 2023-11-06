#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ppltasks.h"

#include "DataStoreManagerCaller.h"

#include "act/ActorLock.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "geo/PointOrder.h"
#include "ptr/SharedBase.h"
#include "ser/PolyStream.h"
#include "ser/PointStream.h"
#include "set/IndexedStrings.h"
#include "set/Token.h"
#include "utl/Environment.h"
#include "utl/SplitPath.h"

#include "stg/AbstrStreamManager.h"

#include "AbstrUnit.h"
#include "DataLocks.h"
#include "DataController.h"
#include "FreeDataManager.h"
#include "ItemLocks.h"
#include "OperationContext.h"
#include "SessionData.h"

#include "LispRef.h"
#include "LispTreeType.h"

//----------------------------------------------------------------------
// DSM
//----------------------------------------------------------------------

// ===============  DataStoreManager Persistence


#include "xct/DmsException.h"

[[noreturn]] void DSM::CancelOrThrow(const TreeItem* item)
{
	if (OperationContext::CancelableFrame::CurrActive())
		concurrency::cancel_current_task(); // assume it was cancelled due to outdated suppliers

	if (item)
		throwPreconditionFailed(item->GetConfigFileName().c_str(), item->GetConfigFileLineNr(), "CancelOrThrow requested without CancelableFrame");
	throwCheckFailed(MG_POS, "CancelOrThrow requested without CancelableFrame and without Item");
}

void DSM::CancelIfOutOfInterest(const TreeItem* item)
{
	if (IsMainThread() && !OperationContext::CancelableFrame::CurrActive())
		return;

	OperationContext::CancelableFrame::CurrActiveCancelIfNoInterestOrForced(DSM::IsCancelling());

	if (OperationContext::CancelableFrame::CurrActiveCanceled() && !std::uncaught_exceptions())
	{
		CancelOrThrow(item);
	}
}


// ===================================================== usage of m_SupplierLevels;

#include "act/ActorVisitor.h"
#include "StateChangeNotification.h"

 // ==== code analysis support: TreeItem_SetAnalysisSource
#include "TicInterface.h"

static SharedPtr<const TreeItem>      s_SourceItem; 
static std::map<const Actor*, supplier_level> s_SupplierLevels;

supplier_level operator & (supplier_level lhs, supplier_level rhs) { return supplier_level(UInt32(lhs) & UInt32(rhs)); }
supplier_level operator | (supplier_level lhs, supplier_level rhs) { return supplier_level(UInt32(lhs) | UInt32(rhs)); }

static void ProcessDeletion(ClientHandle clientHandle, const TreeItem* self, NotificationCode notificationCode)
{
	if (notificationCode == NC_Deleting)
	{
		assert(self != s_SourceItem); // s_SouceItem is reference counted
		s_SupplierLevels.erase(self); // supplier level register is not reference counted.
	}
}

bool MarkSources(const Actor* a, supplier_level level)
{
	assert(a);
	SharedTreeItem ti = dynamic_cast<const TreeItem*>(a); // block a frfom deletion when in process
	if (a->IsPassorOrChecked())
		if (!ti || ti->IsCacheItem())
			return false;

	if (s_SupplierLevels.empty())
		DMS_RegisterStateChangeNotification(ProcessDeletion, nullptr);

	supplier_level& currLevel = s_SupplierLevels[a];

	bool hasSource = (a == s_SourceItem);

	if ((currLevel & supplier_level::usage_flags) < level) // Source bit is also already determined and irrelevant for the decision to search for next level.
	{
		currLevel = level; // if Source bit was set, it will be set again.

		if (ti)
			NotifyStateChange(ti, NC2_InterestChange);

		if (level == supplier_level::calc)
		{
			a->VisitSuppliers(SupplierVisitFlag::CalcAll, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::calc); }));
			a->VisitSuppliers(SupplierVisitFlag::MetaAll, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::meta); }));
		}
		else
		{
			assert(currLevel == supplier_level::meta);
			a->VisitSuppliers(SupplierVisitFlag::All, MakeDerivedProcVisitor([&hasSource](const Actor* s) { hasSource |= MarkSources(s, supplier_level::meta); }));
		}
		if (hasSource)
			currLevel = currLevel | supplier_level::uses_source_flag;
	}
	else if ((currLevel & supplier_level::uses_source_flag) == supplier_level::uses_source_flag)
		hasSource = true;
	return hasSource;
}


TIC_CALL void TreeItem_SetAnalysisTarget(const TreeItem * ti, bool mustClean)
{
	assert(IsMainThread());
	if (mustClean)
	{
//	TODO: issue: registered suppliers may alredy be destroyed (and locations even be reused !). We need std::weak_ptr here.
//		for (auto& supplierRecord: dsm->m_SupplierLevels)
//			if (auto ti = dynamic_cast<const TreeItem*>(supplierRecord.first))
//				NotifyStateChange(ti, NC2_InterestChange);
		DMS_ReleaseStateChangeNotification(ProcessDeletion, nullptr);
		s_SupplierLevels.clear();
	}
	if (!ti)
		return;
	MarkSources(ti, supplier_level::calc);
}

TIC_CALL void TreeItem_SetAnalysisSource(const TreeItem * ti)
{
	DMS_CALL_BEGIN

		s_SourceItem = ti;
		TreeItem_SetAnalysisTarget(ti, true); // sends a refresh at cleaning

	DMS_CALL_END
}

TIC_CALL supplier_level TreeItem_GetSupplierLevel(const TreeItem * ti)
{
	assert(IsMainThread());
	auto iter = s_SupplierLevels.find(ti);
	if (iter != s_SupplierLevels.end())
		return iter->second;
	return supplier_level::none;
}

