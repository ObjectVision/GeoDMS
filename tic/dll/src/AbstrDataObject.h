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

#if !defined(__TIC_ABSTRDATAOBJECT_H)
#define __TIC_ABSTRDATAOBJECT_H

#include "TicBase.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/Geometry.h"
#include "ptr/PtrBase.h"
#include "ptr/SharedPtr.h"
#include "ser/FileCreationMode.h"

#include "TileLock.h"

class AbstrValue;
class IMappedFile;
struct AbstrReadableTileData;
struct DomainChangeInfo;
struct SafeFileWriterArray;
enum class FormattingFlags;

//----------------------------------------------------------------------
// class  : AbstrDataObject
//----------------------------------------------------------------------
// TODO G8: MOVE TO rtc/ptr/xxx, or replace by equivalent in Good Coding Guide

template <typename T>
struct nonnull_ptr : ptr_base<T, copyable>
{
	explicit nonnull_ptr(T* p) : ptr_base<T, copyable>{ p } 
	{
		dms_assert(p != nullptr);
	}
};

template <typename T>
struct ptr : ptr_base<T, copyable>
{
	ptr() = default;
	explicit ptr(T* p) : ptr_base<T, copyable>( p ) {}
};

struct abstr_future_tile : TileBase {
	virtual auto GetTileCRef() -> TileCRef = 0;
};

class AbstrDataObject: public PersistentSharedObj
{
	friend class AbstrDataItem; 
	friend class AbstrStorageManager;
	friend struct DataWriteLock;
	friend struct DataStoreManager;

	using base_type = AbstrDataItem;
public:
	using data_read_begin_handle = locked_seq<ptr<const Byte>, TileCRef>;
	using data_write_begin_handle = locked_seq<ptr<Byte>, TileRef>;

	TIC_CALL SharedPtr<const AbstrTileRangeData> GetTiledRangeData() const;

	TIC_CALL virtual bool IsMemoryObject() const { return false; }
	TIC_CALL virtual bool CheckValuesUnit(const AbstrUnit* valuesUnit) = 0;
//	Meta Info

	TIC_CALL const DataItemClass* GetDataItemClass() const;

	TIC_CALL const ValueClass*  GetValuesType() const;

//	Tiling
	TIC_CALL tile_loc GetTiledLocation(row_id idx) const;

// Abstr Tile retention
	virtual auto GetFutureAbstrTile(tile_id t) const-> SharedPtr<abstr_future_tile> = 0;
	virtual auto GetReadableTileLock(tile_id t) const->TileCRef = 0; // TODO G8: REMOVE
	virtual auto GetWritableTileLock(tile_id t, dms_rw_mode rwMode = dms_rw_mode::read_write)->TileRef = 0; // TODO G8: REMOVE

//	DomainCardinality
	TIC_CALL row_id      GetNrFeaturesNow() const;
	TIC_CALL std::size_t GetNrBytesNow    (bool calcStreamSize = false) const;

	TIC_CALL virtual std::size_t GetNrTileBytesNow(tile_id t, bool calcStreamSize = false) const = 0;
	TIC_CALL virtual bool        IsSmallerThan(SizeT sz) const = 0;

//	Values Cardinality
	TIC_CALL virtual row_id GetValuesRangeCount() const { return UNDEFINED_VALUE(row_id); }
	TIC_CALL virtual bool   IsFirstValueZero() const { return false;  }

//	Data Access
	TIC_CALL virtual auto CreateReadableTileData(tile_id t) const ->AbstrReadableTileData * = 0;
	TIC_CALL virtual auto CreateAbstrValue() const ->AbstrValue * = 0;
	TIC_CALL virtual auto GetValueClass() const -> const ValueClass * = 0;

	TIC_CALL virtual auto GetDataReadBegin (tile_id = no_tile) const ->data_read_begin_handle =0;
	TIC_CALL virtual auto GetDataWriteBegin(tile_id = no_tile) ->data_write_begin_handle =0;

