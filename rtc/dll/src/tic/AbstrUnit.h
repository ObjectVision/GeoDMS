// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__TIC_ABSTRUNIT_H)
#define __TIC_ABSTRUNIT_H

#include "TreeItem.h"

#include "Crs.h"

#include "geom/Geometry.h"
#include "geom/Range.h"
#include "mci/ValueComposition.h"
#include "ptr/OwningPtrSizedArray.h"

class InpStreamBuff;
class OutStreamBuff;

//	A Unit is a piece of information about a property indicating meaning an
//	defining representations. A concrete Unit is instantiated for each value
//	type.

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Domain Change Context
//----------------------------------------------------------------------

struct domain_change_context {

	domain_change_context(row_id changePos_);
	~domain_change_context();

	row_id changePos;

	domain_change_context* prevContext;
	static auto GetCurrContext()->domain_change_context*;
};

struct DomainChangeInfo
{
	const AbstrTileRangeData* oldRangeData;
	const AbstrTileRangeData* newRangeData;

	row_id oldSize, newSize, changePos;
	domain_change_context* domainChangeContext = nullptr;
};

// exported: shv copies the values a calculated palette computes into an editable view-local
// item (issue #634), the same old-object -> new-object copy a domain change uses.
TIC_CALL void CopyData(const AbstrDataObject* oldData, AbstrDataObject* newData, const DomainChangeInfo* info = nullptr);

const UInt32 MAX_TILE_SIZE = 0x10000;

//----------------------------------------------------------------------
// class  : AbstrUnit
//----------------------------------------------------------------------

enum UnifyMode {
	UM_AllowDefaultLeft  =  1,
	UM_AllowDefaultRight =  2,
	UM_AllowDefault      =  UM_AllowDefaultLeft|UM_AllowDefaultRight,
	UM_AllowTypeDiff     =  4,
	UM_Throw             =  8,
	UM_AllowVoidRight    = 16,
	UM_AllowAllEqualCount= 32,

	// UnifyDomain only: allow interning (GetOrCreateDataController) of the RIGHT
	// operand's checked-key DC, making the comparison total and symmetric; both
	// operands are also made meta-info-ready first (UpdateMetaInfoIfNotAlready),
	// as checker-resolved units may not have been updated yet. Without
	// it the right side is looked up only (GetExistingDataController, the #361 fix:
	// DC creation is meta-thread-only), so a->UnifyDomain(b) can be false where
	// b->UnifyDomain(a) is true, depending on DC-interning order. Callers passing
	// this flag MUST run on the meta thread; GetOrCreateDataController enforces
	// that with a release-active MG_CHECK. Do not test IsMetaThread() to decide:
	// worker tasks can incidentally execute on the meta thread, which would make
	// the verdict scheduling-dependent -- the flag is a caller CONTRACT, not a
	// runtime probe.
	UM_AllowRightExpansion = 64,
};

constexpr auto operator | (UnifyMode lhs, UnifyMode rhs) { return UnifyMode(int(lhs) | int(rhs)); }

class AbstrUnit : public TreeItem
{
	typedef TreeItem base_type;

protected:
	 AbstrUnit();
	~AbstrUnit();

public:
	TIC_CALL UInt32 GetNrDataItemsOut() const;
	TIC_CALL const AbstrDataItem* GetDataItemOut(UInt32 index) const;

	virtual bool IsTiled() const { return false; }
	virtual bool IsCurrTiled() const { return false; }

	virtual bool HasTiledRangeData() const = 0;
	virtual SharedPtr<const AbstrTileRangeData> GetTiledRangeData() const;
	TIC_CALL bool HasVarRangeData() const;
	TIC_CALL void SetSpatialReference(TokenID format);

	// The CRS as a first-class property, peer of the metric. GetCrs/GetCurrCrs delegate
	// to the referred item exactly as Unit<V>::GetMetric does (Unit.cpp) -- that
	// delegation is what lets a cache unit answer for its config origin, and its absence
	// on the old side-table channel is why the CRS had to ride inside the metric.
	// See doc/development/crs-metric-decoupling.md.
	const UnitCrs* GetCrs    () const;
	TIC_CALL const UnitCrs* GetCurrCrs() const;
	TIC_CALL void           SetCrs(const UnitCrs* crs);

	// The CRS stored on THIS unit only: no referred-item delegation and no walk up the
	// projection chain. This is the DUMP/serialization view -- it answers "did this item
	// declare a SpatialReference?", not "what CRS is this unit in?". Keeping the two apart
	// is what stops derived units from emitting SpatialReference lines they never declared.
	// See SpatialReferencePropDef::GetRawValue and feedback on the raw-vs-cooked seam.
	const UnitCrs* GetLocalCrs() const { return m_Crs.get_ptr(); }

