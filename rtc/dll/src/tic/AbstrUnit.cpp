// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "AbstrUnit.h"

#include "act/ActorVisitor.h"
#include "act/UpdateMark.h"
#include "dbg/Check.h"        // reportF_without_cancellation_check, for the Stage-2 CRS drift detector
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"

#include <optional>
#include "geo/PointOrder.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "set/StaticQuickAssoc.h"
#include "set/VectorFunc.h"
#include "utl/mySPrintF.h"
#include "utl/Quotes.h"
#include "xct/DmsException.h"

#include "LockLevels.h"
#include "LispList.h"

#include "AbstrCalculator.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Metric.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TiledUnit.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#include "stg/AbstrStorageManager.h"
#include "DataArrayValue.h"

//----------------------------------------------------------------------
// Supporting structures
//----------------------------------------------------------------------

leveled_std_section s_DataItemRefContainer(item_level_type(0), ord_level_type::DataRefContainer, "DataRefContainer");

struct DataItemRefContainer
{
	typedef std::map<const AbstrDataItem*, UInt32> map_t;
	typedef std::vector<const AbstrDataItem*>      vec_t;

	~DataItemRefContainer()
	{
		// Pre-migration m_DomainUnit was an OWNING SharedPtr<const AbstrUnit>, so a data item kept its
		// domain unit alive and every Del() (in ~AbstrDataItem) necessarily ran before the unit's registry
		// was torn down -> the container was always empty here. With the std-ptr migration m_DomainUnit is
		// WEAK (non-owning), so a domain unit may now predecease its still-registered data items (e.g. at
		// config teardown C++ destroys this derived-class member before the base ~TreeItem releases the
		// unit's child attributes). A residual size() is therefore expected and benign: the entries are raw
		// back-refs only ever read on a LIVE unit (GetDataItemOut/GetNrDataItemsOut), and while the unit is
		// live every registered item is live (its Del runs while the unit can still be locked); the stale
		// entries simply vanish with the registry. So no assert(!size()) here.
	}

	void Add(const AbstrDataItem* item)
	{
		leveled_std_section::scoped_lock lock(s_DataItemRefContainer);

		auto pos = m_Map.lower_bound(item);
		if (pos != m_Map.end() && pos->first == item)
		{
			return;
		}
		m_Map.insert(pos, { item, m_Vec.size() });
		m_Vec.push_back(item);
	}

	void Del(const AbstrDataItem* item)
	{
		leveled_std_section::scoped_lock lock(s_DataItemRefContainer);

		map_t::iterator pos = m_Map.find(item);
		if (pos == m_Map.end())
			return;
		dms_assert(pos->second < m_Vec.size() );
		m_Vec[pos->second] = nullptr;
		m_Map.erase(pos);
		if (!m_Map.size())
		{
#if defined(MG_DEBUG)
			for (vec_t::const_iterator i = m_Vec.begin(), e = m_Vec.end(); i!=e; ++i)
			{
				dms_assert(!*i);
			}
#endif //defined(MG_DEBUG)
			vector_clear(m_Vec);
		}
		dbg_assert(!Has(item));
	}
#if defined(MG_DEBUG)
	bool Has(const AbstrDataItem* item) const
	{
		return m_Map.find(item) != m_Map.end();
	}

#endif //defined(MG_DEBUG)

	const AbstrDataItem* GetAt(UInt32 i)
	{
		dms_assert(i < size());
		return m_Vec[i];
	}

	UInt32 size() const { return m_Vec.size(); }

private:
	map_t m_Map;
	vec_t m_Vec;
};

// NOTE: the former `static_quick_assoc<const AbstrUnit*, TokenID> s_SpatialReferenceAssoc`
// lived here. It is replaced by the AbstrUnit::m_Crs member (see Crs.h). The global was an
// unsynchronised std::map keyed on raw pointers, so it could not be written from the worker
// threads that build cache units -- which is precisely why the CRS had to be smuggled
// through the metric instead. See doc/development/crs-metric-decoupling.md.
//----------------------------------------------------------------------
// class  : AbstrUnit 
//----------------------------------------------------------------------

AbstrUnit::AbstrUnit() {}  // ctor calls for ~OwningPtr<DataItemsAssocPair> in case of exception

// DataItemsOut
AbstrUnit::~AbstrUnit()
{
	// The spatial reference used to live in the s_SpatialReferenceAssoc global keyed on
	// `this`, which had to be erased here. It is now the m_Crs member, released with the
	// object -- no destructor coupling, and no global to keep in step.
}

inline DataItemRefContainer& AbstrUnit::GetDataItemsAssoc() const
{
	leveled_std_section::scoped_lock lock(s_DataItemRefContainer);
	if (!HasDataItemsAssoc())
		m_DataItemsAssocPtr.reset(new DataItemRefContainer);
	return *m_DataItemsAssocPtr;
}

SharedPtr<const AbstrTileRangeData> AbstrUnit::GetTiledRangeData() const
{
	return {};
}

UInt32 AbstrUnit::GetNrDataItemsOut() const
{
	if (HasDataItemsAssoc())
	{
		DataItemRefContainer* rc = m_DataItemsAssocPtr.get();
		if (rc)
			return rc->size();
	}
	return 0;
}

const AbstrDataItem* AbstrUnit::GetDataItemOut(UInt32 index) const
{
	dms_assert(index < GetNrDataItemsOut());
	return m_DataItemsAssocPtr->GetAt(index);
}

