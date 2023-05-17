#include "TicPCH.h"
#pragma hdrstop

#include "AbstrUnit.h"

#include "act/ActorVisitor.h"
#include "act/UpdateMark.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "geo/PointOrder.h"
#include "mci/ValueClass.h"
#include "set/StaticQuickAssoc.h"
#include "set/VectorFunc.h"
#include "utl/mySPrintF.h"
#include "xct/DmsException.h"

#include "LockLevels.h"
#include "LispList.h"

#include "AbstrCalculator.h"
#include "DataLocks.h"
#include "DataStoreManagerCaller.h"
#include "Metric.h"
#include "Projection.h"
#include "TiledUnit.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"
#include "Unit.h"
#include "UnitClass.h"

#include "stg/AbstrStorageManager.h"

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
		dms_assert(!size());
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

namespace {
	static_quick_assoc<const AbstrUnit*, TokenID> s_SpatialReferenceAssoc;
}
//----------------------------------------------------------------------
// class  : AbstrUnit 
//----------------------------------------------------------------------

AbstrUnit::AbstrUnit() {}  // ctor calls for ~OwningPtr<DataItemsAssocPair> in case of exception

// DataItemsOut
AbstrUnit::~AbstrUnit() 
{
	if (GetTSF(USF_HasSpatialReference))
		s_SpatialReferenceAssoc.eraseExisting(this);
}

inline DataItemRefContainer& AbstrUnit::GetDataItemsAssoc() const
{
	leveled_std_section::scoped_lock lock(s_DataItemRefContainer);
	if (!HasDataItemsAssoc())
		m_DataItemsAssocPtr.assign(new DataItemRefContainer);
	return *m_DataItemsAssocPtr;
}

const AbstrTileRangeData* AbstrUnit::GetTiledRangeData() const
{
	return nullptr;
}

