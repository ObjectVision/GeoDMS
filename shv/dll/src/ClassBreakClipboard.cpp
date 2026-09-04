// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Copying a classification from one view and applying it in another (issue #734).
//
// The transport is the clipboard, in a text format that a spreadsheet can also produce and
// consume, so that class breaks travel between GeoDMS sessions and between GeoDMS and other
// tools. The reader is deliberately more permissive than the writer.
//
/////////////////////////////////////////////////////////////////////////////

#include "ShvDllPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ClassBreakClipboard.h"

#include <algorithm>
#include <charconv>
#include <string>

#include "act/UpdateMark.h"
#include "dbg/SeverityType.h"
#include "mci/ValueClass.h"
#include "ptr/SharedTreePtr.h"
#include "ser/AsString.h"
#include "utl/StrFormat.h"

#include "AbstrUnit.h"
#include "Aspect.h"
#include "DataArray.h"
#include "DataLockContainers.h"
#include "DataLocks.h"
#include "PropFuncs.h"
#include "TreeItemProps.h"
#include "UnitClass.h"

#include "CalcClassBreaks.h"

#include "Clipboard.h"
#include "DataView.h"
#include "GraphicLayer.h"
#include "LayerClass.h"
#include "LayerInfo.h"
#include "ShvUtils.h"
#include "Theme.h"

//----------------------------------------------------------------------
// text primitives
//----------------------------------------------------------------------