void AbstrUnit::AddDataItemOut(const AbstrDataItem* item) const
{
	if (HasVarRange())
		GetDataItemsAssoc().Add(item);
}

void AbstrUnit::DelDataItemOut(const AbstrDataItem* item) const
{
	DataItemRefContainer* rc = m_DataItemsAssocPtr.get();
//	dms_assert(rc); // once added, it must have an assoc
	if (rc)
		rc->Del(item);
}

#if defined(MG_DEBUG)
bool AbstrUnit::HasDataItemOut(const AbstrDataItem* item) const
{
	DataItemRefContainer* rc = m_DataItemsAssocPtr.get();
	if (rc)
		return rc->Has(item);

	return false;
}

#endif //defined(MG_DEBUG)

SharedStr AbstrUnit::GetProjMetrString() const
{
	return GetMetricStr(FormattingFlags::ThousandSeparator) + GetProjectionStr(FormattingFlags::ThousandSeparator);
}

using CharPtrPair = std::pair<CharPtr, CharPtr>;



auto RelabelX(CharPtr role, CharPtr role2) -> CharPtrPair
{
	if (!role[2]) // zero-termination
		switch (role[1])
		{
		case '0': return CharPtrPair("Common ", role2);
		case '1': return CharPtrPair(role2, " of  first argument");
		case '2': return CharPtrPair(role2, " of  second argument");
		case '3': return CharPtrPair(role2, " of  third argument");
		case '4': return CharPtrPair(role2, " of  fourth argument");
		case '5': return CharPtrPair(role2, " of  fifth argument");
		case '6': return CharPtrPair(role2, " of  sixth argument");
		case '7': return CharPtrPair(role2, " of  seventh argument");
		case '8': return CharPtrPair(role2, " of  eighth argument");
		case '9': return CharPtrPair(role2, " of  ninth argument");
		case 'A': return CharPtrPair(role2, " of  tenth argument");
		}
	return CharPtrPair(role, "");
}

CharPtrPair Relabel(CharPtr role) // parse 'e1', 'e4', 'v1', 'v4' 
{
	assert(role);
	if (*role == 'e')
		return RelabelX(role, "Domain");
	if (*role == 'v')
		return RelabelX(role, "Values");
	return { role, "" };
}

void AbstrUnit::UnifyError(const AbstrUnit* cu, CharPtr reason, CharPtr leftRole, CharPtr rightRole, UnifyMode um, SharedStr* resultMsg, bool isDomain) const
{
	if ((!resultMsg) && !(um & UM_Throw))
		return;

	assert(leftRole  != nullptr && *leftRole  != char(0) || resultMsg == nullptr && !(um & UM_Throw));
	assert(rightRole != nullptr && *rightRole != char(0) || resultMsg == nullptr && !(um & UM_Throw));

	dms_assert(cu);
	dms_assert(reason);

	auto leftPair = Relabel(leftRole);
	auto rightPair = Relabel(rightRole);

	SharedStr msg = mgFormat2SharedStr("{} mismatch between {}{} ({} {}: {}) and {}{} ({} {}: {}){}"
		,	isDomain ? "Domain" : "Values"
		,	leftPair.first, leftPair.second, 	GetFullName(),     GetProjMetrString(),     GetValueType()->GetName()
		,	rightPair.first, rightPair.second, cu->GetFullName(), cu->GetProjMetrString(), cu->GetValueType()->GetName()
		,	reason 
		);

	if (um & UM_Throw)
		throwItemError(msg);

	assert(resultMsg);
	*resultMsg = msg;
}

bool AbstrUnit::DoWriteItem(StorageMetaInfoPtr&& smi) const
{
	assert( GetInterestCount() );

	auto sm = smi->StorageManager();
	return sm->WriteUnitRange(std::move(smi));
}

bool AbstrUnit::UnifyDomain(const AbstrUnit* cu, CharPtr leftRole, CharPtr rightRole, UnifyMode um, SharedStr* resultMsg) const
{
	assert(cu);

	assert(!((um & UM_Throw) && resultMsg));
	assert(leftRole  && *leftRole  || !(um & UM_Throw) && !resultMsg);
	assert(rightRole && *rightRole || !(um & UM_Throw) && !resultMsg);

	if (cu == this)
		return true;

	if (!cu->IsKindOf(GetDynamicClass()))
	{
		if ((um & UM_AllowVoidRight) && const_unit_dynacast<Void>(  cu))
			return true;
		UnifyError(cu, " (different ValueTypes)", leftRole, rightRole, um, resultMsg, true);
		return false;
	}

	if (const_unit_dynacast<Void>(this)) return true;
	if (GetValueType()->HasFixedValues()) return true;

	if ((um & UM_AllowDefaultLeft ) &&     IsDefaultUnit()) return true;
	if ((um & UM_AllowDefaultRight) && cu->IsDefaultUnit()) return true;

	if (um & UM_AllowRightExpansion)
	{
		// the identity walk below (GetCurrUltimateItem, GetCheckedKeyExpr) has
		// meta-info readiness as precondition, but this mode's callers (the typed
		// checker) compare units freshly resolved from scope or a declared
		// signature, which nothing has updated yet. The flag is a meta-thread
		// caller contract (see AbstrUnit.h), so updating here is legitimate.
		UpdateMetaInfoIfNotAlready();
		cu->UpdateMetaInfoIfNotAlready();
	}

	if (GetCurrUltimateItem() != cu->GetCurrUltimateItem())
	{
		SharedTreeItem thisRepresentation = make_shared_tree(this, existing_obj{});
		if (!this->IsCacheItem())
		{
			auto thisDC = GetOrCreateDataController(this->GetCheckedKeyExpr());
			if (!thisDC)
				goto error;
			thisRepresentation = thisDC->MakeResult();
		}
		{
			SharedTreeItem thatRepresentation = make_shared_tree(cu, existing_obj{});
			if (!cu->IsCacheItem())
			{
				// UM_AllowRightExpansion (meta-thread callers only, see AbstrUnit.h):
				// intern the right key too, so the comparison is total and symmetric.
				// Default: lookup only (the #361 fix; worker-thread re-checks must
				// not create DCs) — a missing DC then reads as a mismatch.
				assert(!(um & UM_AllowRightExpansion) || IsMetaThread());
				auto thatDC = (um & UM_AllowRightExpansion)
					? GetOrCreateDataController(cu->GetCheckedKeyExpr())
					: GetExistingDataController(cu->GetCheckedKeyExpr());
				if (!thatDC)
					goto error;
				thatRepresentation = thatDC->MakeResult();
			}
			if (thisRepresentation == thatRepresentation)
				return true;
			if ((um & UM_AllowAllEqualCount) && (GetCount() == cu->GetCount()))
				return true;
		}
	error:
		UnifyError(cu, " (different CheckedKeyExpr)", leftRole, rightRole, um, resultMsg, true);
		return false;
	}
	return true;
}