UInt32 AbstrUnit::GetNrDataItemsOut() const
{
	if (HasDataItemsAssoc())
	{
		DataItemRefContainer* rc = m_DataItemsAssocPtr;
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
	DataItemRefContainer* rc = m_DataItemsAssocPtr;
//	dms_assert(rc); // once added, it must have an assoc
	if (rc)
		rc->Del(item);
}

#if defined(MG_DEBUG)
bool AbstrUnit::HasDataItemOut(const AbstrDataItem* item) const
{
	DataItemRefContainer* rc = m_DataItemsAssocPtr;
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

	SharedStr msg = mgFormat2SharedStr("%s mismatch between %s%s (%s %s: %s) and %s%s (%s %s: %s)%s"
		,	isDomain ? "Domain" : "Values"
		,	leftPair.first, leftPair.second, 	GetFullName(),     GetProjMetrString(),     GetValueType()->GetName()
		,	rightPair.first, rightPair.second, cu->GetFullName(), cu->GetProjMetrString(), cu->GetValueType()->GetName()
		,	reason 
		);

	if (um & UM_Throw)
		throwItemError(msg);

	dms_assert(resultMsg);
	*resultMsg = msg;
}

bool AbstrUnit::DoWriteItem(StorageMetaInfoPtr&& smi) const
{
	dms_assert( GetInterestCount() );

	AbstrStorageManager* sm = smi->StorageManager();

	reportF(SeverityTypeID::ST_MajorTrace, "%s IS PROVIDED TO %s",
		GetSourceName().c_str()
	,	sm->GetNameStr().c_str()
	);	

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

	if (GetCurrUltimateItem() != cu->GetCurrUltimateItem())
	{
		SharedTreeItem thisRepresentation = this; 
		if (!this->IsCacheItem())
		{
			auto thisDC = GetOrCreateDataController(this->GetCheckedKeyExpr());
			if (!thisDC)
				goto error;
			thisRepresentation = thisDC->MakeResult();
		}
		{
			SharedTreeItem thatRepresentation = cu;
			if (!cu->IsCacheItem())
			{
				auto thatDC = GetOrCreateDataController(cu->GetCheckedKeyExpr());
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
	// TODO G8: dms_assert(Was(PS_MetaInfo)); dms_assert(cu->Was(PS_MetaInfo));
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

	if (*GetCurrMetric() != *cu->GetCurrMetric())
	{
		UnifyError(cu, " (incompatible Metrics)", leftRole, rightRole, um, resultMsg, false);
		return false;
	}

	// Projection unification
	if (*GetCurrProjection() != *cu->GetCurrProjection())
		// TODO: specific error, generalise Metric & Projection
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

void AbstrUnit::SetSpatialReference(TokenID format)
{
	dms_assert(!format.empty());
	SetTSF(
		USF_HasSpatialReference,
		s_SpatialReferenceAssoc.assoc(this, format)
	);
}

TokenID AbstrUnit::GetSpatialReference() const
{
	if (GetTSF(USF_HasSpatialReference))
		return s_SpatialReferenceAssoc.GetExisting(this);
	auto m = GetMetric();
	if (m && m->m_BaseUnits.size() == 1 && m->m_BaseUnits.begin()->second == 1)
		return TokenID(m->m_BaseUnits.begin()->first);
	return TokenID::GetEmptyID();
}

TokenID AbstrUnit::GetCurrSpatialReference() const
{
	assert(m_State.GetProgress() >= PS_MetaInfo); //UpdateMetaInfo();

	if (GetTSF(USF_HasSpatialReference))
		return s_SpatialReferenceAssoc.GetExisting(this);
	auto m = GetCurrMetric();
	if (m && m->m_BaseUnits.size() == 1 && m->m_BaseUnits.begin()->second == 1)
		return TokenID(m->m_BaseUnits.begin()->first);
	return TokenID::GetEmptyID();
}

SharedStr AbstrUnit::GetMetricStr(FormattingFlags ff) const
{
	auto m = GetMetric();
	if (m) 
		return m->AsString(ff);
	return SharedStr();
}

SharedStr AbstrUnit::GetCurrMetricStr(FormattingFlags ff) const
{
	const UnitMetric* m = GetCurrMetric();
	if (m)
		return m->AsString(ff);
	return SharedStr();
}

SharedStr AbstrUnit::GetFormattedMetricStr () const
{
	SharedStr result = GetMetricStr(FormattingFlags::ThousandSeparator);
	if (!result.empty())
	{
		if (result != "%")
			result = mySSPrintF(" [%s]", result.c_str());
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

const UnitProjection* AbstrUnit::GetProjection() const
{
	return nullptr;
}

const UnitProjection* AbstrUnit::GetCurrProjection() const
{
	return nullptr;
}


static TokenID s_LabelID = GetTokenID_st("Label"), s_LabelTextID = GetTokenID_st("LabelText");

SharedDataItemInterestPtr AbstrUnit::GetLabelAttr() const
{
	dms_assert(this);

	const TreeItem* si = GetConstSubTreeItemByID(s_LabelID);
	if (!si) 
		si = GetConstSubTreeItemByID(s_LabelTextID); // compatible with newer aspect names
	if (IsDataItem(si))
	{
		SharedDataItemInterestPtr di = MakeShared( AsDataItem(si) );
		if (di->GetAbstrDomainUnit()->UnifyDomain(this, "Domain of attribute named Label", "Unit that has that attribute"))
		{
			di->UpdateMetaInfo();
			return di;
		}
	}
	si = GetSourceItem();
	if (si)
		return AsUnit(si)->GetLabelAttr();
	return {};
}

const AbstrDataItem* GetCurrLabelAttr(const AbstrUnit* au)
{
	dms_assert(au);

	const TreeItem* si = const_cast<AbstrUnit*>(au)->GetSubTreeItemByID(s_LabelID);
	if (!si)
		si = const_cast<AbstrUnit*>(au)->GetSubTreeItemByID(s_LabelTextID); // compatible with newer aspect names
	if (IsDataItem(si))
	{
		auto di = AsDataItem(si);
		if (di->GetAbstrDomainUnit()->UnifyDomain(au, "Domain of attribute named Label", "Unit that has that attribute"))
		{
			return di;
		}
	}
	si = au->GetCurrSourceItem();
	if (si)
		return GetCurrLabelAttr(AsUnit(si));
	return nullptr;
}

SharedStr AbstrUnit::GetLabelAtIndex(SizeT index, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock) const
{
	if (!ipHolder)
		ipHolder = GetLabelAttr();
	dms_assert(ipHolder == GetCurrLabelAttr(this));
	if (!ipHolder)
		return SharedStr();


#if defined(MG_DEBUG_INTERESTSOURCE)
	DemandManagement::BlockIncInterestDetector allowIncInterestsForLabelAccess; // user must choose label wisely; new interest leaks out of this frame.
#endif //defined(MG_DEBUG_INTERESTSOURCE)
	if (!ipHolder->PrepareData())
		return SharedStr();

	try {
	DataReadLock drl(ipHolder);

	const AbstrDataObject* ado = ipHolder->GetCurrRefObj();

	MakeMin(maxLen, ado->AsCharArraySize(index, maxLen, lock, FormattingFlags::ThousandSeparator));
	SharedStr result = SharedStr(SharedArray<char>::Create(maxLen + 1, false));
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
	dms_assert(labelLock == GetCurrLabelAttr(this));
	return visitor.Visit(labelLock.get_ptr());
}

static TokenID s_MissingValueLabelID = GetTokenID_st("MissingValueLabel");

SharedStr AbstrUnit::GetMissingValueLabel() const
{
	dms_assert(this);

	const TreeItem* si = GetConstSubTreeItemByID(s_MissingValueLabelID);
	if (IsDataItem(si))
	{
		const AbstrDataItem* di = AsDataItem(si);
		if (di->GetAbstrDomainUnit()->IsKindOf( Unit<Void>::GetStaticClass() ) )
		{
			PreparedDataReadLock drl(di);
			TileCRef lock;
			return di->GetRefObj()->AsString(0, lock);
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
			orgP = new UnitProjection(AsUnit(src->GetCurrUltimateItem()));
		SetProjection(orgP);
	}
	else
		SetMetric(src->GetMetric());
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
	if (GetTSF(USF_HasSpatialReference) || resultUnit->GetTSF(USF_HasSpatialReference))
		resultUnit->SetSpatialReference(GetSpatialReference());
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

bool AbstrUnit::DoReadItem(StorageMetaInfo* smi)
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

void AbstrUnit::Split(SizeT pos, SizeT len)
{
	throwIllegalAbstract(MG_POS, this, "Split"); 
}

void AbstrUnit::Merge(SizeT pos, SizeT len)
{
	throwIllegalAbstract(MG_POS, this, "Merge"); 
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

bool AbstrUnit::PrepareRange() const  // Returns 0 if non-countable unit
{
	return true;
}

SizeT AbstrUnit::GetCount() const  // Returns 0 if non-countable unit
{
	return 0;
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

row_id AbstrUnit::GetEstimatedCount() const
{
	return GetCount();
}

void AbstrUnit::ValidateCount(SizeT supposedCount) const
{
	row_id count = GetCount();
	if (supposedCount != count)
		throwItemErrorF("ValidateCount(%d) failed because this unit has count %d"
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
	auto si = AsUnit(this->GetCurrRangeItem())->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetFirstRowIndex(t);
}

row_id  AbstrUnit::GetTileIndex(tile_id t, tile_offset tileOffset) const 
{ 
	auto si = AsUnit(this->GetCurrRangeItem())->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetRowIndex(t, tileOffset);
}

tile_id AbstrUnit::GetNrTiles() const
{
	auto si = AsUnit(this->GetCurrRangeItem())->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetNrTiles();
}

tile_offset AbstrUnit::GetTileCount(tile_id t) const
{
	dms_assert(t != no_tile);
	auto si = this->GetTiledRangeData();
	MG_CHECK(si);
	return si->GetTileSize(t);
}

bool AbstrUnit::ContainsUndefined(tile_id t) const
{
	throwIllegalAbstract(MG_POS, "ContainsUndefined");
}

bool AbstrUnit::IsCovered() const
{
	return GetTiledRangeData()->IsCovered();
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
		throwErrorF("Tiles", "The requested number of %u tiles exceeds the GeoDms limit of %u", 
			nrTiles,
			MAX_NR_TILES
		); 
}

AbstrValue* AbstrUnit::CreateAbstrValueAtIndex(SizeT i) const
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

void AbstrUnit::SetRangeAsIPoint(Int32  rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd)
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


void AbstrUnit::SetRangeAsIRect(const IRect& rect)
{
	SetRangeAsIPoint(rect.first.Row(), rect.first.Col(), rect.second.Row(), rect.second.Col());
}

void AbstrUnit::SetRangeAsDRect(const DRect& rect)
{
	SetRangeAsDPoint(rect.first.Row(), rect.first.Col(), rect.second.Row(), rect.second.Col());
}


SharedStr AbstrUnit::GetRangeAsStr() const
{
	throwIllegalAbstract(MG_POS, this, "GetRangeAsStr"); 
}

//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------

IMPL_CLASS(AbstrUnit, 0)

//----------------------------------------------------------------------
// Dumping to Xml_OutStream
//----------------------------------------------------------------------

#include "xml/XmlOut.h"
#include "AbstrDataItem.h"
#include "TicPropDefConst.h"

#include "mci/PropDef.h"
#include "mci/PropDefEnums.h"

namespace {
class FormatPropDef : public PropDef<AbstrUnit, TokenID>
{
  public:
	FormatPropDef()
		:	PropDef<AbstrUnit, TokenID>(FORMAT_NAME, set_mode::optional, xml_mode::none, cpy_mode::none, chg_mode::none, false, true, false)
	{
		SetDepreciated();
	}

	// override base class
	ApiType GetValue(const AbstrUnit* item) const override { return item->GetSpatialReference(); }
	void SetValue(AbstrUnit* item, ParamType val) override
	{ 
		auto fullName = SharedStr(item->GetFullName());
		reportF(SeverityTypeID::ST_Warning, "%s: depreciated specification of the format property: use SpatialReference=\"%s\""
			, fullName
			, val
		);
		item->SetSpatialReference(val); 
	}
};

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

FormatPropDef formatPropDef;
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



