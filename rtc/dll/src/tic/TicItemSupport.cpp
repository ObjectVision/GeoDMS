// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// Item-layer support satellites of tic, merged from six small TUs (2026-08):
// Aspect, Crs, Param, DisplayValue, TreeItemUtils and ExtLockMgr (the
// DMS_TreeItem_AddRef/Release C-API).

// ==== Aspect ====

#include "Aspect.h"

//----------------------------------------------------------------------
// enum   : AspectID
//----------------------------------------------------------------------

//NO LONGER SUPPORTED: 'RotationPalette', VT_FLOAT64


AspectData AspectArrayData[] = 
{

	{ "Feature",            AR_AttrOnly, AT_Feature,  AG_Other,  UNDEFINED_VALUE(TokenID)      },

	{ "OrderBy",            AR_AttrOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)      },

	{ "LabelColor",         AR_Any,      AT_Color,    AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelBackColor",     AR_Any,      AT_Color,    AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelText",          AR_AttrOnly, AT_Text,     AG_Label,  GetTokenID_st("Labels")          },
	{ "LabelSize",          AR_Any,      AT_Numeric,  AG_Label,  GetTokenID_st("SizePalette" )    },
	{ "LabelWorldSize",     AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelFont",          AR_Any,      AT_Text,     AG_Label,  GetTokenID_st("FontPalette" )    }, // oldName not for Symbols
	{ "LabelAngle",         AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },
	{ "LabelTransparency",  AR_Any,      AT_Numeric,  AG_Label,  UNDEFINED_VALUE(TokenID)      },

	{ "SymbolColor",        AR_Any,      AT_Color,    AG_Symbol,  GetTokenID_st("Palette")         }, // oldName only for Points
	{ "SymbolIndex",        AR_Any,      AT_Ordinal,  AG_Symbol,  GetTokenID_st("CharacterIndexPalette") },
	{ "SymbolSize",         AR_Any,      AT_Numeric,  AG_Symbol,  GetTokenID_st("SizePalette" )    },
	{ "SymbolWorldSize",    AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },
	{ "SymbolFont",         AR_Any,      AT_Text,     AG_Symbol,  GetTokenID_st("FontPalette" )    }, // oldName only for Points
	{ "SymbolAngle",        AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },
	{ "SymbolTransparency", AR_Any,      AT_Numeric,  AG_Symbol,  UNDEFINED_VALUE(TokenID)      },

	{ "PenColor",           AR_Any,      AT_Color,    AG_Pen,     GetTokenID_st("Palette")         }, // oldName only for Arcs
	{ "PenStyle",           AR_Any,      AT_Ordinal,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenWidth",           AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenWorldWidth",      AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },
	{ "PenTransparency",    AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },

	{ "NetworkF1",          AR_AttrOnly, AT_Cardinal, AG_Network, UNDEFINED_VALUE(TokenID)      },
	{ "NetworkF2",          AR_AttrOnly, AT_Cardinal, AG_Network, UNDEFINED_VALUE(TokenID)      },

	{ "BrushColor",         AR_Any,      AT_Color,    AG_Brush,   GetTokenID_st("Palette")         },  // oldName only for polygons & grids
	{ "HatchStyle",         AR_Any,      AT_Ordinal,  AG_Brush,   UNDEFINED_VALUE(TokenID)      },
	{ "BrushTransparency",  AR_Any,      AT_Numeric,  AG_Pen,     UNDEFINED_VALUE(TokenID)      },

	{ "MinPixSize",     AR_ParamOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)          },
	{ "MaxPixSize",     AR_ParamOnly, AT_Numeric,  AG_Other,  UNDEFINED_VALUE(TokenID)          },

	{ "Selected",       AR_AttrOnly,  AT_Cardinal, AG_None,   UNDEFINED_VALUE(TokenID)          },

	{ "Unknown", {}, {}, {}, {} } // AspectThemeNr[AN_AspectCount]
};
AspectData* AspectArray = AspectArrayData;

//----------------------------------------------------------------------
// Support func
//----------------------------------------------------------------------

TokenID g_AspectIds[AN_AspectCount];

