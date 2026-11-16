// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Internal to the calculator component: the vocabulary of the typed-HOF language --
 *  function values, closures, resolved call arguments, and the application record that
 *  a reduction works on. AbstrCalculator.cpp used to carry the whole HOF layer in one
 *  anonymous namespace of 5000 lines; it is now split by role over HofClosure.cpp
 *  (this vocabulary), HofTypeUnifier.cpp (type variables and signature contracts),
 *  HofApplication.cpp (reduction) and HofTypeChecker.cpp / HofOperSignatureInfer.cpp
 *  (definition-time checking), which is why these types need a shared declaration.
 *
 *  Everything lives in namespace hof: these are internal names with external linkage
 *  now, and 'CallArg' or 'DefType' are too generic to put in the global namespace.
 *
 *  Not part of the Tic interface -- do not include this outside rtc/dll/src/tic.
 */

#if !defined(__TIC_HOFCLOSURE_H)
#define __TIC_HOFCLOSURE_H

#include "AbstrCalculator.h"
#include "MetaInfo.h"
#include "TreeItemFunctionSpec.h"
#include "LispRef.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

class ValueClass;
struct AbstrOperGroup;

// ---- provided by AbstrCalculator.cpp -----------------------------------------------

// register `supplier` with the buffer's optional visitor, so a body-resolved external
// becomes a supplier of the calling item
void registerSupplier(SubstitutionBuffer& substBuff, const TreeItem* supplier);

// walk `relPath` down from a non-cache item, throwing when a step does not exist
SharedTreeItem FindSubItem(const TreeItem* sourceItem, SharedStr relPath);

namespace hof {

// ---- the marker heads and placeholders of the typed-HOF notation --------------------

extern StaticTokenID t_Hole;             // '_' partial-application placeholder
extern StaticTokenID t_Map;              // built-in map(function, container) metafunction
extern StaticTokenID t_Fold;             // #1347 built-in fold(domain, step, initial state)
extern StaticTokenID t_FoldC;            // #1347 built-in fold_c(step, condition, maxN, initial state)
inline bool IsFoldHead(TokenID id) { return id == t_Fold || id == t_FoldC; }
extern StaticTokenID t_ApplyItem;        // §5.9 'apply X(args)' marker head
extern StaticTokenID t_InstantiateItem;  // §5.9 'instantiate X(args)' marker head
extern StaticTokenID t_ApplyValue;       // §5.10 '(args)' applied to a call result
// #1347: the key under which a body item with the rule 'instantiate fold(...)' records the
// states of its steps in a function applied as a value, (instantiate_item <key of the final
// state>); member access finds the states by it, but it is no value itself
inline bool IsFoldTraceKey(LispPtr key) { return key.IsRealList() && key.Left().IsSymb() && key.Left().GetSymbID() == t_InstantiateItem; }
extern StaticTokenID t_ContainerLiteral; // §5.9 '{ m: e; … }' argument literal
extern StaticTokenID t_Member;           // §5.9 '(member name value)'
extern StaticTokenID t_NoDomain;         // §5.9 domain-less literal marker
extern StaticTokenID t_Dot;              // §5.9 current-domain reference in members

	// a destructured container-literal argument: the domain (if any) and its named members,
	// all already resolved to keys in the caller scope; bound to a structured parameter and
	// consumed member-by-member during body substitution (no anonymous item is materialized).
	struct ContainerLiteralArg
	{
		bool                                     hasDomain = false;
		LispRef                                  domainKey; // resolved domain unit key
		std::vector<std::pair<TokenID, LispRef>> members;   // member name -> resolved value key
	};

	struct FunctionBinding;

	// one resolved argument to a function application: either a data/unit value
	// (key + optional plain-reference item), a function value (binding), or a hole
	struct CallArg
	{
		LispRef                          key;             // data/unit argument (empty otherwise)
		SharedTreeItem                   item;            // plain-reference item (member access), else null
		std::shared_ptr<FunctionBinding> binding;         // function value (plain ref or partial application), else null
		std::shared_ptr<ContainerLiteralArg> literal;     // §5.9 container-literal argument, else null
		std::shared_ptr<const StructuredFunctionResult> structuredResult; // sub-items declared beneath a returned unit
		bool                             isHole = false;  // '_' placeholder
		bool IsFunctionValue() const { return binding != nullptr; }
	};

