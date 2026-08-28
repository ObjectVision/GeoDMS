// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TreeItem meta-info update: binding an item to its calculation key and DataController,
// wrapping a result expression in the integrity-check guardian closure of its ancestors,
// and the UpdateMetaInfo / SuspendibleUpdate / DoUpdate state machine.

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
// #1218: the checks that apply to an item are not only its own and its ancestors'. An
// ExplicitSupplier declares "evaluate me first", and whatever guards THAT item -- its own check,
// its ancestors', its suppliers', transitively -- guards the declaring item's calculation with
// it. TreeItemCheckGuardians is the closure of one item under both relations, reduced to the
// items that actually carry a check, in fold order: the item's own check first, then the checks
// reached through its ExplicitSuppliers (declaration order, each supplier bringing its own
// closure), then the parent's closure. For an item without ExplicitSuppliers anywhere on its
// parent chain this is exactly the pre-#1218 self-to-root walk, in the same order, so the folded
// expression -- and thereby every existing DataController moniker -- is unchanged.
//
// The closure is memoized per config item in ConfigProperties::mc_CheckGuardians (meta-thread
// only, like mc_DC; reset with the other config-derived state). An item that adds nothing shares
// its parent's instance and is not memoized -- re-deriving it is the same walk the pre-#1218
// code did on every fold -- so no ConfigProperties is allocated just to hold a copy of the
// parent's pointer. Whether a collected guard is REDUNDANT for a specific expression is not
// decided here: that stays with the wrap site, per folded expression, against the
// DataController-memoized implied-check sets (#1182).
//
// Well-foundedness along the supplier edge: the DoesContain gate (see above) refuses a checker
// reaching into its holder's subtree, but not one referencing a consumer that declares the
// holder as ExplicitSupplier; that shape closes a cycle through this closure and surfaces as a
// metainfo/DataController circularity, like any other cyclically configured calculation.
struct TreeItemCheckGuardians
{
	std::vector<SharedTreeItem> guardians; // each satisfies HasIntegrityChecker()
};
using TreeItemCheckGuardiansPtr = std::shared_ptr<const TreeItemCheckGuardians>;

static const TreeItemCheckGuardiansPtr& TreeItem_NoCheckGuardians()
{
	// shared derived-none sentinel; holds no TreeItem refs, so its CRT-exit destruction is inert
	static TreeItemCheckGuardiansPtr s_None = std::make_shared<TreeItemCheckGuardians>();
	return s_None;
}

struct CheckGuardianWalkContext
{
	std::vector<const TreeItem*> inProgress; // cycle guard along the ExplicitSuppliers relation
	bool hitCycle = false;                   // a back-edge was skipped somewhere below
};

static auto TreeItem_GetCheckGuardians(const TreeItem* self, CheckGuardianWalkContext& ctx)
	-> TreeItemCheckGuardiansPtr
{
	if (!self)
		return TreeItem_NoCheckGuardians();

	if (auto cfg = self->GetConfigProperties())
		if (cfg->mc_CheckGuardians)
			return cfg->mc_CheckGuardians;

	// ExplicitSuppliers may be configured cyclically; each item contributes once, the back-edge
	// is skipped, and nothing computed under a skipped edge is memoized (it would be incomplete).
	if (std::find(ctx.inProgress.begin(), ctx.inProgress.end(), self) != ctx.inProgress.end())
	{
		ctx.hitCycle = true;
		return TreeItem_NoCheckGuardians();
	}
	ctx.inProgress.push_back(self);
	bool outerHitCycle = ctx.hitCycle; ctx.hitCycle = false;

	std::vector<SharedTreeItem> extra; // own check + supplier-borne checks, in fold order
	if (self->HasIntegrityChecker())
		extra.emplace_back(make_shared_tree(self, existing_obj{}));

	// The supplier edge: config items only. Cache items also carry a SupplCache -- the
	// PhaseContainer fence mirrors, InitAt'ed for error attribution -- but those are engine
	// bookkeeping, not a configured "evaluate me first" relation; and inside a template the
	// configured names need not resolve yet.
	if (self->HasSupplCache() && !self->IsCacheItem() && !self->InTemplate())
	{
		auto supplCache = self->GetSupplCache();
		UInt32 n = supplCache->GetNrConfigured(self); // resolves the configured names; an unknown supplier fails here, as on every other consultation
		for (UInt32 i = 0; i != n; ++i)
			if (auto supplier = supplCache->GetSupplier(i))
				for (const auto& g : TreeItem_GetCheckGuardians(supplier, ctx)->guardians)
					if (std::find(extra.begin(), extra.end(), g) == extra.end())
						extra.emplace_back(g);
	}

	auto parentHolder = self->GetTreeParent();
	auto parentClosure = TreeItem_GetCheckGuardians(parentHolder.get(), ctx);

	ctx.inProgress.pop_back();
	bool incomplete = ctx.hitCycle;
	ctx.hitCycle = outerHitCycle || incomplete;

	TreeItemCheckGuardiansPtr result;
	if (extra.empty())
		result = parentClosure; // share: this item adds nothing (the dominant case)
	else
	{
		for (const auto& g : parentClosure->guardians)
			if (std::find(extra.begin(), extra.end(), g) == extra.end())
				extra.emplace_back(g);
		auto owned = std::make_shared<TreeItemCheckGuardians>();
		owned->guardians = std::move(extra);
		result = std::move(owned);
	}

	if (!incomplete && !self->IsCacheItem() && !self->InTemplate())
		if (result != parentClosure || self->GetConfigProperties())
			self->GetOrCreateConfigProperties().mc_CheckGuardians = result;

	return result;
}

