// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The vocabulary of the typed-HOF language: the marker heads of the notation, resolved
// call arguments, function values and closures, hole filling, and variant dispatch.
// See HofClosure.h for what the rest of the component uses from here.

#include "AbstrCalculator.h"
#include "TreeItemFunctionSpec.h"

#include "RtcInterface.h"
#include "act/ActorVisitor.h"
#include "act/SupplierVisitFlag.h"
#include "act/TriggerOperator.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "ptr/LifetimeProtector.h"
#include "ser/AsString.h"
#include "set/StackUtil.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"
#include "xct/ErrMsg.h"
#include "xml/XMLOut.h"

#include "LispList.h"
#include "LispTreeType.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "CopyTreeContext.h"
#include "DataArray.h"
#include "DataController.h"
#include "DataItemClass.h"
#include "DC_Ptr.h"
#include "ExprRewrite.h"
#include "LispRef.h"
#include "OperGroups.h"
#include "Operator.h"
#include "OperSignature.h"
#include "SessionData.h"
#include "SupplCache.h"
#include "UnitClass.h"

#include "LispContextHandle.h"
#include "TreeItemContextHandle.h"
#include "TreeItemClass.h"
#include "MoreDataControllers.h"
#include "DataArrayValue.h"

#include <algorithm>
#include <bitset>
#include <functional>
#include <tuple>
#include <map>
#include <memory>
#include <set>

#include "HofClosure.h"

namespace hof {

// the marker heads and placeholders of the typed-HOF notation, declared in HofClosure.h
StaticTokenID t_Hole("_");
StaticTokenID t_Map("map");
StaticTokenID t_ApplyItem("apply_item");
StaticTokenID t_InstantiateItem("instantiate_item");
StaticTokenID t_ApplyValue("apply_value");
StaticTokenID t_ContainerLiteral("container_literal");
StaticTokenID t_Member("member");
StaticTokenID t_NoDomain("no_domain");
StaticTokenID t_Dot(".");

	// WP4.5: the auto-imported standard prelude ('prelude' container under the config
	// root). A call head that resolves to a NON-callable item (e.g. a documentation
	// container that happens to carry an operator-like name) does not capture the call:
	// the prelude binding applies instead. Also the fallback for call heads inside
	// strict function scopes, which do not see the root's namespace usage.
	SharedTreeItem FindPreludeFunction(TokenID nameID)
	{
		static TokenID t_PreludeContainer = GetTokenID_st("prelude");
		auto sd = SessionData::Curr();
		if (!sd)
			return {};
		auto root = sd->GetConfigRoot();
		if (!root)
			return {};
		auto pre = root->GetConstSubTreeItemByID(t_PreludeContainer);
		if (!pre)
			return {};
		auto f = pre->GetConstSubTreeItemByID(nameID);
		if (!f || !f->IsTemplate())
			return {};
		return f;
	}

	// replace every bare '.' symbol (the current-domain reference) in a container-literal
	// member expression with the literal's domain expression, before caller-scope resolution
	LispRef ReplaceDot(LispPtr expr, LispPtr domainExpr)
	{
		if (expr.EndP())
			return expr;
		if (expr.IsSymb())
			return (expr.GetSymbID() == t_Dot) ? LispRef(domainExpr) : LispRef(expr);
		if (expr.IsRealList())
			return LispRef(ReplaceDot(expr.Left(), domainExpr), ReplaceDot(expr.Right(), domainExpr));
		return LispRef(expr);
	}

	// build a destructured container-literal argument from
	// (container_literal <domain|no_domain> (member name value)…), resolving the domain and
	// each member value (with '.' rebound to the domain) through `resolve` (caller or body scope)
	std::shared_ptr<ContainerLiteralArg> BuildContainerLiteral(LispPtr litExpr, const std::function<LispRef(LispPtr)>& resolve)
	{
		auto lit = std::make_shared<ContainerLiteralArg>();
		LispPtr domainExpr = litExpr.Right().Left();
		if (!(domainExpr.IsSymb() && domainExpr.GetSymbID() == t_NoDomain))
		{
			lit->hasDomain = true;
			lit->domainKey = resolve(domainExpr);
		}
		for (LispPtr m = litExpr.Right().Right(); !m.EndP(); m = m.Right())
		{
			LispPtr member = m.Left(); // (member name value)
			TokenID name    = member.Right().Left().GetSymbID();
			LispRef value   = ReplaceDot(member.Right().Right().Left(), domainExpr);
			lit->members.emplace_back(name, resolve(value));
		}
		return lit;
	}

