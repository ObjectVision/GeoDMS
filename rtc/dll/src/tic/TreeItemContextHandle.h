// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_TI_CONTEXTHANDLE_H)
#define __TIC_TI_CONTEXTHANDLE_H

#include "dbg/DebugContext.h"
struct TreeItem;
struct AbstrPropWriter;

//----------------------------------------------------------------------
// class  : TreeItemContextHandle
//----------------------------------------------------------------------

struct TreeItemContextHandle : ContextHandle 
{
	TIC_CALL TreeItemContextHandle(const TreeItem* obj, CharPtr role = nullptr);
	TIC_CALL TreeItemContextHandle(const TreeItem* obj, const Class* cls, CharPtr role = nullptr);

	TIC_CALL ~TreeItemContextHandle();
	bool HasItemContext() const override { auto ti = GetItem(); return ti && !ti->IsCacheItem(); }
	auto ItemAsStr() const->SharedStr override;

	const TreeItem* GetItem() const { return m_Obj; }

protected:
	void GenerateDescription() override;

private:
	CharPtr         m_Role = nullptr;
	const TreeItem* m_Obj = nullptr;
};

void GenerateSystemInfo(AbstrPropWriter& apw, const TreeItem* curr);

#endif // __TIC_TI_CONTEXTHANDLE_H
