// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__SHV_CLASSBREAKCLIPBOARD_H)
#define __SHV_CLASSBREAKCLIPBOARD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ShvBase.h"

#include "ptr/SharedStr.h"
#include "vt/color.h"

#include "ValuesTableTypes.h" // break_array

#include <vector>

class AbstrDataItem;
class AbstrUnit;
class DataView;
class GraphicLayer;
class Theme;

//----------------------------------------------------------------------
// struct : ClassBreakClip
//----------------------------------------------------------------------

// The transferable contents of a classification (issue #734): the class-break values plus,
// when available, the per-class colors and labels. m_Colors and m_Labels are either empty
// or exactly as long as m_Breaks.
struct ClassBreakClip
{
	break_array            m_Breaks;
	std::vector<DmsColor>  m_Colors;
	std::vector<SharedStr> m_Labels;

	SharedStr m_SrcThemeName;      // provenance; only used for diagnostics
	SharedStr m_SrcValuesUnitName;

	bool  empty() const { return m_Breaks.empty(); }
	SizeT size () const { return m_Breaks.size (); }
};

//----------------------------------------------------------------------
// the text codec
//----------------------------------------------------------------------

// Renders breakAttr (and the optional colorAttr / labelAttr) as the clipboard text format:
//
//	# GeoDMS ClassBreaks v1
//	# ThemeAttr: /Analysis/Density
//	# ValuesUnit: /Units/per_km2
//	"ClassBreak";"Color";"Label"
//	0;rgb(255,255,204);"0 ... 50"
//
// colorAttr, labelAttr and themeAttr may be null; their columns are then simply absent.
SharedStr ClassBreaks_ToText(const AbstrDataItem* breakAttr, const AbstrDataItem* colorAttr, const AbstrDataItem* labelAttr, const AbstrDataItem* themeAttr);

// Parses the text format above. Deliberately tolerant, so that a classification table copied
// from a spreadsheet or from another GIS also lands: the separator (tab, semicolon or comma) is
// sniffed per text, a header line is optional, '#' lines are metadata, and a bare column of
// numbers is a valid class-break list. Returns false and fills diagnostic when nothing parsed.
bool ClassBreaks_FromText(CharPtr first, CharPtr last, ClassBreakClip& result, SharedStr& diagnostic);

//----------------------------------------------------------------------
// the clipboard
//----------------------------------------------------------------------

bool ClassBreaks_ToClipboard  (const AbstrDataItem* breakAttr, const AbstrDataItem* colorAttr, const AbstrDataItem* labelAttr, const AbstrDataItem* themeAttr);
bool ClassBreaks_FromClipboard(ClassBreakClip& result, SharedStr& diagnostic);

// Cheap sniff used to enable or gray the Paste menu entries; does not fully parse.
bool ClipBoard_HasClassBreaks();

//----------------------------------------------------------------------
// locating the class attributes of a theme
//----------------------------------------------------------------------

// The class attributes of one classification: the palette domain that carries the class count,
// the class-break attribute and the color / label attributes defined on that domain.
struct ClassBreakItems
{
	SharedUnitInterestPtr            m_PaletteDomain;
	SharedMutableDataItemInterestPtr m_BreakAttr;
	SharedMutableDataItemInterestPtr m_ColorAttr;
	SharedMutableDataItemInterestPtr m_LabelAttr;

	explicit operator bool() const { return m_BreakAttr && m_PaletteDomain; }
};

ClassBreakItems ClassBreaks_GetItems(const Theme* theme);

// True when the class attributes were generated into the /Desktops/... tree and carry no
// calculation rule, i.e. when overwriting them affects nothing but this view (issue #734).
bool ClassBreaks_IsOverwritable(const DataView* dv, const ClassBreakItems& items);

//----------------------------------------------------------------------
// applying a clip
//----------------------------------------------------------------------

// Overwrites the given class attributes with clip. Resets the class count on the palette domain
// first and only then takes the DataWriteLocks, and stamps every change with one epoch tick.
// Throws when items is not overwritable, so callers check first or use ClassBreaks_ApplyToLayer.
void ClassBreaks_ApplyToItems(DataView* dv, const ClassBreakItems& items, const AbstrDataItem* themeAttr, const ClassBreakClip& clip);

// Applies clip to the active theme of layer. Config-owned or calculated class attributes are
// left alone: a fresh set is generated under /Desktops/... the way a missing classification is
// instantiated, and the layer is re-themed onto it. Returns true when that regeneration happened,
// which tells an open palette editor that it now shows a stale classification.
bool ClassBreaks_ApplyToLayer(DataView* dv, GraphicLayer* layer, const ClassBreakClip& clip);

#endif // !defined(__SHV_CLASSBREAKCLIPBOARD_H)