	TIC_CALL SharedStr GetBackgroundReference() const;
	TIC_CALL TokenID   GetSpatialReference    () const;
	TIC_CALL TokenID   GetCurrSpatialReference() const;
	TIC_CALL SharedStr GetMetricStr           (FormattingFlags ff) const;
	TIC_CALL SharedStr GetCurrMetricStr       (FormattingFlags ff) const;
	TIC_CALL SharedStr GetFormattedMetricStr  () const;
	TIC_CALL SharedStr GetProjectionStr       (FormattingFlags ff) const;
	TIC_CALL auto GetUnitlabeledScalePair() const->UnitLabelScalePair;

	TIC_CALL auto GetLabelAttr() const -> SharedDataItemInterestPtr;
	auto GetCurrLabelAttr() const -> const AbstrDataItem*;
	TIC_CALL ActorVisitState VisitLabelAttr(const ActorVisitor& visitor, SharedDataItemInterestPtr& labelLock) const;
	SharedStr GetLabelAtIndex(SizeT index, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock) const;
	SharedStr GetMissingValueLabel() const;

	virtual const UnitProjection* GetProjection() const; // impl for GeoUnits
	virtual const UnitProjection* GetCurrProjection() const; // impl for GeoUnits
	virtual const UnitMetric*     GetMetric() const;     // impl for NumericUnits  
	virtual const UnitMetric*     GetCurrMetric() const;     // impl for NumericUnits  
//	TIC_CALL UnitProjection                GetCompositeProjection() const; // combines a chain of projection into a stack object

	SharedStr GetNameOrCurrMetric(FormattingFlags ff) const;

	TIC_CALL const UnitClass*  GetUnitClass () const;
	TIC_CALL bool  UnifyValues(const AbstrUnit* calculatedUnit, CharPtr leftRole = "", CharPtr rightRole = "", UnifyMode um = UnifyMode(), SharedStr* resultMsg = nullptr) const;
	TIC_CALL bool  UnifyDomain(const AbstrUnit* calculatedUnit, CharPtr leftRole = "", CharPtr rightRole = "", UnifyMode um = UnifyMode(), SharedStr* resultMsg = nullptr) const;
	TIC_CALL bool  IsDefaultUnit() const;

// unit methods
	TIC_CALL const ValueClass* GetValueType(ValueComposition vc = ValueComposition::Single) const;
	virtual DimType GetNrDimensions() const =0;

// Support for ranged units
	virtual void SetMaxRange();
	virtual bool HasVarRange() const { return false; }
	virtual bool CanBeDomain() const { return false; }

	virtual tile_id GetThisCurrTileID(SizeT& index, tile_id prevT) const;
	virtual tile_id GetNrTiles() const;

	TIC_CALL row_id GetTileFirstIndex(tile_id t) const;
	TIC_CALL row_id GetTileIndex(tile_id t, tile_offset tileOffset) const;
	TIC_CALL bool IsCovered() const;

	Range<row_id>   GetTileIndexRange(tile_id t) const;

	virtual bool ContainsUndefined(tile_id t) const;

// Support for indexable units
	virtual row_id  GetDimSize(DimType dimNr) const;

// Support for countables
	virtual row_id GetPreparedCount(bool throwOnUndefined = true) const;  // Returns 0 if non-countable unit
	virtual tile_offset GetPreparedTileCount(tile_id t) const;  // Returns 0 if non-countable unit
	TIC_CALL void ValidateCount(row_id) const;

	virtual row_id  GetCount() const;
	virtual row_id  GetDataCount() const;
	virtual tile_offset GetTileCount(tile_id t) const;
	virtual row_id GetBase () const;
	TIC_CALL bool IsOrdinalAndZeroBased() const;
	row_id GetEstimatedCount() const; // == EstimateCount().expected

	// The confidence ladder of doc/development/schedule-with-lookahead.md §4.6: ready data gives the
	// exact count; else a declared SizeUpperbound (reservable) and/or SizeExpectation (a point
	// estimate, never reserved on); else ASSUMED_SIZE. Evaluating a declared rule can throw -- that
	// is a fact about the declaration, so callers that must not fail have to guard it.
	struct CountEstimate
	{
		row_id expected = 0;
		row_id upperBound = 0;
		estimate_confidence confidence = estimate_confidence::assumed;
	};
	TIC_CALL auto EstimateCount() const -> CountEstimate;

	virtual auto CreateAbstrValueAtIndex(SizeT i) const -> std::unique_ptr<AbstrValue>;
	virtual SizeT GetIndexForAbstrValue(const AbstrValue&) const;

// Support for ordinals
	virtual void SetCount(SizeT count);

	void OnDomainChange(const DomainChangeInfo* info);

// Support for Numerics
	virtual void SetRangeAsFloat64(Float64 begin, Float64 end);
	virtual void SetRangeAsUInt64(UInt64 begin, UInt64 end);
	virtual Range<Float64> GetRangeAsFloat64() const;
	virtual Range<Float64> GetTileRangeAsFloat64(tile_id t) const;

// Support for Geometrics
	virtual void SetRangeAsIPoint(Int32  rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd, UInt16 blockSizeY, UInt16 blockSizeX);
	virtual DRect GetRangeAsDRect() const;
	virtual DRect GetTileRangeAsDRect(tile_id t) const;
	virtual void SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd);

	virtual IRect GetRangeAsIRect() const;
	virtual I64Rect GetTileSizeAsI64Rect(tile_id t) const; // asssume 1D; Unit<V> overrules this for all 2D domains
	virtual IRect GetTileRangeAsIRect(tile_id t) const;
	TIC_CALL void SetRangeAsDRect(const DRect& rect);

