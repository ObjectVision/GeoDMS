// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StoragePCH.h"
#include "ImplMain.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// TNameSet: mapping of tree-item names to/from a storage's limited column
// namespace (length-limited, case-insensitive, uniquified).
// Split from stg DllMain.cpp (2026-08); the class is declared in NameSet.h.

#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "vt/Round.h"
#include "geom/Transform.h"
#include "vt/Conversions.h"
#include "mci/CompositeCast.h"
#include "mci/ValueClass.h"
#include "mci/ValueWrap.h"
#include "utl/Environment.h"
#include "utl/StrFormat.h"
#include "utl/Encodes.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "DataLocks.h"
#include "Metric.h"
#include "Projection.h"
#include "PropFuncs.h"
#include "TreeItemClass.h"
#include "TreeItemProps.h"
#include "Unit.h"
#include "UnitClass.h"

#if defined(_MSC_VER)
#include <minmax.h>
#include <share.h>
#endif

#include "NameSet.h"
#include "ViewPortInfoEx.h"
#include "GridStorageManager.h"

const AbstrDataItem* TreeItem_AsColumnItem(const TreeItem* ti, bool allowVoid, bool readOnly); // defined in DllMain.cpp

// ------------------------------------------------------------------------
//
// TNameSet for mapping to/from limited namespace
//
// ------------------------------------------------------------------------

SharedStr ToUpperCase(SharedStr src)
{
	for (auto& ch: src) // non-const begin and end quarantee unique access
		ch = std::toupper(ch);
	return src;
}

TNameSet::TNameSet(const UInt32 len)
	:	m_Len(len)
	{}

void TNameSet::InsertIfColumn(const TreeItem* storageHolder, const AbstrUnit* tableDomain)
{
	bool readOnly = storageReadOnlyPropDefPtr->GetValue(storageHolder);

	const AbstrDataItem* adi = TreeItem_AsColumnItem(storageHolder, true, readOnly);
	if (!adi)
		return;

	if (TableDomain_IsAttr(tableDomain, adi))
		InsertItem(adi);
}

bool TNameSet::EqualName(CharPtr n1, CharPtr n2)
{
	return !stricmp(n1, n2);
}

void NextName(SharedStr& name)
{
	auto i = name.send();
	while (i != name.begin())
	{
		--i;
		if (*i < '0' || *i > '9')
		{
			*i = '0';
			return;
		}
		if (*i < '9')
		{
			++*i;
			return;
		}
		*i = '0';
	}
	throwErrorD(nullptr, "Namespace Full");
}

void TNameSet::InsertItem(const AbstrDataItem* adi)
{
	SharedStr name = SharedStr(adi->GetID());
	auto name2 = name;
	if (name2.ssize() > m_Len)
		name2 = SharedStr(CharPtrRange(name.begin(), name.begin() + m_Len));

	while (HasMappedName(name2.c_str()))
		if (name2.ssize() < m_Len)
			name2 += '0';
		else
			NextName(name2);

	m_Mappings[name] = Couple<SharedStr>(name2, ToUpperCase(name2));
}

bool TNameSet::HasMappedName(CharPtr name)
{
	for (auto& mapping : m_Mappings)
		if (EqualName(mapping.second.first.c_str(), name))
			return true;
	return false;
}


SharedStr TNameSet::FieldNameToMappedName(CharPtr src) const
{
	dms_assert(m_Len > 1);
	dms_assert(strlen(src) <= m_Len);

	std::unique_ptr<char[]> dst(new char[GetMappedNameBufferLength()]);

	char* dstPtr = dst.get();
	char* dstEnd = dstPtr + m_Len;


	if (isdigit(* src)) // first character cannot be numerical
		*dstPtr++ = '_';

	while(dstPtr != dstEnd)
	{
		char ch = *src;
		if (!ch)
			break;

		if (itemNameNextChar_test(ch))
			*dstPtr++ = ch;
		else
			*dstPtr++ = '_';
		++src;
	}
	*dstPtr = char(0);
	return SharedStr(CharPtrRange(dst.get(), dstPtr));
}

SharedStr TNameSet::FieldNameToItemName(CharPtr fieldName) const
{
	SharedStr mappedName = FieldNameToMappedName(fieldName);
	for (auto& mapping : m_Mappings)
		if (EqualName(mapping.second.first.c_str(), mappedName.c_str()))
			return mapping.first;
	throwErrorF("TNameSet", "unknown MappedName {0} for FieldName {1}", mappedName, fieldName);
}

SharedStr TNameSet::ItemNameToFieldName(CharPtr itemName) const
{
	auto posIter = m_Mappings.find(itemName);
	if (posIter == m_Mappings.end())
		throwErrorF("TNameSet", "unknown ItemName {0}", itemName);
	return posIter->second.second;
}

SharedStr TNameSet::ItemNameToMappedName(CharPtr itemName) const
{
	auto posIter = m_Mappings.find(itemName);
	if (posIter == m_Mappings.end())
		return{};
	return posIter->second.first;
}

SharedStr TNameSet::InsertFieldName(CharPtr fieldName)
{
	SharedStr mappedName = FieldNameToMappedName(fieldName);
	if (HasMappedName(mappedName.c_str()))
	{
		throwErrorF("TNameSet", "NameConflict between {0} and FieldName {1}", mappedName, fieldName);
	}
	m_Mappings[mappedName] = Couple<SharedStr>(mappedName, SharedStr(fieldName));
	return mappedName;
}


CharPtr TNameSet::GetItemName(CharPtr fieldName) const
{
	auto mappedName = FieldNameToMappedName(fieldName);

	for (auto & mapping: m_Mappings)
	{
		if ( EqualName(mapping.second.first.c_str(), mappedName.c_str()))
			return mapping.first.begin();
	}
	return nullptr;
}