	bool IsFunctionResultMetaCall(LispPtr expr)
	{
		if (!expr.IsRealList() || !expr.Left().IsSymb())
			return false;
		const AbstrOperGroup* group = AbstrOperGroup::FindName(expr.Left().GetSymbID());
		return !group->MustCacheResult() && group->AllowsAsFunctionResult();
	}

	void RejectNestedFunctionResultMetaCall(LispPtr expr)
	{
		if (IsFunctionResultMetaCall(expr))
			throwErrorF("ExprParser", "a generating meta function such as 'table_spec' can only be the whole result of an inlined function");
	}

	// a plain function reference is a binding with every slot a hole
	std::shared_ptr<FunctionBinding> MakeAllHoles(SharedTreeItem func)
	{
		auto b = std::make_shared<FunctionBinding>();
		b->funcItem = func;
		UInt32 n = TreeItem_GetFunctionParamCount(func.get());
		b->slots.resize(n);
		for (auto& s : b->slots) s.isHole = true;
		return b;
	}

	// #1166: a function nested in another function's BODY is applied in the enclosing
	// application's scope, so its body may reference the enclosing parameters. Give
	// such a callee this application's environment; a callee reached from the
	// definition scope or the prelude is not nested and gets none.
	bool IsNestedInside(const TreeItem* callee, const TreeItem* outer)
	{
		if (!callee || !outer)
			return false;
		for (auto p = callee->GetTreeParent(); p; p = p->GetTreeParent())
			if (p.get() == outer)
				return true;
		return false;
	}

	// fill the holes of `b` with `holeFills` left-to-right; the counts must match --
	// except for a '...x' rest function, whose LAST hole absorbs all surplus fills
	FunctionBinding MergeBinding(const FunctionBinding& b, const std::vector<CallArg>& holeFills)
	{
		bool hasRest = b.funcItem && b.funcItem->IsFunctionItem() && TreeItem_HasFunctionRestParam(b.funcItem.get());
		if (hasRest ? holeFills.size() < b.NrHoles() : holeFills.size() != b.NrHoles())
			throwErrorF("ExprParser", "'{}': function expects {}{} argument(s); {} provided"
				, b.funcItem->GetFullName().c_str(), b.NrHoles(), hasRest ? " or more" : "", holeFills.size());
		FunctionBinding r; r.funcItem = b.funcItem; r.env = b.env;
		UInt32 c = 0;
		for (const auto& slot : b.slots)
			r.slots.push_back(slot.isHole ? holeFills[c++] : slot);
		while (c < holeFills.size()) // rest surplus (guarded: non-rest counts are equal above)
			r.slots.push_back(holeFills[c++]);
		return r;
	}

	// §5.10: reduce a fully-bound application to its VALUE -- a data key, or a closure
	// binding when the applied function has a function-typed result
	CallArg ReduceMergedValue(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder)
	{
		FunctionApplication appl;
		appl.m_FuncItem = merged.funcItem.get();
		appl.m_Parent = parent;
		appl.m_SubstBuff = substBuff;
		appl.m_ErrorHolder = errorHolder;
		appl.m_Env = merged.env;
		for (const auto& slot : merged.slots)
			appl.PushArg(slot);
		return appl.ReduceValue();
	}

	LispRef ReduceMerged(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
		, std::shared_ptr<const StructuredFunctionResult>* structuredResult) // default nullptr in HofClosure.h
	{
		CallArg r = ReduceMergedValue(merged, parent, substBuff, errorHolder);
		if (r.binding)
			throwErrorF("ExprParser", "'{}': a function value can only be applied with '(...)', passed as an argument, or returned as a result"
				, merged.funcItem->GetFullName().c_str());
		if (structuredResult)
			*structuredResult = std::move(r.structuredResult);
		return r.key;
	}

