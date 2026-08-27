// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

// The icon set of the GeoDMS GUI: glyphs from the remixicon font, rendered to pixmaps and kept.

#if !defined(__QTGUI_DMSICONS_H)
#define __QTGUI_DMSICONS_H

#include <QFont>
#include <QPixmap>

#include "TreeItemUtils.h"

// ==== glyph icons (issues #319, #1220) ====
//
// The icons are glyphs from the remixicon font that MainWindow loads, not bitmaps: adding one
// costs a codepoint here instead of a pair of 16x16 .bmp files -- the icon and a greyed-out twin
// for items within a template -- plus their entries in GeoDmsGuiQt.qrc. That per-icon cost is
// why the TreeView had no icon for a grid domain or a base unit for so long. The glyphs also
// stay sharp when Windows scales the GUI, where the bitmaps were resampled.
//
// The TreeView (issue #319) and the View and Window menus and the window titles (issue #1220)
// all draw from this one set, so that an item and a view opened on it wear the same icon: an
// attribute you can map and the map view it opens both carry the earth.
//
// Which icon a tree item gets is decided by GetItemIconKind in rtc, next to the GetItemOrigin
// rule that decides its color (issue #1159), so a second view can show the same icons.

// A glyph from remixicon, optionally with a letter set inside it in the application font.
// remixicon carries a lettered box for 't' alone, so a function -- which is a definition like a
// template and wants to look like one -- gets the empty box plus an 'F' of our own.
struct dms_icon_glyph
{
	char16_t glyph;  // codepoint in the private use area of :/res/fonts/remixicon.ttf
	char     letter; // set inside the glyph, 0 for none
};

// remixicon at the default point size; a caller that paints a glyph itself sets the pixel size
// it needs. MainWindow registers the face with QFontDatabase before the first widget asks.
QFont CreateRemixFont();

// The glyph in that color, at dms_params::treeitem_icon_size and the current device pixel ratio.
auto GetGlyphPixmap(dms_icon_glyph icon, DmsColor color) -> QPixmap;

// The icon for what a tree item IS, in the color GetItemIconColor gives that kind; within a
// template in the grey that the _bw twin of each bitmap used to express.
auto GetItemIconPixmap(item_icon_kind kind, bool isInTemplate) -> QPixmap;

#endif // __QTGUI_DMSICONS_H