bool AbstrUnit::UnifyValues(const AbstrUnit* cu, CharPtr leftRole, CharPtr rightRole, UnifyMode um, SharedStr* resultMsg) const
{
	// TODO G8: dms_assert(Was(ProgressState::MetaInfo)); dms_assert(cu->Was(ProgressState::MetaInfo));
	assert(cu);

	assert(!((um & UM_Throw) && resultMsg));
	assert(leftRole  && *leftRole  || !(um & UM_Throw) && !resultMsg);
	assert(rightRole && *rightRole || !(um & UM_Throw) && !resultMsg);

	if (cu == this)
		return true;

	if (!(um & UM_AllowTypeDiff) && !cu->IsKindOf(GetDynamicClass()))
	{
		UnifyError(cu, " (different ValueTypes)", leftRole, rightRole, um, resultMsg, false);
		return false;
	}

	// Metric unification
	if ((um & UM_AllowDefaultLeft ) &&     IsDefaultUnit()) return true;
	if ((um & UM_AllowDefaultRight) && cu->IsDefaultUnit()) return true;

	auto lhsMetric = GetCurrMetric(), rhsMetric = cu->GetCurrMetric();
	if (!AreEqual(lhsMetric, rhsMetric))
	{
		UnifyError(cu, " (incompatible Metrics)", leftRole, rightRole, um, resultMsg, false);
		return false;
	}

	// Projection unification
	auto lhsProj = GetCurrProjection(), rhsProj = cu->GetCurrProjection();
	if (!AreEqual(lhsProj, rhsProj))
	{
		UnifyError(cu, " (incompatible Projections)", leftRole, rightRole, um, resultMsg, false);
		return false;
	}
	return true;
}

bool AbstrUnit::IsDefaultUnit() const
{
	return this == GetUnitClass()->CreateDefault();
}

bool AbstrUnit::HasVarRangeData() const
{
	return AsUnit(this)->HasTiledRangeData() && !AsUnit(this)->GetValueType()->HasFixedValues();
}

const UnitCrs* AbstrUnit::GetCrs() const
{
	// Delegate to the referred item exactly as RangedUnit<V>::GetMetric does (Unit.cpp).
	// This is the whole point of moving off the side table: a cache unit can now answer
	// for the config unit it refers to.
	if (auto refItem = debug_cast<const AbstrUnit*>(GetReferredItem().get()))
		return refItem->GetCrs();
	return m_Crs.get_ptr();
}

const UnitCrs* AbstrUnit::GetCurrCrs() const
{
	if (auto refItem = debug_cast<const AbstrUnit*>(GetCurrRefItem().get()))
		return refItem->GetCurrCrs();
	return m_Crs.get_ptr();
}

void AbstrUnit::SetCrs(const UnitCrs* crs)
{
	m_Crs = IsEmpty(crs) ? nullptr : crs;
}

void AbstrUnit::SetSpatialReference(TokenID format)
{
	// An empty token legitimately means CLEAR (see the Stage-0 note: the old
	// dms_assert(!format.empty()) contradicted its own body and made CopyProps abort in
	// Debug whenever the TARGET carried a spatial reference and the SOURCE did not).
	SetCrs(format.empty() ? nullptr : new UnitCrs(format));
}

// ---------------------------------------------------------------------------
// LEGACY 0xFF packing -- everything below is deleted in Stage 7 of
// doc/development/crs-metric-decoupling.md. It was triplicated across the three
// accessors; factored into one place so the eventual deletion is a single edit and
// so the drift detector has exactly one "old channel" to compare against.
//
// A coordinate unit's metric could be a CRS identity tag rather than a dimension: a
// single base unit of power 1 whose symbol is "<SpatialReference>\xFF<DialogData>".
// ---------------------------------------------------------------------------
namespace {