namespace {

constexpr CharPtr cbcMagicLine    = "# GeoDMS ClassBreaks v1";
constexpr CharPtr cbcThemePrefix  = "# ThemeAttr:";
constexpr CharPtr cbcValuesPrefix = "# ValuesUnit:";

bool IsSpace(char ch) { return ch == ' ' || ch == '\t' || ch == '\r'; }

void Trim(CharPtr& first, CharPtr& last)
{
	while (first != last && IsSpace(*first)) ++first;
	while (last != first && IsSpace(last[-1])) --last;
}

bool EqualNoCase(CharPtr first, CharPtr last, CharPtr zStr)
{
	for (; first != last; ++first, ++zStr)
	{
		if (!*zStr)
			return false;
		if (tolower(UInt8(*first)) != tolower(UInt8(*zStr)))
			return false;
	}
	return !*zStr;
}

bool ContainsNoCase(CharPtr first, CharPtr last, CharPtr zStr)
{
	auto n = StrLen(zStr);
	if (SizeT(last - first) < n)
		return false;
	for (auto p = first; p + n <= last; ++p)
		if (strnicmp(p, zStr, n) == 0)
			return true;
	return false;
}

// Splits one line into unquoted fields. Both '"' quoting (with "" for an embedded quote) and
// '(' ... ')' nesting suspend the separator, so an rgb(r,g,b) literal survives even when the
// separator is a comma. Every field is returned trimmed and unquoted.
auto SplitFields(CharPtr first, CharPtr last, char sep) -> std::vector<SharedStr>
{
	std::vector<SharedStr> result;
	std::string curr;
	UInt32 parenDepth = 0;
	bool inQuotes = false, wasQuoted = false;

	auto flush = [&]()
	{
		CharPtr f = curr.c_str(), l = f + curr.size();
		if (!wasQuoted)
			Trim(f, l);
		result.emplace_back(std::string(f, l));
		curr.clear();
		wasQuoted = false;
	};

	for (auto p = first; ; ++p)
	{
		if (p == last)
		{
			flush();
			break;
		}
		if (!inQuotes && !parenDepth && *p == sep)
		{
			flush();
			continue;
		}
		if (*p == '"')
		{
			if (inQuotes && p + 1 != last && p[1] == '"')
			{
				curr.push_back('"');
				++p;
				continue;
			}
			inQuotes = !inQuotes;
			wasQuoted = true;
			continue;
		}
		if (!inQuotes)
		{
			if (*p == '(')
				++parenDepth;
			else if (*p == ')' && parenDepth)
				--parenDepth;
		}
		curr.push_back(*p);
	}
	return result;
}

// Counts separator candidates outside quotes and parentheses, so that the separator of a pasted
// text can be told from the commas inside an rgb() literal or inside a quoted label.
UInt32 CountSeps(CharPtr first, CharPtr last, char sep)
{
	UInt32 count = 0, parenDepth = 0;
	bool inQuotes = false;
	for (auto p = first; p != last; ++p)
	{
		if (*p == '"') { inQuotes = !inQuotes; continue; }
		if (inQuotes) continue;
		if (*p == '(') ++parenDepth;
		else if (*p == ')' && parenDepth) --parenDepth;
		else if (!parenDepth && *p == sep) ++count;
	}
	return count;
}

bool TryParseFloat64(WeakStr field, Float64& value)
{
	CharPtr f = field.begin(), l = field.send();
	Trim(f, l);
	if (f == l)
		return false;

	auto fromResult = std::from_chars(f, l, value);
	if (fromResult.ec == std::errc() && fromResult.ptr == l)
		return true;

	// a comma decimal separator, as a Dutch spreadsheet writes it; only when there is no
	// competing period and exactly one comma, so that "1,5" parses but "1,234,567" does not.
	auto commaPos = std::find(f, l, ',');
	if (commaPos == l || std::find(commaPos + 1, l, ',') != l || std::find(f, l, '.') != l)
		return false;

	std::string patched(f, l);
	patched[commaPos - f] = '.';
	auto patchedEnd = patched.c_str() + patched.size();
	fromResult = std::from_chars(patched.c_str(), patchedEnd, value);
	return fromResult.ec == std::errc() && fromResult.ptr == patchedEnd;
}

bool IsUndefinedField(WeakStr field)
{
	CharPtr f = field.begin(), l = field.send();
	Trim(f, l);
	return f == l || EqualNoCase(f, l, "null") || EqualNoCase(f, l, "na");
}

SharedStr ColorToText(DmsColor clr)
{
	return mySSPrintF("rgb({},{},{})", UInt32(GetRed(clr)), UInt32(GetGreen(clr)), UInt32(GetBlue(clr)));
}

bool TryParseHex(CharPtr first, CharPtr last, UInt32& value)
{
	if (first == last)
		return false;
	auto fromResult = std::from_chars(first, last, value, 16);
	return fromResult.ec == std::errc() && fromResult.ptr == last;
}

// Recognizes rgb(r,g,b), #RRGGBB and 0xBBGGRR. A bare decimal number is deliberately NOT a
// color: it would make a second numeric column (a count, an area) indistinguishable from one.
bool TryParseColor(WeakStr field, DmsColor& clr)
{
	CharPtr f = field.begin(), l = field.send();
	Trim(f, l);
	if (f == l)
		return false;

	if (*f == '#')
	{
		UInt32 rgb = 0;
		if (!TryParseHex(f + 1, l, rgb))
			return false;
		if (l - f == 7)  // #RRGGBB
		{
			clr = CombineRGB(UInt8(rgb >> 16), UInt8(rgb >> 8), UInt8(rgb));
			return true;
		}
		if (l - f == 4)  // #RGB
		{
			auto expand = [](UInt32 nibble) -> UInt8 { return UInt8(nibble * 0x11); };
			clr = CombineRGB(expand((rgb >> 8) & 0xF), expand((rgb >> 4) & 0xF), expand(rgb & 0xF));
			return true;
		}
		return false;
	}
	if (l - f > 2 && f[0] == '0' && (f[1] == 'x' || f[1] == 'X'))
	{
		UInt32 raw = 0;
		if (!TryParseHex(f + 2, l, raw))
			return false;
		clr = raw & MAX_COLOR;
		return true;
	}
	if (l - f > 5 && EqualNoCase(f, f + 4, "rgb(") && l[-1] == ')')
	{
		auto components = SplitFields(f + 4, l - 1, ',');
		if (components.size() != 3)
			return false;
		Float64 rgb[3];
		for (int i = 0; i != 3; ++i)
			if (!TryParseFloat64(components[i], rgb[i]) || rgb[i] < 0 || rgb[i] > 255)
				return false;
		clr = CombineRGB(UInt8(rgb[0]), UInt8(rgb[1]), UInt8(rgb[2]));
		return true;
	}
	return false;
}

void AppendStr(std::string& out, CharPtr zStr) { out.append(zStr); }

void AppendQuoted(std::string& out, WeakStr str)
{
	out.push_back('"');
	for (CharPtr p = str.begin(), e = str.send(); p != e; ++p)
	{
		if (*p == '"')
			out.push_back('"');
		out.push_back(*p);
	}
	out.push_back('"');
}

// The role a parsed column plays. Determined from the header line when there is one, and from
// the contents of the first data row when there is not.
enum class ColRole { Ignore, Breaks, Color, Label };

} // anonymous namespace