TokenID GetAspectNameID(AspectNr aNr)
{
	static_assert(AN_AspectCount <= 32);
	dms_assert(AspectArray);
	dms_assert( UInt32(aNr) < AN_AspectCount );
	if (! g_AspectIds[aNr] )
		g_AspectIds[aNr] = GetTokenID_mt(AspectArray[aNr].name);
	return g_AspectIds[aNr];
}

static StaticTokenID stPalette("Palette");

bool IsColorAspectNameID(TokenID id)
{
	if (id == TokenID())
		return false;
	if (id == GetAspectNameID(AN_BrushColor) 
	||  id == GetAspectNameID(AN_SymbolColor)
	||	id == GetAspectNameID(AN_PenColor  )
	||	id == GetAspectNameID(AN_LabelTextColor)
	||	id == GetAspectNameID(AN_LabelBackColor))
		return true;

	// support for Obsolete stuff
	return id == stPalette;
}



// ==== Crs ====

#include "Crs.h"

#include "dbg/Diagnostics.h"
#include "ser/FormattedStream.h"

#include <map>
#include <mutex>

// *****************************************************************************
// UnitCrs -- see Crs.h and doc/development/crs-metric-decoupling.md
// *****************************************************************************

SharedStr UnitCrs::AsString(FormattingFlags /*ff*/) const
{
	// The spatial reference is an opaque authority string ("EPSG:28992") or WKT as read
	// by GDAL; there is nothing to format, and it must NOT be normalised -- token
	// identity is what makes two separately declared same-CRS units unify.
	return SharedStr(m_SpatialRef);
}

bool AreEqual(const UnitCrs* lhs, const UnitCrs* rhs)
{
	if (lhs == rhs)
		return true;
	bool lhsEmpty = IsEmpty(lhs);
	bool rhsEmpty = IsEmpty(rhs);
	if (lhsEmpty || rhsEmpty)
		return lhsEmpty && rhsEmpty;
	return lhs->m_SpatialRef == rhs->m_SpatialRef;
}

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitCrs& repr)
{
	str << repr.m_SpatialRef;
	return str;
}

// *****************************************************************************
// Background-reference registry -- see Crs.h
// *****************************************************************************

namespace {
	// Written from the meta thread while building key expressions, but read from any
	// thread that renders a map layer, so it carries its own mutex. (The side table this
	// project replaces had none, which is one reason it could never hold a property that
	// operators must write.)
	std::mutex                   s_CrsBackgroundMutex;
	std::map<TokenID, SharedStr> s_CrsBackgroundRefs;
}

void RegisterCrsBackgroundRef(TokenID spatialRef, const SharedStr& backgroundRef)
{
	if (spatialRef.empty() || backgroundRef.empty())
		return;

	SharedStr kept;
	{
		auto lock = std::scoped_lock(s_CrsBackgroundMutex);
		auto [it, inserted] = s_CrsBackgroundRefs.try_emplace(spatialRef, backgroundRef);
		if (inserted || it->second == backgroundRef)
			return;
		kept = it->second; // first non-empty wins
	}
	// Report OUTSIDE the lock, and without the cancellation check: reportF calls
	// ASyncContinueCheck, which THROWS when a GUI cancel is pending -- from here that
	// would abort an unrelated config load.
	reportF_without_cancellation_check(SeverityTypeID::ST_Warning
		, "SpatialReference '{}' is declared with two different background layers: keeping '{}', ignoring '{}'. "
		  "The background hint is not part of a unit's type, so units sharing a CRS share one background."
		, SharedStr(spatialRef).c_str(), kept.c_str(), backgroundRef.c_str());
}

SharedStr GetCrsBackgroundRef(TokenID spatialRef)
{
	if (spatialRef.empty())
		return {};
	auto lock = std::scoped_lock(s_CrsBackgroundMutex);
	auto it = s_CrsBackgroundRefs.find(spatialRef);
	return (it == s_CrsBackgroundRefs.end()) ? SharedStr() : it->second;
}

void ClearCrsBackgroundRefs()
{
	auto lock = std::scoped_lock(s_CrsBackgroundMutex);
	s_CrsBackgroundRefs.clear();
}