//	Generalization
	virtual SharedStr GetRangeAsStr(FormattingFlags ff) const;

	void AddDataItemOut(const AbstrDataItem* item) const;
	void DelDataItemOut(const AbstrDataItem* item) const;

#if defined(MG_DEBUG)
	bool HasDataItemOut(const AbstrDataItem* item) const;
#endif

	virtual void InviteUnitProcessor(const UnitProcessor& visitor) const = 0;

// mag alleen vanuit Update of Create worden aangeroepen 
	virtual void SetMetric    (SharedPtr<const UnitMetric    > m);
	virtual void SetProjection(SharedPtr<const UnitProjection> p);
	TIC_CALL void DuplFrom(const AbstrUnit* src);

//	Override TreeItem virtuals
	void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;
	bool DoWriteItem(StorageMetaInfoPtr&& smi) const override;
	auto GetScriptName(const TreeItem* context) const -> SharedStr override;

protected:
	SharedStr GetSignature() const override;
	bool DoReadItem(StorageMetaInfoPtr smi) override;

private:
	void      UnifyError(const AbstrUnit* cu, CharPtr reason, CharPtr leftRole, CharPtr rightRole, UnifyMode um, SharedStr* resultMsg, bool isDomain) const;
	SharedStr GetProjMetrString() const;

	bool                  HasDataItemsAssoc() const { return m_DataItemsAssocPtr != nullptr; }
	DataItemRefContainer& GetDataItemsAssoc() const;

private:
	mutable std::unique_ptr<DataItemRefContainer>    m_DataItemsAssocPtr;

	// Replaces the former s_SpatialReferenceAssoc global (an unsynchronised std::map
	// keyed on raw pointers, which therefore could not be written from worker threads)
	// plus its USF_HasSpatialReference flag bit and ~AbstrUnit erase.
	SharedPtr<const UnitCrs>                         m_Crs;

	DECL_ABSTR(TIC_CALL, Class)
};

#include "dbg/DebugCast.h"

template <typename T> inline bool             IsUnit(const T* self) { return AsDynamicUnit(self) != 0; }
template <typename T> inline const AbstrUnit* AsUnit(const T* self) { return debug_cast  <const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsUnit(T* self) { return debug_cast  <AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsDynamicUnit(const T* self) { return dynamic_cast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsDynamicUnit(      T* self) { return dynamic_cast<      AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsCheckedUnit(const T* self) { return checked_cast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsCheckedUnit(      T* self) { return checked_cast<      AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsCertainUnit(const T* self) { return checked_valcast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsCertainUnit(      T* self) { return checked_valcast<      AbstrUnit*>(self); }

template <typename T> inline       OwningPtr<AbstrUnit> AsUnit(OwningPtr<T> self) { return debug_cast<AbstrUnit*>(self.release()); }
template <typename T> inline       std::shared_ptr<const AbstrUnit> AsUnit(const std::shared_ptr<const T>& self) { return make_shared_tree(debug_cast  <const AbstrUnit*>(self.get()), existing_obj{}); }


template <typename T> inline bool IsUnit(const SharedPtr<T>& self) { return IsUnit(self.get()); }
template <typename T> inline auto AsUnit(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsUnit(self.get())); }
template <typename T> inline auto AsDynamicUnit(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsDynamicUnit(self.get())); }
template <typename T> inline auto AsCheckedUnit(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsCheckedUnit(self.get())); }
template <typename T> inline auto AsCertainUnit(const SharedPtr<T>& self) { return MakeSharedFromBorrowedObjectPtr(AsCertainUnit(self.get())); }

// shared_tree_ptr (std::shared_ptr) overloads: borrow an owning share of the same managed object.
template <typename T> inline bool IsUnit(const std::shared_ptr<T>& self) { return IsUnit(self.get()); }
template <typename T> inline auto AsUnit(const std::shared_ptr<T>& self) { auto* r = AsUnit(self.get()); return make_shared_tree(r, existing_obj{}); }
template <typename T> inline auto AsDynamicUnit(const std::shared_ptr<T>& self) { auto* r = AsDynamicUnit(self.get()); return make_shared_tree(r, existing_obj{}); }
template <typename T> inline auto AsCheckedUnit(const std::shared_ptr<T>& self) { auto* r = AsCheckedUnit(self.get()); return make_shared_tree(r, existing_obj{}); }
template <typename T> inline auto AsCertainUnit(const std::shared_ptr<T>& self) { auto* r = AsCertainUnit(self.get()); return make_shared_tree(r, existing_obj{}); }


#endif // !defined(__TIC_ABSTRUNIT_H)