// How the values unit of a classification is named in the clipboard text. A default unit has no
// full name, in which case its value type is the only thing worth recording.
static SharedStr ValuesUnitLabel(const AbstrUnit* valuesUnit)
{
	if (!valuesUnit)
		return {};
	auto result = valuesUnit->GetFullName();
	if (result.empty() && valuesUnit->GetValueType())
		result = SharedStr(valuesUnit->GetValueType()->GetName());
	return result;
}

//----------------------------------------------------------------------
// the text codec
//----------------------------------------------------------------------

SharedStr ClassBreaks_ToText(const AbstrDataItem* breakAttr, const AbstrDataItem* colorAttr, const AbstrDataItem* labelAttr, const AbstrDataItem* themeAttr)
{
	assert(breakAttr);

	auto paletteDomain = breakAttr->GetAbstrDomainUnit();
	assert(paletteDomain);

	PreparedDataReadLock breakLock(breakAttr, "ClassBreaks_ToText");
	auto breakObj = breakAttr->GetRefObj();
	if (!breakObj)
		return {};

	// a color or label attribute that cannot be prepared is simply left out of the text
	DataReadLockContainer optionalLocks;
	auto lockOptional = [&optionalLocks](const AbstrDataItem* adi) -> SharedPtr<const AbstrDataObject>
	{
		if (!adi)
			return {};
		try {
			optionalLocks.push_back(PreparedDataReadLock(adi, "ClassBreaks_ToText"));
			return adi->GetRefObj();
		}
		catch (...) {
			return {};
		}
	};
	auto colorObj = lockOptional(colorAttr);
	auto labelObj = lockOptional(labelAttr);

	SizeT n = paletteDomain->GetCount();

	std::string result;
	AppendStr(result, cbcMagicLine); result.push_back('\n');
	if (themeAttr)
	{
		AppendStr(result, cbcThemePrefix);
		result.push_back(' ');
		AppendStr(result, themeAttr->GetFullName().c_str());
		result.push_back('\n');
	}
	if (auto valuesUnitLabel = ValuesUnitLabel(breakAttr->GetAbstrValuesUnit()); !valuesUnitLabel.empty())
	{
		AppendStr(result, cbcValuesPrefix);
		result.push_back(' ');
		AppendStr(result, valuesUnitLabel.c_str());
		result.push_back('\n');
	}

	AppendStr(result, "\"ClassBreak\"");
	if (colorObj) AppendStr(result, ";\"Color\"");
	if (labelObj) AppendStr(result, ";\"Label\"");
	result.push_back('\n');

	GuiReadLock lockHolder;
	for (SizeT i = 0; i != n; ++i)
	{
		// FormattingFlags::None: no thousand separator, so that the text parses back (cf. #1112)
		AppendStr(result, breakObj->AsString(i, lockHolder, FormattingFlags::None).c_str());
		if (colorObj)
		{
			result.push_back(';');
			AppendStr(result, ColorToText(colorObj->GetValueAsUInt32(i) & MAX_COLOR).c_str());
		}
		if (labelObj)
		{
			result.push_back(';');
			AppendQuoted(result, labelObj->AsString(i, lockHolder, FormattingFlags::None));
		}
		result.push_back('\n');
	}
	return SharedStr(result);
}