	// Returns the packed symbol and the position of its 0xFF separator, or nullptr.
	auto FindPackedCrsSymbol(const UnitMetric* m) -> std::optional<SharedStr>
	{
		if (!m || m->m_BaseUnits.size() != 1 || m->m_BaseUnits.begin()->second != 1)
			return {};
		const SharedStr& sym = m->m_BaseUnits.begin()->first;
		if (std::find(sym.begin(), sym.send(), char(0xFF)) == sym.send())
			return {};
		return sym;
	}

	auto DecodePackedSpatialRef(const UnitMetric* m) -> TokenID
	{
		auto sym = FindPackedCrsSymbol(m);
		if (!sym)
			return TokenID::GetEmptyID();
		auto sepPos = std::find(sym->begin(), sym->send(), char(0xFF));
		return GetTokenID_mt(sym->begin(), sepPos);
	}

	auto DecodePackedBackgroundRef(const UnitMetric* m) -> SharedStr
	{
		auto sym = FindPackedCrsSymbol(m);
		if (!sym)
			return {};
		auto sepPos = std::find(sym->begin(), sym->send(), char(0xFF));
		return SharedStr(CharPtrRange(sepPos + 1, sym->send()) MG_DEBUG_ALLOCATOR_SRC("DecodePackedBackgroundRef"));
	}

#if defined(MG_DEBUG)
	// Stage-2 drift detector, modelled on the SigUnitChecker metric replay in
	// OperSignature.cpp. While BOTH channels are live, any unit that carries a CRS in its
	// own slot AND a packed one in its metric must agree. This is the cheapest available
	// proof, across the whole regression corpus, that the new channel reproduces the old
	// one BEFORE Stage 7 deletes the old one. Removed together with the packing.
	void CheckCrsChannelDrift(const AbstrUnit* self, TokenID fromSlot, TokenID fromMetric)
	{
		if (fromSlot.empty() || fromMetric.empty() || fromSlot == fromMetric)
			return;
		reportF_without_cancellation_check(SeverityTypeID::ST_Error
			, "CRS channel drift on {}: the m_Crs slot says '{}' but the packed metric says '{}'"
			, self->GetFullName().c_str(), SharedStr(fromSlot).c_str(), SharedStr(fromMetric).c_str());
		assert(!"CRS decoupling: the UnitCrs slot disagrees with the legacy 0xFF-packed metric (see the preceding ST_Error log line)");
	}
#endif

} // anonymous namespace

SharedStr AbstrUnit::GetBackgroundReference() const
{
	auto dd = TreeItem_GetDialogData(this);
	if (not dd.empty())
		return dd;

	// Order: own DialogData -> the legacy packed metric -> the CRS registry.
	//
	// The plan put the registry BEFORE the packing. It is deliberately placed AFTER,
	// which keeps this stage strictly behaviour-preserving: while the packing is still
	// emitted it always answers first, so nothing observable moves. The two differ only
	// where a config declares one CRS with two different backgrounds -- the packing is
	// per-unit and exact, the registry is per-CRS and first-wins -- and there is a real
	// such config (tst/Projects/lus_demo_2023, regression t611). Deferring that change to
	// Stage 7, when the packing goes away and the registry becomes the only source,
	// keeps the switch-over in one place instead of smearing it across stages.
	//
	// The Debug check below still exercises the registry against the packing across the
	// whole corpus now, so it is validated long before it becomes load-bearing. It only
	// WARNS: unlike a CRS mismatch, a background mismatch is cosmetic and the t611
	// divergence above is expected rather than a defect.
	auto fromPacking = DecodePackedBackgroundRef(GetMetric());

#if defined(MG_DEBUG)
	if (auto crs = GetCrs())
	{
		auto fromRegistry = GetCrsBackgroundRef(crs->m_SpatialRef);
		if (!fromPacking.empty() && !fromRegistry.empty() && fromPacking != fromRegistry)
			reportF_without_cancellation_check(SeverityTypeID::ST_Warning
				, "background-reference note on {}: the packed metric says '{}' but the CRS registry says '{}' for '{}'"
				, GetFullName().c_str(), fromPacking.c_str(), fromRegistry.c_str()
				, SharedStr(crs->m_SpatialRef).c_str());
	}
#endif

	if (!fromPacking.empty())
		return fromPacking;

	if (auto crs = GetCrs())
		return GetCrsBackgroundRef(crs->m_SpatialRef);

	return {};
}

TokenID AbstrUnit::GetSpatialReference() const
{
	// Own slot (with referred-item delegation) first; the 0xFF metric decode stays as the
	// LAST resort until the new channel carries everything (Stage 7 deletes it).
	auto crs = GetCrs();

#if defined(MG_DEBUG)
	if (crs)
		CheckCrsChannelDrift(this, crs->m_SpatialRef, DecodePackedSpatialRef(GetMetric()));
#endif

	if (crs)
		return crs->m_SpatialRef;

	return DecodePackedSpatialRef(GetMetric());
}

TokenID AbstrUnit::GetCurrSpatialReference() const
{
	assert(m_State.GetProgress() >= ProgressState::MetaInfo); //UpdateMetaInfo();

	auto crs = GetCurrCrs();

#if defined(MG_DEBUG)
	if (crs)
		CheckCrsChannelDrift(this, crs->m_SpatialRef, DecodePackedSpatialRef(GetCurrMetric()));
#endif

	if (crs)
		return crs->m_SpatialRef;

	return DecodePackedSpatialRef(GetCurrMetric());
}