	// §5.10 closure environment: the enclosing application's parameters and their bound
	// values, captured BY VALUE (already-substituted keys/items/bindings) when a nested
	// function is returned as a result. `next` chains the enclosing function's own
	// environment (nested closures). Because captured values are concrete interned
	// keys -- never unresolved symbols -- capture is hygienic by construction.
	struct ClosureEnv
	{
		const TreeItem*              funcItem = nullptr; // the enclosing function definition
		std::vector<CallArg>         args;               // its bound arguments, positionally
		std::shared_ptr<ClosureEnv>  next;               // the enclosing application's own env
	};

	// a function value: the function plus one slot per declared parameter. A slot with
	// isHole is unbound; applying the binding fills the holes left-to-right. `env` is
	// the captured closure environment when the function was returned as a result.
	struct FunctionBinding
	{
		SharedTreeItem       funcItem;
		std::vector<CallArg> slots;
		std::shared_ptr<ClosureEnv> env;
		UInt32 NrHoles() const { UInt32 n = 0; for (const auto& s : slots) if (s.isHole) ++n; return n; }
	};

	struct FunctionApplication
	{
		const TreeItem*                m_FuncItem = nullptr;
		const FunctionApplication*     m_Parent = nullptr; // enclosing application (nested calls only): recursion detection follows THIS chain, so unrelated re-entry through UpdateMetaInfo of externals cannot raise false 'recursive' errors
		SubstitutionBuffer*            m_SubstBuff = nullptr; // caller's buffer: body-resolved externals register as suppliers of the calling item
		SharedTreeItem                 m_ErrorHolder; // caller item, for failure attribution
		std::vector<LispRef>           m_ArgKeys;     // per param: data/unit key (empty for function-valued params)
		std::vector<SharedTreeItem>    m_ArgItems;    // per param: the referenced item iff the argument was a plain reference (enables member access), else null
		std::vector<std::shared_ptr<FunctionBinding>> m_ArgBindings; // per param: the bound function value iff the argument is a function, else null
		std::vector<std::shared_ptr<ContainerLiteralArg>> m_ArgLiterals; // per param: the container-literal argument, else null
		std::vector<std::shared_ptr<const StructuredFunctionResult>> m_ArgStructured; // per param: the declared sub-items of an argument that is the result of a function application (#1347: the state of a fold), else null
		std::shared_ptr<ClosureEnv>    m_Env;         // §5.10: closure environment of the applied function, else null
		std::vector<const TreeItem*>   m_Params;      // the first N sub-items of m_FuncItem
		const TreeItem*                m_RestParam = nullptr; // '...x' rest param (the last param); binds m_ArgKeys[nrParams-1 .. end)
		std::map<const TreeItem*, LispRef> m_Reductions;
		std::set<const TreeItem*>      m_InProgress;
		std::map<const TreeItem*, std::shared_ptr<const StructuredFunctionResult>> m_StructuredByItem; // #1347, #1365: per body item whose value has declared sub-items (the result of a function or a fold, the states of 'instantiate fold', or a reference to such a value), those sub-items; by item, not by key, because two items can share a calculation and differ in what they declare

		void PushArg(const CallArg& a) { m_ArgKeys.push_back(a.key); m_ArgItems.push_back(a.item); m_ArgBindings.push_back(a.binding); m_ArgLiterals.push_back(a.literal); m_ArgStructured.push_back(a.structuredResult); }

		// §5.10: this application's bound parameters as a closure environment, chained
		// to the environment it was itself applied in. Captured by value, so the result
		// is independent of where the capturing function value travels (#1166).
		std::shared_ptr<ClosureEnv> MakeCurrentEnv() const
		{
			auto env = std::make_shared<ClosureEnv>();
			env->funcItem = m_FuncItem;
			UInt32 nrParams = TreeItem_GetFunctionParamCount(m_FuncItem);
			env->args.reserve(nrParams);
			for (UInt32 i = 0; i != nrParams && i != m_ArgKeys.size(); ++i)
			{
				CallArg a;
				a.key = m_ArgKeys[i]; a.item = m_ArgItems[i];
				a.binding = m_ArgBindings[i]; a.literal = m_ArgLiterals[i];
				a.structuredResult = m_ArgStructured[i];
				env->args.push_back(std::move(a));
			}
			env->next = m_Env;
			return env;
		}

