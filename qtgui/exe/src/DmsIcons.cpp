// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

// The icon set of the GeoDMS GUI; see DmsIcons.h for what it is and why it is glyphs.

#include <QApplication>
#include <QPainter>

#include <algorithm>
#include <map>
#include <tuple>

#include "DmsGuiParameters.h"
#include "DmsIcons.h"

QFont CreateRemixFont()
{
	QFont font;
	font.setFamily("remixicon");
	return font;
}

// In item_icon_kind order.
static const dms_icon_glyph sc_IconGlyphs[] =
{
	{ u'\uED6A', 0   }, // container:         folder-line
	{ u'\uED5E', 0   }, // container_table:   folder-chart-line
	{ u'\uF1D3', 0   }, // template_def:      t-box-line
	{ u'\uEB7F', 'F' }, // function_def:      checkbox-blank-line, the same box with an F in it
	{ u'\uEE92', 0   }, // data_item:         layout-left-2-line, one column of the domain's table
	{ u'\uEC7A', 0   }, // data_item_map:     earth-line
	{ u'\uEFC5', 0   }, // data_item_palette: palette-line
	{ u'\uEE8F', 0   }, // unit_grid_domain:  layout-grid-fill
	{ u'\uF1DE', 0   }, // unit_domain:       table-line, the table its attributes form
	{ u'\uF0A3', 0   }, // unit_base:         ruler-line, the 'rolmaat', now only for base units
	{ u'\uF0B9', 0   }, // unit_values:       scales-line
};
static_assert(std::size(sc_IconGlyphs) == UInt32(item_icon_kind::count), "a kind was added to item_icon_kind without a glyph");

static QPixmap RenderIconGlyph(dms_icon_glyph icon, QColor color, int size, qreal dpr)
{
	QPixmap pixmap(QSize(size, size) * dpr);
	pixmap.setDevicePixelRatio(dpr);
	pixmap.fill(Qt::transparent);

	auto font = CreateRemixFont();
	font.setPixelSize(size);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setFont(font);
	painter.setPen(color);
	painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(QChar(icon.glyph)));

	if (icon.letter)
	{
		// the application font is :/res/fonts/dmstext.ttf, which main_qt installs before any
		// widget is built, so the letter is set in the same face as the item names beside it
		auto letterFont = QApplication::font();
		letterFont.setPixelSize(std::max(6, (size * 13) / 24));
		painter.setFont(letterFont);
		painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(QChar(icon.letter)));
	}

	return pixmap;
}

// Keep the rendered glyphs: the TreeView model is asked for the icon of every visible row on
// every repaint, and the delegate then asks a second time for its width; updateWindowMenu builds
// one per open view every time the Window menu drops down. This is the map the TODO in
// DmsTreeView asked for; before it, each of those calls decoded a .bmp from the resources anew.
// The device pixel ratio is part of the key, as it changes when the window is dragged to a
// monitor with another scale factor. Every caller runs on the GUI thread.
auto GetGlyphPixmap(dms_icon_glyph icon, DmsColor color) -> QPixmap
{
	auto dpr = qApp->devicePixelRatio();

	using icon_key = std::tuple<char16_t, char, DmsColor, int>;
	static std::map<icon_key, QPixmap> s_pixmaps;

	auto key = icon_key(icon.glyph, icon.letter, color, int(dpr * 100));
	auto pos = s_pixmaps.find(key);
	if (pos == s_pixmaps.end())
	{
		auto qColor = QColor(GetRed(color), GetGreen(color), GetBlue(color));
		pos = s_pixmaps.emplace(key, RenderIconGlyph(icon, qColor, dms_params::treeitem_icon_size, dpr)).first;
	}
	return pos->second;
}

auto GetItemIconPixmap(item_icon_kind kind, bool isInTemplate) -> QPixmap
{
	assert(kind < item_icon_kind::count);

	auto color = isInTemplate
		? CombineRGB(0x9E, 0x9E, 0x9E) // what the _bw twin of each bitmap used to express
		: GetItemIconColor(kind);

	return GetGlyphPixmap(sc_IconGlyphs[UInt32(kind)], color);
}
