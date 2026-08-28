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
extern StaticTokenID t_ApplyItem;        // §5.9 'apply X(args)' marker head
extern StaticTokenID t_InstantiateItem;  // §5.9 'instantiate X(args)' marker head
extern StaticTokenID t_ApplyValue;       // §5.10 '(args)' applied to a call result
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
		std::shared_ptr<ClosureEnv>    m_Env;         // §5.10: closure environment of the applied function, else null
		std::vector<const TreeItem*>   m_Params;      // the first N sub-items of m_FuncItem
		const TreeItem*                m_RestParam = nullptr; // '...x' rest param (the last param); binds m_ArgKeys[nrParams-1 .. end)
		std::map<const TreeItem*, LispRef> m_Reductions;
		std::set<const TreeItem*>      m_InProgress;

		void PushArg(const CallArg& a) { m_ArgKeys.push_back(a.key); m_ArgItems.push_back(a.item); m_ArgBindings.push_back(a.binding); m_ArgLiterals.push_back(a.literal); }

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
				env->args.push_back(std::move(a));
			}
			env->next = m_Env;
			return env;
		}

		bool IsRestParamSymbol(TokenID sym) const { return m_RestParam && m_RestParam->GetID() == sym; }
		void SpliceRestArgs(std::vector<CallArg>& out) const
		{
			assert(m_RestParam);
			for (UInt32 k = TreeItem_GetFunctionParamCount(m_FuncItem) - 1; k != m_ArgKeys.size(); ++k)
			{
				CallArg a; a.key = m_ArgKeys[k]; a.item = m_ArgItems[k]; a.binding = m_ArgBindings[k]; a.literal = m_ArgLiterals[k];
				out.push_back(std::move(a));
			}
		}

		LispRef Reduce();
		CallArg ReduceValue(); // §5.10: like Reduce, but a function-typed result yields a closure binding
		bool ResolveEnvSymbol(TokenID symbID, SharedTreeItem* foundItemPtr, LispRef* keyPtr, std::shared_ptr<FunctionBinding>* bindingPtr); // §5.10 closure-env lookup
		LispRef ReduceBodyItem(const TreeItem* bodyItem);
		LispRef SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr);
		CallArg ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr);
		SharedTreeItem ResolveBodyHeadFunction(const TreeItem* refScope, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding, bool mayFail = false);
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

// §5.10 Stage 2: does `tok` name a generic type/domain variable of `fn`, a DOMAIN-sorted
// one, resp. one declared in fn's OWN <...> clause?
bool IsGenericVarOf(const TreeItem* fn, TokenID tok);
bool IsDomainSortedVarOf(const TreeItem* fn, TokenID tok);
bool IsOwnDeclaredVar(const TreeItem* fn, TokenID tok);

// WP3.4 + tranche 3 typed walker; defined in HofTypeChecker.cpp
void CheckFunctionDefinition(const TreeItem* funcItem);

} // namespace hof

#endif // __TIC_HOFCLOSURE_H
