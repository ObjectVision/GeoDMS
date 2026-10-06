// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SymPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// (Assoc merged in, 2026-08)


/****************** Lisp interpreter              *******************/

#include "LispEval.h"
#include "dbg/debug.h"
#include "ser/AsString.h"

#if defined(MG_DEBUG)
	#include "utl/IncrementalLock.h"
	#define MaxAllowedLevel 20000
	#define MG_TRACE_LISP false
#endif

#include <functional>
#include <vector>
#include "set/Cache.h"

// This TU's LispComponent precedes every LispRef static below: within one TU dynamic
// initialisation runs in declaration order, so the caches exist for them and, at exit, outlive
// them (g_applyTopEnvCache holds LispRefs until then). It must stay unconditional: placed inside
// a never-defined #if (2026-09-05, for a few hours) this TU had no component at all, the count
// hit zero before g_applyTopEnvCache was destroyed, and every GeoDmsRun crashed at exit.
LispComponent s_LispServiceSubscription;


/******************  Match                        ******************/

/* Match tests if the constant expression expr can match header */

AssocList Match(AssocListPtr aList, LispPtr header, LispPtr expr)
{
	if (header.IsVar())
		return Insert(aList, Assoc(header, expr) );
	if (!header.IsRealList())
	{
		if (header==expr)
			return aList;
		else
			return AssocList::failed();
	}
	if (!expr.IsRealList())
		return AssocList::failed();
	AssocList newAssocList = Match(aList, header.Left(), expr.Left());
	if (newAssocList.IsFailed())
		return newAssocList;
	return Match(newAssocList, header.Right(), expr.Right());
}

//==============================

#include "RewriteRules.h"
#include "ptr/OwningPtr.h"

static std::unique_ptr<RewriteRuleSet> g_RewriteRuleSetPtr;

void SetEnv(AssocListPtr env)
{
	g_RewriteRuleSetPtr.reset(new RewriteRuleSet(env));
}
LispRef AssocList_RepApplyTopEnv(AssocListPtr unifier, LispPtr templExpr); // forward decl 

LispRef AssocList_RepApplyTopEnvList(AssocListPtr unifier, LispPtr templExprPtr)
{
	DBG_START("AssocList", "RepApplyTopEnvList", MG_TRACE_LISP);
	DBG_TRACE(("templExprList   = {}", AsString(templExprPtr).c_str()));

	// Iterate the list right-spine instead of tail-recursing on .Right(); each
	// processed Left() is collected and the result is folded right-to-left.
	// AssocList_RepApplyTopEnv (per Left) is still called recursively -- the
	// full bound on iterated-calc depths requires also iterativizing the
	// ApplyTopEnv wrap inside RepApplyTopEnv (worklist-driver, deferred).
	struct frame_t { LispPtr originalPair; LispRef processedLeft; };
	std::vector<frame_t> frames;

	LispPtr cursor = templExprPtr;
	while (true)
	{
		if (cursor.IsVar())
			break; // TAIL var: terminal at the unifier lookup below
		assert(cursor.IsList()); // constants cannot be tail in resulting list
		if (!cursor.IsRealList())
			break; // empty list terminates the spine
		LispRef left = AssocList_RepApplyTopEnv(unifier, cursor.Left());
		frames.push_back({cursor, std::move(left)});
		cursor = cursor.Right();
	}

	LispRef tail;
	if (cursor.IsVar())
	{
		auto found = unifier.FindByKey(cursor);
		assert(!found.IsFailed()); // all vars should be matched
		tail = found.Val();
	}
	else
	{
		tail = cursor; // non-real-list (empty terminator)
	}

	for (auto it = frames.rbegin(); it != frames.rend(); ++it)
		tail = LispRef(it->processedLeft, tail);
	return tail;
}

