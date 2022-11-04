//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

#if !defined(__TIC_ABSTRUNIT_H)
#define __TIC_ABSTRUNIT_H

#include "TreeItem.h"

#include "geo/Geometry.h"
#include "geo/Range.h"
#include "mci/ValueComposition.h"
#include "ptr/OwningPtrSizedArray.h"

class InpStreamBuff;
class OutStreamBuff;
enum class FormattingFlags;

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

void CopyData(const AbstrDataObject* oldData, AbstrDataObject* newData, const DomainChangeInfo* info = nullptr);

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
};

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
	virtual const AbstrTileRangeData* GetTiledRangeData() const;
	TIC_CALL bool HasVarRangeData() const;
	TIC_CALL void SetFormat(TokenID format);

	TIC_CALL TokenID   GetFormat             () const;
	TIC_CALL TokenID   GetCurrFormat         () const;
	TIC_CALL SharedStr GetMetricStr          (FormattingFlags ff) const;
	TIC_CALL SharedStr GetCurrMetricStr      (FormattingFlags ff) const;
	TIC_CALL SharedStr GetFormattedMetricStr () const;
	TIC_CALL SharedStr GetProjectionStr      (FormattingFlags ff) const;

	TIC_CALL SharedDataItemInterestPtr GetLabelAttr() const;
	TIC_CALL ActorVisitState VisitLabelAttr(const ActorVisitor& visitor, SharedDataItemInterestPtr& labelLock) const;
	TIC_CALL SharedStr GetLabelAtIndex(SizeT index, SharedDataItemInterestPtr& ipHolder, streamsize_t maxLen, GuiReadLock& lock) const;
	TIC_CALL SharedStr GetMissingValueLabel() const;

	TIC_CALL virtual const UnitProjection* GetProjection() const; // impl for GeoUnits
	TIC_CALL virtual const UnitProjection* GetCurrProjection() const; // impl for GeoUnits
	TIC_CALL virtual const UnitMetric*     GetMetric() const;     // impl for NumericUnits  
	TIC_CALL virtual const UnitMetric*     GetCurrMetric() const;     // impl for NumericUnits  
//	TIC_CALL UnitProjection                GetCompositeProjection() const; // combines a chain of projection into a stack object

	TIC_CALL SharedStr GetNameOrCurrMetric(FormattingFlags ff) const;

	TIC_CALL const UnitClass*  GetUnitClass () const;
	TIC_CALL bool  UnifyValues(const AbstrUnit* calculatedUnit, UnifyMode um = UnifyMode(), SharedStr* resultMsg = nullptr) const;
	TIC_CALL bool  UnifyDomain(const AbstrUnit* calculatedUnit, UnifyMode um = UnifyMode(), SharedStr* resultMsg = nullptr) const;
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

	TIC_CALL row_id  GetTileFirstIndex(tile_id t) const;
	TIC_CALL row_id  GetTileIndex(tile_id t, tile_offset tileOffset) const;
	bool IsCovered() const;

	Range<row_id>   GetTileIndexRange(tile_id t) const;

	virtual bool ContainsUndefined(tile_id t) const;

// Support for indexable units
	virtual row_id  GetDimSize(DimType dimNr) const;

// Support for countables
	virtual row_id GetPreparedCount(bool throwOnUndefined = true) const;  // Returns 0 if non-countable unit
	virtual tile_offset GetPreparedTileCount(tile_id t) const;  // Returns 0 if non-countable unit
	TIC_CALL void ValidateCount(row_id) const;

	virtual row_id  GetCount() const;
	virtual tile_offset GetTileCount(tile_id t) const;
	virtual row_id GetBase () const;
	TIC_CALL bool IsOrdinalAndZeroBased() const;

	virtual AbstrValue* CreateAbstrValueAtIndex(SizeT i) const;
	virtual SizeT GetIndexForAbstrValue(const AbstrValue&) const;