// ==== Param ====

#include "Param.h"

#include "act/InterestRetainContext.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "vt/StringBounds.h"
#include "ser/SequenceArrayStream.h"

#include "AbstrDataItem.h"
#include "Unit.h"
#include "UnitClass.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

#include "TicInterface.h"
#include "DataArrayValue.h"

TIC_CALL Float64 DMS_CONV DMS_NumericParam_GetValueAsFloat64(const AbstrParam* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle context(self, AbstrDataItem::GetStaticClass(), "DMS_NumericParam_GetValueAsFloat64");

		PreparedDataReadLock dlr(self, "DMS_NumericParam_GetValueAsFloat64");

		dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
		return self->GetDataObj()->GetValueAsFloat64(0);

	DMS_CALL_END
	return UNDEFINED_VALUE(Float64);
}

TIC_CALL void DMS_CONV DMS_NumericParam_SetValueAsFloat64(AbstrParam* self, Float64 value)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle context(self, AbstrDataItem::GetStaticClass(), "DMS_NumericParam_SetValueAsFloat64");

		DataWriteLock lock(self);

		dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
		lock->SetValueAsFloat64(0, value);
		lock.Commit();

	DMS_CALL_END
}

Int32 NumericParam_GetValueAsInt32(const AbstrParam* self)
{
	FencedInterestRetainContext irc("NumericParam_GetValueAsInt32");
	irc.Add(self);
	PreparedDataReadLock dlr(self, "NumericParam_GetValueAsInt32");

	dms_assert(self->GetAbstrDomainUnit()->GetCount() == 1);
	return self->GetRefObj()->GetValueAsInt32(0);
}

TIC_CALL void DMS_CONV DMS_StringParam_SetValue(AbstrParam* self, CharPtr value)
{
	DMS_CALL_BEGIN

		CheckPtr(self, DataArray<SharedStr>::GetStaticClass(), "DMS_StringParam_SetValue");
		self->SetTSF(TSF_HasConfigData);
		SetTheValue<SharedStr>(self, SharedStr(value MG_DEBUG_ALLOCATOR_SRC("DMS_StringParam_SetValue")));
		return;

	DMS_CALL_END
	return;
}


// ==== DisplayValue ====

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DisplayValue.h"

#include "dbg/DmsCatch.h"
#include "mci/AbstrValue.h"
#include "mci/ValueWrap.h"
#include "geom/Point.h"
#include "utl/StrFormat.h"

#include "DataLocks.h"
#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "TreeItemContextHandle.h"
#include "TreeItemClass.h"

class AbstrDataItem;
class AbstrUnit;

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

SharedStr AsStrWithLabel(const AbstrUnit* au, const AbstrValue* valuePtr, bool useMetric, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock, WeakStr result)
{
	SharedUnitInterestPtr uip = { au, existing_obj{} }; // covered by ipHolder soon.
	if (!ipHolder)
	{
		assert(IsMetaThread());
		au->PrepareDataUsage(DrlType::Suspendible);
	}

	SizeT i = au->GetIndexForAbstrValue(*valuePtr);
	if (!IsDefined(i))
		return result;
	return mySSPrintF("{} ({})",
		result.c_str(),
		(i < au->GetCount())
			? au->GetLabelAtIndex(i, ipHolder, maxLen, lock).c_str()
			: "<range-error>"
	);
}

