// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  AbstrDataItem: the abstract attribute item -- a TreeItem with a domain
 *  and a values unit, data-check modes, and typed access to its data
 *  object (AbstrDataObject) through the DataItemClass metaclass.
 */

#if !defined(__TIC_ABSTRDATAITEM_H)
#define __TIC_ABSTRDATAITEM_H

#include "TreeItem.h"

#include "DataCheckMode.h"
#include "mci/CompositeCast.h"

#include "DataItemClass.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

class AbstrValue;
struct DomainChangeInfo;
template <class ItemType, class PropType> class PropDef;

//----------------------------------------------------------------------
// class  : AbstrDataItem
//----------------------------------------------------------------------

class AbstrDataObject; // forward decl

class AbstrDataItem : public TreeItem
{
	typedef TreeItem base_type;

	friend class AbstrDataObject;
	friend class DataItemClass;

public:
//	override Object
	const DataItemClass* GetDynamicObjClass() const override;
	const Class* GetCurrentObjClass() const override;
	const Object* _GetAs(const Class* cls) const override;

public:
	AbstrDataItem();
	~AbstrDataItem();

	bool HasDataObj() const { return m_DataObject; }
	TIC_CALL ValueComposition GetValueComposition() const;
	void SetValueComposition(ValueComposition vc);

	void InitAbstrDataItem(TokenID domainUnit, TokenID valuesUnit, ValueComposition vc);

	TIC_CALL auto GetDataObj() const->SharedPtr<const AbstrDataObject>;
	auto GetCurrDataObj() const -> SharedPtr<const AbstrDataObject>;

	TIC_CALL auto GetRefObj() const ->SharedPtr<const AbstrDataObject>;
	TIC_CALL auto GetCurrRefObj() const ->SharedPtr<const AbstrDataObject>;

//	Override Actor
	void StartInterest() const override;
	garbage_can StopInterest () const noexcept override;

//	wrapper funcs that forward to DataObject
	TIC_CALL auto GetAbstrDomainUnit() const -> const AbstrUnit*;
	TIC_CALL auto GetAbstrValuesUnit() const -> const AbstrUnit*;
	// Current (already-resolved) domain/values unit WITHOUT triggering a FindUnit re-resolve -- an owning
	// snapshot of the weak member (empty if unset/expired). Used to keep a cache result's units alive.
	SharedUnit GetCurrDomainUnit() const { return m_DomainUnit.lock(); }
	SharedUnit GetCurrValuesUnit() const { return m_ValuesUnit.lock(); }
	// Guaranteed non-null OWNING unit or throw. The safe accessors for compute paths: GetAbstrDomainUnit/
	// GetAbstrValuesUnit return a null raw when the weak member expired off the meta thread (no re-resolve
	// there) or when FindUnit yields null InTemplate; these convert that state into a reportable item error.
	TIC_CALL SharedUnit GetDomainUnitOrThrow() const;
	TIC_CALL SharedUnit GetValuesUnitOrThrow() const;
	TIC_CALL auto CreateAbstrValue() const -> std::unique_ptr<AbstrValue>;

	TIC_CALL auto GetNonDefaultDomainUnit() const -> const AbstrUnit*;
	TIC_CALL auto GetNonDefaultValuesUnit() const -> const AbstrUnit*;

//	Override TreeItem virtuals
	SharedStr GetDescr() const override;
	bool TryCleanupMemImpl(garbage_can& garbageCan) const override;

//	Override TreeItem virtuals that forward to DataObject
	SharedStr GetSignature() const override;
	bool DoReadItem(StorageMetaInfoPtr smi) override;
	bool DoWriteItem(StorageMetaInfoPtr&& smi) const override;
	void ClearDataObject(garbage_can&) const override;

	void Unify(const TreeItem* refItem, CharPtr leftRole, CharPtr rightRole) const override;
//REMOVE	LispRef GetKeyExprImpl() const override;

//	override Actor
	ActorVisitState VisitSuppliers(SupplierVisitFlag svf, const ActorVisitor& visitor) const override;
	TIC_CALL ActorVisitState VisitLabelAttr(const ActorVisitor& visitor, SharedDataItemInterestPtr& labelLock) const;

//	override Object
	TokenID GetXmlClassID() const override;
	void XML_DumpData(OutStreamBase* xmlOutStr) const override;

//	additional interface funcs
	TIC_CALL bool HasUndefinedValues() const;
	TIC_CALL DataCheckMode GetRawCheckMode() const;
	DataCheckMode DetermineRawCheckMode() const;
	TIC_CALL DataCheckMode GetCheckMode() const;
	TIC_CALL DataCheckMode DetermineActualCheckMode() const;
	DataCheckMode GetTiledCheckMode(tile_id t) const;

	TIC_CALL bool HasVoidDomainGuarantee() const;

	// TODO G8: REMOVE
//	bool IsTiled() const { return GetAbstrDomainUnit()->IsTiled(); } 
//	bool IsCurrTiled() const { return GetAbstrDomainUnit()->IsTiled(); } 

	void OnDomainUnitRangeChange(const DomainChangeInfo* info);
	Int32 GetDataObjLockCount() const { return m_DataLockCount; }
	TIC_CALL Int32 GetDataRefLockCount() const; // exported: stg storage managers reference it in Debug links (/OPT:REF strips the reference in Release)

	void LoadBlobStream (const InpStreamBuff*) override;
	void StoreBlobStream(      OutStreamBuff*) const override;