	TIC_CALL virtual void GetAbstrValue   (SizeT index, AbstrValue& valueHolder) const=0;
	TIC_CALL virtual void SetAbstrValue   (SizeT index, const AbstrValue& valueHolder)=0;
	TIC_CALL virtual void SetNull         (SizeT index) = 0;
	TIC_CALL virtual bool IsNull          (SizeT index) const = 0;
	TIC_CALL virtual bool   AsCharArray(SizeT index, char* sink, streamsize_t buflen, GuiReadLock& lockHolder, FormattingFlags ff) const=0;
	TIC_CALL virtual SizeT  AsCharArraySize(SizeT index, streamsize_t maxLen, GuiReadLock& lockHolder, FormattingFlags ff) const=0;
	TIC_CALL virtual SharedStr AsString (SizeT index, GuiReadLock& lockHolder) const=0;

// Support for numerics (optional)
	// TODO G8: add TileCRef& resourceHolder as last parameter to these functions to avold file (un)mapping per row
	TIC_CALL virtual Float64 GetValueAsFloat64(SizeT index) const;
	TIC_CALL virtual void    SetValueAsFloat64(SizeT index, Float64 val);
	TIC_CALL virtual SizeT   FindPosOfFloat64 (Float64 val, SizeT startPos = 0) const;
	TIC_CALL virtual Int32   GetValueAsInt32(SizeT index) const;
	TIC_CALL virtual void    SetValueAsInt32(SizeT index, Int32 val);
	TIC_CALL virtual UInt32  GetValueAsUInt32(SizeT index) const;
	TIC_CALL virtual void    SetValueAsUInt32(SizeT index, UInt32 val);
	TIC_CALL virtual SizeT   GetValueAsSizeT (SizeT index) const;
	TIC_CALL virtual void    SetValueAsSizeT (SizeT index, SizeT val);
	TIC_CALL virtual void    SetValueAsSizeT(SizeT index, SizeT val, tile_id t);
	TIC_CALL virtual UInt8   GetValueAsUInt8 (SizeT index) const;
	TIC_CALL virtual SizeT   FindPosOfSizeT(SizeT val, SizeT startPos = 0) const;
	TIC_CALL virtual Float64 GetSumAsFloat64() const;
	TIC_CALL virtual SizeT   GetNrNulls() const =0;

	TIC_CALL virtual SizeT GetValuesAsFloat64Array(tile_loc tl, SizeT len, Float64* data) const;
	TIC_CALL virtual SizeT GetValuesAsUInt32Array (tile_loc tl, SizeT len, UInt32*  data) const;
	TIC_CALL virtual SizeT GetValuesAsInt32Array  (tile_loc tl, SizeT len, Int32*   data) const;
	TIC_CALL virtual SizeT GetValuesAsUInt8Array  (tile_loc tl, SizeT len, UInt8*   data) const;

	TIC_CALL virtual void SetValuesAsFloat64Array(tile_loc tl, SizeT len, const Float64* data);
	TIC_CALL virtual void SetValuesAsInt32Array  (tile_loc tl, SizeT len, const Int32*   data);
	TIC_CALL virtual void SetValuesAsUInt8Array  (tile_loc tl, SizeT len, const UInt8*   data);

	TIC_CALL virtual void FillWithFloat64Values  (tile_loc tl, SizeT len, Float64 fillValue);
	TIC_CALL virtual void FillWithUInt32Values   (tile_loc tl, SizeT len, UInt32  fillValue);
	TIC_CALL virtual void FillWithInt32Values    (tile_loc tl, SizeT len, Int32   fillValue);
	TIC_CALL virtual void FillWithUInt8Values    (tile_loc tl, SizeT len, UInt8   fillValue);

// Support for Geometrics (optional)
	TIC_CALL virtual DRect GetActualRangeAsDRect(bool checkForNulls) const;

// Support for GeometricPoints (optional)
	TIC_CALL virtual DPoint  GetValueAsDPoint(SizeT index) const;
	TIC_CALL virtual void    SetValueAsDPoint(SizeT index, const DPoint& val);

// Support for GeometricSequences (optional)
	TIC_CALL virtual void GetValueAsDPoints(SizeT index, std::vector<DPoint>& dpoints) const;

// new virtuals
	TIC_CALL virtual void DoReadData (BinaryInpStream&, tile_id t = no_tile)=0;
	TIC_CALL virtual void DoWriteData(BinaryOutStream&, tile_id t = no_tile) const=0;

  protected: // declare interface for forwarded TreeItem virtuals
	// new virtuals

