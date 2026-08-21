// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__VALUEFILLER_H)
#define __VALUEFILLER_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "UnitProcessor.h"

//----------------------------------------------------------------------
// FastUndefiner
//----------------------------------------------------------------------

struct FastUndefineBase : UnitProcessor
{
	// Closure
	FastUndefineBase()
		:	m_TileID(0)
		,	m_Res(nullptr)
	{}

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		dms_assert(m_Res);

		auto resData = mutable_array_cast<E>(m_Res)->GetDataWrite(m_TileID, dms_rw_mode::write_only_all);
		fast_undefine(resData.begin(), resData.end());
	}
public:
	AbstrDataObject* m_Res;
	tile_id          m_TileID;
};

struct FastUndefiner : tl::fold_t<typelists::fields, FastUndefineBase, UnitVisitorImpl>
{
	FastUndefiner(const AbstrUnit* resValues, AbstrDataObject* resObj, tile_id tileID)
	{
		m_Res    = resObj;
		m_TileID = tileID;
		resValues->InviteUnitProcessor(*this);
	}

};


#endif // __VALUEFILLER_H
