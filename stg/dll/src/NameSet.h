// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__STG_NAMESET_H)
#define __STG_NAMESET_H

#include "RtcBase.h"
#include <map>

#include "set/IndexedStrings.h"

#include "AbstrUnit.h"

//  -----------------------------------------------------------------------

inline bool lex_caseinsensitive_compare(CharPtr f1, CharPtr l1, CharPtr f2, CharPtr l2)
{
	assert(f1 && l1 || f1 == l1);
	assert(f2 && l2 || f2 == l2);

	SizeT
		sz1 = l1 - f1,
		sz2 = l2 - f2;
	SizeT sz_min = Min<SizeT>(sz1, sz2);
	if (!sz_min)
	{
		assert(f1 == l1 || f2 == l2);
		return sz2;
	}
	assert(f1 && l1 && f2 && l2 && f1 != l1 && f2 != l2);
	int cmpRes = strnicmp(f1, f2, sz_min);
	return (cmpRes < 0)
		|| (cmpRes == 0 && sz1 < sz2);
}

inline bool lex_caseinsensitive_compare(CharPtr f1, CharPtr f2)
{
	if (!f1 || !*f1)
		return f2 && *f2;
	if (!f2)
		return false;
	return stricmp(f1, f2) < 0;
}

struct SharedPtrInsensitiveCompare {
	bool operator ()(CharPtr lhs, CharPtr rhs) const
	{
		return lex_caseinsensitive_compare(lhs, rhs);
	}


	bool operator ()(const SharedStr& lhs, CharPtr rhs) const { return operator ()(lhs.begin(), rhs); }
	bool operator ()(CharPtr lhs, const SharedStr& rhs) const { return operator ()(lhs, rhs.begin()); }
	bool operator ()(const SharedStr& lhs, const SharedStr& rhs) const { return operator ()(lhs.begin(), rhs.begin()); }
	typedef std::true_type is_transparent;
};


// ------------------------------------------------------------------------
//
// TNameSet for mapping to/from limited namespace
//
// ------------------------------------------------------------------------

struct TNameSet : SharedBase
{
	STGDLL_CALL TNameSet(UInt32 len);

	void Release() const { if (!DecRef()) delete this;	}

	void InsertItem(const AbstrDataItem* ti);
	STGDLL_CALL SharedStr InsertFieldName(CharPtr fieldName);
	void InsertIfColumn(const TreeItem* ti, const AbstrUnit* tableDomain);

	STGDLL_CALL SharedStr FieldNameToMappedName(CharPtr fieldName) const;
	STGDLL_CALL SharedStr FieldNameToItemName(CharPtr fieldName) const;
	SharedStr ItemNameToFieldName(CharPtr itemName) const;
	STGDLL_CALL SharedStr ItemNameToMappedName(CharPtr itemName) const;

	CharPtr GetItemName(CharPtr fieldName) const;

	UInt32 GetMappedNameBufferLength() const { return m_Len+1; }

	STGDLL_CALL static bool EqualName(CharPtr n1, CharPtr n2);

private:
	bool HasMappedName(CharPtr name);

	UInt32 m_Len;

	std::map < SharedStr, Couple<SharedStr>, SharedPtrInsensitiveCompare> m_Mappings;
};


#endif __STG_NAMESET_H