	TIC_CALL virtual DataCheckMode DoGetCheckMode() const =0;
	TIC_CALL virtual DataCheckMode DoDetermineCheckMode() const = 0;
	TIC_CALL virtual void DoSimplifyCheckMode(DataCheckMode& dcm) const =0;
  public:
	template <typename V> void SetValue(row_id index, param_type_t<typename sequence_traits<V>::value_type> value)
	{
//		dms_assert(m_DataLockCount <= 0);
		mutable_array_checkedcast<V>(this)->SetIndexedValue(index, value);
	}
	template <typename V> V GetValue(row_id index) const
	{
//		dms_assert(m_DataLockCount > 0);
		return const_array_checkedcast<V>(this)->GetIndexedValue(index);
	}

  private:
	[[noreturn]] void illegalNumericOperation() const;
	[[noreturn]] void illegalGeometricOperation() const;

  public:
//	called from override Object
	TIC_CALL virtual void XML_DumpObjData(OutStreamBase* xmlOutStr, const AbstrDataItem* owner) const =0;
	TIC_CALL virtual LispRef GetValuesAsKeyArgs(LispPtr valuesUnitKeyExpr) const = 0;

// Serialization
	DECL_ABSTR(TIC_CALL, Class)

public:
	SharedPtr<const AbstrTileRangeData> m_TileRangeData; // this replaces m_DomainUnitCopy

#if defined(MG_DEBUG_ALLOCATOR)
	SharedStr md_SrcStr;
#endif
};


template<typename V> 
auto const_array_cast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return debug_valcast<const TileFunctor<V>*>(ptr);
}

template<typename V> 
auto const_opt_array_cast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return (ptr != nullptr) ? const_array_cast<V>(ptr) : nullptr;
}

template<typename V>
auto const_array_dynacast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return dynamic_cast<const TileFunctor<V>*>(ptr);
}

template<typename V>
auto const_array_checkedcast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return checked_cast<const TileFunctor<V>*>(ptr);
}

template<typename V>
auto const_opt_array_checkedcast(const AbstrDataObject* ptr) -> const TileFunctor<V>*
{
	return (ptr != nullptr) ? const_array_checkedcast<V>(ptr) : nullptr;
}

template<typename V>
auto mutable_array_cast(AbstrDataObject* ptr) -> TileFunctor<V>*
{
	return debug_valcast<TileFunctor<V>*>(ptr);
}

template<typename V>
auto mutable_opt_array_cast(AbstrDataObject* ptr) -> TileFunctor<V>*
{
	return ptr ? debug_valcast<TileFunctor<V>*>(ptr) : nullptr;
}

template<typename V>
auto mutable_array_dynacast(AbstrDataObject* ptr) -> TileFunctor<V>*
{
	return dynamic_cast<TileFunctor<V>*>(ptr);
}

template<typename V>
auto mutable_array_checkedcast(AbstrDataObject* ptr) -> TileFunctor<V>*
{
	return checked_cast<TileFunctor<V>*>(ptr);
}



template<typename V>
TIC_CALL auto CreateHeapTileArray(const AbstrTileRangeData* tdr, const Unit<field_of_t<V>>* valuesUnitPtr, bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<TileFunctor<V>>;

auto CreateAbstrHeapTileFunctor(const AbstrDataItem* adi, const bool mustClear MG_DEBUG_ALLOCATOR_SRC_ARG)->std::unique_ptr<AbstrDataObject>;
auto CreateFileTileArray(const AbstrDataItem* adi, dms_rw_mode rwMode, SharedStr filenameBase, bool isTmp, SafeFileWriterArray* sfwa)->std::unique_ptr<AbstrDataObject>;

struct ReadableTileLock : TileCRef // TODO G8: REMOVE
{
	ReadableTileLock(const AbstrDataObject* ado, tile_id t)
		: TileCRef(ado ? ado->GetReadableTileLock(t) : nullptr)
	{}
};

struct WritableTileLock: TileRef // TODO G8: REMOVE
{
	WritableTileLock(AbstrDataObject * ado, tile_id t, dms_rw_mode rwMode = dms_rw_mode::read_write)
		: TileRef(ado->GetWritableTileLock(t, rwMode))
	{}
};

#endif
