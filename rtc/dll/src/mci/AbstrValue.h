// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __MCI_ABSTRVALUE_H
#define __MCI_ABSTRVALUE_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Object.h"
class ValueClass;
enum class FormattingFlags;

//----------------------------------------------------------------------
// class  : AbstrValue
//----------------------------------------------------------------------

class AbstrValue : public Object
{
	using base_type = Object;
public:
	virtual const ValueClass* GetValueClass() const = 0;

	virtual bool      AsCharArray(char* sink, SizeT buflen, FormattingFlags ff) const=0;
	virtual SizeT     AsCharArraySize(SizeT maxLen, FormattingFlags ff) const = 0;
	virtual SharedStr AsString()  const = 0;
	virtual Float64   AsFloat64() const = 0;
	virtual bool      IsNull()    const = 0;

	virtual void AssignFromCharPtr (CharPtr data) =0;
	virtual void AssignFromCharPtrs(CharPtr first, CharPtr last) =0;

	DECL_ABSTR(RTC_CALL, Class)
};

#endif // __MCI_ABSTRVALUE_H

