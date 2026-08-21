// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#pragma once

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

#if !defined(__TIC_TREEITEMUTILS_H)
#define __TIC_TREEITEMUTILS_H

#include "xct/DmsException.h"
#include "vt/color.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "TreeItem.h"
#include "TreeItemProps.h"

#include <array>

auto _GetHistoricUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>;
auto _GetCurrUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>;
auto _GetCurrRangeItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>;
auto _GetUltimateItem(const TreeItem* ti) noexcept -> std::shared_ptr<const TreeItem>;

bool HasVisibleSubItems(const TreeItem* refItem) noexcept;

//----------------------------------------------------------------------
// item origin and its text color
//----------------------------------------------------------------------

// The (static) origin of an item, i.e. where its value(s) come from. It determines the color
// an item is rendered in, both as a name in the TreeView and as a column caption and column
// values in a TableView (issue #1159), so that a user recognizes a read-in attribute as such
// wherever it is shown.
enum class item_origin
{
	container,    // no associated value(s)
	calculated,   // has a calculation rule
	exogenic,     // read from a storage, i.e. from a database or a file
	template_def, // defined within a template

	count
};

TIC_CALL item_origin GetItemOrigin(const TreeItem* ti);

// Default text colors per origin. The GUI reads overrides from the registry section "Colors"
// (see LoadColors in qtgui) and pushes them here through SetItemOriginTextColor, so that the
// TreeView and the TableView keep using the same colors.
constexpr std::array<DmsColor, UInt32(item_origin::count)> sc_DefaultOriginTextColors =
{
	CombineRGB(0x30, 0x30, 0x30), // container:    dark grey
	CombineRGB(0x00, 0x00, 0x00), // calculated:   black
	CombineRGB(0x1F, 0x4E, 0x79), // exogenic:     dark blue
	CombineRGB(0x6A, 0x3D, 0x9A), // template_def: purple
};

TIC_CALL DmsColor GetItemOriginTextColor(item_origin io);
TIC_CALL DmsColor GetItemOriginTextColor(const TreeItem* ti);
TIC_CALL void     SetItemOriginTextColor(item_origin io, DmsColor clr);

//----------------------------------------------------------------------

TreeItem* CheckedAs(TreeItem* self, const Class* requiredClass);

auto CreateAndInitItem(TreeItem* self, TokenID id, const Class* requiredClass) -> SharedMutableTreeItem;

NotificationCode NotificationCodeFromProblem(FailType ft);

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
