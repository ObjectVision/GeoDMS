// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__TIC_ABSTRDATAITEM_H)
#define __TIC_ABSTRDATAITEM_H

#include "TreeItem.h"

#include "act/InterestRetainContext.h"
#include "dbg/DebugCast.h"
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
	TIC_CALL ~AbstrDataItem();

	bool HasDataObj() const { return m_DataObject; }
	TIC_CALL ValueComposition GetValueComposition() const;

	void InitAbstrDataItem(TokenID domainUnit, TokenID valuesUnit, ValueComposition vc);

	TIC_CALL const AbstrDataObject* GetDataObj() const;
	const AbstrDataObject* GetCurrDataObj() const { dms_assert(m_DataObject); return m_DataObject; }

	TIC_CALL const AbstrDataObject* GetRefObj() const;
	TIC_CALL const AbstrDataObject* GetCurrRefObj() const;

//	Override Actor
	TIC_CALL void StartInterest() const override;
	TIC_CALL garbage_t StopInterest () const noexcept override;

//	wrapper funcs that forward to DataObject
	TIC_CALL const AbstrUnit*  GetAbstrDomainUnit() const;
	TIC_CALL const AbstrUnit*  GetAbstrValuesUnit() const;
	TIC_CALL AbstrValue*       CreateAbstrValue  () const;

//	Override TreeItem virtuals
	SharedStr GetDescr() const override;
	bool TryCleanupMemImpl(garbage_t& garbageCan) const override;

//	Override TreeItem virtuals that forward to DataObject
	SharedStr GetSignature() const override;
	bool DoReadItem(StorageMetaInfoPtr smi) override;
	bool DoWriteItem(StorageMetaInfoPtr&& smi) const override;
	void ClearData(garbage_t&) const override;

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
	TIC_CALL DataCheckMode DetermineRawCheckMode() const;
	TIC_CALL DataCheckMode GetCheckMode() const;
	TIC_CALL DataCheckMode DetermineActualCheckMode() const;
	TIC_CALL DataCheckMode GetTiledCheckMode(tile_id t) const;

	TIC_CALL bool HasVoidDomainGuarantee() const;

	// TODO G8: REMOVE
//	bool IsTiled() const { return GetAbstrDomainUnit()->IsTiled(); } 
//	bool IsCurrTiled() const { return GetAbstrDomainUnit()->IsTiled(); } 

	void OnDomainUnitRangeChange(const DomainChangeInfo* info);
	Int32 GetDataObjLockCount() const { return m_DataLockCount; }
	TIC_CALL Int32 GetDataRefLockCount() const;

	void LoadBlobStream (const InpStreamBuff*) override;
	void StoreBlobStream(      OutStreamBuff*) const override;

	template <typename V> V GetValue(SizeT index) const
	{
		dms_assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_cast<V>(this)->GetIndexedValue(index);
	}
	template <typename V> typename sequence_array<V>::const_reference GetReference(SizeT index, GuiReadLock& lockHolder) const
	{
		dms_assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_cast<V>(this)->GetIndexedReference(index, lockHolder);
	}
	template <typename V> SizeT CountValues(typename param_type<typename sequence_traits<V>::value_type>::type value) const
	{
		dms_assert(AsDataItem(GetCurrUltimateItem())->m_DataLockCount != 0);
		return const_array_cast<V>(this)->CountValues(value);
	}

	template <typename V> V LockAndGetValue(SizeT index) const;
	template <typename V> SizeT LockAndCountValues(param_type_t<typename sequence_traits<V>::value_type> value) const;

	TokenID DomainUnitToken() const { return m_tDomainUnit; }
	TokenID ValuesUnitToken() const { return m_tValuesUnit; }

protected:
	TIC_CALL void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;

private:
	bool CheckResultItem(const TreeItem* refItem) const override;
	const AbstrUnit* FindUnit(TokenID, CharPtr role, ValueComposition* vcPtr) const;
	void InitDataItem(const AbstrUnit* du, const AbstrUnit* vu, const DataItemClass* dic);
	garbage_t CleanupMem(bool hasSourceOrExit, std::size_t minNrBytes) noexcept;
	void GetRawCheckModeImpl() const;
	DataCheckMode DetermineRawCheckModeImpl() const;

	TokenID                                  m_tDomainUnit = TokenID::GetUndefinedID(),
	                                         m_tValuesUnit = TokenID::GetUndefinedID();
	mutable SharedPtr<const AbstrUnit>       m_DomainUnit, m_ValuesUnit;

public: // TODO G8: Re-encapsulate
	mutable SharedPtr<const AbstrDataObject> m_DataObject;
	mutable std::atomic<Int32>               m_DataLockCount = 0; // -1 = WriteLock; positive: nr Of Read Locks on Data
	SharedStr                                m_FileName;

	friend struct DataReadLock;
	friend struct DataReadLockAtom; 
	friend struct DataWriteLock;
	friend struct DomainUnitPropDef;
	friend struct ValuesUnitPropDef;

	friend TIC_CALL BestItemRef TreeItem_GetErrorSource(const TreeItem* src, bool tryCalcSuppliers);

//	Serialization
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

TIC_CALL extern PropDef<AbstrDataItem, SharedStr>* s_ValuesUnitPropDefPtr;
TIC_CALL extern PropDef<AbstrDataItem, SharedStr>* s_DomainUnitPropDefPtr;

TIC_CALL const AbstrUnit* AbstrValuesUnit(const AbstrDataItem* adi);
TIC_CALL UInt32 ElementWeight(const AbstrDataItem* adi);
TIC_CALL UInt32 LTF_ElementWeight(const AbstrDataItem* adi);


#endif