SharedStr DisplayValue(const AbstrDataItem* adi, SizeT index, bool useMetric, SharedDataItemInterestPtrTuple& ippHolders, streamsize_t maxLen, GuiReadLockPair& locks)
{
	ippHolders.m_ThemeAttr = adi;
	if (adi->PrepareDataUsage(DrlType::Suspendible) && IsDataReady(adi->GetUltimateItem().get()))
	{
		assert(!adi->IsFailed(FailType::Data));
		SharedStr result;
		try {
			DataReadLock drl(adi);

			streamsize_t valueLen = drl->AsCharArraySize(index, maxLen, locks.first, FormattingFlags::ThousandSeparator);
			dms_assert(valueLen <= maxLen);

			result = SharedArray<char>::Create(valueLen + 1, false MG_DEBUG_ALLOCATOR_SRC("DisplayValue"));
			drl->AsCharArray(index, result.begin(), valueLen, locks.first, FormattingFlags::ThousandSeparator);
			result.begin()[valueLen] = char(0);
			if (valueLen < maxLen && adi->GetValueComposition() == ValueComposition::Single)
			{
				maxLen -= valueLen;

				auto avu = adi->GetAbstrValuesUnit();
				SharedDataItemInterestPtr labelAttr = avu->GetLabelAttr();
				if (labelAttr)
				{
					auto valuePtr = drl->CreateAbstrValue();
					drl->GetAbstrValue(index, *valuePtr);
					return AsStrWithLabel(avu, valuePtr.get(), useMetric, ippHolders.m_valuesLabel, maxLen, locks.second, result);
				}
				if (useMetric)
					return result + avu->GetFormattedMetricStr();
			}
		}
		catch (...)
		{
			if (adi->IsFailed(FailType::Data))
				goto returnFail;
			throw;
		}
		return result;
	}
	if (!adi->IsFailed(FailType::Data))
	{
		static SharedStr pendingStr("...");
		return pendingStr;
	}
returnFail:
	auto fr = adi->GetFailReason();
	if (!fr)
		return {};
	return fr->Why();
}

SharedStr DisplayValue(const AbstrUnit* au, const AbstrValue* valuePtr, bool useMetric, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock)
{
	assert(valuePtr);
	assert(au);

	if (valuePtr->IsNull()) 
		return au->GetMissingValueLabel();

	SharedStr result;
	streamsize_t valueLen = valuePtr->AsCharArraySize(maxLen, FormattingFlags::ThousandSeparator);
	assert(valueLen <= maxLen);

	result = SharedArray<char>::Create(valueLen + 1, false MG_DEBUG_ALLOCATOR_SRC("DisplayValue"));
	valuePtr->AsCharArray(result.begin(), valueLen, FormattingFlags::ThousandSeparator);
	result.begin()[valueLen] = char(0);

	if (maxLen > valueLen)
	{
		maxLen -= valueLen;

		if (!dynamic_cast<const ValueWrap<SharedStr>*>(valuePtr)) // used to indicate error or strings
		{
			if (!ipHolder)
				ipHolder = au->GetCurrLabelAttr();
			if (ipHolder)
				return AsStrWithLabel(au, valuePtr, useMetric, ipHolder, maxLen, lock, result);
		}
		if (useMetric)
			return result + au->GetFormattedMetricStr();
	}
	return result;
}

SharedStr DisplayValue(const AbstrUnit* au, SizeT index, bool useMetric, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock)
{
	auto valuePtr = au->CreateAbstrValueAtIndex(index);
	return DisplayValue(au, valuePtr.get(), useMetric, ipHolder, maxLen, lock);
}

extern "C" TIC_CALL CharPtr DMS_CONV DMS_AbstrDataItem_DisplayValue(const AbstrDataItem* adi, UInt32 index, bool useMetric)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(adi, AbstrDataItem::GetStaticClass(), "DMS_AbstrDataItem_DisplayValue");

		static SharedStr resultBuffer;
		GuiReadLockPair locks;

		SharedDataItemInterestPtrTuple tmpInterestHolders;
		resultBuffer = DisplayValue(adi, index, useMetric, tmpInterestHolders, -1, locks);
		return resultBuffer.c_str();
		
	DMS_CALL_END
	return "<err>";
}


// ==== TreeItemUtils ====

// Assorted TreeItem helper functions: naming and path resolution, and
// state-change notification support.

#include "TreeItemUtils.h"

#include "act/TriggerOperator.h" // SuspendTrigger
#include "utl/StrFormat.h"
#include "utl/splitPath.h"

#include "AbstrCalculator.h"
#include "LispTreeType.h"
#include "StateChangeNotification.h"
#include "Unit.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// functions 
//----------------------------------------------------------------------

auto _GetHistoricUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);

	while (true)
	{
		auto refItem = ti->mc_RefItem.lock();
		if (!refItem)
			return make_shared_tree(ti, no_zombies{}); // re-own the tree-managed item (no_zombies: empty if ti is mid-destruction, e.g. SetKeepDataState from ~AbstrDataItem -- existing_obj would throw bad_weak_ptr there)
		ti = refItem.get();
	}
}