bool ClassBreaks_FromText(CharPtr first, CharPtr last, ClassBreakClip& result, SharedStr& diagnostic)
{
	result = ClassBreakClip();

	// split into lines, separating the '#' metadata from the data
	std::vector<std::pair<CharPtr, CharPtr>> dataLines;
	for (auto lineBegin = first; lineBegin != last; )
	{
		auto lineEnd = std::find(lineBegin, last, '\n');
		auto f = lineBegin, l = lineEnd;
		Trim(f, l);
		lineBegin = (lineEnd == last) ? last : lineEnd + 1;

		if (f == l)
			continue;
		if (*f == '#')
		{
			auto readTail = [f, l](CharPtr prefix) -> SharedStr
			{
				auto n = StrLen(prefix);
				if (SizeT(l - f) < n || strnicmp(f, prefix, n) != 0)
					return {};
				auto tf = f + n, tl = l;
				Trim(tf, tl);
				return SharedStr(std::string(tf, tl));
			};
			if (auto theme = readTail(cbcThemePrefix); !theme.empty())
				result.m_SrcThemeName = theme;
			if (auto values = readTail(cbcValuesPrefix); !values.empty())
				result.m_SrcValuesUnitName = values;
			continue;
		}
		dataLines.emplace_back(f, l);
	}

	if (dataLines.empty())
	{
		diagnostic = SharedStr("the clipboard holds no class breaks");
		return false;
	}

	// sniff the separator on the first data line: tab, then semicolon, then comma
	char sep = ';';
	for (char candidate : { '\t', ';', ',' })
		if (CountSeps(dataLines[0].first, dataLines[0].second, candidate))
		{
			sep = candidate;
			break;
		}

	std::vector<ColRole> roles;
	SizeT firstDataRow = 0;

	auto headerFields = SplitFields(dataLines[0].first, dataLines[0].second, sep);
	Float64 probe = 0;
	bool hasHeader = !headerFields.empty() && !IsUndefinedField(headerFields[0]) && !TryParseFloat64(headerFields[0], probe);
	if (hasHeader)
	{
		firstDataRow = 1;
		bool haveBreaks = false, haveColor = false, haveLabel = false;
		for (const auto& name : headerFields)
		{
			CharPtr f = name.begin(), l = name.send();
			if (!haveBreaks && (ContainsNoCase(f, l, "break") || ContainsNoCase(f, l, "class") || ContainsNoCase(f, l, "value")))
			{
				roles.push_back(ColRole::Breaks); haveBreaks = true;
			}
			else if (!haveColor && (ContainsNoCase(f, l, "color") || ContainsNoCase(f, l, "colour")))
			{
				roles.push_back(ColRole::Color); haveColor = true;
			}
			else if (!haveLabel && (ContainsNoCase(f, l, "label") || ContainsNoCase(f, l, "descr") || ContainsNoCase(f, l, "name")))
			{
				roles.push_back(ColRole::Label); haveLabel = true;
			}
			else
				roles.push_back(ColRole::Ignore);
		}
		if (!haveBreaks && !roles.empty())
			roles[0] = ColRole::Breaks;   // an unrecognized header still has its breaks up front
	}

	if (roles.empty())
	{
		// no header: column 0 holds the breaks and the role of each further column follows from
		// what the first data row puts in it.
		auto fields = SplitFields(dataLines[firstDataRow].first, dataLines[firstDataRow].second, sep);
		bool haveColor = false, haveLabel = false;
		for (SizeT j = 0; j != fields.size(); ++j)
		{
			if (!j)
			{
				roles.push_back(ColRole::Breaks);
				continue;
			}
			DmsColor clr = 0;
			if (!haveColor && TryParseColor(fields[j], clr))
			{
				roles.push_back(ColRole::Color); haveColor = true;
			}
			else if (!haveLabel && !TryParseFloat64(fields[j], probe))
			{
				roles.push_back(ColRole::Label); haveLabel = true;
			}
			else
				roles.push_back(ColRole::Ignore);
		}
	}

	auto roleOf = [&roles](SizeT j) { return j < roles.size() ? roles[j] : ColRole::Ignore; };

	SizeT nrSkipped = 0, nrWithColor = 0, nrWithLabel = 0;
	bool anyColorColumn = std::find(roles.begin(), roles.end(), ColRole::Color) != roles.end();
	bool anyLabelColumn = std::find(roles.begin(), roles.end(), ColRole::Label) != roles.end();

	for (SizeT i = firstDataRow; i != dataLines.size(); ++i)
	{
		auto fields = SplitFields(dataLines[i].first, dataLines[i].second, sep);

		Float64 breakValue = UNDEFINED_VALUE(Float64);
		bool haveBreak = false;
		DmsColor color = 0;
		bool haveColor = false;
		SharedStr label;
		bool haveLabel = false;

		for (SizeT j = 0; j != fields.size(); ++j)
			switch (roleOf(j))
			{
			case ColRole::Breaks:
				if (IsUndefinedField(fields[j]))
					haveBreak = true;                             // an explicit hole in the palette
				else
					haveBreak = TryParseFloat64(fields[j], breakValue);
				break;
			case ColRole::Color:
				haveColor = TryParseColor(fields[j], color);
				break;
			case ColRole::Label:
				label = fields[j];
				haveLabel = true;
				break;
			default:
				break;
			}

		if (!haveBreak)
		{
			++nrSkipped;
			continue;
		}
		result.m_Breaks.push_back(breakValue);
		if (haveColor) { result.m_Colors.push_back(color); ++nrWithColor; }
		if (haveLabel) { result.m_Labels.push_back(label); ++nrWithLabel; }
	}

	// colors and labels are all-or-nothing: a partial column would silently misalign the palette
	if (nrWithColor != result.m_Breaks.size()) result.m_Colors.clear();
	if (nrWithLabel != result.m_Breaks.size()) result.m_Labels.clear();

	if (result.m_Breaks.empty())
	{
		diagnostic = SharedStr("the clipboard holds no line that starts with a class-break value");
		return false;
	}
	if (nrSkipped)
		diagnostic = mySSPrintF("{} class breaks read, {} unreadable lines skipped", result.m_Breaks.size(), nrSkipped);
	else if (anyColorColumn && result.m_Colors.empty())
		diagnostic = SharedStr("colors were ignored: not every class break carries one");
	else if (anyLabelColumn && result.m_Labels.empty())
		diagnostic = SharedStr("labels were ignored: not every class break carries one");

	return true;
}

