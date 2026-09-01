// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Reduction of a function application: substituting an inlined function's body in the
// caller's scope, resolving body symbols against parameters, closure environment and
// definition scope, and yielding the resulting calculation key or closure binding.

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

#include "HofTypeUnifier.h"

namespace hof {

	LispRef FunctionApplication::Reduce()
	{
		CallArg r = ReduceValue();
		if (r.binding)
			throwErrorF("ExprParser", "'{}': a function value can only be applied with '(...)', passed as an argument, or returned as a result"
				, m_FuncItem->GetFullName().c_str());
		return r.key;
	}

	CallArg FunctionApplication::ReduceValue()
	{
		assert(m_FuncItem && m_FuncItem->IsTemplate()); // functions are IsTemplate too
		assert(m_ErrorHolder);

		for (const FunctionApplication* ancestor = m_Parent; ancestor; ancestor = ancestor->m_Parent)
			if (ancestor->m_FuncItem == m_FuncItem && ancestor->m_Env == m_Env
				// same function AND same closure environment: §5.10 allows distinct
				// closures of one nested function within a single reduction chain.
				// '...x' rest folds recurse with STRICTLY FEWER arguments -- well-founded
				// on the parent chain, so permitted; equal-or-more args = true recursion
				&& m_ArgKeys.size() >= ancestor->m_ArgKeys.size())
				throwErrorF("ExprParser", "'{}': recursive function application is not supported"
					, m_FuncItem->GetFullName().c_str());

		// §5.9 'apply T(args)': a plain template applied as an ad-hoc function -- its params
		// are its first N sub-items with N = the number of provided arguments (the template
		// binding rule), and its designated result is the CI-unique 'result' sub-item
		bool isPlainTemplate = !m_FuncItem->IsFunctionItem();

		UInt32 nrParams = isPlainTemplate ? m_ArgKeys.size() : TreeItem_GetFunctionParamCount(m_FuncItem);
		bool hasRest = !isPlainTemplate && TreeItem_HasFunctionRestParam(m_FuncItem);
		if (hasRest ? m_ArgKeys.size() < nrParams : m_ArgKeys.size() != nrParams)
			throwErrorF("ExprParser", "'{}': function expects {}{} argument(s); {} provided"
				, m_FuncItem->GetFullName().c_str(), nrParams, hasRest ? " or more" : "", m_ArgKeys.size());
		if (isPlainTemplate)
		{
			UInt32 nrChildren = 0;
			for (const TreeItem* c = m_FuncItem->_GetFirstSubItem(); c; c = c->GetNextItem())
				++nrChildren;
			if (nrChildren < nrParams)
				throwErrorF("ExprParser", "'apply' on template '{}': {} argument(s) provided but the template has only {} sub-item(s)"
					, m_FuncItem->GetFullName().c_str(), nrParams, nrChildren);
		}
		else
			// tranche 3: definition-time check at every application entry (once per
			// function) -- uniformly covers closures, prelude functions and variant
			// members, which do not all pass through the direct-call substitution site
			CheckFunctionDefinition(m_FuncItem);

		// WP4.1: signature-instantiation constraints collected from 'sig<V, D>'-typed
		// parameters, merged into the type/domain variable bindings after the data
		// arguments have been processed
		struct SigConstraint
		{
			UInt32 paramIndex;
			SharedTreeItem sig, boundFn;
			const std::vector<std::pair<TokenID, TokenID>>* sigVars;
			const std::vector<TokenID>* typeArgs;
		};
		std::vector<SigConstraint> sigConstraints;

		m_Params.clear(); m_Params.reserve(nrParams);
		const TreeItem* child = m_FuncItem->_GetFirstSubItem();
		for (UInt32 i = 0; i != nrParams; ++i, child = child->GetNextItem())
		{
			MG_CHECK(child); // guaranteed by the parser: params are the first nrParams sub-items
			m_Params.push_back(child);

			// '...x' rest parameter (always last): binds the argument TAIL m_ArgKeys[i..end),
			// spliced where the body passes it as a trailing call argument -- never a scalar
			if (hasRest && i == nrParams - 1)
			{
				m_RestParam = child;
				continue;
			}

			// meta-reference parameter ('item x'): bind the RAW item reference (the same
			// sourceDescr form PropValue's subst_never argument gets in a direct call),
			// never the argument's calculation/range key -- so PropValue & co read the
			// CONFIG item's metadata, and the reduced key equals the direct call's key
			if (TreeItem_IsFunctionMetaRefParam(m_FuncItem, i))
			{
				if (!m_ArgItems[i])
					throwErrorF("ExprParser", "'{}': parameter '{}' is an item (meta-reference) parameter; its argument must be a reference to a config item, not a calculated expression"
						, m_FuncItem->GetFullName().c_str()
						, child->GetID().GetStrLock().c_str());
				m_ArgKeys[i] = CreateLispTree(m_ArgItems[i].get(), false);
			}
			m_Reductions[child] = m_ArgKeys[i];

			if (auto declaredSig = TreeItem_GetFunctionParamSignature(m_FuncItem, i))
			{
				if (!m_ArgBindings[i])
					throwErrorF("ExprParser", "'{}': parameter '{}' requires a function argument matching signature '{}'"
						, m_FuncItem->GetFullName().c_str()
						, child->GetID().GetStrLock().c_str()
						, declaredSig->GetFullName().c_str());
				// a partial application's residual arity must match; the full structural
				// check applies only to plain (all-holes) function references (WP3.1 v1)
				UInt32 residualArity = m_ArgBindings[i]->NrHoles();
				UInt32 requiredArity = TreeItem_GetFunctionParamCount(declaredSig.get());
				if (residualArity == TreeItem_GetFunctionParamCount(m_ArgBindings[i]->funcItem.get()))
					CheckFunctionSignature(m_ArgBindings[i]->funcItem.get(), declaredSig.get(), child->GetID().GetStrLock().c_str());
				else if (residualArity != requiredArity)
					throwErrorF("ExprParser", "'{}': partial application bound to parameter '{}' has {} remaining argument(s); signature '{}' requires {}"
						, m_FuncItem->GetFullName().c_str(), child->GetID().GetStrLock().c_str()
						, residualArity, declaredSig->GetFullName().c_str(), requiredArity);

				// WP4.1: enforce the type application 'sig<V, D>' -- the bound function's
				// CONCRETE positions constrain this application's type variables, shared
				// with (and checked against) the data-argument bindings below
				if (auto sigTypeArgs = TreeItem_GetFunctionParamSigTypeArgs(m_FuncItem, i))
					if (auto sigVars = TreeItem_GetFunctionTypeVars(declaredSig.get()); sigVars && sigTypeArgs->size() == sigVars->size())
						sigConstraints.push_back({ i, declaredSig, m_ArgBindings[i]->funcItem, sigVars, sigTypeArgs });
			}

			// K11a-3: a structured / by-example unit parameter carries a declared
			// member interface -- validate the ACTUAL argument against it here, at the
			// call boundary (clear attribution), instead of deep inside the reduced
			// body. The argument's CONFIG item (m_ArgItems -- the same reference the
			// body's member access binds to) carries the members; an expression
			// argument has none here and defers to the body's own resolution.
			// FUNCTION items only (review finding): a plain template's unit-parameter
			// sub-items (helper locals with calculation rules) are NOT a declared
			// member contract -- 'apply' on such templates must keep working.
			// K11a-4: CONTAINER parameters (plain non-data, non-function items with a
			// member block or exemplar) carry the same declared interface.
			if (!isPlainTemplate && m_ArgItems[i]
				&& (IsUnit(child) || (!IsDataItem(child) && !child->IsFunctionItem())))
			{
				const TreeItem* memberSrc = child->_GetFirstSubItem() ? child : nullptr;
				SharedTreeItem exKeep;
				if (!memberSrc)
					if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, i); ex && ex->_GetFirstSubItem())
					{
						exKeep = ex;
						memberSrc = exKeep.get();
					}
				if (memberSrc)
					CheckStructuredParamContract(m_FuncItem, child, memberSrc, m_ArgItems[i].get());
			}
		}

		// generic type variables: bind each variable from the actual arguments' value
		// classes / domain units into the unification store (WP4.1 tranche 2), which
		// also receives variable-variable links from the signature-typed parameters
		// below -- consistency and constraint satisfaction are checked per equivalence
		// class, with attribution
		TypeUnifier unifier{ m_FuncItem };
		SharedStr declSource = mySSPrintF("declared by function '{}'", m_FuncItem->GetFullName().c_str());
		UInt32 seqNr = 0, gpIndex; TokenID gpVar, gpConstraint; bool gpIsDomain;
		while (TreeItem_GetFunctionGenericParam(m_FuncItem, seqNr++, &gpIndex, &gpVar, &gpConstraint, &gpIsDomain))
		{
			MG_CHECK(gpIndex < nrParams);
			const TreeItem* gpParam = m_Params[gpIndex];
			if (m_ArgKeys[gpIndex].EndP())
				throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute or unit argument"
					, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStrLock().c_str());

			auto dc = GetOrCreateDataController(m_ArgKeys[gpIndex]);
			auto argResult = dc->MakeResult();
			if (!argResult)
			{
				dms_assert(dc->WasFailed(FailType::MetaInfo));
				m_ErrorHolder->ThrowFail(dc.get());
			}

			if (gpIsDomain)
			{
				// bind the domain variable from the argument's domain unit; a void
				// domain broadcasts into any D (the language's single implicit coercion)
				const AbstrUnit* du = IsDataItem(argResult.get()) ? AsDataItem(argResult.get())->GetAbstrDomainUnit() : nullptr;
				if (!du)
					throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute argument (its domain binds '{}')"
						, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStrLock().c_str(), gpVar.GetStrLock().c_str());
				if (du->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
				{ /* void broadcasts into any D and does not constrain it */ }
				else
					unifier.BindUnit(unifier.UnitVar(m_FuncItem, 0, gpVar), argResult, du
						, mySSPrintF("via parameter '{}'", gpParam->GetID().GetStrLock().c_str()));
				continue;
			}
			const ValueClass* vt = nullptr;
			if (IsDataItem(argResult.get()))
				vt = AsDataItem(argResult.get())->GetAbstrValuesUnit()->GetValueType();
			else if (IsUnit(argResult.get()))
				vt = AsUnit(argResult.get())->GetValueType();
			if (!vt)
				throwErrorF("ExprParser", "'{}': parameter '{}' requires an attribute or unit argument"
					, m_FuncItem->GetFullName().c_str(), gpParam->GetID().GetStrLock().c_str());
			unifier.BindValue(unifier.ValueVar(m_FuncItem, 0, gpVar, declSource), vt
				, mySSPrintF("parameter '{}'", gpParam->GetID().GetStrLock().c_str()));
		}

		// WP4.1: merge signature-instantiation constraints -- for each 'sig<V, D>'-typed
		// parameter, the bound function's positions instantiate or LINK the applied
		// variables (LinkSignatureBinding). Each binding gets its OWN instance of the
		// bound function's variables: two independent bindings of the same generic
		// function must not link through a shared node (tranche 3 fix).
		UInt32 bindingInstance = 0;
		for (const auto& sc : sigConstraints)
		{
			const TreeItem* viaParam = m_Params[sc.paramIndex];
			SharedStr sigSource = mySSPrintF("function '{}' bound to parameter '{}'"
				, sc.boundFn->GetFullName().c_str(), viaParam->GetID().GetStrLock().c_str());
			LinkSignatureBinding(unifier, sc.sig.get(), sc.boundFn.get(), sc.sigVars, sc.typeArgs
				, [&](TokenID t) { return unifier.ValueVar(m_FuncItem, 0, t, declSource); }
				, [&](TokenID t) { return unifier.UnitVar(m_FuncItem, 0, t); }
				, ++bindingInstance, sigSource);
		}

		SharedTreeItem resultChild;
		if (isPlainTemplate)
		{
			// CI-unique 'result' sub-item designates the value of an applied template
			for (const TreeItem* c = m_FuncItem->_GetFirstSubItem(); c; c = c->GetNextItem())
				if (!stricmp(c->GetID().GetStrLock().c_str(), "result"))
				{
					if (resultChild)
						throwErrorF("ExprParser", "'apply' on template '{}': multiple sub-items named 'result'"
							, m_FuncItem->GetFullName().c_str());
					resultChild = make_shared_tree(c, existing_obj{});
				}
			if (!resultChild)
				throwErrorF("ExprParser", "'apply' on template '{}': no 'result' sub-item to take as the value; use 'instantiate {}(…)' for the steps"
					, m_FuncItem->GetFullName().c_str(), m_FuncItem->GetID().GetStrLock().c_str());
			if (resultChild->GetExpr().empty())
				throwErrorF("ExprParser", "'apply' on template '{}': the 'result' sub-item has no calculation rule"
					, m_FuncItem->GetFullName().c_str());
		}
		else
		{
			TokenID resultName = TreeItem_GetFunctionResultName(m_FuncItem);
			resultChild = m_FuncItem->GetConstSubTreeItemByID(resultName);
			if (!resultChild)
				throwErrorF("ExprParser", "'{}': designated result '{}' not found"
					, m_FuncItem->GetFullName().c_str(), resultName.GetStrLock().c_str());
		}

		// §5.10: a function-typed result yields a closure -- the nested function plus
		// this application's bound parameters, captured by value
		if (resultChild->IsFunctionItem())
		{
			auto env = MakeCurrentEnv();

			CallArg r;
			r.binding = std::make_shared<FunctionBinding>();
			r.binding->funcItem = resultChild;
			r.binding->slots.resize(TreeItem_GetFunctionParamCount(resultChild.get()));
			for (auto& s : r.binding->slots)
				s.isHole = true;
			r.binding->env = std::move(env);
			return r;
		}

		if (resultChild->GetExpr().empty())
			throwErrorF("ExprParser", "'{}' is a function signature without implementation and cannot be applied"
				, m_FuncItem->GetFullName().c_str());

		CallArg r;
		r.key = ReduceBodyItem(resultChild.get());
		// A generic unit result is represented as a container in the inert
		// function tree (its concrete UnitClass is only known after applying the
		// function).  Retain its member block, but do not treat every container
		// result as structured: doing so changes ordinary container-returning HOFs
		// and can eagerly expand large generated trees.
		if ((IsUnit(resultChild.get()) || TreeItem_IsFunctionResultGenericUnit(m_FuncItem))
			&& resultChild->_GetFirstSubItem())
		{
			auto structured = std::make_shared<StructuredFunctionResult>();
			auto collectSubItems = [&](auto&& self, const TreeItem* source, std::vector<StructuredFunctionResultMember>& members) -> void
			{
				for (const TreeItem* child = source->_GetFirstSubItem(); child; child = child->GetNextItem())
				{
					StructuredFunctionResultMember member;
					member.id = child->GetID();
					if (!child->GetExpr().empty() || IsDataItem(child) || IsUnit(child))
						member.key = ReduceBodyItem(child);
					self(self, child, member.subItems);
					members.push_back(std::move(member));
				}
			};
			collectSubItems(collectSubItems, resultChild.get(), structured->subItems);
			r.structuredResult = std::move(structured);
		}
		return r;
	}

	LispRef FunctionApplication::ReduceBodyItem(const TreeItem* bodyItem)
	{
		auto memo = m_Reductions.find(bodyItem);
		if (memo != m_Reductions.end())
			return memo->second;

		if (!m_InProgress.insert(bodyItem).second)
			throwErrorF("ExprParser", "'{}': circular reference in function body"
				, bodyItem->GetFullName().c_str());

		LispRef result;
		SharedStr exprStr = bodyItem->GetExpr();
		if (exprStr.empty())
		{
			if (IsUnit(bodyItem))
				result = bodyItem->GetCheckedKeyExpr(); // local base unit: nominal identity, shared by all applications
			else
				throwErrorF("ExprParser", "'{}': local item without calculation rule cannot be used in an inlined function application"
					, bodyItem->GetFullName().c_str());
		}
		else
		{
			if (AbstrCalculator::MustEvaluate(exprStr.c_str()))
				throwErrorF("ExprParser", "'{}': leading-'=' string indirection is not supported inside function bodies"
					, bodyItem->GetFullName().c_str());
			auto bodyCalc = AbstrCalculator::ConstructFromStr(bodyItem, exprStr, CalcRole::Calculator);
			auto refScope = bodyItem->GetTreeParent(); // names resolve from the referencing item's own scope outward, as in instantiated form
			MG_CHECK(refScope);
			result = SubstituteBodyExpr(refScope.get(), RewriteExpr(bodyCalc->GetLispExprOrg()));
		}

		m_InProgress.erase(bodyItem);
		m_Reductions[bodyItem] = result;
		return result;
	}

	LispRef FunctionApplication::SubstituteBodyExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return expr;

		if (expr.IsRealList())
		{
			MG_CHECK(expr.Left().IsSymb());
			LispRef head = expr.Left();
			TokenID headID = head.GetSymbID();

			if (headID == token::sourceDescr)
				return ResolveBodySymbol(refScope, expr.Right().Left().GetSymbID(), nullptr);

			if (headID == token::arrow || headID == token::scope || headID == token::subitem)
				throwErrorF("ExprParser", "the '{}' construct is not yet supported inside inlined function bodies"
					"; bind the function application to a container to use the instantiating form"
					, headID.GetStrLock().c_str());

			// §5.10 applied call result in a data position: must reduce all the way to data
			if (headID == t_ApplyValue)
			{
				CallArg r = ResolveBodyArg(refScope, expr);
				if (r.binding)
					throwErrorF("ExprParser", "a function value can only be applied with '(...)', passed as an argument, or returned as a result");
				return r.key;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			assert(og);
			// arity-aware head dispatch: an argument count no operator member accepts may
			// be served by a same-named function (prelude folds, log(x,base), ...).
			// The count is the EFFECTIVE arity: a trailing '...x' rest symbol expands to
			// its captured argument count -- this is what lets a fold body's recursive
			// call resolve to the binary OPERATOR on the last step (rest = 1 element)
			// and back to the function while more remain
			bool arityFallback = false;
			if (!og->IsTemplateCall())
			{
				UInt32 nrCallArgs = 0;
				for (LispPtr argPtr = expr.Right(); argPtr.IsRealList(); argPtr = argPtr.Right())
				{
					LispPtr a = argPtr.Left();
					if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()) && argPtr.Right().EndP())
					{
						nrCallArgs += m_ArgKeys.size() - (TreeItem_GetFunctionParamCount(m_FuncItem) - 1);
						break;
					}
					++nrCallArgs;
				}
				arityFallback = !og->AcceptsArity(nrCallArgs);
			}
			if (og->IsTemplateCall() || arityFallback)
			{
				// a function application in a data (body-expression) position: resolve the
				// head to a function value (a function-valued parameter's binding, or a
				// plain import), fill its holes with the call arguments, and reduce; a
				// residual (partially applied) result cannot stand in a data position.
				std::shared_ptr<FunctionBinding> paramBinding;
				auto headFn = ResolveBodyHeadFunction(refScope, headID, &paramBinding, /*mayFail*/ arityFallback);
				if (headFn)
				{
					std::vector<CallArg> holeFills;
					for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
					{
						LispPtr a = argPtr.Left();
						if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()))
						{
							if (!argPtr.Right().EndP())
								throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
									, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str());
							SpliceRestArgs(holeFills); // '...x' passed on: splice the captured tail
							break;
						}
						holeFills.push_back(ResolveBodyArg(refScope, a));
					}

					// §5.7: a variant set called from a body dispatches by argument type, exactly
					// like the direct-call site (also reached by '...x' recursive fold steps)
					if (!paramBinding && TreeItem_IsFunctionVariantSet(headFn.get()))
					{
						auto variant = ResolveVariant(headFn.get(), holeFills, m_ErrorHolder);
						headFn = make_shared_tree(variant, existing_obj{});
						if (m_SubstBuff) registerSupplier(*m_SubstBuff, variant);
						CheckFunctionDefinition(variant);
					}
					FunctionBinding calleeBinding = paramBinding ? *paramBinding : *MakeAllHoles(headFn);
					if (!paramBinding && !calleeBinding.env && IsNestedInside(headFn.get(), m_FuncItem))
						calleeBinding.env = MakeCurrentEnv(); // #1166: nested callee sees the enclosing parameters

					FunctionBinding merged = MergeBinding(calleeBinding, holeFills);
					if (merged.NrHoles() != 0)
						throwErrorF("ExprParser", "'{}': a partial application can only be passed as an argument, not used as a value"
							, headID.GetStrLock().c_str());
					return ReduceMerged(merged, this, m_SubstBuff, m_ErrorHolder);
				}
				// arity-fallback probe found no function: fall through to the operator path,
				// whose FindOper reports the arity error
			}
			if (!og->MustCacheResult())
			{
				if (!og->AllowsAsFunctionResult())
					throwErrorF("ExprParser", "'{}': meta function call is not supported inside function bodies"
						, headID.GetStrLock().c_str());

				// A result-eligible generating call survives beta reduction so the
				// caller's typed holder can instantiate it.  Tree arguments must remain
				// item references; calculated arguments are reduced normally.  Embedding
				// sites reject a surviving call before placing it in another expression.
				std::vector<LispRef> substArgs;
				arg_index argNr = 0;
				for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right(), ++argNr)
				{
					LispPtr a = argPtr.Left();
					auto argPolicy = og->GetArgPolicy(argNr, nullptr);
					if (argPolicy == oper_arg_policy::calc_never || argPolicy == oper_arg_policy::is_templ)
					{
						CallArg bound = ResolveBodyArg(refScope, a);
						if (!bound.item)
							throwErrorF("ExprParser", "'{}': argument {} of meta function '{}' must reduce to a direct item reference"
								, m_FuncItem->GetFullName().c_str(), argNr + 1, headID.GetStrLock().c_str());
						substArgs.emplace_back(TokenID(bound.item->GetFullName()));
					}
					else
					{
						auto substArg = SubstituteBodyExpr(refScope, a);
						RejectNestedFunctionResultMetaCall(substArg);
						substArgs.push_back(std::move(substArg));
					}
				}

				LispRef argList;
				for (auto ri = substArgs.rbegin(); ri != substArgs.rend(); ++ri)
					argList = LispRef(*ri, argList);
				return LispRef(head, std::move(argList));
			}

			// ordinary operator application: substitute the arguments
			std::vector<LispRef> substArgs;
			for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
			{
				LispPtr a = argPtr.Left();
				if (a.IsSymb() && IsRestParamSymbol(a.GetSymbID()))
				{
					// trailing '...x' into an OPERATOR call: splice the captured argument
					// keys (a fold body's last recursive step lands here: rest = 1 element
					// -> the binary operator)
					if (!argPtr.Right().EndP())
						throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
							, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str());
					for (UInt32 k = TreeItem_GetFunctionParamCount(m_FuncItem) - 1; k != m_ArgKeys.size(); ++k)
					{
						if (m_ArgBindings[k] || m_ArgLiterals[k])
							throwErrorF("ExprParser", "'{}': a function value or container literal in '...{}' cannot be passed to operator '{}'"
								, m_FuncItem->GetFullName().c_str(), a.GetSymbStr().c_str(), headID.GetStrLock().c_str());
						RejectNestedFunctionResultMetaCall(m_ArgKeys[k]);
						substArgs.push_back(m_ArgKeys[k]);
					}
					break;
				}
				auto substArg = SubstituteBodyExpr(refScope, a);
				RejectNestedFunctionResultMetaCall(substArg);
				substArgs.push_back(std::move(substArg));
			}

			LispRef argList;
			for (auto ri = substArgs.rbegin(); ri != substArgs.rend(); ++ri)
				argList = LispRef(*ri, argList);

			LispRef result = RewriteExprTop(LispRef(head, std::move(argList)));

			if (og->CanResultToConfigItem())
			{
				DataControllerRef dc = GetOrCreateDataController(result);
				auto supplier = dc->MakeResult();
				if (!supplier)
				{
					dms_assert(dc->WasFailed(FailType::MetaInfo));
					m_ErrorHolder->ThrowFail(dc.get());
				}
				if (!supplier->IsCacheItem())
					result = supplier->GetCheckedKeyExpr();
			}
			return result;
		}

		if (expr.IsSymb())
		{
			TokenID symbID = expr.GetSymbID();
			if (token::isConst(symbID))
				return ExprList(symbID);
			if (ValueClass::FindByScriptName(symbID))
				return List(LispRef(expr)); // unitName -> [UnitName []], i.e. unitName()
			return ResolveBodySymbol(refScope, symbID, nullptr);
		}

		return expr; // numeric, string and UInt64 literals
	}

	// §5.10: look a name up in the captured closure environment(s): the enclosing
	// applications' parameters, nearest enclosure first. Returns true when bound.
	bool FunctionApplication::ResolveEnvSymbol(TokenID symbID, SharedTreeItem* foundItemPtr, LispRef* keyPtr, std::shared_ptr<FunctionBinding>* bindingPtr)
	{
		for (auto env = m_Env; env; env = env->next)
		{
			UInt32 i = 0;
			for (const TreeItem* c = env->funcItem->_GetFirstSubItem(); c && i < env->args.size(); c = c->GetNextItem(), ++i)
				if (c->GetID() == symbID)
				{
					const CallArg& a = env->args[i];
					if (foundItemPtr) *foundItemPtr = a.item;
					if (keyPtr)       *keyPtr = a.key;
					if (bindingPtr)   *bindingPtr = a.binding;
					return true;
				}
		}
		return false;
	}

	LispRef FunctionApplication::ResolveBodySymbol(const TreeItem* refScope, TokenID symbID, SharedTreeItem* foundItemPtr)
	{
		SharedStr fullStr(symbID.AsStrRangeLock());
		CharPtr b = fullStr.begin(), e = fullStr.send();

		if (b != e && (*b == '.' || *b == '/'))
			throwErrorF("ExprParser", "'{}': dot-relative and absolute references are not supported inside function bodies"
				, fullStr.c_str());

		CharPtr slash = std::find(b, e, '/');
		TokenID firstTok = (slash == e) ? symbID : GetTokenID_mt(b, slash);

		// nearest-scope resolution: from the referencing item's scope outward, up to and
		// including the function item -- matching the resolution order of the instantiated form
		for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
		{
			bool atFuncRoot = (scope == m_FuncItem);
			auto child = scope->GetConstSubTreeItemByID(firstTok);
			if (child)
			{
				// parameter?
				for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
					if (m_Params[i] == child.get())
					{
						if (child.get() == m_RestParam)
							throwErrorF("ExprParser", "'{}': parameter '{}' is a '...' rest parameter; it can only be passed on as the trailing argument of a function call"
								, m_FuncItem->GetFullName().c_str(), child->GetID().GetStrLock().c_str());
						// §5.9 parameter bound to a container literal: reduce a bare use to the
						// domain and 'param/member' to the named member value -- no arg item exists
						if (m_ArgLiterals[i])
						{
							const auto& lit = *m_ArgLiterals[i];
							if (foundItemPtr)
								*foundItemPtr = nullptr;
							if (slash == e)
							{
								if (!lit.hasDomain)
									throwErrorF("ExprParser", "'{}': the container literal bound to parameter '{}' has no domain unit and cannot be used as a unit"
										, fullStr.c_str(), firstTok.GetStrLock().c_str());
								return lit.domainKey;
							}
							TokenID memberName = GetTokenID_mt(slash + 1, e);
							for (const auto& mv : lit.members)
								if (mv.first == memberName)
									return mv.second;
							throwErrorF("ExprParser", "'{}': the container literal bound to parameter '{}' has no member '{}'"
								, fullStr.c_str(), firstTok.GetStrLock().c_str(), SharedStr(CharPtrRange(slash + 1, e)).c_str());
						}

						bool boundToFunction = (m_ArgBindings[i] != nullptr);
						if (slash == e)
						{
							if (boundToFunction)
								throwErrorF("ExprParser", "'{}': a function-valued parameter can only be applied or passed on as an argument"
									, fullStr.c_str());
							if (foundItemPtr)
								*foundItemPtr = m_ArgItems[i];
							return m_ArgKeys[i];
						}
						// member access through a (structured or composite-typed) parameter:
						// descend into the actual argument only (never its ancestors)
						auto argItem = m_ArgItems[i];
						if (!argItem)
							throwErrorF("ExprParser", "'{}': member access through parameter '{}' requires the corresponding argument to be a direct item reference"
								, fullStr.c_str(), firstTok.GetStrLock().c_str());
						if (boundToFunction)
							throwErrorF("ExprParser", "'{}': member access through a function-valued parameter is not supported"
								, fullStr.c_str());
						auto member = FindSubItem(argItem.get(), SharedStr(CharPtrRange(slash + 1, e)));
						if (!member)
							throwErrorF("ExprParser", "'{}': the argument '{}' bound to parameter '{}' has no member '{}'"
								, fullStr.c_str(), argItem->GetFullName().c_str(), firstTok.GetStrLock().c_str()
								, SharedStr(CharPtrRange(slash + 1, e)).c_str());
						member->UpdateMetaInfo();
						if (m_SubstBuff)
							registerSupplier(*m_SubstBuff, member.get());
						if (foundItemPtr)
							*foundItemPtr = member;
						return member->GetCheckedKeyExpr();
					}

				// local body item (possibly a nested path into it)
				SharedTreeItem target = child;
				if (slash != e)
				{
					// §12.7 slSubItemCall tranche: descend the DECLARED structure
					// segment-wise; a miss below an item WITH a calculation rule
					// resolves INTO its computed result -- slSubItemCall(reducedKey,
					// rest), the cache layer's canonical sub-item form (u/Values on
					// u := unique(x) & co.). SubItemOperator reports a missing
					// member per application; meta rules keep their own rejection
					// (ReduceBodyItem throws it). The deepest rule-bearing item on
					// the walked path wins (its members are keyed from itself);
					// rule-less misses keep the FindSubItem report exactly as before.
					CharPtr segBegin = slash + 1;
					std::vector<std::pair<SharedTreeItem, CharPtr>> descended;
					descended.emplace_back(target, segBegin);
					while (segBegin != e)
					{
						CharPtr segEnd = std::find(segBegin, e, DELIMITER_CHAR);
						auto sub = target->GetConstSubTreeItemByID(GetTokenID_mt(segBegin, segEnd));
						if (!sub)
						{
							for (auto ri = descended.rbegin(); ri != descended.rend(); ++ri)
								if (!ri->first->GetExpr().empty())
								{
									LispRef baseKey = ReduceBodyItem(ri->first.get());
									// A CONFIG-item reference (a sourceDescr key: the
									// local's rule was a bare import/def-scope/param alias
									// to a config item) is NOT a cache result, so the
									// cache-layer SubItemOperator cannot take it as a base.
									// But the member is directly resolvable: descend the
									// remaining path against the referenced config item and
									// emit that item's own key -- exactly as member access
									// through a structured parameter does (see above) --
									// instead of routing through the cache layer.
									if (baseKey.IsRealList() && baseKey.Left().IsSymb()
										&& baseKey.Left().GetSymbID() == token::sourceDescr)
									{
										LispPtr fullNameRef = baseKey.Right().Left();
										if (fullNameRef.IsSymb())
										{
											// materialize the name first: a TokenStr range holds the
											// token-registry lock, which ResolveItemPath (parse-capable) must not span
											SharedStr baseName(fullNameRef.GetSymbID().AsStrRangeLock());
											if (auto baseItem = m_FuncItem->ResolveItemPath(baseName))
											{
												// FindSubItem throws a clean FindSubItem error on a genuinely missing member
												auto member = FindSubItem(baseItem.get(), SharedStr(CharPtrRange(ri->second, e)));
												member->UpdateMetaInfo();
												if (m_SubstBuff)
													registerSupplier(*m_SubstBuff, member.get());
												if (foundItemPtr)
													*foundItemPtr = member;
												return member->GetCheckedKeyExpr();
											}
										}
										break; // config base could not be resolved -> keep the pre-tranche throw
									}
									if (foundItemPtr)
										*foundItemPtr = nullptr;
									return slSubItemCall(std::move(baseKey), CharPtrRange(ri->second, e));
								}
							throwErrorF("FindSubItem", "Cannot find {} from {}"
								, SharedStr(CharPtrRange(segBegin, segEnd)), target->GetFullName().c_str());
						}
						target = sub;
						segBegin = (segEnd == e) ? e : segEnd + 1;
						descended.emplace_back(target, segBegin);
					}
				}
				if (foundItemPtr)
					*foundItemPtr = nullptr; // reduced local: no item identity to bind member access to
				return ReduceBodyItem(target.get());
			}
			if (atFuncRoot)
				break;
		}

		// §5.10: the captured closure environment -- the enclosing applications' bound
		// parameters -- is lexically nearer than any import or definition-scope item
		if (m_Env)
		{
			SharedTreeItem envItem; LispRef envKey; std::shared_ptr<FunctionBinding> envBnd;
			if (ResolveEnvSymbol(firstTok, &envItem, &envKey, &envBnd))
			{
				if (envBnd)
					throwErrorF("ExprParser", "'{}': a captured function value can only be applied or passed on as an argument"
						, fullStr.c_str());
				if (slash == e)
				{
					if (foundItemPtr)
						*foundItemPtr = envItem;
					return envKey;
				}
				// member access through a captured structured value: descend into the
				// argument item, as for a directly bound parameter
				if (!envItem)
					throwErrorF("ExprParser", "'{}': member access through captured '{}' requires the corresponding argument to be a direct item reference"
						, fullStr.c_str(), firstTok.GetStrLock().c_str());
				auto member = FindSubItem(envItem.get(), SharedStr(CharPtrRange(slash + 1, e)));
				if (!member)
					throwErrorF("ExprParser", "'{}': the argument captured as '{}' has no member '{}'"
						, fullStr.c_str(), firstTok.GetStrLock().c_str(), SharedStr(CharPtrRange(slash + 1, e)).c_str());
				member->UpdateMetaInfo();
				if (m_SubstBuff)
					registerSupplier(*m_SubstBuff, member.get());
				if (foundItemPtr)
					*foundItemPtr = member;
				return member->GetCheckedKeyExpr();
			}
		}

		// imports and externals: own scope + explicit imports first, then the lexical
		// definition scope (§4.6 revision 2026-07-13: identifiers resolve to what is
		// visible from the point of definition; the call site stays invisible).
		// The definition scope is the WHOLE enclosing chain, not just the immediate
		// parent: a function nested in another function's body must still see the
		// container scope. FindTreeItemByID stops ascending at the first item with a
		// using-cache, so one GetTreeParent() step only suffices while the function
		// sits directly in a container; from '/outer/inner' it lands on '/outer' and
		// the container was never consulted (ObjectVision/GeoDMS#1166).
		auto found = m_FuncItem->ResolveItemPath(fullStr);
		for (auto defScope = m_FuncItem->GetTreeParent(); !found && defScope; defScope = defScope->GetTreeParent())
			found = defScope->ResolveItemPath(fullStr);
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, 'using' imports, and the definition scope)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		if (found->IsFunctionItem())
		{
			if (foundItemPtr)
			{
				if (m_SubstBuff)
					registerSupplier(*m_SubstBuff, found.get());
				*foundItemPtr = found; // function passed on as an argument: binding only, no key expression
				return {};
			}
			throwErrorF("ExprParser", "'{}': a function can only be applied or passed on as an argument"
				, fullStr.c_str());
		}
		if (found->InTemplate())
		{
			// #1166: a nested function's body may reference the ENCLOSING function's
			// LOCAL items. Those have no value of their own here -- they must be reduced
			// in the enclosing application, which holds its bindings. Its parameters are
			// captured by value in the environment (resolved above); a local is reduced
			// in place by delegating to that application on the parent chain. When the
			// nested function was returned as a closure and applied elsewhere, no parent
			// on the chain owns 'found' and the reference is rejected as before.
			for (const FunctionApplication* p = m_Parent; p; p = p->m_Parent)
				if (IsNestedInside(found.get(), p->m_FuncItem))
				{
					// non-const: the parent is a live stack application, and this is the
					// very reduction it would perform for the item itself (its memo and
					// in-progress set keep circular-reference detection intact)
					auto* encl = const_cast<FunctionApplication*>(p);
					if (foundItemPtr)
						*foundItemPtr = nullptr; // reduced in the enclosing scope: no item identity to bind member access to
					return encl->ReduceBodyItem(found.get());
				}
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		}
		found->UpdateMetaInfo();
		if (m_SubstBuff)
			registerSupplier(*m_SubstBuff, found.get());
		if (foundItemPtr)
			*foundItemPtr = found;
		return found->GetCheckedKeyExpr();
	}

	// resolve a body-call head to the function being applied; sets *paramBinding when the
	// head is a function-valued parameter (so its pre-bound slots participate).
	// mayFail: arity-fallback probe -- return null instead of throwing when no function
	// is found (the caller then falls through to the operator path).
	SharedTreeItem FunctionApplication::ResolveBodyHeadFunction(const TreeItem* /*refScope*/, TokenID headID, std::shared_ptr<FunctionBinding>* paramBinding, bool mayFail)
	{
		if (paramBinding) *paramBinding = nullptr;

		if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(headID))
			for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
				if (m_Params[i] == headChild.get())
				{
					if (!m_ArgBindings[i])
						throwErrorF("ExprParser", "'{}': parameter is applied as a function but the corresponding argument is not a function reference"
							, headID.GetStrLock().c_str());
					if (paramBinding) *paramBinding = m_ArgBindings[i];
					return m_ArgBindings[i]->funcItem;
				}

		// §5.10: a captured function value from the closure environment
		if (m_Env)
		{
			SharedTreeItem envItem; LispRef envKey; std::shared_ptr<FunctionBinding> envBnd;
			if (ResolveEnvSymbol(headID, &envItem, &envKey, &envBnd))
			{
				if (!envBnd)
					throwErrorF("ExprParser", "'{}': captured value is applied as a function but is not a function reference"
						, headID.GetStrLock().c_str());
				if (paramBinding) *paramBinding = envBnd;
				return envBnd->funcItem;
			}
		}

		auto callee = m_FuncItem->ResolveItemPath(SharedStr(headID.AsStrRangeLock()));
		if (!callee || !callee->IsFunctionItem())
			if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
				if (auto lex = defParent->ResolveItemPath(SharedStr(headID.AsStrRangeLock())); lex && lex->IsFunctionItem())
					callee = lex;
		if (!callee || !callee->IsFunctionItem())
			// the auto-imported prelude is the implicit outermost namespace for call heads
			if (auto pf = FindPreludeFunction(headID); pf && pf->IsFunctionItem())
				callee = pf;
		if (!callee || !callee->IsFunctionItem())
		{
			if (mayFail) // arity-aware head dispatch probe: no function -> operator path reports
				return {};
			if (!callee)
				throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
					, headID.GetStrLock().c_str(), m_FuncItem->GetFullName().c_str());
			throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
				, headID.GetStrLock().c_str());
		}
		if (m_SubstBuff)
			registerSupplier(*m_SubstBuff, callee.get());
		return callee;
	}

	// resolve one argument of a body-level function application to a CallArg (which may be
	// a data key, a function value / partial binding, or a hole).
	CallArg FunctionApplication::ResolveBodyArg(const TreeItem* refScope, LispPtr argExpr)
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
				SharedStr s(sym.AsStrRangeLock());
				bool bare = std::find(s.begin(), s.send(), '/') == s.send();
				if (bare)
				{
					// function-valued parameter?
					if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(sym))
						for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
							if (m_Params[i] == headChild.get() && m_ArgBindings[i])
							{
								CallArg a; a.binding = m_ArgBindings[i]; return a;
							}
					// §5.10: a captured value from the closure environment
					if (m_Env)
					{
						CallArg a;
						if (ResolveEnvSymbol(sym, &a.item, &a.key, &a.binding))
							return a;
					}
					// import or lexically visible function?
					auto callee = m_FuncItem->ResolveItemPath(s);
					if (!callee || !callee->IsFunctionItem())
						if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
							if (auto lex = defParent->ResolveItemPath(s); lex && lex->IsFunctionItem())
								callee = lex;
					if (!callee)
						if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
							callee = pf; // prelude: implicit outermost namespace, also for function references
					if (callee && callee->IsFunctionItem())
					{
						if (m_SubstBuff) registerSupplier(*m_SubstBuff, callee.get());
						CallArg a; a.binding = MakeAllHoles(callee); return a;
					}
				}
				CallArg a; a.key = ResolveBodySymbol(refScope, sym, &a.item);
				RejectNestedFunctionResultMetaCall(a.key);
				return a;
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb())
		{
			TokenID headID = argExpr.Left().GetSymbID();

			// §5.9 container literal passed to a nested call: resolve in body scope ('.' -> domain)
			if (headID == t_ContainerLiteral)
			{
				CallArg a; a.literal = BuildContainerLiteral(argExpr,
					[&](LispPtr e) { return SubstituteBodyExpr(refScope, e); });
				return a;
			}

			// §5.10 applied call result: reduce the inner expression to a function value,
			// bind the outer arguments; the result may again be a value or a binding
			if (headID == t_ApplyValue)
			{
				CallArg fnVal = ResolveBodyArg(refScope, argExpr.Right().Left());
				if (!fnVal.binding)
					throwErrorF("ExprParser", "'(...)' applied to an expression that is not a function value");
				std::vector<CallArg> outer;
				for (LispPtr a = argExpr.Right().Right(); !a.EndP(); a = a.Right())
					outer.push_back(ResolveBodyArg(refScope, a.Left()));
				FunctionBinding merged = MergeBinding(*fnVal.binding, outer);
				if (merged.NrHoles() == 0)
				{
					auto result = ReduceMergedValue(merged, this, m_SubstBuff, m_ErrorHolder);
					RejectNestedFunctionResultMetaCall(result.key);
					return result;
				}
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}

			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall())
			{
				std::shared_ptr<FunctionBinding> pb;
				auto headFn = ResolveBodyHeadFunction(refScope, headID, &pb);
				std::vector<CallArg> sub;
				for (LispPtr a = argExpr.Right(); !a.EndP(); a = a.Right())
				{
					LispPtr ae = a.Left();
					if (ae.IsSymb() && IsRestParamSymbol(ae.GetSymbID()))
					{
						if (!a.Right().EndP())
							throwErrorF("ExprParser", "'{}': rest parameter '{}' must be the trailing argument of the call"
								, m_FuncItem->GetFullName().c_str(), ae.GetSymbStr().c_str());
						SpliceRestArgs(sub); // '...x' passed on: splice the captured tail
						break;
					}
					sub.push_back(ResolveBodyArg(refScope, ae));
				}
				// §5.7: variant sets dispatch by argument type on nested calls too
				if (!pb && TreeItem_IsFunctionVariantSet(headFn.get()))
				{
					auto variant = ResolveVariant(headFn.get(), sub, m_ErrorHolder);
					headFn = make_shared_tree(variant, existing_obj{});
					if (m_SubstBuff) registerSupplier(*m_SubstBuff, variant);
					CheckFunctionDefinition(variant);
				}
				FunctionBinding calleeBinding = pb ? *pb : *MakeAllHoles(headFn);
				if (!pb && !calleeBinding.env && IsNestedInside(headFn.get(), m_FuncItem))
					calleeBinding.env = MakeCurrentEnv(); // #1166: nested callee sees the enclosing parameters
				FunctionBinding merged = MergeBinding(calleeBinding, sub);
				if (merged.NrHoles() == 0)
				{
					auto result = ReduceMergedValue(merged, this, m_SubstBuff, m_ErrorHolder); // §5.10: data key OR closure binding
					RejectNestedFunctionResultMetaCall(result.key);
					return result;
				}
				CallArg a; a.binding = std::make_shared<FunctionBinding>(std::move(merged)); return a;
			}
		}
		CallArg a; a.key = SubstituteBodyExpr(refScope, argExpr);
		RejectNestedFunctionResultMetaCall(a.key);
		return a;
	}


} // namespace hof