		bool IsRestParamSymbol(TokenID sym) const { return m_RestParam && m_RestParam->GetNameID() == sym; }
		void SpliceRestArgs(std::vector<CallArg>& out) const
		{
			assert(m_RestParam);
			for (UInt32 k = TreeItem_GetFunctionParamCount(m_FuncItem) - 1; k != m_ArgKeys.size(); ++k)
			{
				CallArg a; a.key = m_ArgKeys[k]; a.item = m_ArgItems[k]; a.binding = m_ArgBindings[k]; a.literal = m_ArgLiterals[k];
				a.structuredResult = m_ArgStructured[k];
				out.push_back(std::move(a));
			}
		}

		LispRef Reduce();
		CallArg ReduceValue(); // §5.10: like Reduce, but a function-typed result yields a closure binding
		const CallArg* FindEnvArg(TokenID symbID) const; // §5.10 closure-env lookup: the captured argument, or null
		LispRef ReduceBodyItem(const TreeItem* bodyItem);
		// structuredPtr, when given, receives the declared sub-items of the value when the whole
		// expression (resp. the symbol) is a value that has them, such as the result of a function
		LispRef SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr, std::shared_ptr<const StructuredFunctionResult>* structuredPtr = nullptr);
		LispRef ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr, std::shared_ptr<const StructuredFunctionResult>* structuredPtr = nullptr);
		CallArg ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr);
		SharedTreeItem ResolveBodyHeadFunction(const TreeItem* refScope, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding, bool mayFail = false);
		// #1366: the refusal of the meta function call `headID`(argsList) in this body, with what to do instead
		[[noreturn]] void ThrowMetaCallInBody(const AbstrOperGroup* og, TokenID headID, const TreeItem* refScope, LispPtr argsList) const;
	};

// ---- the operations on them ---------------------------------------------------------

// WP4.5: look a call head up in the auto-imported standard prelude ('prelude' container
// under the config root); empty when there is none
SharedTreeItem FindPreludeFunction(TokenID nameID);

// replace every bare '.' (the current-domain reference) in a container-literal member
// expression with the literal's domain expression, before caller-scope resolution
LispRef ReplaceDot(LispPtr expr, LispPtr domainExpr);

// build a destructured container-literal argument from
// (container_literal <domain|no_domain> (member name value)…), resolving through `resolve`
std::shared_ptr<ContainerLiteralArg> BuildContainerLiteral(LispPtr litExpr, const std::function<LispRef(LispPtr)>& resolve);

bool IsFunctionResultMetaCall(LispPtr expr);
void RejectNestedFunctionResultMetaCall(LispPtr expr);

// a plain function reference is a binding with every slot a hole
std::shared_ptr<FunctionBinding> MakeAllHoles(SharedTreeItem func);

// #1166: is `callee` nested in `outer`'s body, so that it is applied in the enclosing
// application's scope?
bool IsNestedInside(const TreeItem* callee, const TreeItem* outer);

// fill the holes of `b` with `holeFills` left-to-right
FunctionBinding MergeBinding(const FunctionBinding& b, const std::vector<CallArg>& holeFills);

// §5.10: reduce a fully-bound application to its VALUE -- a data key, or a closure
// binding when the applied function has a function-typed result
CallArg ReduceMergedValue(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder);
LispRef ReduceMerged(const FunctionBinding& merged, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
	, std::shared_ptr<const StructuredFunctionResult>* structuredResult = nullptr);

// #1347: reduce a fold to its value, the state after the last step. `head` is t_Fold,
// fold(domain, step, initial state), which applies the step to each element of the domain
// in order, or t_FoldC, fold_c(step, condition, maxN, initial state), which applies the
// step for as long as the condition holds for the state, at most maxN times. The
// arguments are already resolved, in the caller's scope or in a function body. When
// `trace` is given, it receives the state after each step that is taken. The step and the
// condition may be sets of variants (#1367); the variant is chosen on the initial state.
CallArg ReduceFoldCall(TokenID head, const std::vector<CallArg>& args
	, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
	, std::vector<CallArg>* trace = nullptr);