//----------------------------------------------------------------------
// the clipboard
//----------------------------------------------------------------------

bool ClassBreaks_ToClipboard(const AbstrDataItem* breakAttr, const AbstrDataItem* colorAttr, const AbstrDataItem* labelAttr, const AbstrDataItem* themeAttr)
{
	if (!breakAttr)
		return false;

	auto text = ClassBreaks_ToText(breakAttr, colorAttr, labelAttr, themeAttr);
	if (text.empty())
		return false;

	ClipBoard clipBoard(false);
	if (!clipBoard.IsOpen())
		return false;
	clipBoard.SetText(text.begin(), text.send());
	return true;
}

bool ClassBreaks_FromClipboard(ClassBreakClip& result, SharedStr& diagnostic)
{
	ClipBoard clipBoard(false);
	if (!clipBoard.IsOpen())
	{
		diagnostic = SharedStr("no clipboard available");
		return false;
	}
	auto text = clipBoard.GetText();
	if (text.empty())
	{
		diagnostic = SharedStr("the clipboard is empty");
		return false;
	}
	return ClassBreaks_FromText(text.begin(), text.send(), result, diagnostic);
}

bool ClipBoard_HasClassBreaks()
{
	ClipBoard clipBoard(false);
	if (!clipBoard.IsOpen())
		return false;
	auto text = clipBoard.GetText();
	if (text.empty())
		return false;

	// cheap sniff: some line outside the '#' metadata must start with a number
	for (CharPtr lineBegin = text.begin(), last = text.send(); lineBegin != last; )
	{
		auto lineEnd = std::find(lineBegin, last, '\n');
		auto f = lineBegin, l = lineEnd;
		Trim(f, l);
		lineBegin = (lineEnd == last) ? last : lineEnd + 1;

		if (f == l || *f == '#')
			continue;
		if (*f == '"' || *f == '-' || *f == '+' || *f == '.' || (*f >= '0' && *f <= '9'))
			return true;
	}
	return false;
}

//----------------------------------------------------------------------
// locating the class attributes of a theme
//----------------------------------------------------------------------

// The aspect a generated palette attribute belongs to, recovered from its dialog type; the
// palette generators stamp that dialog type with GetAspectNameID(aNr).
static AspectNr AspectNrOfPaletteAttr(const AbstrDataItem* adi)
{
	if (!adi)
		return AN_AspectCount;
	auto dialogType = TreeItem_GetDialogType(adi);
	for (AspectNr aNr = AspectNr(0); aNr != AN_AspectCount; aNr = AspectNr(aNr + 1))
		if (GetAspectNameID(aNr) == dialogType)
			return aNr;
	return AN_AspectCount;
}

static AbstrDataItem* FindColorAttrOfDomain(const AbstrUnit* paletteDomain)
{
	assert(paletteDomain);

	for (AspectNr aNr = AspectNr(0); aNr != AN_AspectCount; aNr = AspectNr(aNr + 1))
		if (IsColorAspectNameID(GetAspectNameID(aNr)))
			if (auto adi = GetSystemPalette(paletteDomain, aNr))
				return const_cast<AbstrDataItem*>(adi);

	// palettes that were committed against this domain but do not sit inside it
	UInt32 k = paletteDomain->GetNrDataItemsOut();
	while (k)
	{
		auto adi = const_cast<AbstrDataItem*>(paletteDomain->GetDataItemOut(--k));
		if (adi && IsColorAspectNameID(TreeItem_GetDialogType(adi)))
			return adi;
	}
	return nullptr;
}

