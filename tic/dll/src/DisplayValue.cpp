// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DisplayValue.h"

#include "dbg/DmsCatch.h"
#include "mci/AbstrValue.h"
#include "mci/ValueWrap.h"
#include "geo/Point.h"
#include "utl/mySPrintF.h"

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
	SharedUnitInterestPtr uip = au; // covered by ipHolder soon.
	if (!ipHolder)
	{
		assert(IsMainThread());
		au->PrepareDataUsage(DrlType::Suspendible);
	}

	SizeT i = au->GetIndexForAbstrValue(*valuePtr);
	if (!IsDefined(i))
		return result;
	return mySSPrintF("%s (%s)",
		result.c_str(),
		(i < au->GetCount())
			? au->GetLabelAtIndex(i, ipHolder, maxLen, lock).c_str()
			: "<range-error>"
	);
}

SharedStr DisplayValue(const AbstrDataItem* adi, SizeT index, bool useMetric, SharedDataItemInterestPtrTuple& ippHolders, streamsize_t maxLen, GuiReadLockPair& locks)
{
	ippHolders.m_ThemeAttr = adi;
	if (adi->PrepareDataUsage(DrlType::Suspendible) && IsDataReady(adi->GetUltimateItem()))
	{
		dms_assert(!adi->IsFailed(FR_Data));
		SharedStr result;
		try {
			DataReadLock drl(adi);

			streamsize_t valueLen = drl->AsCharArraySize(index, maxLen, locks.first, FormattingFlags::ThousandSeparator);
			dms_assert(valueLen <= maxLen);

			result = SharedArray<char>::Create(valueLen + 1, false);
			drl->AsCharArray(index, result.begin(), valueLen, locks.first, FormattingFlags::ThousandSeparator);
			result.begin()[valueLen] = char(0);
			if (valueLen < maxLen)
			{
				maxLen -= valueLen;

				auto avu = adi->GetAbstrValuesUnit();
				SharedDataItemInterestPtr labelAttr = avu->GetLabelAttr();
				if (labelAttr)
				{
					OwningPtr<AbstrValue> valuePtr(drl->CreateAbstrValue());
					drl->GetAbstrValue(index, *valuePtr);
					return AsStrWithLabel(avu, valuePtr, useMetric, ippHolders.m_valuesLabel, maxLen, locks.second, result);
				}
				if (useMetric)
					return result + avu->GetFormattedMetricStr();
			}
		}
		catch (...)
		{
			if (adi->IsFailed(FR_Data))
				goto returnFail;
			throw;
		}
		return result;
	}
	if (!adi->IsFailed(FR_Data))
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

	result = SharedArray<char>::Create(valueLen + 1, false);
	valuePtr->AsCharArray(result.begin(), valueLen, FormattingFlags::ThousandSeparator);
	result.begin()[valueLen] = char(0);

	if (maxLen > valueLen)
	{
		maxLen -= valueLen;

		if (!dynamic_cast<const ValueWrap<SharedStr>*>(valuePtr)) // used to indicate error or strings
		{
			if (!ipHolder)
				ipHolder = au->GetLabelAttr();
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
	OwningPtr<AbstrValue> valuePtr = au->CreateAbstrValueAtIndex(index);
	return DisplayValue(au, valuePtr, useMetric, ipHolder, maxLen, lock);
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
