// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __SHV_THEMECMD_H
#define __SHV_THEMECMD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "AbstrCmd.h"

//----------------------------------------------------------------------
// class  : SubLayerCmd
//----------------------------------------------------------------------

struct SubLayerCmd : AbstrCmd
{
	SubLayerCmd(AspectGroup ag, bool enable)
		:	m_AspectGroup(ag)
		,	m_Enable(enable)
	{}

	GraphVisitState Visit(GraphicObject* ago) override
	{
		GraphicLayer* go = dynamic_cast<GraphicLayer*>(ago);
		if (!go)
			return AbstrCmd::Visit(ago); // try forwarding to Active entry or its parents as defined by AbstrCmd

		go->EnableAspectGroup(m_AspectGroup, m_Enable);
		return GVS_Handled;
	}

private:
	AspectGroup m_AspectGroup;
	bool        m_Enable;
};

//----------------------------------------------------------------------
// class  : ThemeCmd
//----------------------------------------------------------------------

struct ThemeCmd : AbstrCmd
{
	typedef void (Theme::*MembFunc)();

	ThemeCmd(MembFunc mf, AspectNr a)
		:	m_MembFunc(mf) 
		,	m_Aspect(a)
	{}

	GraphVisitState Visit(GraphicObject* ago) override
	{
		GraphicLayer* go = dynamic_cast<GraphicLayer*>(ago);
		if (!go)
			return AbstrCmd::Visit(ago); // try forwarding to Active entry or its parents as defined by AbstrCmd
		auto theme = go->GetTheme(m_Aspect);
		dms_assert(theme);
		(theme.get()->*m_MembFunc)();
		go->Invalidate(); // flush m_MaxLabelStrLen, m_PenIndexCache, m_FontIndexCaches, m_BoundingBoxCache and calls InvalidateDraw
		return GVS_Break;
	}

private:
	MembFunc m_MembFunc;
	AspectNr m_Aspect;
};


#endif // __SHV_THEMECMD_H


