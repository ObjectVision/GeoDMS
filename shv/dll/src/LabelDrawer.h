// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// LabelDrawer: draws text labels for feature layers.
// Separated into its own header so DrawPolygons.h can use it.
//

#pragma once

#ifndef __SHV_LABELDRAWER_H
#define __SHV_LABELDRAWER_H

#include "FeatureLayer.h"
#include "DrawContext.h"
#include "GraphVisitor.h"
#include "Theme.h"
#include "ThemeValueGetter.h"

struct LabelDrawer : WeakPtr<const FontIndexCache>
{
	LabelDrawer(const FeatureDrawer& fd)
		:	WeakPtr<const FontIndexCache>(fd.GetUpdatedLabelFontIndexCache())
		,	m_FeatureDrawer(fd)
		,	m_TextColorTheme(fd.m_Layer->GetEnabledTheme(AN_LabelTextColor))
		,	m_BackColorTheme(fd.m_Layer->GetEnabledTheme(AN_LabelBackColor))
		,	m_LabelTextValueGetter     ( GetValueGetter(fd.m_Layer->GetEnabledTheme(AN_LabelText     ).get()) )
		,	m_LabelTextColorValueGetter( (m_TextColorTheme && !m_TextColorTheme->IsAspectParameter()) ? m_TextColorTheme->GetValueGetter() : nullptr )
		,	m_LabelBackColorValueGetter( (m_BackColorTheme && !m_BackColorTheme->IsAspectParameter()) ? m_BackColorTheme->GetValueGetter() : nullptr )
		,	m_DefaultTextColor( (m_TextColorTheme && m_TextColorTheme->IsAspectParameter()) ? m_TextColorTheme->GetColorAspectValue() : fd.m_Layer->GetDefaultTextColor() )
		,	m_DefaultBackColor( (m_BackColorTheme && m_BackColorTheme->IsAspectParameter()) ? m_BackColorTheme->GetColorAspectValue() : fd.m_Layer->GetDefaultBackColor() )
	{
		dms_assert(fd.m_Layer->m_FontIndexCaches[FR_Label] == NULL || fd.m_Layer->GetTheme(AN_LabelText) ); // maybe the label sublayer has been turned off.
		auto* dc = fd.m_Drawer.GetDrawContext();
		dc->SetTextColor(m_DefaultTextColor);
		dc->SetBkColor(m_DefaultBackColor);
		dc->SetBkMode(!(m_BackColorTheme && m_BackColorTheme->IsAspectParameter() && m_BackColorTheme->GetColorAspectValue() != DmsTransparent));
		if (has_ptr() && get_ptr()->GetNrKeys() == 1)
		{
			SelectFontViaDrawContext(dc, 0);
			WeakPtr<const FontIndexCache>::reset();
		}
	}

	void DrawLabel(entity_id entityIndex, const GPoint& location)
	{
		auto* dc = m_FeatureDrawer.m_Drawer.GetDrawContext();

		if (has_ptr())
		{
			auto keyIndex = get_ptr()->GetKeyIndex(entityIndex);
			if (get_ptr()->GetFontHeight(keyIndex) == 0)
				return;
			SelectFontViaDrawContext(dc, keyIndex);
		}

		DmsColor textColor = m_DefaultTextColor;
		if (m_LabelTextColorValueGetter)
		{
			textColor = m_LabelTextColorValueGetter->GetColorValue(entityIndex);
			dc->SetTextColor(textColor);
		}

		if (m_LabelBackColorValueGetter)
		{
			DmsColor backColor = m_LabelBackColorValueGetter->GetColorValue(entityIndex);
			if (backColor == DmsTransparent)
				dc->SetBkMode(true);
			else
			{
				dc->SetBkMode(false);
				dc->SetBkColor(backColor);
			}
		}

		dms_assert(m_FeatureDrawer.HasLabelText() );
		SharedStr label = m_LabelTextValueGetter->GetStringValue(entityIndex, m_LabelLock);
		dc->TextOut(location, label.c_str(), label.ssize(), textColor);
	}

private:
	void SelectFontViaDrawContext(DrawContext* dc, UInt32 keyIndex)
	{
		auto fontNameId = get_ptr()->GetFontNameId(keyIndex);
		auto fontHeight = get_ptr()->GetFontHeight(keyIndex);
		auto fontAngle  = get_ptr()->GetFontAngle(keyIndex);
		dc->SetFont(GetTokenStr(fontNameId).c_str(), fontHeight, fontAngle);
	}

	const FeatureDrawer& m_FeatureDrawer;

	std::shared_ptr<const Theme> m_TextColorTheme;
	std::shared_ptr<const Theme> m_BackColorTheme;

	WeakPtr<const AbstrThemeValueGetter> m_LabelTextValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_LabelTextColorValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_LabelBackColorValueGetter;

	DmsColor m_DefaultTextColor;
	DmsColor m_DefaultBackColor;

	mutable GuiReadLock  m_LabelLock;
};

#endif // __SHV_LABELDRAWER_H
