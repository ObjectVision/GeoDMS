// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Type variables of a function application: the unification store's non-inline parts,
// and the contracts a bound argument must satisfy -- a declared 'sig<...>' signature and
// the member block of a structured or by-example parameter.

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
	ValueClassSet GenericConstraintSet(TokenID constraintName)
	{
		ValueClassSet r;
		for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count); ++v)
			if (auto vc = ValueClass::FindByValueClassID(ValueClassID(v)))
				if (MatchesGenericConstraint(vc, constraintName))
					r.set(v);
		return r;
	}

	// the declared constraint of `var`: fn's ordered type-variable list, else the
	// generic-parameter records (which carry the same `<var: constraint>` pairs)
	TokenID DeclaredConstraintOf(const TreeItem* fn, TokenID var)
	{
		if (!fn)
			return TokenID(); // operator-signature variables have no owning function
		if (auto tvs = TreeItem_GetFunctionTypeVars(fn))
			for (const auto& tv : *tvs)
				if (tv.first == var)
					return tv.second;
		UInt32 seqNr = 0, idx; TokenID gv, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(fn, seqNr++, &idx, &gv, &cons, &isDom))
			if (gv == var)
				return cons;
		return TokenID();
	}

	// WP4.1: enforce one 'sig<...>'-typed binding -- the bound function's positions
	// instantiate or LINK the target variables named by the type application: a
	// concrete position BINDS the mapped variable; a position naming the bound
	// function's OWN generic variable LINKS that variable (under its own fresh
	// `boundInstance`) to the mapped one. The target nodes come from callbacks so the
	// same pass serves application-time checking (targets = the applied function's
	// variables, instance 0) and the definition-time walker (targets resolved per
	// type-application argument; NO_TYPE_VAR skips a position).
	void LinkSignatureBinding(TypeUnifier& u, const TreeItem* sig, const TreeItem* boundFn,
		const std::vector<std::pair<TokenID, TokenID>>* sigVars, const std::vector<TokenID>* typeArgs,
		const std::function<SizeT(TokenID)>& targetValueNode,
		const std::function<SizeT(TokenID)>& targetUnitNode,
		UInt32 boundInstance, const SharedStr& bindSource)
	{
		std::map<TokenID, TokenID> sig2target;
		for (SizeT k = 0; k != sigVars->size(); ++k)
			sig2target[(*sigVars)[k].first] = (*typeArgs)[k];

		auto constrainPos = [&](const TreeItem* sigPos, const TreeItem* fnPos)
		{
			if (!sigPos || !fnPos || !IsDataItem(sigPos) || !IsDataItem(fnPos))
				return;

			auto itV = sig2target.find(AsDataItem(sigPos)->ValuesUnitToken());
			if (itV != sig2target.end())
				if (SizeT target = targetValueNode(itV->second); target != NO_TYPE_VAR)
				{
					TokenID fnVU = AsDataItem(fnPos)->ValuesUnitToken();
					if (fnVU && IsGenericVarOf(boundFn, fnVU))
						u.LinkValue(target, u.ValueVar(boundFn, boundInstance, fnVU, bindSource), bindSource);
					else if (auto vc = ParamValueClass(fnPos))
						u.BindValue(target, vc, bindSource);
				}

			auto itD = sig2target.find(AsDataItem(sigPos)->DomainUnitToken());
			if (itD != sig2target.end())
				if (SizeT target = targetUnitNode(itD->second); target != NO_TYPE_VAR)
				{
					TokenID fnDU = AsDataItem(fnPos)->DomainUnitToken();
					if (fnDU && IsGenericVarOf(boundFn, fnDU))
						u.LinkUnit(target, u.UnitVar(boundFn, boundInstance, fnDU), bindSource);
					else if (fnDU)
						if (auto defP = boundFn->GetTreeParent())
						{
							SharedStr fnDUName(fnDU.AsStrRangeLock()); // materialized: a TokenStr must not span ResolveItemPath (parse-capable, token-registry lock)
							if (auto unitItem = defP->ResolveItemPath(fnDUName); unitItem && IsUnit(unitItem.get()))
								u.BindUnit(target, unitItem, AsUnit(unitItem.get())
									, mySSPrintF("by {}", bindSource.c_str()));
						}
				}
		};

		const TreeItem* sp = sig->_GetFirstSubItem();
		const TreeItem* fp = boundFn->_GetFirstSubItem();
		for (UInt32 k = 0, n = TreeItem_GetFunctionParamCount(sig); k != n && sp && fp; ++k, sp = sp->GetNextItem(), fp = fp->GetNextItem())
			constrainPos(sp, fp);
		constrainPos(
			sig->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sig)).get(),
			boundFn->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(boundFn)).get());
	}

	void CheckFunctionSignature(const TreeItem* boundFn, const TreeItem* sigExemplar, CharPtr paramName)
	{
		UInt32 nrSigParams = TreeItem_GetFunctionParamCount(sigExemplar);
		UInt32 nrFnParams = TreeItem_GetFunctionParamCount(boundFn);
		if (nrSigParams != nrFnParams)
			throwErrorF("ExprParser", "function '{}' bound to parameter '{}' has {} parameter(s); its declared signature '{}' requires {}"
				, boundFn->GetFullName().c_str(), paramName
				, nrFnParams, sigExemplar->GetFullName().c_str(), nrSigParams);

		// a domain/values reference that names a parameter must name the positionally
		// same parameter on both sides (alpha-invariant); other references must match
		// literally
		auto normalizeUnitRef = [](const TreeItem* fn, TokenID unitRef) -> SharedStr
		{
			if (unitRef)
			{
				const TreeItem* param = fn->_GetFirstSubItem();
				for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fn); j != n && param; ++j, param = param->GetNextItem())
					if (param->GetNameID() == unitRef)
						return mySSPrintF("#{}", j);
			}
			return SharedStr(unitRef);
		};

		const TreeItem* sigParam = sigExemplar->_GetFirstSubItem();
		const TreeItem* fnParam = boundFn->_GetFirstSubItem();
		for (UInt32 i = 0; i != nrSigParams; ++i, sigParam = sigParam->GetNextItem(), fnParam = fnParam->GetNextItem())
		{
			MG_CHECK(sigParam && fnParam);
			auto sigCls = sigParam->GetDynamicClass();
			if (sigCls == TreeItem::GetStaticClass())
				continue; // wildcard position
			if (fnParam->GetDynamicClass() != sigCls)
				throwErrorF("ExprParser", "function '{}' bound to parameter '{}': parameter {} is a {} but its declared signature '{}' requires a {}"
					, boundFn->GetFullName().c_str(), paramName, i + 1
					, fnParam->GetDynamicClass()->GetNameID()
					, sigExemplar->GetFullName().c_str()
					, sigCls->GetNameID());
			if (IsDataItem(sigParam))
			{
				auto sigADI = AsDataItem(sigParam);
				auto fnADI = AsDataItem(fnParam);
				if (sigADI->GetValueComposition() != fnADI->GetValueComposition())
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': parameter {} differs in value composition from its declared signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
				// a signature-side reference naming one of the signature's generic
				// variables is a wildcard position in v1 (kind-level checking, §5.10)
				if (!IsGenericVarOf(sigExemplar, sigADI->DomainUnitToken())
					&& normalizeUnitRef(sigExemplar, sigADI->DomainUnitToken()) != normalizeUnitRef(boundFn, fnADI->DomainUnitToken()))
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': the domain of parameter {} does not match the domain relationship required by signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
				if (!IsGenericVarOf(sigExemplar, sigADI->ValuesUnitToken())
					&& normalizeUnitRef(sigExemplar, sigADI->ValuesUnitToken()) != normalizeUnitRef(boundFn, fnADI->ValuesUnitToken()))
					throwErrorF("ExprParser", "function '{}' bound to parameter '{}': the values unit of parameter {} does not match signature '{}'"
						, boundFn->GetFullName().c_str(), paramName, i + 1
						, sigExemplar->GetFullName().c_str());
			}
		}

		auto sigResult = sigExemplar->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(sigExemplar));
		auto fnResult = boundFn->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(boundFn));
		if (sigResult && fnResult && sigResult->GetDynamicClass() != TreeItem::GetStaticClass()
			&& fnResult->GetDynamicClass() != sigResult->GetDynamicClass())
			throwErrorF("ExprParser", "function '{}' bound to parameter '{}': its result is a {} but the declared signature '{}' requires a {}"
				, boundFn->GetFullName().c_str(), paramName
				, fnResult->GetDynamicClass()->GetNameID()
				, sigExemplar->GetFullName().c_str()
				, sigResult->GetDynamicClass()->GetNameID());
	}

	// K11a-3: instantiation-point contract check for a structured / by-example unit
	// parameter (op-sig scope doc §3(c)). The declared member block is a CLOSED
	// interface, so each declared member must be PRESENT on the actual argument,
	// KIND-compatible, CLASS-compatible (incl. generic-constraint satisfaction and
	// cross-member consistency of a shared type variable), and -- for the relations
	// checkable at the boundary -- co-domained: a member declared over a SIBLING
	// member unit must relate to the argument's own that-unit, and a default-domain
	// member must be an attribute of the argument unit itself. Violations are
	// reported AT THE APPLICATION, naming the parameter and the member, instead of
	// transitively inside the reduced body. Deferred to the body's own reduction:
	// members over telescope parameters or generic DOMAIN variables, deep member
	// paths, members whose declared values token resolves outside the block, and
	// anything whose actual units are not resolvable here (null guards defer, never
	// misreport). memberSrc is the parameter (explicit block) or its by-example
	// exemplar; generic-variable handling applies to explicit blocks only (exemplar
	// tokens are the exemplar's lexical world).
	void CheckStructuredParamContract(const TreeItem* funcItem, const TreeItem* paramItem,
		const TreeItem* memberSrc, const TreeItem* argRoot)
	{
		assert(IsMetaThread()); // reduction runs on the meta thread; UnifyDomain below relies on it
		// K11a-4: a UNIT parameter requires a unit argument (whose identity the
		// default-domain members check against); a CONTAINER parameter accepts any
		// item carrying the members. A kind-mismatched argument fails the ordinary
		// binding diagnostics.
		bool unitParam = IsUnit(paramItem);
		if (unitParam && !IsUnit(argRoot))
			return;
		bool byExample = memberSrc != paramItem;
		SharedStr fnName(funcItem->GetFullName());
		SharedStr pName(paramItem->GetNameID().AsStrRangeLock());
		constexpr UnifyMode um = UnifyMode(UM_AllowVoidRight | UM_AllowRightExpansion);

		// shared type variables: the first member's actual class binds; later members must agree
		std::map<TokenID, std::pair<const ValueClass*, SharedStr>> varBindings;

		// K11a-4: recurse into declared CONTAINER members (presence + the same
		// per-member checks under the nested block; nested blocks have no enclosing
		// unit, so default-domain membership is not claimed there)
		std::function<void(const TreeItem*, const TreeItem*, bool, const SharedStr&)> walkBlock;
		walkBlock = [&](const TreeItem* srcBlock, const TreeItem* argBlock, bool blockIsParamUnit, const SharedStr& prefix)
		{
		for (const TreeItem* m = srcBlock->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			// review finding: nested FUNCTIONS, TEMPLATES and type-alias exemplars are
			// implementation content, never a member contract (a template's internals
			// are exactly what the K11a-3 plain-template exemption already ruled out)
			if (m->IsTemplate() || m->IsFunctionItem())
				continue;
			if (!IsUnit(m) && !IsDataItem(m))
			{
				// declared container member: must be present; recurse for its members.
				// BY-EXAMPLE: the exemplar is a real config item whose sub-containers
				// are INCIDENTAL, not a declared interface -- never require them
				// (review finding: an exemplar's 'container meta { … }' made every
				// alternative argument fail).
				if (byExample || !m->_GetFirstSubItem())
					continue;
				SharedStr cName(prefix + SharedStr(m->GetNameID().AsStrRangeLock()));
				auto c = argBlock->GetConstSubTreeItemByID(m->GetNameID());
				if (!c)
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: member '{}' is missing"
						, fnName.c_str(), pName.c_str(), cName.c_str());
				walkBlock(m, c.get(), false, cName + "/");
				continue;
			}
			SharedStr mName(prefix + SharedStr(m->GetNameID().AsStrRangeLock()));
			auto a = argBlock->GetConstSubTreeItemByID(m->GetNameID());
			if (!a)
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: member '{}' is missing"
					, fnName.c_str(), pName.c_str(), mName.c_str());
			if (IsUnit(m))
			{
				if (!IsUnit(a.get()))
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be a unit"
						, fnName.c_str(), pName.c_str(), mName.c_str());
				auto wantCls = AsUnit(m)->GetValueType();
				auto gotCls = AsUnit(a.get())->GetValueType();
				if (wantCls && gotCls && wantCls != gotCls)
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: unit '{}' must be a unit<{}>, not a unit<{}>"
						, fnName.c_str(), pName.c_str(), mName.c_str()
						, wantCls->GetNameID(), gotCls->GetNameID());
				continue;
			}

			// data member
			if (!IsDataItem(a.get()))
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute"
					, fnName.c_str(), pName.c_str(), mName.c_str());
			auto declared = AsDataItem(m);
			auto actual   = AsDataItem(a.get());
			if (declared->GetValueComposition() != actual->GetValueComposition())
				throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' differs in value composition"
					, fnName.c_str(), pName.c_str(), mName.c_str());

			auto avu = actual->GetAbstrValuesUnit();
			if (TokenID vt = declared->ValuesUnitToken())
			{
				// ladder order mirrors BuildParamMembers (review finding: sibling and
				// generic-variable rungs BEFORE the ValueClass vocabulary, so a type
				// variable named like a value class stays the variable here too)
				bool isSibling = false;
				for (const TreeItem* u = srcBlock->_GetFirstSubItem(); u; u = u->GetNextItem())
					if (u->GetNameID() == vt && IsUnit(u))
					{
						isSibling = true;
						break;
					}
				if (isSibling)
				{
					// a sibling MEMBER UNIT: the actual member must relate to the
					// argument's own that-unit (the K2 identity, at the boundary)
					auto aSib = argBlock->GetConstSubTreeItemByID(vt);
					if (avu && aSib && IsUnit(aSib.get())
						&& !avu->UnifyDomain(AsUnit(aSib.get()), "", "", um))
					{
						SharedStr vtName(vt.AsStrRangeLock());
						throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: the values of '{}' must be '{}' (the argument's own member unit)"
							, fnName.c_str(), pName.c_str(), mName.c_str(), vtName.c_str());
					}
				}
				else if (!byExample && (IsOwnDeclaredVar(funcItem, vt) || IsGenericVarOf(funcItem, vt)))
				{
					// generic value variable (own <...> clause OR positional generic --
					// same pair BuildParamMembers tests): satisfy the declared
					// constraint and stay consistent with any earlier member sharing
					// the variable
					auto gotCls = avu ? avu->GetValueType() : nullptr;
					if (gotCls)
					{
						if (TokenID cons = DeclaredConstraintOf(funcItem, vt))
							if (!GenericConstraintSet(cons).test(UInt32(gotCls->GetValueClassID())))
							{
								SharedStr vtName(vt.AsStrRangeLock()), consName(cons.AsStrRangeLock());
								throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' is an attribute<{}>, which does not satisfy '{}: {}'"
									, fnName.c_str(), pName.c_str(), mName.c_str()
									, gotCls->GetNameID(), vtName.c_str(), consName.c_str());
							}
						auto [it, isNew] = varBindings.try_emplace(vt, gotCls, mName);
						if (!isNew && it->second.first != gotCls)
						{
							SharedStr vtName(vt.AsStrRangeLock());
							throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' ({}) and '{}' ({}) must share one value type for '{}'"
								, fnName.c_str(), pName.c_str()
								, it->second.second.c_str(), it->second.first->GetNameID()
								, mName.c_str(), gotCls->GetNameID(), vtName.c_str());
						}
					}
				}
				else if (auto wantCls = ValueClass::FindByScriptName(vt))
				{
					auto gotCls = avu ? avu->GetValueType() : nullptr;
					if (gotCls && gotCls != wantCls)
						throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute<{}>, not an attribute<{}>"
							, fnName.c_str(), pName.c_str(), mName.c_str()
							, wantCls->GetNameID(), gotCls->GetNameID());
				}
				// else: resolves outside the block / telescope / unknown -- defer
			}

			// default-domain members are attributes OF the argument unit (claimed
			// ONLY at a unit parameter's TOP block -- containers/nested blocks have
			// no member-domain default); a VOID actual domain broadcasts (the
			// language's single implicit coercion -- review finding: a parameter<>
			// member must not be rejected); explicit non-default domains (telescope
			// params, generic domain vars, scope units) are checked transitively by
			// the body's reduction
			TokenID dt = declared->DomainUnitToken();
			bool defaultDomain = !dt || dt == t_Dot || dt == srcBlock->GetNameID()
				|| (!byExample && srcBlock == memberSrc && dt == paramItem->GetNameID());
			if (defaultDomain && blockIsParamUnit)
			{
				auto adu = actual->GetAbstrDomainUnit();
				if (adu
					&& adu->GetValueType()->GetValueClassID() != ValueClassID::VT_Void
					&& !adu->UnifyDomain(AsUnit(argRoot), "", "", um))
					throwErrorF("ExprParser", "'{}': the argument for parameter '{}' does not match its declared members: '{}' must be an attribute of the argument unit itself"
						, fnName.c_str(), pName.c_str(), mName.c_str());
			}
		}
		};
		walkBlock(memberSrc, argRoot, unitParam, SharedStr());
	}

	// K11 leftover (2026-07-29): the INSTANTIATE path bypasses ReduceValue's binding
	// loop (it is a CopyTreeContext tree copy), so the K11a-3 contract check never
	// ran there -- a missing member surfaced as a transitive 'Unknown identifier
	// nw/F2' inside the copied body instead of the boundary message. This helper
	// applies the same check from MetaFuncCurry; expression arguments and
	// non-function apply-items (plain templates) defer, as at the inline site.
	//
	// Review finding (reproduced both ways, fixed): arguments must resolve from the
	// TARGET's parent -- the context the copied parameter's ArgCalc calculator will
	// bind in (param.parent.parent = target.parent) -- NOT via ac->FindItem (the
	// calculator's search context). The two coincide for a Calculator-role holder,
	// but when the 'instantiate f(...)' expression sits on a copied TEMPLATE
	// ARGUMENT (ArgCalc role) inside a template whose LOCAL shadows the call-site
	// name, ac resolved the call-site item while the copy binds the template-local
	// one: the checker validated the WRONG item (a false boundary rejection of a
	// previously-working config, and a false pass of a broken one).
	void CheckStructuredParamContracts(const TreeItem* applyItem, LispPtr argList, const TreeItem* target)
	{
		if (!applyItem || !applyItem->IsFunctionItem() || !target)
			return;
		auto bindScope = target->GetTreeParent();
		if (!bindScope)
			return;
		UInt32 nrParams = TreeItem_GetFunctionParamCount(applyItem);
		const TreeItem* param = applyItem->_GetFirstSubItem();
		LispPtr a = argList;
		for (UInt32 i = 0; i != nrParams && param && !a.EndP(); ++i, param = param->GetNextItem(), a = a.Right())
		{
			if (IsDataItem(param) || param->IsFunctionItem())
				continue;
			if (!(IsUnit(param) || !IsDataItem(param))) // unit or container parameters only
				continue;
			const TreeItem* memberSrc = param->_GetFirstSubItem() ? param : nullptr;
			SharedTreeItem exKeep;
			if (!memberSrc)
				if (auto ex = TreeItem_GetFunctionParamTypeExemplar(applyItem, i); ex && ex->_GetFirstSubItem())
				{
					exKeep = ex;
					memberSrc = exKeep.get();
				}
			if (!memberSrc)
				continue;
			LispPtr ae = a.Left();
			if (!ae.IsSymb())
				continue; // expression argument: defers, as at the inline site
			auto argItem = bindScope->ResolveItemPath(SharedStr(ae.GetSymbID().AsStrRangeLock()));
			if (!argItem)
				continue; // an unresolvable argument fails through the ordinary path
			CheckStructuredParamContract(applyItem, param, memberSrc, argItem.get());
		}
	}


} // namespace hof
