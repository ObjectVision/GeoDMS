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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TreeItemUtils.h"

#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "AbstrCalculator.h"
#include "LispTreeType.h"
#include "StateChangeNotification.h"
#include "TreeItemUtils.h"
#include "Unit.h"

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// functions 
//----------------------------------------------------------------------

const TreeItem* _GetHistoricUltimateItem(const TreeItem* ti)
{
	dms_assert(ti);

	while (true)
	{
		const TreeItem* refItem = ti->mc_RefItem;
		if (!refItem)
			return ti;
		ti = refItem;
	}
}

const TreeItem* _GetCurrUltimateItem(const TreeItem* ti)
{
	dms_assert(ti);
	dbg_assert(ti->CheckMetaInfoReadyOrPassor());

	return _GetHistoricUltimateItem(ti);
}

const TreeItem* _GetCurrRangeItem(const TreeItem* ti)
{
	return _GetCurrUltimateItem(ti);
/*
	dms_assert(ti);
	dbg_assert(ti->CheckMetaInfoReadyOrPassor());
	if (!IsUnit(ti))
		return _GetCurrUltimateItem(ti);
	auto u = AsUnit(ti);
	while (true)
	{
		if (u->HasVarRangeData()) // range set in combination with calculation rule
			return u;

		const TreeItem* refItem = u->mc_RefItem;
		if (!refItem)
			return u;
		dms_assert(IsUnit(refItem));
		u = AsUnit(refItem);
	}
*/
}

const TreeItem* _GetUltimateItem(const TreeItem* ti)
{
	dms_assert(ti);
	while (true)
	{
		const TreeItem* refItem = ti->GetReferredItem();
		if (!refItem)
			return ti;
		ti = refItem;
	}
}

bool HasVisibleSubItems(const TreeItem* refItem)
{
	while (true)
	{
		if (refItem->HasSubItems())
			return true;
		const TreeItem* ref2 = refItem->GetReferredItem();
		if (!ref2)
			return false;
		refItem = ref2;
	}
}


NotificationCode NotificationCodeFromProblem(FailType ft)
{
	switch (ft)
	{
	case FR_Data:  return NC2_DataFailed;
	case FR_Validate:  return NC2_CheckFailed;
	case FR_Committed: return NC2_CommitFailed;
	}
	return NC2_MetaFailed;
}

SharedStr GetPartialName(const TreeItem* themeDisplayItem, UInt32 nameLevel)
{
	dms_assert(themeDisplayItem);
	dms_assert(!themeDisplayItem->IsCacheItem());

	const TreeItem* themeDisplayItemCopy = themeDisplayItem;
	SharedStr result;

	for (; nameLevel; --nameLevel)
	{
		const TreeItem* parent = themeDisplayItem->GetTreeParent();
		if (!parent)
			return themeDisplayItemCopy->GetFullName();

		result = DelimitedConcat(themeDisplayItem->GetName().c_str(), result.c_str());
		themeDisplayItem = parent;
		dms_assert(themeDisplayItem); // not root
	}
	return result;
}

const AbstrDataItem* GeometrySubItem(const TreeItem* ti)
{
	dms_assert(ti);
	ti->UpdateMetaInfo();
	const TreeItem* si = const_cast<TreeItem*>(ti)->GetSubTreeItemByID(token::geometry);
	if (!IsDataItem(si))
		return nullptr;
	auto gi = AsDataItem(si);
	if (gi->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
		return nullptr;
	return gi;
}

#include "PropFuncs.h"

bool IsThisMappable(const TreeItem* ti)
{
	dms_assert(ti);
	return HasMapType(ti) || GeometrySubItem(ti);
}

auto GetMappingItem(const TreeItem* ti) -> const TreeItem*
{
	dms_assert(ti); // PRECONDITION
	do
	{
		dms_assert(!SuspendTrigger::DidSuspend());
		if (IsThisMappable(ti))
			return ti;
		ti = ti->GetReferredItem();
	} while (ti);
	return nullptr;
}