// Support for ordinals
	virtual void SetCount(SizeT count);
	virtual void Split   (SizeT pos, SizeT len);
	virtual void Merge   (SizeT pos, SizeT len);

	void OnDomainChange(const DomainChangeInfo* info);

// Support for Numerics
	virtual void SetRangeAsFloat64(Float64 begin, Float64 end);
	virtual Range<Float64> GetRangeAsFloat64() const;
	virtual Range<Float64> GetTileRangeAsFloat64(tile_id t) const;

// Support for Geometrics
	virtual void SetRangeAsIPoint(Int32  rowBegin, Int32  colBegin, Int32  rowEnd, Int32  colEnd);
	virtual DRect GetRangeAsDRect() const;
	virtual DRect GetTileRangeAsDRect(tile_id t) const;
	virtual void SetRangeAsDPoint(Float64  rowBegin, Float64  colBegin, Float64  rowEnd, Float64  colEnd);

	virtual IRect GetRangeAsIRect() const;
	virtual I64Rect GetTileSizeAsI64Rect(tile_id t) const; // asssume 1D; GeoUnitAdapter overrules this for all 2D domains
	virtual IRect GetTileRangeAsIRect(tile_id t) const;
	TIC_CALL void SetRangeAsIRect(const IRect& rect);
	TIC_CALL void SetRangeAsDRect(const DRect& rect);

//	Generalization
	virtual SharedStr GetRangeAsStr() const;

	void AddDataItemOut(const AbstrDataItem* item) const;
	void DelDataItemOut(const AbstrDataItem* item) const;

#if defined(MG_DEBUG)
	bool HasDataItemOut(const AbstrDataItem* item) const;
#endif

	virtual void InviteUnitProcessor(const UnitProcessor& visitor) const = 0;

// mag alleen vanuit Update of Create worden aangeroepen 
	TIC_CALL virtual void SetMetric    (const UnitMetric    * m);
	TIC_CALL virtual void SetProjection(const UnitProjection* p);
	TIC_CALL void DuplFrom(const AbstrUnit* src);

//	Override TreeItem virtuals
	TIC_CALL void CopyProps(TreeItem* result, const CopyTreeContext& copyContext) const override;
	TIC_CALL bool DoWriteItem(StorageMetaInfoPtr&& smi) const override;
	TIC_CALL auto GetScriptName(const TreeItem* context) const -> SharedStr override;

protected:
	SharedStr GetSignature() const override;
	bool DoReadItem(StorageMetaInfo* smi) override;

private:
	void      UnifyError(const AbstrUnit* cu, CharPtr reason, UnifyMode um, SharedStr* resultMsg, bool isDomain) const;
	SharedStr GetProjMetrString() const;

	bool                  HasDataItemsAssoc() const { return m_DataItemsAssocPtr.has_ptr(); }
	DataItemRefContainer& GetDataItemsAssoc() const;

private:
	mutable OwningPtr<DataItemRefContainer>    m_DataItemsAssocPtr;

	DECL_ABSTR(TIC_CALL, Class)
};

#include "dbg/DebugCast.h"

template <typename T> inline const AbstrUnit* AsDynamicUnit(const T* self) { return dynamic_cast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsDynamicUnit(      T* self) { return dynamic_cast<      AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsCheckedUnit(const T* self) { return checked_cast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsCheckedUnit(      T* self) { return checked_cast<      AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsCertainUnit(const T* self) { return checked_valcast<const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsCertainUnit(      T* self) { return checked_valcast<      AbstrUnit*>(self); }
template <typename T> inline const AbstrUnit* AsUnit       (const T* self) { return debug_cast  <const AbstrUnit*>(self); }
template <typename T> inline       AbstrUnit* AsUnit       (      T* self) { return debug_cast  <      AbstrUnit*>(self); }
template <typename T> inline bool             IsUnit       (const T* self) { return AsDynamicUnit(self) != 0;             }



#endif // !defined(__TIC_ABSTRUNIT_H)
