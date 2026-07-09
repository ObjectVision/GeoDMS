// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Convenience value-accessor free functions for data items (GetValue / GetTheValue /
// SetValue / ...). Split out of DataArray.h (its author-noted "TODO G8 MOVE to separate
// header file") so translation units that only need the DataArray<V> value types do not
// have to parse these DataLocks-dependent accessors. Include this header where the
// GetValue/SetValue family is actually called.

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_DATAARRAYVALUE_H)
#define __TIC_DATAARRAYVALUE_H

#include "DataArray.h"
#include "DataLocks.h"
#include "AbstrDataItem.h"

template <typename V>
auto GetValue(const AbstrDataItem* adi, SizeT idx) 
	->	DataArrayBase<V>::value_type
{
	assert(adi);
	assert(adi->GetInterestCount());
	adi->PrepareDataUsage(DrlType::CertainOrThrow);
	DataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
auto GetReference(const AbstrDataItem* adi, SizeT idx) 
	-> DataArrayBase<V>::const_reference
{
	PreparedDataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
auto GetCurrValue(const AbstrDataItem* adi, SizeT idx)
	-> typename DataArrayBase<V>::value_type
{
	DataReadLock lck(adi);
	return const_array_cast<V>(adi)->GetIndexedValue(idx);
}

template <typename V>
auto GetValue(const TreeItem* ti, SizeT idx)
	-> typename DataArrayBase<V>::value_type
{
	return GetValue<V>(checked_valcast<const AbstrDataItem*>(ti), idx);
}

template <typename V>
auto GetCurrValue(const TreeItem* ti, SizeT idx)
	-> typename DataArrayBase<V>::value_type
{
	return GetCurrValue<V>(checked_valcast<const AbstrDataItem*>(ti), idx);
}

template <typename V>
auto GetTheValue(const AbstrDataItem* adi)
	-> typename DataArrayBase<V>::value_type
{
	assert(adi);
	if (!adi->HasVoidDomainGuarantee())
	{
		auto adu = adi->GetAbstrDomainUnit();
		throwDmsErrF("GetTheValue was called on {}, which assumes it to have a void domain, but it has a domain {}, which is non-void and thus has or could have multiple values"
			, adi->GetSourceName()
			, adu ? adu->GetSourceName() : SharedStr("Undefined")
		);
	}
	return GetValue<V>(adi, 0);
}

template <typename V>
auto GetTheValue(const TreeItem* ti)
	-> typename DataArrayBase<V>::value_type
{
	assert(ti);
	return GetTheValue<V>(checked_valcast<const AbstrDataItem*>(ti));
}

template <typename V, typename TI>
auto GetTheCurrValue(const TI* item)
	-> typename DataArrayBase<V>::value_type
{
	return GetCurrValue<V>(item, 0);
}

//----------------------------------------------------------------------
// GetValue, SetValue, TODO G8 MOVE to separate header file
//----------------------------------------------------------------------


template <typename V>
void SetValue(AbstrDataItem* adi, row_id index, typename DataArrayBase<V>::param_t value)
{
	DataWriteLock dataHolder(adi, dms_rw_mode::read_write);
	assert(adi->m_DataLockCount < 0);

	dataHolder->SetValue<V>(index, value);
	adi->m_DataObject = std::move(dataHolder);
}

template <typename V>
void SetTheValue(AbstrDataItem* adi, typename DataArrayBase<V>::param_t value)
{
	DataWriteLock dataHolder(adi);
	MG_CHECK(dataHolder->GetTiledRangeData()->GetRangeSize() == 1);
	assert(adi->m_DataLockCount < 0);

	dataHolder->SetValue<V>(0, value);
	dataHolder.Commit();
}

#endif // __TIC_DATAARRAYVALUE_H
