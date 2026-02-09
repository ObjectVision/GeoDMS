// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_DISPLAYVALUE_H)
#define __TIC_DISPLAYVALUE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

class AbstrDataItem;
class AbstrUnit;
class AbstrValue;
#include "ptr/InterestHolders.h"
#include "ptr/SharedStr.h"

struct TextInfo {
	SharedStr m_Text;
	bool      m_Grayed;
};

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

struct SharedDataItemInterestPtrTuple {
	SharedDataItemInterestPtr m_ThemeAttr, m_DomainLabel, m_valuesLabel;
};

TIC_CALL SharedStr DisplayValue(const AbstrDataItem* adi, SizeT index,             bool useMetric, SharedDataItemInterestPtrTuple& ippHolders, streamsize_t maxLen, GuiReadLockPair& labelLocks);
TIC_CALL SharedStr DisplayValue(const AbstrUnit*     au,  SizeT index,             bool useMetric, SharedDataItemInterestPtr&     ipHolder, streamsize_t maxLen, GuiReadLock& lock);
TIC_CALL SharedStr DisplayValue(const AbstrUnit*     au,  const AbstrValue* value, bool useMetric, SharedDataItemInterestPtr&     ipHolder, streamsize_t maxLen, GuiReadLock& lock);

extern "C" {

TIC_CALL CharPtr DMS_CONV DMS_AbstrDataItem_DisplayValue(const AbstrDataItem* adi, UInt32 index, bool useMetric);

}

#endif // !defined(__TIC_DISPLAYVALUE_H)