auto _GetCurrUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);
	dbg_assert(ti->CheckMetaInfoReadyOrPassor());

	return _GetHistoricUltimateItem(ti);
}

auto _GetCurrRangeItem(const TreeItem* ti)  noexcept -> std::shared_ptr<const TreeItem>
{
	return _GetCurrUltimateItem(ti);
}

auto _GetUltimateItem(const TreeItem* ti)  noexcept -> std::shared_ptr<const TreeItem>
{
	assert(ti);
	while (true)
	{
		auto refItem = ti->GetReferredItem();
		if (!refItem)
			return make_shared_tree(ti, no_zombies{}); // re-own the tree-managed item (no_zombies: empty if ti is mid-destruction; existing_obj would throw bad_weak_ptr)
		ti = refItem.get();
	}
}

bool HasVisibleSubItems(const TreeItem* refItem)  noexcept
{
	while (true)
	{
		if (refItem->HasSubItems())
			return true;
		auto ref2 = refItem->GetReferredItem();
		if (!ref2)
			return false;
		refItem = ref2.get();
	}
}


item_origin GetItemOrigin(const TreeItem* ti)
{
	assert(ti);

	// order matters: an item within a template is shown as such even when it has a calculation
	// rule, and a calculation rule prevails over the storage of an enclosing container.
	if (ti->InTemplate())
		return item_origin::template_def;
	if (ti->HasCalculator())
		return item_origin::calculated;
	if (ti->GetStorageParent(false))
		return item_origin::exogenic;
	return item_origin::container;
}

static std::array<DmsColor, UInt32(item_origin::count)> s_OriginTextColors = sc_DefaultOriginTextColors;

DmsColor GetItemOriginTextColor(item_origin io)
{
	assert(io < item_origin::count);
	return s_OriginTextColors[UInt32(io)];
}

DmsColor GetItemOriginTextColor(const TreeItem* ti)
{
	return GetItemOriginTextColor(GetItemOrigin(ti));
}

void SetItemOriginTextColor(item_origin io, DmsColor clr)
{
	assert(io < item_origin::count);
	clr &= MAX_COLOR;
	s_OriginTextColors[UInt32(io)] = clr;
}

#include "PropFuncs.h"
#include "Metric.h"

bool IsGridDomain(const AbstrUnit* au)
{
	if (!au) // an in-template generic (unresolved) domain resolves to null; it is not a grid domain
		return false;
	return au->GetNrDimensions() == 2 && au->CanBeDomain();
}

bool IsBaseUnit(const AbstrUnit* au)
{
	assert(au);

	// GetCurrMetric follows the referred item and thus requires the meta info to be there. A
	// painter meets items that are not that far along and must not force them, so an unknown
	// metric answers 'not a base unit' rather than triggering an update.
	if (!au->Was(ProgressState::MetaInfo) || au->WasFailed(FailType::MetaInfo))
		return false;

	const UnitMetric* m = au->GetCurrMetric();
	return m
		&& m->m_Factor == 1.0
		&& m->m_BaseUnits.size() == 1
		&& m->m_BaseUnits.begin()->second == 1;
}

TokenID GetItemSpatialReference(const TreeItem* ti)
{
	assert(ti);

	if (!IsUnit(ti))
		return TokenID::GetEmptyID();

	// AbstrUnit::GetCurrSpatialReference asserts on the progress state; see IsBaseUnit above for
	// why a painter cannot promise it.
	if (!ti->Was(ProgressState::MetaInfo) || ti->WasFailed(FailType::MetaInfo))
		return TokenID::GetEmptyID();

	return AsUnit(ti)->GetCurrSpatialReference();
}

