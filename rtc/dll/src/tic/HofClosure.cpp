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
#include "DataLocks.h"
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
StaticTokenID t_Fold("fold");
StaticTokenID t_FoldC("fold_c");
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
		static TokenID t_PreludeContainer = GetTokenID_mt("prelude"); // _mt: run time, workers may be registering tokens
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

	// #1347: the key of a declared sub-item of a function result, by its path below the
	// result ('point', or 'meta/factor' for a member of a container member)
	LispRef FindStructuredMember(const StructuredFunctionResult& sr, CharPtrRange relPath)
	{
		const std::vector<StructuredFunctionResultMember>* level = &sr.subItems;
		while (true)
		{
			CharPtr slash = std::find(relPath.first, relPath.second, '/');
			TokenID id = GetTokenID_mt(relPath.first, slash);
			auto m = std::find_if(level->begin(), level->end(), [id](const StructuredFunctionResultMember& sub) { return sub.id == id; });
			if (m == level->end())
				return {};
			if (slash == relPath.second)
				return m->key;
			level = &m->subItems;
			relPath.first = slash + 1;
		}
	}

	// #1347: fold(D, f, x0) is f(e[n-1], ... f(e[1], f(e[0], x0))), with e[k] the k-th
	// element of the unit D, as a parameter<D>. The number of steps is read from D when
	// the expression is reduced, at meta time, as iterate and loop read theirs. Each step
	// is a beta reduction of f, so the result is the key of one calculation that does all
	// n steps, and no item is made per step. A state with declared sub-items (a unit
	// result with its attributes) carries them to the next step as a structured result.
	// When `trace` is given, it receives the state after each step (instantiate fold).
	CallArg ReduceFold(const CallArg& domainArg, const CallArg& stepArg, CallArg state
		, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
		, std::vector<CallArg>* trace)
	{
		constexpr CharPtr foldSig = "fold(domain, step, initial state)";
		if (domainArg.isHole || stepArg.isHole || state.isHole)
			throwErrorF("ExprParser", "{}: '_' cannot be an argument of fold itself; it marks the open arguments of the step", foldSig);
		if (domainArg.binding || domainArg.literal || domainArg.key.EndP())
			throwErrorF("ExprParser", "{}: the first argument must be a unit, the domain whose elements the step visits", foldSig);
		if (!stepArg.binding)
			throwErrorF("ExprParser", "{}: the second argument must be a function, or a partial application that leaves two arguments open", foldSig);
		if (state.binding)
			throwErrorF("ExprParser", "{}: the initial state must be a value, not a function", foldSig);
		const FunctionBinding& step = *stepArg.binding;
		if (TreeItem_IsFunctionVariantSet(step.funcItem.get()))
			throwErrorF("ExprParser", "{}: the step '{}' is a set of variants; pass the variant to use"
				, foldSig, step.funcItem->GetFullName().c_str());
		if (step.NrHoles() != 2)
			throwErrorF("ExprParser", "{}: the step '{}' must take two arguments, an element of the domain and the state; it leaves {} open"
				, foldSig, step.funcItem->GetFullName().c_str(), step.NrHoles());

		// the elements of the domain, read at meta time
		row_id nrSteps = 0;
		Float64 firstElem = 0;
		{
			FencedInterestRetainContext irc("hof::ReduceFold");
			auto dc = GetOrCreateDataController(domainArg.key);
			MG_CHECK(dc);
			irc.Add(dc.get());
			auto res = dc->MakeResult();
			if (!res)
			{
				dms_assert(dc->WasFailed(FailType::MetaInfo));
				if (errorHolder)
					errorHolder->ThrowFail(dc.get());
				throwErrorF("ExprParser", "{}: the domain cannot be determined", foldSig);
			}
			if (!IsUnit(res.get()))
				throwErrorF("ExprParser", "{}: the first argument must be a unit, the domain whose elements the step visits", foldSig);
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
			{
				if (errorHolder)
					errorHolder->ThrowFail(dc.get());
				throwErrorF("ExprParser", "{}: the domain cannot be calculated", foldSig);
			}
			auto domainUnit = AsUnit(fd->GetOld());
			auto vc = domainUnit->GetValueType();
			if (domainUnit->GetNrDimensions() != 1 || !vc->IsIntegral())
				throwErrorF("ExprParser", "{}: the domain must be a unit of integers, whose elements can be counted; it has values of type {}"
					, foldSig, vc->GetNameID());
			nrSteps = domainUnit->GetCount();
			firstElem = domainUnit->GetRangeAsFloat64().first;
		}

		for (row_id k = 0; k != nrSteps; ++k)
		{
			CallArg elem;
			elem.key = AsLispRef(Float64(firstElem + k), domainArg.key); // (convert value D): a parameter<D>
			FunctionBinding merged = MergeBinding(step, { std::move(elem), std::move(state) });
			state = ReduceMergedValue(merged, parent, substBuff, errorHolder);
			if (state.binding)
				throwErrorF("ExprParser", "{}: the step '{}' returned a function; it must return the next state"
					, foldSig, step.funcItem->GetFullName().c_str());
			if (trace)
				trace->push_back(state);
		}
		return state;
	}

	namespace {
		// #1347: calculate the parameter `key` now, at meta time, as fold calculates its
		// domain, and return its value. fold_c reads its maximum number of steps and the
		// condition of each state this way; `what` names the argument in an error
		Float64 CalcMetaParameter(LispPtr key, bool mustBeBool, CharPtr sig, CharPtr what, SharedTreeItem errorHolder)
		{
			FencedInterestRetainContext irc("hof::fold_c");
			auto dc = GetOrCreateDataController(key);
			MG_CHECK(dc);
			irc.Add(dc.get());
			auto res = dc->MakeResult();
			if (!res)
			{
				dms_assert(dc->WasFailed(FailType::MetaInfo));
				if (errorHolder)
					errorHolder->ThrowFail(dc.get());
				throwErrorF("ExprParser", "{}: {} cannot be determined", sig, what);
			}
			const AbstrDataItem* adi = IsDataItem(res.get()) ? AsDataItem(res.get()) : nullptr;
			const ValueClass* vc = adi ? adi->GetAbstrValuesUnit()->GetValueType() : nullptr;
			if (!adi || !adi->HasVoidDomainGuarantee()
				|| !(mustBeBool ? vc->GetValueClassID() == ValueClassID::VT_Bool : vc->IsIntegral()))
				throwErrorF("ExprParser", "{}: {} must be a {}", sig, what
					, mustBeBool ? "parameter<bool>" : "parameter with an integer value type, such as parameter<uint32>");
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
			{
				if (errorHolder)
					errorHolder->ThrowFail(dc.get());
				throwErrorF("ExprParser", "{}: {} cannot be calculated", sig, what);
			}
			auto resAdi = AsDataItem(fd->GetOld());
			PreparedDataReadLock lock(resAdi, "hof::fold_c");
			return resAdi->GetRefObj()->GetValueAsFloat64(0);
		}

		// interest in the data of a state and of its declared sub-items, so that the
		// calculation of the next state finds it instead of recalculating the chain before it
		void HoldState(const CallArg& state, std::vector<FutureData>& holds)
		{
			auto add = [&holds](LispPtr key)
			{
				if (!key.EndP())
					if (auto dc = GetOrCreateDataController(key))
						holds.emplace_back(dc.get());
			};
			add(state.key);
			if (!state.structuredResult)
				return;
			std::vector<const std::vector<StructuredFunctionResultMember>*> levels = { &state.structuredResult->subItems };
			while (!levels.empty())
			{
				auto level = levels.back(); levels.pop_back();
				for (const auto& m : *level)
				{
					add(m.key);
					levels.push_back(&m.subItems);
				}
			}
		}
	}

	// #1347: fold_c(f, cond, maxN, x0) applies f while cond holds for the state, at most
	// maxN times: with s[0] = x0, s[k+1] = f(k, s[k]) for as long as cond(s[k]) is true,
	// and the result is the first state for which cond is false. The step takes the number
	// of the step, a parameter<uint32> counting from 0, and the state. Whether to take
	// another step depends on data, so the condition of each state is calculated when the
	// expression is reduced, at meta time, as fold calculates its domain; a state whose
	// condition still holds after maxN steps is an error, not a truncation. The data of each
	// state is held while the next one is calculated, so a chain of n steps costs n steps.
	CallArg ReduceFoldC(const CallArg& stepArg, const CallArg& condArg, const CallArg& maxNArg, CallArg state
		, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
		, std::vector<CallArg>* trace)
	{
		constexpr CharPtr foldSig = "fold_c(step, condition, maxN, initial state)";
		if (stepArg.isHole || condArg.isHole || maxNArg.isHole || state.isHole)
			throwErrorF("ExprParser", "{}: '_' cannot be an argument of fold_c itself; it marks the open arguments of the step and the condition", foldSig);
		if (!stepArg.binding)
			throwErrorF("ExprParser", "{}: the first argument must be a function, or a partial application that leaves two arguments open", foldSig);
		if (!condArg.binding)
			throwErrorF("ExprParser", "{}: the second argument must be a function, or a partial application that leaves one argument open, the condition on the state", foldSig);
		if (maxNArg.binding || maxNArg.literal || maxNArg.key.EndP())
			throwErrorF("ExprParser", "{}: the third argument must be a number, the most steps that may be taken", foldSig);
		if (state.binding)
			throwErrorF("ExprParser", "{}: the initial state must be a value, not a function", foldSig);
		const FunctionBinding& step = *stepArg.binding;
		const FunctionBinding& cond = *condArg.binding;
		for (const FunctionBinding* fb : { &step, &cond })
			if (TreeItem_IsFunctionVariantSet(fb->funcItem.get()))
				throwErrorF("ExprParser", "{}: the {} '{}' is a set of variants; pass the variant to use"
					, foldSig, fb == &step ? "step" : "condition", fb->funcItem->GetFullName().c_str());
		if (step.NrHoles() != 2)
			throwErrorF("ExprParser", "{}: the step '{}' must take two arguments, the number of the step and the state; it leaves {} open"
				, foldSig, step.funcItem->GetFullName().c_str(), step.NrHoles());
		if (cond.NrHoles() != 1)
			throwErrorF("ExprParser", "{}: the condition '{}' must take one argument, the state; it leaves {} open"
				, foldSig, cond.funcItem->GetFullName().c_str(), cond.NrHoles());

		Float64 maxN = CalcMetaParameter(maxNArg.key, false, foldSig, "the third argument, the most steps that may be taken,", errorHolder);
		if (maxN < 0)
			throwErrorF("ExprParser", "{}: the third argument, the most steps that may be taken, is {}; it cannot be negative", foldSig, maxN);

		std::vector<FutureData> heldPrev; // the data of the state before the current one
		for (row_id k = 0; ; ++k)
		{
			std::vector<FutureData> heldCurr;
			HoldState(state, heldCurr); // before the condition calculates it, so that its data stays

			FunctionBinding condMerged = MergeBinding(cond, { state });
			CallArg c = ReduceMergedValue(condMerged, parent, substBuff, errorHolder);
			if (c.binding || c.key.EndP())
				throwErrorF("ExprParser", "{}: the condition '{}' must return a parameter<bool>"
					, foldSig, cond.funcItem->GetFullName().c_str());
			bool goOn = CalcMetaParameter(c.key, true, foldSig, "the result of the condition", errorHolder) != 0;
			heldPrev = std::move(heldCurr);
			if (!goOn)
				return state;
			if (Float64(k) >= maxN)
				throwErrorF("ExprParser", "{}: the condition '{}' still holds after {} steps, the most that the third argument allows"
					, foldSig, cond.funcItem->GetFullName().c_str(), k);

			CallArg elem;
			elem.key = AsLispRef(UInt32(k)); // the number of the step: a parameter<uint32>
			FunctionBinding merged = MergeBinding(step, { std::move(elem), std::move(state) });
			state = ReduceMergedValue(merged, parent, substBuff, errorHolder);
			if (state.binding)
				throwErrorF("ExprParser", "{}: the step '{}' returned a function; it must return the next state"
					, foldSig, step.funcItem->GetFullName().c_str());
			if (trace)
				trace->push_back(state);
		}
	}

	CallArg ReduceFoldCall(TokenID head, const std::vector<CallArg>& args
		, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
		, std::vector<CallArg>* trace)
	{
		if (head == t_FoldC)
		{
			if (args.size() != 4)
				throwErrorF("ExprParser", "fold_c(step, condition, maxN, initial state): expects 4 arguments; {} provided", args.size());
			return ReduceFoldC(args[0], args[1], args[2], args[3], parent, substBuff, errorHolder, trace);
		}
		dms_assert(head == t_Fold);
		if (args.size() != 3)
			throwErrorF("ExprParser", "fold(domain, step, initial state): expects 3 arguments; {} provided", args.size());
		return ReduceFold(args[0], args[1], args[2], parent, substBuff, errorHolder, trace);
	}

	std::shared_ptr<const StructuredFunctionResult> MakeFoldTrace(const std::vector<CallArg>& trace, const CallArg& result)
	{
		auto sr = std::make_shared<StructuredFunctionResult>();
		auto add = [&sr](TokenID id, const CallArg& s)
		{
			auto& m = sr->subItems.emplace_back();
			m.id = id;
			m.key = s.key;
			if (s.structuredResult)
				m.subItems = s.structuredResult->subItems;
		};
		for (SizeT k = 0; k != trace.size(); ++k)
			add(GetTokenID_mt(mySSPrintF("step{}", k).c_str()), trace[k]);
		add(GetTokenID_mt("result"), result);
		return sr;
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

			// #1347: fold(...) or fold_c(...) as an argument: its value, including the
			// declared sub-items of a structured state
			if (IsFoldHead(headID))
			{
				std::vector<CallArg> foldArgs;
				for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
					foldArgs.push_back(ResolveCallerArg(a.Left(), resolveData, findItem, substBuff, errorHolder));
				if (substBuff && substBuff->avs == AVS_SuspendedOrFailed)
					return {};
				return ReduceFoldCall(headID, foldArgs, nullptr, substBuff, errorHolder);
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
			candidates = candidates + SharedStr(v->GetNameID());

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
					, setItem->GetFullName().c_str(), best->GetNameID(), v->GetNameID());
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