static bool TreeItem_HasIntegrityCheckerInclAncestors(const TreeItem* self)
{
	bool chainHasSupplierEdge = false;
	SharedTreeItem holder; // keeps the ancestor alive while it is inspected
	for (auto guardian = self; guardian; holder = guardian->GetTreeParent(), guardian = holder.get())
	{
		if (guardian->HasIntegrityChecker())
			return true;
		if (guardian->HasSupplCache() && !guardian->IsCacheItem() && !guardian->InTemplate())
			chainHasSupplierEdge = true;
	}
	if (!chainHasSupplierEdge)
		return false; // the common case: no supplier edges, so the walk above was exhaustive

	// #1218: a check may also apply through the ExplicitSuppliers relation
	CheckGuardianWalkContext ctx;
	return !TreeItem_GetCheckGuardians(self, ctx)->guardians.empty();
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
// #1197: a check that cannot be BUILT fails where the guarded expression is instantiated, which is
// after the check text was parsed and long after the "Create IntegrityCheck" context below has gone.
// The error then names the item being checked and nothing else -- "eq Error: Cannot find operator for
// these arguments" with no hint that an IntegrityCheck is involved, let alone which one. That is
// unhelpful for a hand-written check and worse for a generated one, such as the restriction an .mmd
// dictionary carries (#1195), which the modeller never wrote and cannot see.
//
// The text names the check itself, since that is what a reader has to look at.
SharedStr TreeItem_IntegrityCheckText(const TreeItem* guardian)
{
	return mySSPrintF("'{}' of {}"
		, integrityCheckPropDefPtr->GetValue(guardian)
		, guardian->GetFullName()
	);
}

// Only the checks that this fold actually inserted, which is normally one: an ancestor check that is
// already implied by the expression is skipped by the fold (#1182) and would be a false lead here.
// Composed only when an error unwinds through the handle, so nothing is paid when nothing fails.
static SharedStr TreeItem_IntegrityCheckBuildContextStr(const std::vector<const TreeItem*>& foldedChecks)
{
	SharedStr checks;
	for (auto guardian : foldedChecks)
	{
		if (!checks.empty())
			checks += " and ";
		checks += TreeItem_IntegrityCheckText(guardian);
	}
	if (checks.empty())
		return SharedStr();
	return mySSPrintF("while building the IntegrityCheck {}", checks);
}

static auto TreeItem_CreateCheckedExpr(LispPtr resultExpr, const DataController* resultExprDC, const TreeItem* self
	, std::vector<const TreeItem*>* foldedChecks = nullptr) -> LispRef
{
	dms_assert(TreeItem_HasIntegrityCheckerInclAncestors(self));
	assert(!resultExprDC || resultExprDC->GetLispRef() == resultExpr);

	check_set enforced;
	if (resultExprDC)
		if (auto implied = resultExprDC->GetImpliedChecks())
			enforced = implied->thing;

	LispRef result = resultExpr;

	// #1218: fold the whole closure of applicable checks -- self, ancestors, and the checks
	// reached through ExplicitSuppliers, transitively over both relations (see
	// TreeItem_GetCheckGuardians above; the list keeps each guardian alive while folded).
	CheckGuardianWalkContext ctx;
	auto closure = TreeItem_GetCheckGuardians(self, ctx);
	for (const auto& guardianHolder : closure->guardians)
	{
		auto guardian = guardianHolder.get();
		assert(guardian->HasIntegrityChecker());

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
		if (foldedChecks) // for the #1197 context: what went in, in the order it went in
			foldedChecks->emplace_back(guardian);
	}
	return result;
}

void TreeItem::UpdateDC() const
{
	if (mc_DC || !mc_RefItem.expired() || WasFailed(FailType::MetaInfo) || InTemplate() || IsCacheItem() || IsPassor())
		return;

	auto [resultDC, srcItem] = GetOrgDC();

	// #795 in the meta phase: this is where the calculation rule of THIS item gets its result made,
	// below and again in SetDC, and an operator that raises a diagnostic from CreateResult has no
	// item to name yet -- the back reference that lets a result name itself is only installed once
	// that result exists. Hand the DC the item whose calculation this is, the same name that
	// CallCalcResultImpl adopts in the data phase and under the same rule: fill an empty slot only,
	// so a calculation keeps the name it started with, and let the back reference take over later.
	if (resultDC && !resultDC->GetOriginItem())
		resultDC->SetOriginItem(make_shared_tree(this, existing_obj{}));

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
		std::vector<const TreeItem*> foldedChecks; // #1197: the guardians whose check went into checkedExpr
		auto checkedExpr = TreeItem_CreateCheckedExpr(resultExpr, resultDC.get(), this, &foldedChecks);

		// #1197: the operators of a folded check are resolved when the guarded DataController is made
		// -- GetOrCreateDataController, and then SetDC -> FuncDC::MakeResult -> GetArgs ->
		// AbstrOperGroup::FindOper -- so the context has to span both calls, not just the first.
		auto buildContext = MakeLCH([&foldedChecks]() -> SharedStr { return TreeItem_IntegrityCheckBuildContextStr(foldedChecks); });
		resultDC = GetOrCreateDataController(checkedExpr);
		SetDC(resultDC, srcItem.get());
		return;
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
	{
		auto sourceExpr = CreateLispTree(this, false);
		// #1209: an item read from a storage has no calculation rule, so UpdateDC has no
		// DataController to fold ancestor checks into and the exit here handed consumers an
		// UNGUARDED source reference: a check on the read item or its ancestors was then only
		// evaluated out-of-band by the validate phase of DoUpdate, after data preparation.
		// Fold here the same way GetCheckedKeyExpr already folds this very representation for
		// expression consumers, so a source-referencing consumer gets the guard woven into its
		// calculation, scheduled and computed like any other supplier.
		//
		// No recursion: delivering the guarded expression evaluates its sourceDescr argument,
		// whose SymbDC delegates to this item's PrepareDataUsage -- and PrepareDataUsageImpl
		// consults GetCheckedDC only for items WITH a calculator (PrepareDataCalc); a loadable
		// item without one goes straight to PrepareDataRead. That gate must stay as it is:
		// routing a calculator-less item's own data preparation through this checked DC would
		// close exactly that cycle.
		if (TreeItem_HasIntegrityCheckerInclAncestors(this))
		{
			std::vector<const TreeItem*> foldedChecks; // #1197: the guardians whose check went into checkedExpr
			auto checkedExpr = TreeItem_CreateCheckedExpr(sourceExpr, nullptr, this, &foldedChecks);
			auto buildContext = MakeLCH([&foldedChecks]() -> SharedStr { return TreeItem_IntegrityCheckBuildContextStr(foldedChecks); });
			return GetOrCreateDataController(checkedExpr);
		}
		return GetOrCreateDataController(sourceExpr);
	}
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

	// #1167: a PhaseContainer must run when the update path reaches it, and the update path reaches
	// it in two shapes. The container item's own DC IS the PhaseContainer FuncDC; a member of it
	// (phase/x) carries a SubItem FuncDC whose arg 0 is that PhaseContainer, because the endogenous
	// shadow of a fence member is given the calculation subitem(<phase expr>, 'x')
	// (GetLispRefForTreeItem -> slSubItemCall), and a deeper path nests such calls.
	// Only the first shape used to be recognised here, so a configuration that reaches fence members
	// at UPDATE level only -- the 'Ready' + ExplicitSuppliers driver idiom, which GeoDmsRun turns
	// into DMS_TreeItem_Update and never into a data demand -- walked straight past the fence into
	// the source container: the work ran unfenced and the phase never executed, so it reported
	// nothing and ordered nothing. A DATA demand on such a member does reach the phase, since
	// SubItem's arg 0 has oper_arg_policy::calc_subitem_root; that path is unchanged.
	// Joining arg 0 rather than the member's own DC keeps the collection semantics: PreCalcUpdate
	// still gathers only the mirror members that carry interest, so nothing extra is materialised.
	{
		auto phaseDC = GetOrgDC().first;
		while (phaseDC)
		{
			auto subItemCall = dynamic_cast<const FuncDC*>(phaseDC.get());
			if (!subItemCall || subItemCall->m_OperatorGroup->GetNameID() != token::subitem)
				break;
			phaseDC = DataControllerRef(subItemCall->GetArgDC(0), existing_obj{});
		}
		if (phaseDC)
			if (auto fc = dynamic_cast<const FuncDC*>(phaseDC.get()))
				if (fc->m_OperatorGroup->GetNameID() == token::PhaseContainer)
				{
					FutureData phaseInterest = phaseDC; // CallCalcResult requires interest, which an arg DC reached through a SubItem call need not have yet
					if (auto fd = phaseDC->CallCalcResult())
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
