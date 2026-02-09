// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SHV_ABSTRCMD_H
#define __SHV_ABSTRCMD_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

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


template <typename Func>
struct LambdaCmd : AbstrCmd
{
	//	using MembFunc = RT(GO::*)(Args...);

	LambdaCmd(Func&& f) : m_Func(std::move(f))
	{}

	GraphVisitState Visit(GraphicObject* ago) override
	{
		m_Func();
		return GVS_Handled;
	}

private:
	Func m_Func;
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

template <typename Func>
auto make_LambdaCmd(Func&& f)
{
	return std::make_unique<LambdaCmd<Func>>(std::forward<Func>(f));
}

template <typename GO, typename RT, typename ...Args>
auto make_MembFuncCmd(RT(GO::* mf)(Args...), Args ...args)
{
	return std::make_unique<MembFuncCmd<GO, RT, Args...>>(mf, std::forward<Args>(args)...);
}

#endif // __SHV_ABSTRCMD_H