	template <typename V> V GetValue(SizeT index) const
	{
		assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_checked_cast<V>(this)->GetIndexedValue(index);
	}
	template <typename V> typename sequence_array<V>::const_reference GetReference(SizeT index, GuiReadLock& lockHolder) const
	{
		assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_checked_cast<V>(this)->GetIndexedReference(index, lockHolder);
	}
	template <typename V> SizeT CountValues(param_type_t<V> value) const
	{
		assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_checked_cast<V>(this)->CountValues(value);
	}

	template <typename V> SizeT FindPos(param_type_t<V> value, SizeT startPos) const
	{
		assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_checked_cast<V>(this)->FindPos(value, startPos);
	}

	template <typename V> V LockAndGetValue(SizeT index) const;

	TokenID DomainUnitToken() const { return m_tDomainUnit; }
	TokenID ValuesUnitToken() const { return m_tValuesUnit; }

	// Per-element volume for a value composition that has no fixed width (a sequence, a string).
	// 0 means nobody knows better than EstimateDataBytes' ASSUMED_* guesses.
	UInt32 GetEstimatedBytesPerElement() const noexcept
	{
		return m_EstimatedBytesPerElement.load(std::memory_order_relaxed);
	}
	// Monotone (keeps the larger of old and new), so concurrent estimators cannot make a booking
	// shrink, and so a coarse publisher can never undercut a precise one.
	TIC_CALL void SetEstimatedBytesPerElement(SizeT bytesPerElement) const noexcept;

protected:
	void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;

private:
	bool CheckResultItem(const TreeItem* refItem) const override;
	// Whether an expired/unset weak unit member may be (re)resolved by name right now; see the comment
	// at its definition for each of the four conditions.
	bool CanResolveUnitByName(TokenID) const;
	const AbstrUnit* FindUnit(TokenID, CharPtr role, ValueComposition* vcPtr) const;
	void InitDataItem(const AbstrUnit* du, const AbstrUnit* vu, const DataItemClass* dic);
	garbage_can CleanupMem(bool hasSourceOrExit, std::size_t minNrBytes) noexcept;
	void GetRawCheckModeImpl() const; // DataCheckMode that always works for the value type of the data object
	DataCheckMode DetermineRawCheckModeImpl() const; // scan the actual values once to determine the minimally required DataCheckMode 

	TokenID                                  m_tDomainUnit = TokenID::GetUndefinedID(),
	                                         m_tValuesUnit = TokenID::GetUndefinedID();
	mutable WeakUnit                         m_DomainUnit, m_ValuesUnit; // NON-owning std::weak_ptr back-refs: a data item refers to but does not own its units (the tree owns them); .lock() gives real liveness

public: // TODO G8: Re-encapsulate
	mutable SharedPtr<const AbstrDataObject> m_DataObject;
	mutable std::atomic<Int32>               m_DataLockCount = 0; // -1 = WriteLock; positive: nr Of Read Locks on Data

	// See GetEstimatedBytesPerElement above. Atomic because estimates are taken from worker threads
	// (RefreshEstimateForAdmission) as well as from the meta thread.
	mutable std::atomic<UInt32>              m_EstimatedBytesPerElement = 0;

	// (The admission ledger's retained booking used to live here. It now lives on the data object --
	// AbstrDataObject::m_LedgerRetainedBytes -- because an object outlives this item's reference to
	// it whenever an active operation or a tile future still shares ownership.)

	friend struct DataReadLock;
	friend struct DataReadLockAtom; 
	friend struct DataWriteLock;
	friend struct DomainUnitPropDef;
	friend struct ValuesUnitPropDef;

	friend BestItemRef TreeItem_GetErrorSource(const TreeItem* src, bool tryCalcSuppliers);

//	Serialization & RTTI
	DECL_RTTI(TIC_CALL, TreeItemClass)
};

//----------------------------------------------------------------------
// PropDefPtrs
//----------------------------------------------------------------------

struct TableColumnSpec
{
	SharedDataItemInterestPtr m_DataItem;
	TokenID m_ColumnName;
	bool    m_RelativeDisplay = false;
	mutable Float64 m_ColumnTotal = 0.0;
};

//----------------------------------------------------------------------
// PropDefPtrs
//----------------------------------------------------------------------

extern PropDef<AbstrDataItem, SharedStr>* s_ValuesUnitPropDefPtr;
extern PropDef<AbstrDataItem, SharedStr>* s_DomainUnitPropDefPtr;

const AbstrUnit* AbstrValuesUnit(const AbstrDataItem* adi);
UInt32 ElementWeight(const AbstrDataItem* adi);

// Bytes a data block of nrElements elements of adi's value type occupies, sub-byte packing
// included. For variable-width elements (strings, non-Single value compositions) the per-row
// volume is a guess; a declared SizeUpperbound is what will replace that guess.
// See doc/development/schedule-with-lookahead.md §4.
TIC_CALL SizeT EstimateDataBytes(const AbstrDataItem* adi, SizeT nrElements);

// If adi is variable-width (a sequence, a string) and its data object is complete AND resident,
// measure bytes-per-row and publish it via SetEstimatedBytesPerElement, so EstimateDataBytes stops
// guessing for this item and consumers inherit the real width. Call ONLY where residency is
// guaranteed -- right after a storage read or a DataWriteLock::Commit -- because the measurement
// walks GetTile over all tiles. noexcept and self-guarding: a width is an optimization, never a
// reason to fail the read or commit that just succeeded.
void PublishMeasuredElementWidth(const AbstrDataItem* adi) noexcept;


#endif // __TIC_ABSTRDATAITEM_H