item_icon_kind GetItemIconKind(const TreeItem* ti, bool isMapViewable, bool hasCommonDomain)
{
	assert(ti);

	// order matters: a template is shown as one whatever it holds, and among data items the
	// map-viewability that the caller established prevails over the palette role, as it did when
	// these icons were still derived from ViewStyleFlags.
	//
	// A function is asked about FIRST because IsTemplate() holds for it as well: a function body
	// is inert like a template body (TreeItem::SetIsFunction), so the two would otherwise share
	// one icon.
	if (ti->IsTemplate())
		return ti->IsFunctionItem() ? item_icon_kind::function_def : item_icon_kind::template_def;

	if (IsUnit(ti))
	{
		auto au = AsUnit(ti);
		if (au->CanBeDomain())
			return IsGridDomain(au) ? item_icon_kind::unit_grid_domain : item_icon_kind::unit_domain;
		return IsBaseUnit(au) ? item_icon_kind::unit_base : item_icon_kind::unit_values;
	}

	if (IsDataItem(ti))
	{
		if (isMapViewable)
			return item_icon_kind::data_item_map;
		if (IsClassBreakAttr(ti) || IsColorAspectNameID(TreeItem_GetDialogType(ti)))
			return item_icon_kind::data_item_palette;
		return item_icon_kind::data_item;
	}

	// a container, with or without subitems: whether it has any is what the tree expander says.
	return hasCommonDomain ? item_icon_kind::container_table : item_icon_kind::container;
}

DmsColor GetItemIconColor(item_icon_kind ik)
{
	assert(ik < item_icon_kind::count);
	return sc_DefaultIconColors[UInt32(ik)];
}

NotificationCode NotificationCodeFromProblem(FailType ft)
{
	switch (ft)
	{
	case FailType::Data:      return NC2_DataFailed;
	case FailType::Validate:  return NC2_CheckFailed;
	case FailType::Committed: return NC2_CommitFailed;
	}
	return NC2_MetaFailed;
}

SharedStr GetPartialName(const TreeItem* themeDisplayItem, UInt32 nameLevel)
{
	dms_assert(themeDisplayItem);
	dms_assert(!themeDisplayItem->IsCacheItem());

	const TreeItem* themeDisplayItemCopy = themeDisplayItem;
	SharedStr result;

	for (; nameLevel; --nameLevel)
	{
		auto parent = themeDisplayItem->GetTreeParent();
		if (!parent)
			return themeDisplayItemCopy->GetFullName();

		result = DelimitedConcat(themeDisplayItem->GetName().c_str(), result.c_str());
		themeDisplayItem = parent.get();
		dms_assert(themeDisplayItem); // not root
	}
	return result;
}

const AbstrDataItem* GeometrySubItem(const TreeItem* ti)
{
	dms_assert(ti);
	ti->UpdateMetaInfo();
	const TreeItem* si = const_cast<TreeItem*>(ti)->GetSubTreeItemByID(token::geometry);
	if (!IsDataItem(si))
		return nullptr;
	auto gi = AsDataItem(si);
	if (gi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
		return nullptr;
	return gi;
}

#include "PropFuncs.h"

bool IsThisMappable(const TreeItem* ti)
{
	dms_assert(ti);
	return HasMapType(ti) || GeometrySubItem(ti);
}

auto GetMappingItem(const TreeItem* ti) -> const TreeItem*
{
	dms_assert(ti); // PRECONDITION
	do
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		if (IsThisMappable(ti))
			return ti;
		ti = ti->GetReferredItem().get();
	} while (ti);
	return nullptr;
}




// ==== ExtLockMgr ====

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include "TicInterface.h"

#include "act/InterestRetainContext.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"

#include "TreeItemClass.h"
#include "TreeItemContextHandle.h"

#include <set> // s_ExtPins (std::multiset) is used unconditionally; <set> otherwise only reached via the MG_DEBUG path

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------


//----------------------------------------------------------------------
// local TreeItemMultiSet manager for debugging resource leaks
//----------------------------------------------------------------------


#if defined(MG_DEBUG)

namespace {
	using TreeItemMultiSetType = std::multiset<const TreeItem*>;

	struct ItemCountAdm : std::unique_ptr<TreeItemMultiSetType>
	{
		ItemCountAdm(CharPtr objName)
			:	std::unique_ptr<TreeItemMultiSetType>(new TreeItemMultiSetType)
			,	m_ObjName(objName)
		{}