auto AbstrUnit_GetMetricStr(const AbstrUnit* u, const UnitMetric* m, FormattingFlags ff) -> SharedStr
{
	assert(u);
	auto labelStr = labelPropDefPtr->GetValue(u);

	if (m)
	{
		auto metricStr = m->AsString(ff);
		if (!metricStr.empty())
		{
			if (labelStr.empty())
				return metricStr;
			return mySSPrintF("{}: {}", labelStr, metricStr);
		}
	}
	return labelStr;
}

SharedStr AbstrUnit::GetMetricStr(FormattingFlags ff) const
{
	return AbstrUnit_GetMetricStr(this, GetMetric(), ff);
}

SharedStr AbstrUnit::GetCurrMetricStr(FormattingFlags ff) const
{
	return AbstrUnit_GetMetricStr(this, GetCurrMetric(), ff);
}

SharedStr AbstrUnit::GetFormattedMetricStr () const
{
	SharedStr result = GetMetricStr(FormattingFlags::ThousandSeparator);
	if (!result.empty())
	{
		if (result != "%")
			result = mySSPrintF(" [{}]", result.c_str());
	}
	return result;
}

SharedStr AbstrUnit::GetProjectionStr (FormattingFlags ff) const
{
	const UnitProjection* p = GetProjection();
	if (!p) 
		return SharedStr();
	return p->AsString(ff);
}

TIC_CALL GetUnitlabeledScalePairFuncType s_GetUnitlabeledScalePairFunc = nullptr;

auto AbstrUnit::GetUnitlabeledScalePair() const -> UnitLabelScalePair
{
	if (!s_GetUnitlabeledScalePairFunc)
		return {};
	auto srToken = GetSpatialReference();
	return (*s_GetUnitlabeledScalePairFunc)(srToken);
}

const UnitProjection* AbstrUnit::GetProjection() const
{
	return nullptr;
}

const UnitProjection* AbstrUnit::GetCurrProjection() const
{
	return nullptr;
}


static StaticTokenID s_LabelID("Label"), s_LabelTextID("LabelText");

auto AbstrUnit::GetLabelAttr() const -> SharedDataItemInterestPtr
{

	auto si = GetConstSubTreeItemByID(s_LabelID);
	if (!si) 
		si = GetConstSubTreeItemByID(s_LabelTextID); // compatible with newer aspect names
	if (IsDataItem(si))
	{
		SharedDataItemInterestPtr di = AsDataItem(si);
		if (di->GetAbstrDomainUnit()->UnifyDomain(this, "Domain of attribute named Label", "Unit that has that attribute"))
		{
			di->UpdateMetaInfo();
			return di;
		}
	}
	si = make_shared_tree(GetSourceItem(), existing_obj{});
	if (si)
		return AsUnit(si)->GetLabelAttr();
	return {};
}

auto AbstrUnit::GetCurrLabelAttr() const -> const AbstrDataItem*
{
	const TreeItem* si = const_cast<AbstrUnit*>(this)->GetSubTreeItemByID(s_LabelID);
	if (!si)
		si = const_cast<AbstrUnit*>(this)->GetSubTreeItemByID(s_LabelTextID); // compatible with newer aspect names
	if (IsDataItem(si))
	{
		auto di = AsDataItem(si);
		auto adu = di->GetAbstrDomainUnit();
		if (adu && adu->UnifyDomain(this, "Domain of attribute named Label", "Unit that has that attribute"))
		{
			return di;
		}
	}
	si = this->GetCurrSourceItem();
	if (si)
		return AsUnit(si)->GetCurrLabelAttr();
	return nullptr;
}

SharedStr AbstrUnit::GetLabelAtIndex(SizeT index, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock) const
{
	if (!ipHolder)
	{
		assert(IsMetaThread());
		ipHolder = GetLabelAttr();
	}
	assert(ipHolder == this->GetCurrLabelAttr());
	if (!ipHolder)
		return SharedStr();


#if defined(MG_DEBUG_INTERESTSOURCE)
	DemandManagement::BlockIncInterestDetector allowIncInterestsForLabelAccess; // user must choose label wisely; new interest leaks out of this frame.
#endif //defined(MG_DEBUG_INTERESTSOURCE)
	if (IsMetaThread())
	{
		if (!ipHolder->PrepareDataUsage(DrlType::Certain))
			return SharedStr();
	}
	else
	{
		if (!IsDataReady(ipHolder->GetCurrRangeItem().get()))
			return SharedStr();
	}

	try {
		DataReadLock drl(ipHolder);

		const AbstrDataObject* ado = ipHolder->GetCurrRefObj().get();

		MakeMin(maxLen, ado->AsCharArraySize(index, maxLen, lock, FormattingFlags::ThousandSeparator));
		SharedStr result = SharedStr(SharedArray<char>::Create(maxLen + 1, false MG_DEBUG_ALLOCATOR_SRC("AbstrUnit::GetLabelAtIndex")));
		ado->AsCharArray(index, result.begin(), maxLen, lock, FormattingFlags::ThousandSeparator);
		result.begin()[maxLen] = char(0);
		return result;
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg()->Why();
	}
}

ActorVisitState AbstrUnit::VisitLabelAttr(const ActorVisitor& visitor, SharedDataItemInterestPtr& labelLock) const
{
	if (!labelLock)
		labelLock = GetLabelAttr();
	assert(labelLock == this->GetCurrLabelAttr());
	return visitor.Visit(labelLock.get_ptr());
}

static StaticTokenID s_MissingValueLabelID("MissingValueLabel");