ClassBreakItems ClassBreaks_GetItems(const Theme* theme)
{
	ClassBreakItems result;
	if (!theme)
		return result;

	auto breakAttr = theme->GetClassification();
	auto paletteDomain = theme->GetPaletteDomain();
	if (!breakAttr || !paletteDomain)
		return result;                            // a unique-values palette has no class breaks

	result.m_BreakAttr = const_cast<AbstrDataItem*>(breakAttr);
	result.m_PaletteDomain = paletteDomain;

	auto paletteAttr = theme->GetPaletteAttr();
	if (paletteAttr && IsColorAspectNameID(TreeItem_GetDialogType(paletteAttr)))
		result.m_ColorAttr = const_cast<AbstrDataItem*>(paletteAttr);
	else
		result.m_ColorAttr = FindColorAttrOfDomain(paletteDomain);

	auto labelAttr = paletteDomain->GetLabelAttr();
	if (labelAttr)
		result.m_LabelAttr = const_cast<AbstrDataItem*>(labelAttr.get_ptr());
	else
		result.m_LabelAttr = const_cast<AbstrDataItem*>(GetSystemPalette(paletteDomain, AN_LabelText));

	return result;
}

// A class attribute may be overwritten only when it was generated into the desktop tree and
// carries no calculation rule of its own; a configured or calculated one belongs to the project
// and is shared with every other view that uses it (issue #734).
static bool IsGeneratedAndWritable(const DataView* dv, const TreeItem* ti)
{
	if (!ti || !dv)
		return false;
	auto desktopItem = dv->GetDesktopContext();
	if (!desktopItem || !desktopItem->DoesContain(ti))
		return false;
	return ti->IsEditable();
}

bool ClassBreaks_IsOverwritable(const DataView* dv, const ClassBreakItems& items)
{
	if (!items || !dv)
		return false;

	auto paletteDomain = items.m_PaletteDomain.get_ptr();
	if (!IsGeneratedAndWritable(dv, paletteDomain) || paletteDomain->IsDerivable())
		return false;

	return IsGeneratedAndWritable(dv, items.m_BreakAttr.get_ptr());
}

//----------------------------------------------------------------------
// applying a clip
//----------------------------------------------------------------------

// Class breaks are stored in the value type of the thematic values, so a break that this type
// cannot represent has to be refused up front, before anything is created or overwritten;
// otherwise the conversion throws halfway, from deep inside the write.
static void CheckBreaksFitValuesUnit(const AbstrDataItem* context, const AbstrUnit* valuesUnit, const break_array& breaks)
{
	assert(context && valuesUnit);

	auto vc = valuesUnit->GetValueType();
	if (!vc || !vc->IsNumericOrBool())
		return;

	Float64 lo = vc->GetMinValueAsFloat64(), hi = vc->GetMaxValueAsFloat64();
	for (auto breakValue : breaks)
	{
		if (!IsDefined(breakValue) || (breakValue >= lo && breakValue <= hi))
			continue;
		context->throwItemErrorF(
			"Cannot paste class break {} into class breaks of type {}, which holds values from {} to {}"
		,	breakValue
		,	vc->GetNameID()
		,	lo
		,	hi
		);
	}
}

