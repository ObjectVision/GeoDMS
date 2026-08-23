// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#if !defined(__TIC_METAINFO_H)
#define __TIC_METAINFO_H

#include "TicBase.h"

#include <memory>
#include <variant>
#include <vector>

#include "ptr/WeakPtr.h"

#include "LispRef.h"

//----------------------------------------------------------------------
// MetaFuncCurry
//----------------------------------------------------------------------

struct StructuredFunctionResultMember
{
	TokenID id;
	LispRef key;
	std::vector<StructuredFunctionResultMember> subItems;
};

struct StructuredFunctionResult
{
	std::vector<StructuredFunctionResultMember> subItems;
};

struct MetaFuncCurry
{
//	LispRef callArgList;
	LispRef fullLispExpr;
	std::shared_ptr<const StructuredFunctionResult> structuredResult;

	const TreeItem* applyItem = nullptr;
	const AbstrOperGroup* og = nullptr;
	bool isMapCall = false; // built-in 'map(function, container)' metafunction (WP3.3)

	void operator ()(TreeItem* target, const AbstrCalculator* ac) const;
	LispRef GetAsLispRef() const;
};

//----------------------------------------------------------------------
// MetaInfo
//----------------------------------------------------------------------

using MetaInfo = std::variant<MetaFuncCurry, LispRef, SharedTreeItem>;
LispRef GetAsLispRef(const MetaInfo&);

#endif // __TIC_METAINFO_H
