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

#if !defined(__RTC_MCI_VALUEWRAP_H)
#define __RTC_MCI_VALUEWRAP_H

#include "dbg/Check.h"
#include "mci/AbstrValue.h"
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
	template <typename U> void Set(U&& value) { m_Value = Convert<T>(std::forward<U>(value)); }
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