	// caller-side (non-body) argument resolution: build a CallArg from a caller-scope
	// expression. `resolveData` substitutes an ordinary data expression to its key;
	// `findItem` resolves a bare symbol to its item (null if absent). A function
	// application with holes yields a partial binding; a full one is reduced to data.
	CallArg ResolveCallerArg(LispPtr argExpr,
		const std::function<LispRef(LispPtr)>& resolveData,
		const std::function<SharedTreeItem(TokenID)>& findItem,
		SubstitutionBuffer* substBuff, SharedTreeItem errorHolder)
	{
		if (argExpr.IsSymb())
		{
			TokenID sym = argExpr.GetSymbID();
			if (sym == t_Hole)
			{
				CallArg a; a.isHole = true; return a;
			}
			if (!token::isConst(sym) && !ValueClass::FindByScriptName(sym))
			{
				SharedTreeItem item = findItem(sym); // plain-reference item (member access) + function detection
				if (!item)
					if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
						item = pf; // prelude: implicit outermost namespace, also for function references
				if (item && item->IsFunctionItem())
				{
					if (substBuff) registerSupplier(*substBuff, item.get());
					CallArg a; a.binding = MakeAllHoles(item); return a;
				}
				CallArg a; a.key = resolveData(argExpr); a.item = item; return a;
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb())
		{
			TokenID headID = argExpr.Left().GetSymbID();

			// §5.9 container literal: resolve domain + members in caller scope ('.' -> domain)
			if (headID == t_ContainerLiteral)
			{
				CallArg a; a.literal = BuildContainerLiteral(argExpr, resolveData); return a;
			}

			// §5.10 applied call result as an argument: value or residual binding
			if (headID == t_ApplyValue)
			{
				CallArg fnVal = ResolveCallerArg(argExpr.Right().Left(), resolveData, findItem, substBuff, errorHolder);
				if (!fnVal.binding)
					throwErrorF("ExprParser", "'(...)' applied to an expression that is not a function value");
				std::vector<CallArg> outer;
				for (LispPtr a = argExpr.Right().Right(); !a.EndP(); a = a.Right())
					outer.push_back(ResolveCallerArg(a.Left(), resolveData, findItem, substBuff, errorHolder));
				FunctionBinding merged = MergeBinding(*fnVal.binding, outer);
				if (merged.NrHoles() == 0)
					return ReduceMergedValue(merged, nullptr, substBuff, errorHolder);
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall())
			{
				auto callee = findItem(headID);
				if (callee && callee->IsFunctionItem())
				{
					if (substBuff) registerSupplier(*substBuff, callee.get());
					std::vector<CallArg> sub;
					for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
						sub.push_back(ResolveCallerArg(a.Left(), resolveData, findItem, substBuff, errorHolder));
					// §5.7: variant sets dispatch by argument type on nested calls too
					if (TreeItem_IsFunctionVariantSet(callee.get()))
					{
						auto variant = ResolveVariant(callee.get(), sub, errorHolder);
						callee = make_shared_tree(variant, existing_obj{});
						if (substBuff) registerSupplier(*substBuff, variant);
						CheckFunctionDefinition(variant);
					}
					FunctionBinding merged = MergeBinding(*MakeAllHoles(callee), sub);
					if (merged.NrHoles() == 0)
						return ReduceMergedValue(merged, nullptr, substBuff, errorHolder); // §5.10: data key OR closure binding
					CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
				}
			}
		}
		CallArg a; a.key = resolveData(argExpr); return a;
	}

	// §5.7 variant dispatch: value class of a reduced argument key
	const ValueClass* ArgValueClass(LispRef key, SharedTreeItem errorHolder)
	{
		if (key.EndP())
			return nullptr;
		auto dc = GetOrCreateDataController(key);
		auto res = dc->MakeResult();
		if (!res)
		{
			dms_assert(dc->WasFailed(FailType::MetaInfo));
			errorHolder->ThrowFail(dc.get());
		}
		if (IsDataItem(res.get()))
			return AsDataItem(res.get())->GetAbstrValuesUnit()->GetValueType();
		if (IsUnit(res.get()))
			return AsUnit(res.get())->GetValueType();
		return nullptr;
	}

	// declared value class of a variant parameter (params are inert, so prefer the
	// declared values-unit token -- a value-type name -- over resolving the unit)
	const ValueClass* ParamValueClass(const TreeItem* param)
	{
		if (IsDataItem(param))
		{
			if (auto vc = ValueClass::FindByScriptName(AsDataItem(param)->ValuesUnitToken()))
				return vc;
			auto vu = AsDataItem(param)->GetAbstrValuesUnit();
			return vu ? vu->GetValueType() : nullptr;
		}
		if (IsUnit(param))
			return AsUnit(param)->GetValueType();
		return nullptr;
	}

	// §5.7 v2: select the variant of `setItem` whose acceptance set matches the argument
	// value classes, taking the MOST SPECIFIC match (per-parameter subset comparison over
	// the closed value-class universe); a fully generic or plain position accepts more
	// than a concrete or tighter-constrained one. Definition-time disjointness
	// (TreeItem_CheckVariantSetDisjointness) guarantees overlapping variants are
	// specificity-ordered, so the most specific match is unique.
	const TreeItem* ResolveVariant(const TreeItem* setItem, const std::vector<CallArg>& callArgs, SharedTreeItem errorHolder)
	{
		std::vector<const ValueClass*> argVCs;
		argVCs.reserve(callArgs.size());
		for (const auto& a : callArgs)
			argVCs.push_back(ArgValueClass(a.key, errorHolder));

		const TreeItem* best = nullptr;
		SharedStr candidates;
		for (const TreeItem* v = setItem->_GetFirstSubItem(); v; v = v->GetNextItem())
		{
			if (!v->IsFunctionItem())
				continue;
			if (!candidates.empty())
				candidates = candidates + SharedStr(", ");
			candidates = candidates + SharedStr(v->GetID());

			if (!TreeItem_VariantMatches(v, argVCs))
				continue;
			if (!best)
			{
				best = v;
				continue;
			}
			int cmp = TreeItem_CompareVariantSpecificity(v, best);
			if (cmp == -1)
				best = v;
			else if (cmp != +1)
				throwErrorF("ExprParser", "call to variant set '{}': the arguments match variants '{}' and '{}' and neither is more specific"
					, setItem->GetFullName().c_str(), best->GetID().GetStrLock().c_str(), v->GetID().GetStrLock().c_str());
		}
		if (!best)
			throwErrorF("ExprParser", "call to variant set '{}': no variant matches the argument types (variants: {})"
				, setItem->GetFullName().c_str(), candidates.c_str());
		return best;
	}

	// structural compatibility of a bound function against a declared signature
	// exemplar: same arity, per-parameter and result item classes equal (a plain
	// TreeItem-classed signature position is a wildcard)
	// §5.10 Stage 2: does `tok` name a generic type/domain variable of `fn`?
	bool IsGenericVarOf(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &var, &cons, &isDom))
			if (var == tok)
				return true;
		return false;
	}

	// is `tok` a DOMAIN-sorted generic (`<D: domains>`) of fn? Only those carry a
	// unit IDENTITY; value-sorted generics range over classes. (isDom rides the
	// generic-parameter records, not the typeVars pair list.)
	bool IsDomainSortedVarOf(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &var, &cons, &isDom))
			if (var == tok)
				return isDom;
		return false;
	}

	// does `tok` appear in fn's OWN <...> type-parameter clause? (genericParams also
	// record lexically inherited enclosing variables; the ordered typeVars list holds
	// only the function's own declarations, so an own clause shadows the origin's)
	bool IsOwnDeclaredVar(const TreeItem* fn, TokenID tok)
	{
		if (!tok)
			return false;
		if (auto tvs = TreeItem_GetFunctionTypeVars(fn))
			for (const auto& tv : *tvs)
				if (tv.first == tok)
					return true;
		return false;
	}


} // namespace hof
