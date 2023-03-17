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

#ifndef __SHV_ABSTRCMD_H
#define __SHV_ABSTRCMD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "ptr/PersistentSharedObj.h"

#include "GraphVisitor.h"

//----------------------------------------------------------------------
// class  : AbstrCmd
//----------------------------------------------------------------------

class AbstrCmd : public AbstrVisitor
{
public:
	AbstrCmd() {}

	GraphVisitState DoMovableContainer(MovableContainer* obj) override; // forward to active item if available 
	GraphVisitState DoLayerSet        (LayerSet*         obj) override; // forward to active item if available 
};


template <typename GO, typename RT, typename ...Args>
struct MembFuncCmd : AbstrCmd
{
	using MembFunc = RT(GO::* )(Args...);

	MembFuncCmd(MembFunc mf, Args... args) : m_MembFunc(mf), m_Args(std::forward<Args>(args)...)
	{}

	GraphVisitState Visit(GraphicObject* ago) override
	{
		GO* go = dynamic_cast<GO*>(ago);
		if (!go)
			return AbstrCmd::Visit(ago); // try forwarding to Active entry or its parents as defined by AbstrCmd

		callFunc(go, std::index_sequence_for<Args...>{});
		return GVS_Handled;
	}

	template<std::size_t ...I> void callFunc(GO* go, std::index_sequence<I...>) { (go->*m_MembFunc)(std::get<I>(m_Args) ...); }

private:
	MembFunc m_MembFunc;
	std::tuple<Args...> m_Args;
};

template <typename GO, typename RT, typename ...Args>
auto make_MembFuncCmd(RT (GO::* mf)(Args...), Args ...args)
{
	return MakeOwned<AbstrCmd, MembFuncCmd<GO, RT, Args...>>(mf, std::forward<Args>(args)...);
}

#endif // __SHV_ABSTRCMD_H