void ClassBreaks_ApplyToItems(DataView* dv, const ClassBreakItems& items, const AbstrDataItem* themeAttr, const ClassBreakClip& clip)
{
	assert(dv);
	MG_CHECK(items);
	MG_CHECK(!clip.empty());

	auto paletteDomain = items.m_PaletteDomain.get_ptr();
	auto breakAttr = items.m_BreakAttr.get_ptr();

	SizeT k = clip.size();
	SizeT maxNrClasses = SizeT(paletteDomain->GetValueType()->GetMaxValueAsFloat64()) + 1;
	if (k > maxNrClasses)
		paletteDomain->throwItemErrorF(
			"Cannot paste {} class breaks into a palette domain of type {}, which allows a maximum of {} classes"
		,	k
		,	paletteDomain->GetValueType()->GetNameID()
		,	maxNrClasses
		);

	CheckBreaksFitValuesUnit(breakAttr, breakAttr->GetAbstrValuesUnit(), clip.m_Breaks);

	// One epoch tick for the whole paste: the class count, the breaks, the colors and the labels
	// change together, so every consumer sees one consistent new state rather than four.
	UpdateMarker::ChangeSourceLock changeStamp(
		UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("ClassBreaks_ApplyToItems"))
	,	"ClassBreaks_ApplyToItems"
	);

	// (1) the class count FIRST: resizing the palette domain reallocates the data of every
	//     attribute defined on it, so no DataWriteLock may be open while this happens.
	auto nrClassesBefore = paletteDomain->GetCount();
	if (k != nrClassesBefore)
	{
		if (paletteDomain->IsDerivable())
			paletteDomain->throwItemError("Cannot change a derived cardinality");
		const_cast<AbstrUnit*>(paletteDomain)->SetCount(k);
	}

	// (2) the class breaks; FillBreakAttrFromArray also maintains the sorted-values flag
	auto valuesRangeData = breakAttr->GetAbstrValuesUnit()->GetTiledRangeData();
	FillBreakAttrFromArray(breakAttr, clip.m_Breaks, valuesRangeData.get());

	// (3) the colors
	if (clip.m_Colors.empty())
	{
		// No colors on the clipboard. The class count may have changed, which reallocated the
		// palette, so regenerate the standard ramp over the new breaks, as the Classify commands
		// do through EditPaletteControl::FillRampColors.
		auto aNr = AspectNrOfPaletteAttr(items.m_ColorAttr.get_ptr());
		if (aNr != AN_AspectCount && IsGeneratedAndWritable(dv, items.m_ColorAttr.get_ptr()))
			CreatePaletteData(dv, paletteDomain, aNr, true, true, begin_ptr(clip.m_Breaks), end_ptr(clip.m_Breaks));
		else if (items.m_ColorAttr && k != nrClassesBefore)
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: the number of classes changed but {} could not be regenerated; check its colors"
			,	items.m_ColorAttr->GetFullName()
			);
	}
	else
	{
		auto colorAttr = items.m_ColorAttr.get_ptr();
		if (!colorAttr)
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: {} has no color attribute; only the class breaks were applied"
			,	paletteDomain->GetFullName()
			);
		else if (!IsGeneratedAndWritable(dv, colorAttr))
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: the colors were not applied because {} is configured or calculated"
			,	colorAttr->GetFullName()
			);
		else if (colorAttr->GetAbstrValuesUnit()->GetValueType() != ValueWrap<UInt32>::GetStaticClass())
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: the colors were not applied because {} is not a color attribute"
			,	colorAttr->GetFullName()
			);
		else
		{
			DataWriteLock dwl(colorAttr);
			auto colorArray = mutable_array_cast<UInt32>(dwl)->GetDataWrite(no_tile, dms_rw_mode::write_only_all).begin();
			for (SizeT i = 0; i != k; ++i)
				colorArray[i] = clip.m_Colors[i];
			dwl.Commit();
		}
	}

	// (4) the labels
	auto labelAttr = items.m_LabelAttr.get_ptr();
	if (labelAttr && !IsGeneratedAndWritable(dv, labelAttr))
	{
		if (!clip.m_Labels.empty())
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: the labels were not applied because {} is configured or calculated"
			,	labelAttr->GetFullName()
			);
	}
	else if (labelAttr)
	{
		DataWriteLock dwl(labelAttr);
		if (!clip.m_Labels.empty())
		{
			for (SizeT i = 0; i != k; ++i)
				dwl->SetValue<SharedStr>(i, clip.m_Labels[i]);
		}
		else
		{
			// No labels on the clipboard: name each class after its range, the way
			// EditPaletteControl::ReLabelRanges does, reading back what was just written so that
			// the value type of the class breaks decides the notation.
			PreparedDataReadLock breakLock(breakAttr, "ClassBreaks_ApplyToItems");
			auto breakObj = breakAttr->GetRefObj();
			GuiReadLock lockHolder;
			bool ascending = std::is_sorted(clip.m_Breaks.begin(), clip.m_Breaks.end());
			for (SizeT i = 0; i != k; ++i)
			{
				auto lo = breakObj->AsString(i, lockHolder, FormattingFlags::ThousandSeparator);
				if (!ascending)
					dwl->SetValue<SharedStr>(i, lo);
				else if (i + 1 != k)
					dwl->SetValue<SharedStr>(i, lo + " ... " + breakObj->AsString(i + 1, lockHolder, FormattingFlags::ThousandSeparator));
				else
					dwl->SetValue<SharedStr>(i, ">= " + lo);
			}
		}
		dwl.Commit();
	}

	// provenance mismatch is worth a note but never a refusal: reusing a classification across
	// comparable-but-differently-named units is exactly what issue #734 asks for.
	if (!clip.m_SrcValuesUnitName.empty() && themeAttr)
	{
		auto targetName = ValuesUnitLabel(themeAttr->GetAbstrValuesUnit());
		if (!targetName.empty() && !(targetName == clip.m_SrcValuesUnitName))
			reportF(SeverityTypeID::ST_Warning, "PasteClassBreaks: the class breaks were made for values in {} but are applied to values in {}"
			,	clip.m_SrcValuesUnitName
			,	targetName
			);
	}
}

