// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_MCI_VALUEWRAP_H)
#define __RTC_MCI_VALUEWRAP_H

#include "dbg/Check.h"
#include "mci/AbstrValue.h"
#include "mem/ManagedAllocData.h"
#include "geo/BitValue.h"
#include "geo/SequenceTraits.h"
#include "geo/SequenceArray.h"

template<typename T, typename U> struct equal_type : std::false_type {};
template<typename TU> struct equal_type<TU, TU> : std::true_type {};

template <class T>
class ValueWrap : public AbstrValue
{
	typedef AbstrValue base_type;
public:
	typedef typename sequence_traits<T>::value_type value_type;
	typedef typename param_type<value_type>::type   cref_type;
	RTC_CALL ValueWrap();
	explicit ValueWrap(cref_type val) : m_Value(val) {}
	
	// property access
	cref_type Get() const    { return m_Value;  }
	void SetValue(T value) { m_Value = std::move(value); }
	void Clear() { m_Value = UNDEFINED_OR_ZERO(T); }

	// override virtuals fo AbstrValue
	RTC_CALL const     ValueClass* GetValueClass() const override { return GetStaticClass(); }

	RTC_CALL bool      AsCharArray(char* buffer, SizeT bufLen, FormattingFlags ff) const override;
	RTC_CALL SizeT     AsCharArraySize(SizeT maxLen, FormattingFlags ff) const override;
	RTC_CALL SharedStr AsString() const override;

	RTC_CALL Float64   AsFloat64() const override;
	RTC_CALL bool      IsNull() const override;

	RTC_CALL void      AssignFromCharPtr(CharPtr data) override;
	RTC_CALL void      AssignFromCharPtrs(CharPtr first, CharPtr last) override;

private:
	value_type m_Value;
	DECL_RTTI(RTC_CALL, ValueClass)
};

//template<> class ValueWrap<bool> : public ValueWrap<Bool> {};

#endif // __RTC_MCI_VALUEWRAP_H

