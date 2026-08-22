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
// unit predicates
//----------------------------------------------------------------------

// A grid domain: a domain over a 2-dimensional value type, such as unit<spoint> or unit<upoint>.
// Both properties are compile-time properties of the value type, so this costs nothing and is
// answerable whatever state the unit is in. The CanBeDomain half is what keeps a 2-dimensional
// *coordinate* unit such as unit<fpoint> rdc, which is a values unit and not a grid, out.
TIC_CALL bool IsGridDomain(const AbstrUnit* au);

// A base unit, i.e. what BaseUnit(symbol, valuetype) yields: a metric of exactly one symbol,
// with exponent 1 and no scale factor (see AbstrBaseUnitOperator in clc/OperUnit.cpp). A derived
// unit such as m/s or 1000 * m answers false, as does a unit that is not far enough along for
// its metric to be known.
TIC_CALL bool IsBaseUnit(const AbstrUnit* au);

// The CRS this item is in, following the projection chain, or an empty TokenID when the item is
// not a unit, is not georeferenced, or is not far enough along to tell. Unlike
// AbstrUnit::GetCurrSpatialReference this is safe to call in any state, which is what a painter
// needs; see doc/development/crs-metric-decoupling.md for the Curr/local distinction.
TIC_CALL TokenID GetItemSpatialReference(const TreeItem* ti);

//----------------------------------------------------------------------
// item icon kind
//----------------------------------------------------------------------

// What an item IS, as far as a view has to draw it, and thus which icon it gets. Not to be
// confused with ViewStyleFlags, which says which views can be opened ON an item. The tree view
// derived its icons from those flags until issue #319, which is why a unit that happened to have
// subitems was drawn as a container, and why an empty container -- matching no flag at all --
// fell through to the base-unit icon at the end of the chain.
enum class item_icon_kind
{
	container,         // anything that is neither a unit nor a data item, subitems or not
	container_table,   // ... whose subitems share a domain, so it can be shown as one table
	template_def,      // a template definition
	function_def,      // a function definition -- IsTemplate() holds for these too, see below

	data_item,         // an attribute or a parameter
	data_item_map,     // ... that can be drawn on a map
	data_item_palette, // ... that holds class breaks or a color aspect

	unit_grid_domain,  // a 2-dimensional domain, i.e. a grid
	unit_domain,       // any other domain, i.e. a countable unit
	unit_base,         // a base unit as BaseUnit(symbol, valuetype) yields it
	unit_values,       // any other values unit, i.e. one with a derived or an absent metric

	count
};

// isMapViewable and hasCommonDomain are the two facts this level cannot establish; the GUI reads
// them from SHV_GetViewStyleFlags (vsfMapView resp. vsfTableContainer) and passes them in. The
// rest of the decision lives here, so that every view that shows these icons gets the same
// answer, as with GetItemOrigin above (issue #1159).
TIC_CALL item_icon_kind GetItemIconKind(const TreeItem* ti, bool isMapViewable, bool hasCommonDomain);

// Default icon colors per kind, chosen to stay recognizable next to the bitmaps these icons
// replaced: the container keeps the folder's amber, the map item the globe's blue, the template
// the purple that item_origin::template_def already uses for its name.
constexpr std::array<DmsColor, UInt32(item_icon_kind::count)> sc_DefaultIconColors =
{
	CombineRGB(0xE0, 0xA0, 0x30), // container:         amber
	CombineRGB(0xE0, 0xA0, 0x30), // container_table:   amber
	CombineRGB(0x6A, 0x3D, 0x9A), // template_def:      purple
	CombineRGB(0x6A, 0x3D, 0x9A), // function_def:      purple, a definition like a template is
	CombineRGB(0x45, 0x5A, 0x64), // data_item:         slate
	CombineRGB(0x1F, 0x6F, 0xB2), // data_item_map:     blue
	CombineRGB(0xC2, 0x18, 0x5B), // data_item_palette: magenta
	CombineRGB(0x00, 0x79, 0x6B), // unit_grid_domain:  teal
	CombineRGB(0x00, 0x79, 0x6B), // unit_domain:       teal
	CombineRGB(0x8D, 0x6E, 0x63), // unit_base:         brown
	CombineRGB(0x8D, 0x6E, 0x63), // unit_values:       brown
};

TIC_CALL DmsColor GetItemIconColor(item_icon_kind ik);

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