bool ClassBreaks_ApplyToLayer(DataView* dv, GraphicLayer* layer, const ClassBreakClip& clip)
{
	assert(dv && layer);
	MG_CHECK(!clip.empty());

	auto theme = layer->GetActiveTheme();
	if (!theme)
		throwDmsErrF("PasteClassBreaks: {} has no active theme to classify", GetThemeDisplayName(layer));

	auto themeAttr = theme->GetThemeAttr();
	auto items = ClassBreaks_GetItems(theme.get());

	if (items && ClassBreaks_IsOverwritable(dv, items))
	{
		ClassBreaks_ApplyToItems(dv, items, themeAttr, clip);
		layer->Invalidate();
		return false;
	}

	// The current class attributes are configured, calculated, or absent. Leave them alone and
	// generate a fresh set the way a missing classification is instantiated (cf. Theme::Create
	// and ActivateClassificationCmd), then re-theme this layer onto it.
	if (!themeAttr)
		throwDmsErrF("PasteClassBreaks: {} has no thematic attribute to classify", GetThemeDisplayName(layer));

	// Refuse before creating anything, so that a paste that cannot land leaves no half-built
	// classification behind in the desktop tree.
	CheckBreaksFitValuesUnit(themeAttr, themeAttr->GetAbstrValuesUnit(), clip.m_Breaks);

	// One epoch tick for the generated items and their contents together; the nested lock that
	// ClassBreaks_ApplyToItems takes resolves to this same time stamp.
	UpdateMarker::ChangeSourceLock changeStamp(
		UpdateMarker::GetActiveTS(MG_DEBUG_TS_SOURCE_CODE("ClassBreaks_ApplyToLayer"))
	,	"ClassBreaks_ApplyToLayer"
	);

	auto aNr = theme->GetAspectNr();
	auto nbai = CreateBreakAttr(dv, themeAttr->GetAbstrValuesUnit(), themeAttr, clip.size());
	auto paletteDomain = nbai.paletteDomain.get_ptr();

	SharedDataItemInterestPtr palette = FindAspectAttr(aNr, nbai.breakAttr.get_ptr(), paletteDomain, layer->GetLayerClass());
	if (!palette)
		palette = FindAspectAttr(aNr, paletteDomain, paletteDomain, layer->GetLayerClass());
	if (!palette)
		palette = CreatePaletteData(dv, paletteDomain, aNr, true, false, nullptr, nullptr);
	if (!palette)
		throwDmsErrF("PasteClassBreaks: cannot create a {} palette for {}", GetAspectName(aNr), GetThemeDisplayName(layer));

	ClassBreakItems fresh;
	fresh.m_PaletteDomain = paletteDomain;
	fresh.m_BreakAttr = nbai.breakAttr.get_ptr();
	if (IsColorAspectNameID(TreeItem_GetDialogType(palette.get_ptr())))
		fresh.m_ColorAttr = const_cast<AbstrDataItem*>(palette.get_ptr());
	fresh.m_LabelAttr = const_cast<AbstrDataItem*>(CreateSystemLabelPalette(dv, paletteDomain, AN_LabelText, true).get_ptr());

	ClassBreaks_ApplyToItems(dv, fresh, themeAttr, clip);

	layer->ChangeTheme(Theme::Create(aNr, themeAttr, nbai.breakAttr.get_ptr(), palette.get_ptr()).get());
	layer->Invalidate();
	return true;
}