LispRef AssocList_RepApplyTopEnv(AssocListPtr unifier, LispPtr templExpr)
{
	DBG_START("AssocList", "RepApplyTopEnv", MG_TRACE_LISP);
	DBG_TRACE(("templExpr   = {}", AsString(templExpr).c_str()));

	if (templExpr.IsVar())
	{
		AssocPtr found = unifier.FindByKey(templExpr);
		dms_assert( !found.IsFailed() ); // all vars should be matched
		return found.Val();
	}
	if (!templExpr.IsRealList())
		return templExpr;

	return ApplyTopEnv(
		LispRef(
			templExpr.Left() // FUNC-HEAD
		,	AssocList_RepApplyTopEnvList(unifier, templExpr.Right())
		)
	);
}

// The key and value types of g_applyTopEnvCache; ApplyTopEnv computes the values itself and
// stores them with UnorderedMapCache::store.
struct ApplyTopEnvFunc
{
	using argument_type = LispRef;
	using result_type = LispRef;
	using hasher = std::hash<const LispObj*>;
	using equality_compare = std::equal_to<LispPtr>;
};

UnorderedMapCache<ApplyTopEnvFunc> g_applyTopEnvCache;

#if defined(MG_DEBUG)
std::atomic<UInt32> gd_ApplyTopLevel = 0;
#endif

LispRef ApplyTopEnv(LispPtr root_expr)
{
	assert(IsMetaThread());
#if defined(MG_DEBUG)
	StaticMtIncrementalLock<gd_ApplyTopLevel> levelLock;
	assert(gd_ApplyTopLevel <= MaxAllowedLevel);
#endif
	assert(root_expr.IsRealList());

	// Iterative driver for the outer rewrite-rule application chain. Each
	// loop iteration tries to match one rule against `current`; on match the
	// substituted result becomes the new `current` and we loop. When no rule
	// matches, `current` is the fixed point.
	//
	// The inner AssocList_RepApplyTopEnv recursion remains -- it walks the
	// matched rule's template tree, whose depth is bounded by the template
	// shape (typically 3-5 levels). The wraps in ApplyTopEnv inside that
	// recursion now re-enter THIS iterative driver, so the chain of nested
	// ApplyTopEnv invocations in iterated-calc configs no longer grows the
	// C stack proportional to chain length.
	//
	// Cache write-back at the end matches the recursive form's behavior:
	// the original root_expr and every intermediate expression in the chain
	// map to the fixed-point result.
	std::vector<LispRef> chain;
	LispRef current = root_expr;

	while (current.IsRealList())
	{
		auto cached = g_applyTopEnvCache.lookup(current);
		if (cached)
		{
			current = *cached;
			break;
		}

		// Find the first matching rule.
		LispPtr head = current.Left();
		auto rulePtr = g_RewriteRuleSetPtr->FindLowerBoundByFuncName(head);
		auto ruleEnd = g_RewriteRuleSetPtr->End();
		bool ruleMatched = false;
		LispRef substituted;
		while (rulePtr != ruleEnd)
		{
			LispPtr pattern = rulePtr->Key();
			if (pattern.Left() != head)
				break;
			AssocList unifier = Match(AssocList(), pattern, current);
			if (!unifier.IsFailed())
			{
				substituted = AssocList_RepApplyTopEnv(unifier, rulePtr->Val());
				ruleMatched = true;
				break;
			}
			++rulePtr;
		}

		if (!ruleMatched)
			break; // current is the fixed point

		chain.push_back(current);
		current = substituted;
	}

	for (auto& e : chain)
		g_applyTopEnvCache.store(e, current);
	g_applyTopEnvCache.store(root_expr, current);

	return current;
}

// ==== Assoc ====

#include "Assoc.h"

#include "ser/FormattedStream.h"


auto Assoc::failed() -> AssocPtr
{
	static AssocPtr result = AssocPtr(LispPtr());
	return result;
}

auto AssocList::empty() -> AssocListPtr
{
	static AssocListPtr result = AssocListPtr(LispPtr());
	return result;
}

auto AssocList::failed() ->AssocListPtr
{
	static AssocList assocListFailed = AssocList(Assoc::failed(), AssocList::empty());
	return AssocListPtr(assocListFailed.get());
}

/**************** operators      *****************/

FormattedOutStream& operator <<(FormattedOutStream& output, AssocPtr a)
{
	return output << a.Key() << CharPtr(" = ") << a.Val();
}
