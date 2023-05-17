#include "TicPCH.h"
#pragma hdrstop

#include "ppltasks.h"

#include "DataStoreManagerCaller.h"

#include "act/ActorLock.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
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

const UInt32 SL_USAGE_MASK  = 0x03;
const UInt32 SL_USES_SOURCE = 0x04;

bool MarkSources(SessionData* dsm, const Actor* a, UInt32 level)
{
	dms_assert(a);
	if (a->IsPassorOrChecked())
		return false;

	UInt32& currLevel = dsm->m_SupplierLevels[a];

	bool hasSource = (a == dsm->m_SourceItem);

	if ((currLevel & SL_USAGE_MASK) < level) // Source bit is also already determined and irrelevant for the decision to search for next level.
	{
		currLevel = level; // if Source bit was set, it will be set again.

		auto ti = dynamic_cast<const TreeItem*>(a);
		if (ti)
			NotifyStateChange(ti, NC2_InterestChange);

		if (level == 1)
		{
			a->VisitSuppliers(SupplierVisitFlag::CalcAll, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 1); }));
			a->VisitSuppliers(SupplierVisitFlag::MetaAll, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 2); }));
		}
		else
		{
			dms_assert(currLevel == 2);
			a->VisitSuppliers(SupplierVisitFlag::All, MakeDerivedProcVistor([dsm, &hasSource](const Actor* s) { hasSource |= MarkSources(dsm, s, 2); }));
		}
		if (hasSource)
			currLevel |= SL_USES_SOURCE;
	}
	else
		hasSource |= ((currLevel & SL_USES_SOURCE) != 0);
	return hasSource;
}

extern "C" TIC_CALL void DMS_TreeItem_SetAnalysisTarget(const TreeItem* ti, bool mustClean)
{
	DMS_CALL_BEGIN

		auto dsm = DSM::Curr();
		if (mustClean)
		{
			dsm->m_SupplierLevels.clear();
			NotifyStateChange(ti, NC2_InterestChange);
		}
		if (!ti)
			return;
		MarkSources(dsm.get(), ti, 1);

	DMS_CALL_END
}

extern "C" TIC_CALL void DMS_TreeItem_SetAnalysisSource(const TreeItem* ti)
{
	DMS_CALL_BEGIN

		auto dsm = DSM::Curr();
		dsm->m_SourceItem = ti;
		DMS_TreeItem_SetAnalysisTarget(ti, true); // sends a refresh at cleaning

	DMS_CALL_END
}

extern "C" TIC_CALL UInt32 DMS_TreeItem_GetSupplierLevel(const TreeItem* ti)
{
	DMS_CALL_BEGIN

	auto dsm = DSM::Curr();
	if (dsm) {
		auto iter = dsm->m_SupplierLevels.find(ti);
		if (iter != dsm->m_SupplierLevels.end())
			return iter->second;
	}

	DMS_CALL_END
	return 0;
}

