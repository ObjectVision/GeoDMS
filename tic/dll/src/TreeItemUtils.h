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

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

#if !defined(__TIC_TREEITEMUTILS_H)
#define __TIC_TREEITEMUTILS_H

#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "TreeItem.h"
#include "TreeItemProps.h"

TIC_CALL const TreeItem* _GetHistoricUltimateItem(const TreeItem* ti);
TIC_CALL const TreeItem* _GetCurrUltimateItem(const TreeItem* ti);
TIC_CALL const TreeItem* _GetCurrRangeItem(const TreeItem* ti);
TIC_CALL const TreeItem* _GetUltimateItem(const TreeItem* ti);

TIC_CALL bool HasVisibleSubItems(const TreeItem* refItem);

//----------------------------------------------------------------------

TIC_CALL TokenID TreeItem_GetFindableNameID(const TreeItem* self, const TreeItem* subItem);

TIC_CALL TreeItem* CheckedAs(TreeItem* self, const Class* requiredClass);

TIC_CALL TreeItem* CreateAndInitItem(TreeItem* self, TokenID id, const Class* requiredClass);

TIC_CALL NotificationCode NotificationCodeFromProblem(FailType ft);

TIC_CALL SharedStr GetPartialName(const TreeItem* themeDisplayItem, UInt32 nameLevel);

template<typename Func>
SharedStr GetDisplayNameWithinContext(const AbstrDataItem* item, bool inclMetric, Func peerIter)
{
	if (item->IsCacheItem())
		return item->GetFullName();

	SharedStr resultLabel = TreeItemPropertyValue(item, labelPropDefPtr);
	UInt32 nameLevel = resultLabel.empty() ? 1 : 0;

again:
	SharedStr result = GetPartialName(item, nameLevel);

	if (nameLevel < 10) // avoid unintended looping
	{
		Func peer2 = peerIter;
		while (true) {
			const AbstrDataItem* sibblingDisplayItem = peer2();
			if (!sibblingDisplayItem)
				break;

   			if (sibblingDisplayItem != item && !sibblingDisplayItem->IsCacheItem())
			{
				if (resultLabel.empty() || resultLabel == TreeItemPropertyValue(sibblingDisplayItem, labelPropDefPtr))
					if (result == GetPartialName(sibblingDisplayItem, nameLevel))
					{
						++nameLevel;
						goto again;
					}
			}
		}
	}
	if (!resultLabel.empty())
	{
		if (result.empty())
			result = resultLabel;
		else
			result = resultLabel + " " + result;
	}

	if (inclMetric)
		return result + item->GetAbstrValuesUnit()->GetFormattedMetricStr();

	return result;
}

TIC_CALL const AbstrDataItem* GeometrySubItem(const TreeItem* ti);
TIC_CALL bool IsThisMappable(const TreeItem* ti);
TIC_CALL auto GetMappingItem(const TreeItem* ti) -> const TreeItem*;

#endif !defined(__TIC_TREEITEMUTILS_H)