SharedStr AbstrUnit::GetMissingValueLabel() const
{

	const TreeItem* si = GetConstSubTreeItemByID(s_MissingValueLabelID).get();
	if (IsDataItem(si))
	{
		const AbstrDataItem* di = AsDataItem(si);
		if (di->GetAbstrDomainUnit()->IsKindOf( Unit<Void>::GetStaticClass() ) )
		{
			PreparedDataReadLock drl(di, "AbstrUnit::GetMissingValueLabel()");
			TileCRef lock;
			return di->GetRefObj()->AsString(0, lock, FormattingFlags::ThousandSeparator);
		}
	}
	si = GetSourceItem();
	if (si)
		return AsUnit(si)->GetMissingValueLabel();
	return AsString(Undefined());
}

const UnitMetric* AbstrUnit::GetMetric() const
{
	return nullptr;
}

const UnitMetric* AbstrUnit::GetCurrMetric() const
{
	return nullptr;
}

SharedStr AbstrUnit::GetNameOrCurrMetric(FormattingFlags ff) const
{
	if (!IsCacheItem())
		return SharedStr(GetID());
	const UnitMetric* m = GetCurrMetric();
	if (m)
		return m->AsString(ff);
	return SharedStr("x");
}

// should only be called from PrepareData
void AbstrUnit::SetMetric(SharedPtr<const UnitMetric> m)
{
}

void AbstrUnit::SetProjection(SharedPtr<const UnitProjection> p)
{
}

void AbstrUnit::DuplFrom(const AbstrUnit* src)
{
	dms_assert(src);
	if (GetNrDimensions() == 2)
	{
		const UnitProjection*  orgP = src->GetCurrProjection();
		if (!orgP && !src->IsDefaultUnit())
		{
			auto srcUltimateUnit = AsUnit(src->GetCurrUltimateItem());
			MG_CHECK(srcUltimateUnit);
			orgP = new UnitProjection(srcUltimateUnit.get());
		}
		SetProjection(orgP);
	}
	else
		SetMetric(src->GetMetric());

	// A duplicated unit is in the same coordinate reference system as its source. Without
	// this a range()/cat_range() result knew its CRS only if the source's metric happened
	// to carry a packed 0xFF tag -- and for the 2D branch above it never did, because the
	// result is projection-bearing and its own metric is empty. See
	// doc/development/crs-metric-decoupling.md Stage 4.
	SetCrs(src->GetCrs());
}

const UnitClass* AbstrUnit::GetUnitClass() const
{
	const UnitClass* rtc = debug_cast<const UnitClass*>(GetDynamicClass());
	dms_assert(rtc);
	return rtc;
}

const ValueClass* AbstrUnit::GetValueType(ValueComposition vc) const
{
	dms_assert(vc != ValueComposition::Unknown);
	const ValueClass* result = GetUnitClass()->GetValueType(vc);
	MG_CHECK(result);
	return result;
}

void AbstrUnit::CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const
{
	base_type::CopyProps(result, copyContext);

	AbstrUnit* resultUnit = debug_cast<AbstrUnit*>(result);
	// Copy the CRS slot straight across, including the "source has none, clear the target"
	// case that the old flag-pair condition expressed (and that used to trip an assert).
	if (m_Crs || resultUnit->m_Crs)
		resultUnit->SetCrs(m_Crs.get_ptr());
	if (GetTSF(USF_HasConfigRange))
		resultUnit->SetTSF(USF_HasConfigRange);
}

SharedStr AbstrUnit::GetSignature() const
{
	return SharedStr("unit<") + GetValueType()->GetName().c_str() + ">";
}

auto AbstrUnit::GetScriptName(const TreeItem* context) const -> SharedStr
{
	if (IsDefaultUnit() || IsCacheItem() || !GetTreeParent())
		return SharedStr(GetValueType()->GetID());
	return base_type::GetScriptName(context);
}

bool AbstrUnit::DoReadItem(StorageMetaInfoPtr smi)
{
	dms_assert(!IsDisabledStorage());
	dms_assert(IsInWriteLock(this));

	if (!smi->StorageManager()->ReadUnitRange(*smi))
		return false;
	MG_CHECK(HasTiledRangeData() or IsDefaultUnit());
	return true;
}

//----------------------------------------------------------------------
// Illegal Abstract implementation
//----------------------------------------------------------------------

void AbstrUnit::SetCount(SizeT)
{ 
	throwIllegalAbstract(MG_POS, this, "SetCount"); 
}

void AbstrUnit::OnDomainChange(const DomainChangeInfo* info)
{
	UInt32 i = GetNrDataItemsOut();
	dms_assert(i); // PRECONDITION;

	UpdateMarker::ChangeSourceLock lock(this, "OnDomainChange");
	while (i--)
	{
		const AbstrDataItem* adi = GetDataItemOut(i);
		if (adi)
			const_cast<AbstrDataItem*>( adi )->OnDomainUnitRangeChange( info );
	}
}

SizeT AbstrUnit::GetPreparedCount(bool throwOnUndefined) const  // Returns 0 if non-countable unit
{
	return GetCount();
}

SizeT AbstrUnit::GetCount() const  // Returns 0 if non-countable unit
{
	return 0;
}

SizeT AbstrUnit::GetDataCount() const  // Returns 0 if non-countable unit
{
	return GetCount();
}

tile_offset AbstrUnit::GetPreparedTileCount(tile_id t) const  // Returns 0 if non-countable unit
{
	return GetTileCount(t);
}

