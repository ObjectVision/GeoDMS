// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SymPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "assoc.h"

#include "ser/FormattedStream.h"


AssocPtr AssocPtr::failed() {
	static  AssocPtr result = AssocPtr(LispPtr());
	return result;
}

auto AssocList::empty() -> AssocListPtr
{
	static AssocListPtr result = AssocListPtr(LispPtr());
	return result;
}

auto AssocList::failed() ->AssocListPtr
{
	static AssocList assocListFailed = AssocList(AssocPtr::failed(), AssocList::empty());
	return AssocListPtr(assocListFailed.get());
}

/**************** operators      *****************/

FormattedOutStream& operator <<(FormattedOutStream& output, AssocPtr a)
{
	return output << a.Key() << CharPtr(" = ") << a.Val();
}
