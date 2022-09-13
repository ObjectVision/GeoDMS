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