row_id AbstrUnit::GetBase() const  // Returns 0 if non-countable unit
{
	throwIllegalAbstract(MG_POS, this, "GetBase");
}

bool AbstrUnit::IsOrdinalAndZeroBased() const
{
	return GetNrDimensions() == 1 && GetBase() == 0;
}

const row_id ASSUMED_SIZE = 1000000;

row_id AbstrUnit::GetEstimatedCount() const
{
	if (IsDataReady(this))
		return GetCount();
	if (HasSizeEstimator())
	{
		auto se = GetSizeEstimator();
		assert(se);
		auto dc = CalledCalcHandle(se.get(), AbstrDataItem::GetStaticClass());
		auto fd = dc->CalcCertainResult();
		auto ri = fd->MakeResult();

		if (!ri || !IsDataItem(ri))
			throwDmsErrF("SizeEstimator must define a numeric result, but is defined as {}"
				, se->GetExpr()
				);
		auto ari = AsDataItem(ri);
		if (!ari->HasVoidDomainGuarantee())
			throwDmsErrF("SizeEstimator must define a single result, but is defined as {}"
				, se->GetExpr()
			);
		DataReadLock drl(ari.get());
		auto ado = ari->GetRefObj();
		assert(ado);
		return ado->GetValueAsSizeT(0);
	}
	return ASSUMED_SIZE;
}

void AbstrUnit::ValidateCount(SizeT supposedCount) const
{
	auto range_item = this->GetCurrRangeItem();
	MG_CHECK(range_item);
	auto sm = AsUnit(range_item)->GetTiledRangeData();
	if (!sm)
		throwItemErrorF("ValidateCount({}) failed because this unit has no segment info", supposedCount);

	row_id count = sm->GetElemCount();

	if (supposedCount != count)
		throwItemErrorF("ValidateCount({}) failed because this unit has count {}"
			, supposedCount, count
		);
}


row_id AbstrUnit::GetDimSize(DimType dimNr) const
{
	throwIllegalAbstract(MG_POS, this, "GetDimSize"); 
}

// Support for Ranged Units
void AbstrUnit::SetMaxRange()
{
//	SetDataInMem();
}

tile_id AbstrUnit::GetThisCurrTileID(SizeT& index, tile_id prevT) const
{
	return 0;
}

I64Rect AbstrUnit::GetTileSizeAsI64Rect(tile_id t) const // asssume 1D; GeoUnitAdapter overrules this for all 2D domains
{
	SizeT
		fi = (t == no_tile) ? GetBase () : GetTileFirstIndex(t),
		sz = (t == no_tile) ? GetCount() : GetTiledRangeData()->GetTileSize(t);
	if (!sz)
	{
		dms_assert(!t || t == no_tile);
		dms_assert(fi == 0 || !IsDefined(fi));
		fi = 0;
	}
	return AsI64Rect(Range<SizeT>(fi, fi + sz));
}

row_id  AbstrUnit::GetTileFirstIndex(tile_id t) const
{
	auto range_item = this->GetCurrRangeItem();
	MG_CHECK(range_item);
	auto si = AsUnit(range_item)->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetFirstRowIndex(t);
}

row_id  AbstrUnit::GetTileIndex(tile_id t, tile_offset tileOffset) const 
{ 
	auto range_item = this->GetCurrRangeItem();
	MG_CHECK(range_item);
	auto si = AsUnit(range_item)->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetRowIndex(t, tileOffset);
}

tile_id AbstrUnit::GetNrTiles() const
{
	auto range_item = this->GetCurrRangeItem();
	MG_CHECK(range_item);
	auto si = AsUnit(range_item)->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetNrTiles();
}

tile_offset AbstrUnit::GetTileCount(tile_id t) const
{
	assert(t != no_tile);

	auto range_item = this->GetCurrRangeItem();
	auto si = AsUnit(range_item)->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetTileSize(t);
}

bool AbstrUnit::ContainsUndefined(tile_id t) const
{
	throwIllegalAbstract(MG_POS, "ContainsUndefined");
}

bool AbstrUnit::IsCovered() const
{
	auto range_item = this->GetCurrRangeItem();
	auto si = AsUnit(range_item)->GetTiledRangeData();
	MG_CHECK(si);
	return si->IsCovered();
}

Range<row_id> AbstrUnit::GetTileIndexRange(tile_id t) const
{
	MG_CHECK(GetNrDimensions() == 1);
	row_id firstIndex = GetTileFirstIndex(t);
	return { firstIndex, firstIndex + GetTileCount(t) };
}

void CheckNrTiles(SizeT nrTiles)
{
	if (nrTiles > MAX_VALUE(tile_id))
		throwErrorF("Tiles", "The requested number of {} tiles exceeds the GeoDms limit of {}", 
			nrTiles,
			MAX_NR_TILES
		); 
}

auto AbstrUnit::CreateAbstrValueAtIndex(SizeT i) const -> std::unique_ptr<AbstrValue>
{
	throwIllegalAbstract(MG_POS, this, "CreateAbstrValueAtIndex"); 
}

SizeT AbstrUnit::GetIndexForAbstrValue(const AbstrValue&) const
{
	throwIllegalAbstract(MG_POS, this, "GetIndexForAbstrValue"); 
}

// Support for Numerics
void AbstrUnit::SetRangeAsFloat64(Float64 begin, Float64 end)
{
	throwIllegalAbstract(MG_POS, this, "SetRangeAsFloat64"); 
}