CallArg ReduceFold(const CallArg& domainArg, const CallArg& stepArg, CallArg state
	, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
	, std::vector<CallArg>* trace = nullptr);
CallArg ReduceFoldC(const CallArg& stepArg, const CallArg& condArg, const CallArg& maxNArg, CallArg state
	, const FunctionApplication* parent, SubstitutionBuffer* substBuff, SharedTreeItem errorHolder
	, std::vector<CallArg>* trace = nullptr);

// #1347: the items that 'instantiate fold(...)' and 'instantiate fold_c(...)' publish:
// step0, step1, ..., one per step that is taken, each holding the state after that step,
// and result, the final state, which is the initial state when no step is taken
std::shared_ptr<const StructuredFunctionResult> MakeFoldTrace(const std::vector<CallArg>& trace, const CallArg& result);

// #1347: the key of the declared sub-item `relPath` ('a' or 'a/b') of a function
// result; empty when it declares no such sub-item
LispRef FindStructuredMember(const StructuredFunctionResult& sr, CharPtrRange relPath);

// resolve one argument expression in the CALLER's scope to a CallArg
CallArg ResolveCallerArg(LispPtr argExpr,
	const std::function<LispRef(LispPtr)>& resolveData,
	const std::function<SharedTreeItem(TokenID)>& findItem,
	SubstitutionBuffer* substBuff, SharedTreeItem errorHolder);

// §5.7 variant dispatch: the value class of a reduced argument key, resp. the declared
// value class of a variant parameter
const ValueClass* ArgValueClass(LispRef key, SharedTreeItem errorHolder);
const ValueClass* ParamValueClass(const TreeItem* param);

// §5.7 v2: select the variant of `setItem` whose acceptance set matches the argument
// value classes, taking the MOST SPECIFIC match
const TreeItem* ResolveVariant(const TreeItem* setItem, const std::vector<CallArg>& callArgs, SharedTreeItem errorHolder);
// #1367: the selection of ResolveVariant without its errors: null when no variant matches, or
// when two match and neither is more specific, which *tiePtr then names (two nulls otherwise);
// the names of all variants are appended to *candidatesPtr when given
const TreeItem* SelectVariant(const TreeItem* setItem, const std::vector<const ValueClass*>& argVCs
	, std::pair<const TreeItem*, const TreeItem*>* tiePtr = nullptr, SharedStr* candidatesPtr = nullptr);

// §5.10 Stage 2: does `tok` name a generic type/domain variable of `fn`, a DOMAIN-sorted
// one, resp. one declared in fn's OWN <...> clause?
bool IsGenericVarOf(const TreeItem* fn, TokenID tok);
bool IsDomainSortedVarOf(const TreeItem* fn, TokenID tok);
bool IsOwnDeclaredVar(const TreeItem* fn, TokenID tok);

// WP3.4 + tranche 3 typed walker; defined in HofTypeChecker.cpp
void CheckFunctionDefinition(const TreeItem* funcItem);

// #1366: what keeps a body expression open over the parameters of its function, as the
// definition check finds it (FunctionChecker::TryBuildClosedKeyExpr): parameters of the
// function itself, and parameters and body items of an enclosing function, which a nested
// function reads through its closure
struct BodyExprOpenRefs
{
	std::vector<const TreeItem*> params;
	std::vector<const TreeItem*> captures;
	bool empty() const { return params.empty() && captures.empty(); }
};

// #1366: per argument of a call in the body of `funcItem`, whose parameters are `params`:
// `closed` when the argument depends on no parameter, as the definition check decides it, and
// else the names it depends on. An argument that is neither closed nor depends on a name,
// such as one that calls a function, has `closed` false and no names. Defined in
// HofTypeChecker.cpp
struct BodyCallArgDeps
{
	bool closed = false;
	BodyExprOpenRefs refs;
};
std::vector<BodyCallArgDeps> AnalyseBodyCallArgs(const TreeItem* funcItem, const std::vector<const TreeItem*>& params
	, const TreeItem* refScope, LispPtr argsList);

} // namespace hof

#endif // __TIC_HOFCLOSURE_H