		~ItemCountAdm()
		{
			assert(get());

			UInt32 n = get()->size();
			if (!n) 
				return;

			TreeItemMultiSetType::iterator i = get()->begin();
			TreeItemMultiSetType::iterator e = get()->end();
			while (i!=e)
			{
				const TreeItem* ti = *i++;
				reportF(MsgCategory::memory, SeverityTypeID::ST_MajorTrace, "{} Leak: {} ({},{}) {}",
					m_ObjName,
					ti->GetDynamicClass()->GetName(), 
					ti->weak_from_this().use_count(), 
					ti->IsCacheItem(), 
					ti->GetFullName().c_str());
			}

			reportF(SeverityTypeID::ST_Error, "{} Leak of {} TreeItems. See EventLog for details.",
				m_ObjName,
				n
			);
		}

	private:
		CharPtr              m_ObjName;
	};

	ItemCountAdm 
		refCountAdm     ("RefCount"),
		interestCountAdm("Interest");

}	// anonymous namespace

#endif // defined(MG_DEBUG)

//----------------------------------------------------------------------
// C style Interface functions to AddRef & Release
//----------------------------------------------------------------------

// External clients pin a TreeItem's lifetime through this C API. The intrusive refcount is gone (TreeItem is
// std::shared_ptr-managed), so a pin is now an OWNING shared_tree_ptr held in this registry; Release drops one.
static std::mutex                      s_ExtPinMutex;
static std::multiset<SharedTreeItem>   s_ExtPins; // ordered by stored pointer (std::shared_ptr operator<)

TIC_CALL void DMS_CONV DMS_TreeItem_AddRef(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "AddRef", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_AddRef");
		DBG_TRACE(("self = {}", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		refCountAdm->insert(self);
#endif
		{
			std::lock_guard lock(s_ExtPinMutex);
			s_ExtPins.insert(make_shared_tree(self, existing_obj{})); // owning pin
		}

	DMS_CALL_END
}

TIC_CALL void DMS_CONV DMS_TreeItem_Release(TreeItem* self)
{
	DMS_CALL_BEGIN

		DBG_START("DMS_TreeItem", "Release", false);
		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_Release");
		DBG_TRACE(("self = {}", self->GetName().c_str()));

#if defined(MG_DEBUG)
		dms_assert(refCountAdm);
		TreeItemMultiSetType::iterator p = refCountAdm->find(self);
		dms_assert(p != refCountAdm->end());
		refCountAdm->erase(p);
#endif
		{
			std::lock_guard lock(s_ExtPinMutex);
			auto it = s_ExtPins.find(make_shared_tree(self, existing_obj{}));
			if (it != s_ExtPins.end())
				s_ExtPins.erase(it); // drop one owning pin
		}

	DMS_CALL_END
}

/********** InterestCount and managed actors **********/


//----------------------------------------------------------------------
// C style Interface functions for InterestCounting
//----------------------------------------------------------------------

TIC_CALL UInt32 DMS_CONV DMS_TreeItem_GetInterestCount(const TreeItem* self)
{
	DMS_CALL_BEGIN

		TreeItemContextHandle checkPtr(self, TreeItem::GetStaticClass(), "DMS_TreeItem_GetInteresetCount");
		return self->GetInterestCount();

	DMS_CALL_END
	return -1;
}


//----------------------------------------------------------------------
// C style Interface functions for Dynamic Stored PropDefs
//----------------------------------------------------------------------

#include "StoredPropDef.h"
#include "mci/PropdefEnums.h"

TIC_CALL AbstrPropDef* DMS_CONV DMS_StoredStringPropDef_Create(CharPtr name)
{
	DMS_CALL_BEGIN

		return new StoredPropDef<TreeItem, SharedStr>(name, set_mode::optional, xml_mode::element, cpy_mode::all, false);

	DMS_CALL_END
	return 0;
}

TIC_CALL void         DMS_CONV DMS_StoredStringPropDef_Destroy(AbstrPropDef* apd)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(apd, AbstrPropDef::GetStaticClass(), "DMS_StoredStringPropDef_Destroy");
		delete apd;

	DMS_CALL_END
}