void AbstrUnit::SetRangeAsUInt64(UInt64 begin, UInt64 end)
{
	throwIllegalAbstract(MG_POS, this, "SetRangeAsUInt64");
}

Range<Float64> AbstrUnit::GetRangeAsFloat64() const
{ 
	throwIllegalAbstract(MG_POS, this, "GetRangeAsFloat64");
}

Range<Float64> AbstrUnit::GetTileRangeAsFloat64(tile_id t) const
{
	throwIllegalAbstract(MG_POS, this, "GetTileRangeAsFloat64");
}

// Support for Geometrics
IRect AbstrUnit::GetRangeAsIRect() const
{
	throwIllegalAbstract(MG_POS, this, "GetRangeAsIRect"); 
}

IRect AbstrUnit::GetTileRangeAsIRect(tile_id) const
{
	throwIllegalAbstract(MG_POS, this, "GetTileRangeAsIRect");
}

void AbstrUnit::SetRangeAsIPoint(Int32  rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd, UInt16 blockSizeY, UInt16 blockSizeX)
{
	throwIllegalAbstract(MG_POS, this, "SetRangeAsIPoint"); 
}

DRect AbstrUnit::GetRangeAsDRect() const
{
	throwIllegalAbstract(MG_POS, this, "GetRangeAsDRect"); 
}

DRect AbstrUnit::GetTileRangeAsDRect(tile_id t) const
{
	throwIllegalAbstract(MG_POS, this, "GetRangeAsDRect");
}

void AbstrUnit::SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd )
{
	throwIllegalAbstract(MG_POS, this, "SetRangeAsDPoint"); 
}


void AbstrUnit::SetRangeAsDRect(const DRect& rect)
{
	SetRangeAsDPoint(rect.first.Row(), rect.first.Col(), rect.second.Row(), rect.second.Col());
}


SharedStr AbstrUnit::GetRangeAsStr(FormattingFlags ff) const
{
	throwIllegalAbstract(MG_POS, this, "GetRangeAsStr"); 
}

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------

IMPL_CLASS(AbstrUnit, nullptr)

//----------------------------------------------------------------------
// Dumping to Xml_OutStream
//----------------------------------------------------------------------
#include "RtcInterface.h"

#include "xml/XmlOut.h"
#include "AbstrDataItem.h"
#include "TicPropDefConst.h"

#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"

namespace {
class SpatialReferencePropDef : public PropDef<AbstrUnit, TokenID>
{
public:
	SpatialReferencePropDef()
		: PropDef<AbstrUnit, TokenID>(SR_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, chg_mode::none, false, true, false)
	{}
	// override base class
	ApiType GetValue(const AbstrUnit* item) const override { return item->GetSpatialReference(); }
	void SetValue(AbstrUnit* item, ParamType val) override { item->SetSpatialReference(val); }
};

struct MetricPropDef : ReadOnlyPropDef<AbstrUnit, SharedStr>
{
	using typename ReadOnlyPropDef::ApiType;

	MetricPropDef()
		:	ReadOnlyPropDef<AbstrUnit, SharedStr>(METRIC_NAME)
	{}
//	override base class
	ApiType GetValue(const AbstrUnit* item) const override { return item->GetMetricStr(FormattingFlags::None); }
};

struct ProjectionPropDef : ReadOnlyPropDef<AbstrUnit, SharedStr>
{
	ProjectionPropDef()
		:	ReadOnlyPropDef<AbstrUnit, SharedStr>(PROJECTION_NAME)
	{}
	// override base class
	ApiType GetValue(const AbstrUnit* item) const override{ return item->GetProjectionStr(FormattingFlags::None); }
};

struct ValueTypePropDef : ReadOnlyPropDef<AbstrUnit, TokenID>
{
	ValueTypePropDef()
		:	ReadOnlyPropDef<AbstrUnit, TokenID>(VALUETYPE_NAME, set_mode::construction, xml_mode::signature)
	{}
	// override base class
	ApiType GetValue(const AbstrUnit* item) const override{ return item->GetValueType()->GetID();	}
};

SpatialReferencePropDef srPropDef;
MetricPropDef metricPropDef;
ProjectionPropDef projectionPropDef;
ValueTypePropDef valueTypePropDef;

} // end anonymous namespace

//----------------------------------------------------------------------
// C style Interface functions for Metadata retrieval
//----------------------------------------------------------------------

#include "TicInterface.h"

TIC_CALL SizeT DMS_CONV DMS_Unit_GetCount(const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_Unit_GetCount");

		SharedTreeItemInterestPtr ptr(self);
		self->PrepareDataUsage(DrlType::Certain);
		return self->GetCount();

	DMS_CALL_END
	return 0;
}

TIC_CALL const AbstrDataItem* DMS_CONV DMS_Unit_GetLabelAttr(const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_Unit_GetLabelAttr");
		return self->GetLabelAttr();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const Class* DMS_CONV DMS_AbstrUnit_GetStaticClass()
{
	return AbstrUnit::GetStaticClass();
}

TIC_CALL const ValueClass* DMS_CONV DMS_Unit_GetValueType(const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_Unit_GetValueType");
		return self->GetValueType();

	DMS_CALL_END
	return nullptr;
}

TIC_CALL ValueClassID DMS_CONV DMS_Unit_GetValueTypeID(const AbstrUnit* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, AbstrUnit::GetStaticClass(), "DMS_Unit_GetValueTypeID");
		return self->GetValueType()->GetValueClassID();

	DMS_CALL_END
	return ValueClassID::VT_Unknown;
}



