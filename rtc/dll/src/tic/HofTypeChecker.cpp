// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Definition-time type checking of a function body (WP3.4 + WP4.1 tranche 3): the
// bottom-up walk that derives a DefType for every body expression, resolves names
// against parameters, generics and enclosing scopes, and reports what cannot hold for
// EVERY instantiation -- once, without any application.

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

#include "HofDefType.h"

namespace hof {

	// WP3.4 + WP4.1 tranche 3: definition-time validation of a function body -- the
	// scope/shape walk (every identifier must resolve; operator/function heads must be
	// known; direct calls have the right arity) now also derives TYPES, bottom-up over
	// the body reachable from the designated result. The function's own type/domain
	// variables (and its unit parameters) are RIGID: the body must be well-typed for
	// EVERY instantiation, so anything that would pin them to a concrete type/unit,
	// force two of them equal, or narrow them below their declared constraints is a
	// definition error -- caught here once, without any application. Each callee
	// instantiates its declared signature under a fresh variable instance. Built-in
	// operators, externals, variant selections, partial applications and
	// member/container accesses stay DEFERRED (type Unknown) and remain checked per
	// application by the reduction -- operator signatures are the next tranche.

	// §12.7: may `item`'s calculation rule COMPUTE sub-items at application --
	// a meta head GENERATING config items (for_each_*), or a cacheable rule
	// whose composite cache result carries members (unique/Values & co., now
	// reachable through slSubItemCall)? Any non-empty rule qualifies since the
	// slSubItemCall tranche: reduction resolves the remaining path against the
	// rule's result, so a declared-tree miss below such an item is never
	// reported as unknown at definition -- it types via the pseudo-expanded or
	// described member set (a code-3 access) or defers to the per-application
	// SubItem check. (A leading-'=' rule qualifies too: InferBodyItem reports
	// its own honest error, matching ReduceBodyItem's.)
	bool RuleMayComputeSubItems(const TreeItem* item)
	{
		return !item->GetExpr().empty();
	}

	// K11a by-example (review finding): an EXEMPLAR is a real config item, so its
	// declared children are the member set only when nothing can ADD to them at
	// instantiation. A storage manager (GDAL & co. generate layer sub-items at
	// UpdateMetaInfo) or a calculation rule (a composite result contributes its
	// members) leaves the set OPEN -- membersComplete must then stay false, or a
	// body reference to a generated member is a false definition-time
	// "declares no member" error whose verdict even depends on whether the
	// exemplar's meta info happened to be updated first. An explicitly written
	// member block is always closed: it declares an interface, not an item.
	bool ExemplarMemberSetIsClosed(const TreeItem* exemplar)
	{
		return !RuleMayComputeSubItems(exemplar) && !exemplar->HasStorageManager();
	}

	// K11b: is `item` a plain CONTAINER -- something that can carry an operator's
	// ArgContainer members? Units and data items have their own positions, function
	// items and templates are inert type/logic carriers, never member bags.
	bool IsPlainContainer(const TreeItem* item)
	{
		return item && !IsUnit(item) && !IsDataItem(item) && !item->IsFunctionItem() && !item->IsTemplate();
	}

	int FunctionChecker::ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local, UInt32* paramIdx,
		SharedTreeItem* externalOut, ExtRefKind* extKindPtr, SharedStr* genSubPathOut)
	{
		if (local) *local = nullptr;
		if (externalOut) *externalOut = nullptr;
		if (extKindPtr) *extKindPtr = ExtRefKind::DefScopeExternal;
		SharedStr fullStr(sym.AsStrRangeLock());
		CharPtr b = fullStr.begin(), e = fullStr.send();
		if (b != e && (*b == '.' || *b == '/'))
			throwErrorF("ExprParser", "'{}': dot-relative and absolute references are not supported inside function bodies"
				, fullStr.c_str());
		CharPtr slash = std::find(b, e, '/');
		TokenID firstTok = (slash == e) ? sym : GetTokenID_mt(b, slash);

		for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
		{
			bool atFuncRoot = (scope == m_FuncItem);
			auto child = scope->GetConstSubTreeItemByID(firstTok);
			if (child)
			{
				for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
					if (m_Params[i] == child.get())
					{
						if (paramIdx) *paramIdx = i;
						if (slash != e)
						{
							if (extKindPtr) *extKindPtr = ExtRefKind::ParamMember; // OPEN: depends on the argument
							// K11a-2: hand the member path to InferExpr, which types it
							// against a structured parameter's member map (else defers)
							if (genSubPathOut) *genSubPathOut = SharedStr(CharPtrRange(slash + 1, e));
							return 2;
						}
						return 0;
					}
				SharedTreeItem cursor = child;
				if (slash != e)
				{
					// segment-wise descend (mirroring FindSubItem, message included) so
					// a miss can be attributed to a GENERATING item on the walked path:
					// when an item's rule may GENERATE sub-items at instantiation
					// (§12.7 for_each tranche), the path remaining FROM that item
					// resolves against its pseudo-expanded member set (code 3) instead
					// of being unknown. The deepest generating item wins (its member
					// paths are keyed from itself); a generating ancestor above a
					// declared child covers names that route through declared items.
					CharPtr segBegin = slash + 1;
					std::vector<std::pair<SharedTreeItem, CharPtr>> descended;
					descended.emplace_back(cursor, segBegin);
					while (segBegin != e)
					{
						CharPtr segEnd = std::find(segBegin, e, DELIMITER_CHAR);
						auto sub = cursor->GetConstSubTreeItemByID(GetTokenID_mt(segBegin, segEnd));
						if (!sub)
						{
							if (genSubPathOut)
								for (auto ri = descended.rbegin(); ri != descended.rend(); ++ri)
									if (RuleMayComputeSubItems(ri->first.get()))
									{
										if (local) *local = ri->first.get();
										*genSubPathOut = SharedStr(CharPtrRange(ri->second, e));
										return 3;
									}
							throwErrorF("FindSubItem", "Cannot find {} from {}"
								, SharedStr(CharPtrRange(segBegin, segEnd)), cursor->GetFullName().c_str());
						}
						cursor = sub;
						segBegin = (segEnd == e) ? e : segEnd + 1;
						descended.emplace_back(cursor, segBegin);
					}
				}
				if (local) *local = cursor.get();
				return 1;
			}
			if (atFuncRoot)
				break;
		}

		auto found = m_FuncItem->ResolveItemPath(fullStr);
		// lexical definition scope (§4.6): the whole enclosing chain -- see the
		// matching walk in ResolveBodySymbol (ObjectVision/GeoDMS#1166). This site
		// types the reference and throws first, so it needs the same ascent.
		for (auto defScope = m_FuncItem->GetTreeParent(); !found && defScope; defScope = defScope->GetTreeParent())
			found = defScope->ResolveItemPath(fullStr);
		if (!found && slash == e)
			if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
			{
				if (extKindPtr) *extKindPtr = ExtRefKind::PreludeFunc; // a function value: not an evaluable data spec
				return 2; // prelude: implicit outermost namespace, also for function references
			}
		if (!found && FindEnclosingFunctionMember(firstTok))
		{
			if (extKindPtr) *extKindPtr = ExtRefKind::ClosureCapture; // OPEN: bound per application
			return 2; // captured through the closure environment; typed per application
		}
		if (!found)
			throwErrorF("ExprParser", "'{}': unknown identifier in body of function '{}' (visible are: parameters, local items, 'using' imports, and the definition scope)"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		if (!found->IsFunctionItem() && found->InTemplate())
		{
			// FindItem ascends the parent chain, so an ENCLOSING function's data/unit
			// parameter or local is 'found' here -- those are §5.10 closure captures,
			// bound through the environment at reduction: defer, don't reject
			if (FindEnclosingFunctionMember(firstTok))
			{
				if (extKindPtr) *extKindPtr = ExtRefKind::ClosureCapture; // OPEN
				return 2;
			}
			throwErrorF("ExprParser", "'{}': reference to (part of) a template or function from body of function '{}'"
				, fullStr.c_str(), m_FuncItem->GetFullName().c_str());
		}
		// §12.7 (review finding): reduction resolves the closure ENVIRONMENT before
		// imports/definition scope (ResolveBodySymbol), so a definition-scope item
		// SHADOWED by an enclosing function's member is bound to the CAPTURE at
		// reduction -- classifying it DefScopeExternal would evaluate the wrong
		// (and formal-dependent) binding. Probe the shadow for the §12.7 caller
		if (extKindPtr && FindEnclosingFunctionMember(firstTok))
		{
			*extKindPtr = ExtRefKind::ClosureCapture; // OPEN: the capture shadows `found`
			return 2;
		}
		if (externalOut) *externalOut = found; // DefScopeExternal: CLOSED by construction (§12.7)
		return 2;
	}

	// §12.7: reduce a body sub-expression that is CLOSED over the formals to its
	// DataController key -- the SAME hash-consed key every application interns
	// (β-substitution is the identity on closed expressions), so a definition-time
	// evaluation reads the value once through the very DC reduction will use.
	// Empty result = open or not buildable: the caller defers. Mirrors the closed
	// subset of FunctionApplication::SubstituteBodyExpr; memoized (the strong
	// LispRefs in the memo also pin the built keys for the checker's lifetime).
	LispRef FunctionChecker::TryBuildClosedKeyExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return {};
		auto memoKey = std::make_pair(refScope, LispRef(expr));
		if (auto it = m_ClosedKeyMemo.find(memoKey); it != m_ClosedKeyMemo.end())
			return it->second;
		LispRef built = TryBuildClosedKeyExprImpl(refScope, expr);
		m_ClosedKeyMemo.emplace(std::move(memoKey), built);
		return built;
	}

	LispRef FunctionChecker::TryBuildClosedKeyExprImpl(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.IsRealList())
		{
			if (!expr.Left().IsSymb())
				return {};
			LispRef head = expr.Left();
			TokenID headID = head.GetSymbID();
			if (headID == token::sourceDescr)
				return TryBuildClosedKeyExpr(refScope, expr.Right().Left());
			if (headID == token::arrow || headID == token::scope || headID == token::subitem
				|| headID == t_ApplyValue || headID == t_ContainerLiteral)
				return {};
			const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
			if (og->IsTemplateCall() && !ValueClass::FindByScriptName(headID))
				return {}; // function-call heads: not buildable in v1 (ReduceValue re-entrancy)
			if (!og->IsTemplateCall() && !og->MustCacheResult())
				return {}; // meta heads: reduction rejects them in bodies anyway
			std::vector<LispRef> substArgs;
			for (LispPtr argPtr = expr.Right(); !argPtr.EndP(); argPtr = argPtr.Right())
			{
				auto sub = TryBuildClosedKeyExpr(refScope, argPtr.Left());
				if (sub.EndP() && !argPtr.Left().EndP())
					return {}; // an open/unbuildable argument
				substArgs.push_back(sub);
			}
			LispRef argList;
			for (auto ri = substArgs.rbegin(); ri != substArgs.rend(); ++ri)
				argList = LispRef(*ri, argList);
			LispRef result = RewriteExprTop(LispRef(head, std::move(argList)));
			if (!og->IsTemplateCall() && og->CanResultToConfigItem())
			{
				DataControllerRef dc = GetOrCreateDataController(result);
				auto supplier = dc->MakeResult();
				if (!supplier)
					return {}; // metainfo failure: defer (the application reports it)
				if (!supplier->IsCacheItem())
					result = supplier->GetCheckedKeyExpr();
			}
			return result;
		}
		if (expr.IsSymb())
		{
			TokenID symbID = expr.GetSymbID();
			if (symbID == t_Hole)
				return {}; // (a '...x' rest symbol resolves to its param child below: class 0 = open)
			if (token::isConst(symbID))
				return ExprList(symbID);
			if (ValueClass::FindByScriptName(symbID))
				return List(LispRef(expr));
			const TreeItem* local = nullptr; UInt32 paramIdx = 0;
			SharedTreeItem external; ExtRefKind extKind = ExtRefKind::DefScopeExternal;
			switch (ResolveName(refScope, symbID, &local, &paramIdx, &external, &extKind))
			{
			case 0:
				return {}; // a formal: OPEN
			case 1:
			{
				if (!local)
					return {};
				if (!m_ScanBusy.insert(local).second)
					return {}; // cyclic body-local reference: defer (reduction reports)
				LispRef r;
				SharedStr exprStr = local->GetExpr();
				if (!exprStr.empty() && !AbstrCalculator::MustEvaluate(exprStr.c_str()))
				{
					auto calc = AbstrCalculator::ConstructFromStr(local, exprStr, CalcRole::Calculator);
					auto localScope = local->GetTreeParent();
					r = TryBuildClosedKeyExpr(localScope.get(), RewriteExpr(calc->GetLispExprOrg()));
				}
				m_ScanBusy.erase(local);
				return r;
			}
			default:
				if (extKind != ExtRefKind::DefScopeExternal || !external)
					return {}; // param-member / prelude-fn / closure capture: OPEN or not a data value
				if (external->IsFunctionItem() || external->InTemplate())
					return {};
				external->UpdateMetaInfo();
				return external->GetCheckedKeyExpr();
			}
		}
		return LispRef(expr); // numeric / string / UInt64 literal: a valid DC key of its own
	}

	// §12.7: evaluate a closed spec sub-expression at definition scan. Literal
	// fast path reads straight off the parse tree; everything else -- including
	// storage-backed definition-scope items, per the explicit ruling -- goes
	// through the standard meta-thread calculation (the CalcCertainResult idiom
	// the dynamic-argument-policies spec read already uses). ANY failure,
	// including a transient storage failure, yields nullopt = defer: the
	// application retries the same DC and reports properly if it persists.
	std::optional<SharedStr> FunctionChecker::EvalClosedSpec(const TreeItem* refScope, LispPtr specExpr)
	{
		if (specExpr.IsStrn())
			return SharedStr(CharPtrRange(specExpr.GetStrnBeg(), specExpr.GetStrnEnd()));
		auto defScope = m_FuncItem->GetTreeParent();
		if (!defScope || defScope->InTemplate())
			return std::nullopt; // a function nested in a template: no evaluation context
		LispRef key;
		try
		{
			key = TryBuildClosedKeyExpr(refScope, specExpr);
		}
		catch (...)
		{
			m_ScanBusy.clear(); // unwind the cycle-guard marks of the aborted scan
			return std::nullopt;
		}
		if (key.EndP())
			return std::nullopt;
		try
		{
			FencedInterestRetainContext irc("FunctionChecker::EvalClosedSpec");
			auto dc = GetOrCreateDataController(key);
			if (!dc)
				return std::nullopt;
			irc.Add(dc.get());
			auto resItem = dc->MakeResult();
			if (!resItem || dc->WasFailed() || !IsDataItem(resItem.get()))
				return std::nullopt;
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
				return std::nullopt;
			auto adi = AsDataItem(fd->GetOld());
			if (!adi || !adi->HasVoidDomainGuarantee()
				|| adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() != ValueClassID::VT_SharedStr)
				return std::nullopt;
			return GetTheValue<SharedStr>(adi);
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	// §12.7 for_each tranche: the array sibling of EvalClosedSpec -- evaluate a
	// closed string ARRAY (any domain) at definition scan and read all its
	// values, storage-backed sources included per the ruling. The evaluation
	// runs through the very hash-consed DC every instantiation of the meta
	// application will use (closedness ⟺ β-substitution is the identity), so
	// the value is read once and the cache entry is warmed. ANY failure yields
	// nullopt = defer: the instantiation retries the same DC and reports
	// properly if it persists.
	std::optional<std::vector<SharedStr>> FunctionChecker::EvalClosedStrArray(const TreeItem* refScope, LispPtr expr)
	{
		auto defScope = m_FuncItem->GetTreeParent();
		if (!defScope || defScope->InTemplate())
			return std::nullopt; // a function nested in a template: no evaluation context
		LispRef key;
		try
		{
			key = TryBuildClosedKeyExpr(refScope, expr);
		}
		catch (...)
		{
			m_ScanBusy.clear(); // unwind the cycle-guard marks of the aborted scan
			return std::nullopt;
		}
		if (key.EndP())
			return std::nullopt;
		try
		{
			FencedInterestRetainContext irc("FunctionChecker::EvalClosedStrArray");
			auto dc = GetOrCreateDataController(key);
			if (!dc)
				return std::nullopt;
			irc.Add(dc.get());
			auto resItem = dc->MakeResult();
			if (!resItem || dc->WasFailed() || !IsDataItem(resItem.get()))
				return std::nullopt;
			FutureData fd = dc->CalcCertainResult();
			if (!fd || fd->WasFailed())
				return std::nullopt;
			auto adi = AsDataItem(fd->GetOld());
			if (!adi || adi->GetAbstrValuesUnit()->GetValueType()->GetValueClassID() != ValueClassID::VT_SharedStr)
				return std::nullopt;
			DataReadLock lock(adi);
			auto sa = const_array_cast<SharedStr>(lock);
			if (!sa)
				return std::nullopt;
			SizeT n = adi->GetAbstrDomainUnit()->GetDataCount();
			std::vector<SharedStr> result;
			result.reserve(n);
			for (SizeT i = 0; i != n; ++i)
				result.push_back(sa->GetIndexedValue(i));
			return result;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	// the declared type of parameter idx: rigid variables for generic positions, a
	// rigid identity for unit parameters, a Func value for signature-typed parameters
	DefType FunctionChecker::ParamType(UInt32 idx)
	{
		if (auto it = m_ParamTypes.find(idx); it != m_ParamTypes.end())
			return it->second;
		DefType pt = ParamTypeImpl(idx);
		m_ParamTypes[idx] = pt;
		return pt;
	}

	DefType FunctionChecker::ParamTypeImpl(UInt32 idx)
	{
		const TreeItem* p = m_Params[idx];
		if (auto sig = TreeItem_GetFunctionParamSignature(m_FuncItem, idx))
		{
			DefType r; r.kind = DefType::Kind::Func; r.fn = sig.get();
			m_Keep.push_back(sig);
			auto sigVars = TreeItem_GetFunctionTypeVars(sig.get());
			auto typeArgs = TreeItem_GetFunctionParamSigTypeArgs(m_FuncItem, idx);
			if (sigVars && typeArgs && typeArgs->size() == sigVars->size())
			{
				r.varsOwner = m_FuncItem; r.instance = 0;
				r.tok2owner = std::make_shared<std::map<TokenID, TokenID>>();
				for (SizeT k = 0; k != sigVars->size(); ++k)
					(*r.tok2owner)[(*sigVars)[k].first] = (*typeArgs)[k];
			}
			return r;
		}
		if (IsUnit(p))
		{
			DefType r; r.kind = DefType::Kind::UnitVal;
			r.vc = AsUnit(p)->GetValueType();
			r.dom = DefType::Dom::Node;
			// rigid identity (the unit bound at application); the declared class
			// pins the companion class node concretely (batch U)
			r.dNode = UNode(m_FuncItem, 0, p->GetNameID(), r.vc);
			// K11a-1: a structured unit parameter carries a member block (sub-items).
			// K11a by-example ('nw: network_links'): the parse-time clone carries only
			// the CLASS (ConfigProd.cpp DoRefTypeSignature) -- the retained UNIT
			// exemplar supplies the declared member block instead, typed through the
			// SAME BuildParamMembers ladder (declared kind/type only; exemplar DATA is
			// never read, risk R-b).
			if (p->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, r.dNode);
				r.membersComplete = true;
			}
			else if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, idx); ex && ex->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, r.dNode, ex.get());
				r.membersComplete = ExemplarMemberSetIsClosed(ex.get());
				m_Keep.push_back(ex);
			}
			return r;
		}
		if (IsDataItem(p))
			return PositionType(p, m_FuncItem, 0, nullptr, 0, nullptr);
		if (p->IsFunctionItem())
		{
			DefType r; r.kind = DefType::Kind::Func; r.fn = p; // bare 'name: function' / cloned exemplar
			return r;
		}
		// K11a-4: a CONTAINER parameter ('container cfg { … }', or by-example
		// 'cfg: Settings') -- the declared member block types exactly like a
		// structured unit parameter's, except there is no parameter unit: members
		// without an explicit domain DEFER instead of defaulting (NO_TYPE_VAR).
		// A plain member-less TreeItem parameter ('item x' meta-refs and friends)
		// stays deferred.
		{
			DefType r; r.kind = DefType::Kind::Container;
			if (p->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, NO_TYPE_VAR);
				r.membersComplete = true;
				return r;
			}
			if (auto ex = TreeItem_GetFunctionParamTypeExemplar(m_FuncItem, idx); ex && ex->_GetFirstSubItem())
			{
				r.members = BuildParamMembers(p, NO_TYPE_VAR, ex.get());
				r.membersComplete = ExemplarMemberSetIsClosed(ex.get());
				m_Keep.push_back(ex);
				return r;
			}
		}
		return {}; // member-less plain parameters: deferred
	}

	// K11a-1: build the member map of a structured unit parameter `p`. Each declared
	// member sub-item is typed: a member UNIT gets a per-instantiation identity node; a
	// member ATTRIBUTE is Data with its declared value class and domain. K11a-1b: a
	// member attribute whose values token names a sibling member unit also carries that
	// unit's IDENTITY node (vuNode) -- so members sharing a node unit (F1,F2 both
	// attribute<nodeset>) unify over the SAME node at definition, and members over
	// different node units fail to unify.
	// K11a-3.1 (generic member types): member tokens resolve through the SAME ladder a
	// positional declaration uses (PositionType), innermost first --
	//   values: sibling member unit → the function's generic variables (a domain-
	//     sorted variable also carries unit identity, the K2 bridge) → ValueClass
	//     name → a telescope unit parameter → a definition-scope unit;
	//   domain: the parameter itself / '.' (the default) → sibling member unit →
	//     generic domain variable → telescope unit parameter → definition-scope
	//     unit (Void broadcasts) → otherwise DEFER (Dom::Unknown).
	// (Review findings, K11a-3.1 round:) generic variables come BEFORE ValueClass
	// names, matching PositionType -- a type variable named like a value class must
	// type members and body items to the SAME rigid node; a member declared without
	// a domain carries the implicit '.' entity token (ConfigProd RetrieveEntity),
	// never an empty one, so '.' selects the parameter-unit default; and member-unit
	// nodes are keyed by the PARAMETER-QUALIFIED token 'p/member' so same-named
	// member units of different structured parameters (or a member unit shadowing a
	// same-named telescope parameter) stay DISTINCT rigid units.
	// An explicit domain token must NEVER silently fall back to the parameter unit:
	// that mistyped `cost (E2)` as over the parameter and falsely rejected a correct
	// body item declared over E2 (rigid-rigid 'nw'≠'E2' conflict) -- unresolvable
	// tokens defer.
	std::shared_ptr<const std::map<TokenID, DefType>>
	FunctionChecker::BuildParamMembers(const TreeItem* p, SizeT paramDomNode, const TreeItem* memberSrc)
	{
		// memberSrc: the item whose DECLARED sub-items form the member block -- the
		// parameter itself (explicit block) or its by-example UNIT exemplar. Node
		// qualification stays on the PARAMETER's name either way (two by-example
		// parameters of one exemplar must still be distinct rigid units).
		// By-example review finding (reproduced): EXEMPLAR member tokens are
		// lexically the EXEMPLAR's world -- they must never resolve against the
		// function's generic variables, telescope parameters, definition scope, or
		// the caller-chosen parameter name (a same-named parameter/scope unit
		// CAPTURED them, falsely rejecting correct programs at definition). In
		// by-example mode the non-sibling rungs are the ValueClass vocabulary and
		// the exemplar's own lexical scope; everything else defers.
		bool byExample = memberSrc != nullptr && memberSrc != p;
		if (!memberSrc)
			memberSrc = p;
		const TreeItem* scopeAnchor = byExample ? memberSrc : m_FuncItem;
		auto members = std::make_shared<std::map<TokenID, DefType>>();
		SharedStr pName(p->GetNameID().AsStrRangeLock()); // materialized: TokenStr must not span token creation below

		// K11a-4: one WALK per block, recursing into declared CONTAINER members with
		// the member path as prefix -- the map is FLAT, keyed by the full relative
		// path ('meta', 'meta/factor'), so deep member access types directly.
		// blockDomNode is the enclosing unit's node, or NO_TYPE_VAR when the block
		// has no enclosing unit (a container parameter / a nested container block):
		// there, members without an explicit domain DEFER instead of defaulting.
		// Sibling resolution is per block; qualified node tokens carry the full
		// path ('p/meta/subunit').
		std::function<void(const TreeItem*, const SharedStr&, SizeT)> walkBlock;
		walkBlock = [&](const TreeItem* block, const SharedStr& prefix, SizeT blockDomNode)
		{
			auto qualTok = [&](TokenID memberTok) -> TokenID
			{
				SharedStr mName(memberTok.AsStrRangeLock());
				return prefix.empty()
					? GetTokenID_mt(mySSPrintF("{}/{}", pName.c_str(), mName.c_str()).c_str())
					: GetTokenID_mt(mySSPrintF("{}/{}{}", pName.c_str(), prefix.c_str(), mName.c_str()).c_str());
			};
			for (const TreeItem* m = block->_GetFirstSubItem(); m; m = m->GetNextItem())
			{
				// review finding: nested FUNCTIONS, TEMPLATES and type-alias exemplars
				// are implementation content, not members -- they must neither be typed
				// nor (through membersComplete) make a same-named reference an error
				if (m->IsTemplate() || m->IsFunctionItem())
					continue;
				DefType md;
				if (IsUnit(m))
				{
					md.kind = DefType::Kind::UnitVal;
					md.vc   = AsUnit(m)->GetValueType();
					md.dom  = DefType::Dom::Node;
					md.dNode = UNode(m_FuncItem, 0, qualTok(m->GetNameID()), md.vc);
				}
				else if (IsDataItem(m))
				{
					auto adi = AsDataItem(m);
					md.kind  = DefType::Kind::Data;
					md.vcomp = adi->GetValueComposition();

					if (TokenID vt = adi->ValuesUnitToken())
					{
						bool vMatched = false;
						for (const TreeItem* u = block->_GetFirstSubItem(); u; u = u->GetNextItem())
							if (u->GetNameID() == vt && IsUnit(u))
							{
								// K11a-1b: the member attribute's values unit IS the sibling
								// member unit -- carry its IDENTITY node, not just its class.
								// The node is keyed by the QUALIFIED token, so it is the SAME
								// node the member unit itself got above. Hence F1,F2 both
								// `attribute<nodeset>` share one node: the body's
								// pcount(nw/F1)+pcount(nw/F2) unifies over the single nodeset
								// domain at definition, and two members over DIFFERENT node
								// units fail to unify.
								md.vc = AsUnit(u)->GetValueType();
								md.vuNode = UNode(m_FuncItem, 0, qualTok(vt), md.vc);
								vMatched = true;
								break;
							}
						if (!vMatched && !byExample && (IsOwnDeclaredVar(m_FuncItem, vt) || IsGenericVarOf(m_FuncItem, vt)))
						{
							// K11a-3.1: `w: attribute<V>` under `<V: numerics>` -- the member's
							// values class IS the function's rigid variable, so body uses of
							// nw/w are checked under ∀ exactly like a positional attribute<V>
							md.vNode = ValNode(m_FuncItem, 0, vt);
							if (IsDomainSortedVarOf(m_FuncItem, vt))
								md.vuNode = UNode(m_FuncItem, 0, vt); // K2: identity through the values role
							vMatched = true;
						}
						if (!vMatched)
							if (auto vc = ValueClass::FindByScriptName(vt))
							{
								md.vc = vc;
								vMatched = true;
							}
						if (!vMatched && !byExample)
						{
							const TreeItem* q = m_FuncItem->_GetFirstSubItem();
							for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(m_FuncItem); j != n && q; ++j, q = q->GetNextItem())
								if (q->GetNameID() == vt && IsUnit(q))
								{
									// a telescope unit parameter in the VALUES role: its declared
									// class + per-instantiation identity (as PositionType does)
									md.vc = AsUnit(q)->GetValueType();
									md.vuNode = UNode(m_FuncItem, 0, vt, md.vc);
									vMatched = true;
									break;
								}
						}
						if (!vMatched)
							if (auto u = ResolveUnitInScope(vt, scopeAnchor))
							{
								md.vc = AsUnit(u.get())->GetValueType();
								md.vKeep = u; md.vUnit = AsUnit(u.get()); // identity too (batch U)
							}
						// else: unknown values class -- checked per application
					}

					// domain: default = the enclosing unit (or DEFER when the block has
					// none); an explicit token resolves through the ladder or DEFERS
					// (never silently the parameter unit). The block's own name (the
					// exemplar, in the by-example case) also selects the default: inside
					// its declaration that name IS the enclosing unit. The caller-chosen
					// PARAMETER name selects the default only for an explicit member
					// block (an exemplar token never means it).
					TokenID dt = adi->DomainUnitToken();
					if (!dt || dt == t_Dot || dt == block->GetNameID() || (!byExample && block == memberSrc && dt == p->GetNameID()))
					{
						if (blockDomNode != NO_TYPE_VAR)
						{
							md.dom = DefType::Dom::Node;
							md.dNode = blockDomNode;
						}
						// else: containers have no member-domain default -- defer
					}
					else
					{
						bool dMatched = false;
						for (const TreeItem* u = block->_GetFirstSubItem(); u; u = u->GetNextItem())
							if (u->GetNameID() == dt && IsUnit(u))
							{
								// over a sibling member unit -- the SAME (qualified) node that
								// member got
								md.dom = DefType::Dom::Node;
								md.dNode = UNode(m_FuncItem, 0, qualTok(dt), AsUnit(u)->GetValueType());
								dMatched = true;
								break;
							}
						if (!dMatched && !byExample && (IsOwnDeclaredVar(m_FuncItem, dt) || IsGenericVarOf(m_FuncItem, dt)))
						{
							md.dom = DefType::Dom::Node;
							md.dNode = UNode(m_FuncItem, 0, dt); // generic domain variable
							dMatched = true;
						}
						if (!dMatched && !byExample)
						{
							const TreeItem* q = m_FuncItem->_GetFirstSubItem();
							for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(m_FuncItem); j != n && q; ++j, q = q->GetNextItem())
								if (q->GetNameID() == dt && IsUnit(q))
								{
									// over a telescope unit parameter (UNode self-pins its class)
									md.dom = DefType::Dom::Node;
									md.dNode = UNode(m_FuncItem, 0, dt);
									dMatched = true;
									break;
								}
						}
						if (!dMatched)
							if (auto u = ResolveUnitInScope(dt, scopeAnchor))
							{
								if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
									md.dom = DefType::Dom::Void;
								else
								{
									md.dom = DefType::Dom::Concrete;
									md.domKeep = u; md.domUnit = AsUnit(u.get());
								}
								dMatched = true;
							}
						// !dMatched: md.dom stays Dom::Unknown -- defer
					}
				}
				// else (container / nested-function members): md stays Unknown -- the
				// member IS declared, so it must be IN the map (K11a-3 review finding:
				// dropping it while membersComplete=true made a direct `nw/meta`
				// reference a false "declares no member" definition error)
				SharedStr mName(m->GetNameID().AsStrRangeLock());
				SharedStr mPath = prefix.empty() ? mName : prefix + mName;
				// a direct member's key IS its item token; a nested path interns once here
				(*members)[prefix.empty() ? m->GetNameID() : GetTokenID_mt(mPath.c_str())] = md;

				// K11a-4: recurse into a declared CONTAINER member -- its members type
				// under the flattened path ('meta/factor'); nested blocks have no
				// enclosing unit, so their default domains defer. Nested UNIT members'
				// sub-items stay deferred (the argument may carry label attrs etc.).
				if (!IsUnit(m) && !IsDataItem(m) && m->_GetFirstSubItem())
					walkBlock(m, mPath + "/", NO_TYPE_VAR);
			}
		};
		walkBlock(memberSrc, SharedStr(), paramDomNode);
		return members;
	}

	// K11b: type the members of a CONCRETE container (a definition-scope item passed
	// as an operator's ArgContainer argument). Unlike a parameter's member block these
	// members are real items, so their types are CONCRETE: a member unit is itself the
	// unit, and a member attribute's domain/values tokens resolve in the CONTAINER's
	// OWN scope (never the function's -- the by-example capture-shadowing lesson).
	// Members whose tokens do not resolve stay Unknown and simply defer.
	std::shared_ptr<const std::map<TokenID, DefType>>
	FunctionChecker::BuildConcreteContainerMembers(const TreeItem* c)
	{
		auto members = std::make_shared<std::map<TokenID, DefType>>();
		for (const TreeItem* m = c->_GetFirstSubItem(); m; m = m->GetNextItem())
		{
			DefType md;
			if (IsUnit(m))
			{
				md.kind = DefType::Kind::UnitVal;
				md.vc = AsUnit(m)->GetValueType();
				if (AsUnit(m)->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
					md.dom = DefType::Dom::Void;
				else
				{
					md.dom = DefType::Dom::Concrete;
					md.domKeep = m->shared_from_this(); md.domUnit = AsUnit(m);
				}
			}
			else if (IsDataItem(m))
			{
				auto adi = AsDataItem(m);
				md.kind = DefType::Kind::Data;
				md.vcomp = adi->GetValueComposition();
				if (TokenID vt = adi->ValuesUnitToken())
				{
					if (auto vc = ValueClass::FindByScriptName(vt))
						md.vc = vc;
					else if (auto u = ResolveUnitInScope(vt, c))
					{
						md.vc = AsUnit(u.get())->GetValueType();
						md.vKeep = u; md.vUnit = AsUnit(u.get());
					}
				}
				TokenID dt = adi->DomainUnitToken();
				if (!dt || dt == t_Dot || dt == c->GetNameID())
					; // a container has no enclosing unit: an implicit domain defers
				else if (auto u = ResolveUnitInScope(dt, c))
				{
					if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
						md.dom = DefType::Dom::Void;
					else
					{
						md.dom = DefType::Dom::Concrete;
						md.domKeep = u; md.domUnit = AsUnit(u.get());
					}
				}
			}
			// else: nested containers & co stay Unknown (declared, but untyped here)
			(*members)[m->GetNameID()] = md;
		}
		return members;
	}

	// K11a-2: type `P/member` access at definition against the structured parameter's
	// member map. Hit → the member's type (so the body's use of it is checked under ∀).
	// K11a-3: a DIRECT-member miss under a COMPLETE declared interface is a
	// definition-time error -- the member block is the parameter's declared contract
	// (§4.6 strict scope), so a body reference outside it is wrong regardless of what
	// extra members an argument happens to provide. DEEP paths defer (the argument may
	// legitimately carry structure BELOW a declared member, e.g. nw/nodeset/label);
	// non-structured parameters defer as before (the per-application SubItem check runs).
	DefType FunctionChecker::InferParamMember(UInt32 paramIdx, const SharedStr& memberPath)
	{
		DefType pt = ParamType(paramIdx);
		if (pt.members)
		{
			// keys are TokenIDs; a path whose token does not even exist names no member
			TokenID pathTok = GetExistingTokenID_mt(memberPath.begin(), memberPath.send());
			if (IsDefined(pathTok))
				if (auto it = pt.members->find(pathTok); it != pt.members->end())
					return it->second;
			if (pt.membersComplete)
			{
				// a DIRECT miss is an error; a DEEP miss is an error only when its
				// parent path is a declared container block we WALKED (K11a-4: the
				// flat map then holds that block's complete member set, so
				// 'cfg/nested/wrong' is as wrong as a direct miss). A deep path below
				// a UNIT member or an unwalked item still defers: the argument may
				// legitimately carry sub-structure there (label attributes, a
				// composite rule's generated members).
				auto slash = std::find(memberPath.begin(), memberPath.send(), '/');
				bool report = slash == memberPath.send();
				if (!report)
				{
					CharPtr lastSlash = memberPath.send();
					for (CharPtr q = memberPath.begin(); q != memberPath.send(); ++q)
						if (*q == '/')
							lastSlash = q;
					// NB: CharPtrRange, NOT (begin, end) -- SharedStr has no two-pointer
					// ctor, so that silently binds to SharedStr(zStr, debugSrcName) and
					// yields the WHOLE path
					SharedStr parentPath{ CharPtrRange(memberPath.begin(), lastSlash) };
					SharedStr parentPrefix = parentPath + "/";
					TokenID parentTok = GetExistingTokenID_mt(parentPath.begin(), parentPath.send());
					auto pit = IsDefined(parentTok) ? pt.members->find(parentTok) : pt.members->end();
					if (pit != pt.members->end()
						&& pit->second.kind != DefType::Kind::Data && pit->second.kind != DefType::Kind::UnitVal)
						for (const auto& kv : *pt.members)
						{
							// prefix test on materialized key text; folded, so the scan
							// agrees with the map's own (token) equality -- the former
							// exact-case std::equal silently deferred on a case-mismatched
							// parent spelling instead of reporting the closed-set miss
							SharedStr kStr(kv.first.AsStrRangeLock());
							if (kStr.ssize() > parentPrefix.ssize()
								&& std::equal(parentPrefix.begin(), parentPrefix.send(), kStr.begin(),
									[](char x, char y) { return AsciiTokenFold(x) == AsciiTokenFold(y); }))
							{
								report = true; // the parent block was walked: its member set is closed
								break;
							}
						}
				}
				if (report)
				{
					SharedStr pName(m_Params[paramIdx]->GetNameID().AsStrRangeLock());
					throwErrorF("ExprParser", "the definition of '{}': parameter '{}' declares no member '{}'"
						, m_FuncItem->GetFullName().c_str(), pName.c_str(), memberPath.c_str());
				}
			}
		}
		return {};
	}

	// the declared annotation of a body item (or result); Unknown when undeclared
	DefType FunctionChecker::DeclaredItemType(const TreeItem* item)
	{
		if (IsDataItem(item))
			return PositionType(item, m_FuncItem, 0, nullptr, 0, nullptr, item->GetTreeParent().get());
		return {}; // units, containers, nested functions: no data annotation to check
	}

	// the declared type of one position (parameter or result declaration) of fnDef,
	// instantiated for one application under `instance`. Token resolution order:
	// the type-application translation (tok2owner -> varsOwner's variables), the
	// varsOwner's own variables (nested results reference their origin's variables),
	// fnDef's own generic variables, fnDef's unit parameters (per-instantiation
	// identity), and finally fnDef's definition scope (concrete units).
	DefType FunctionChecker::PositionType(const TreeItem* posItem, const TreeItem* fnDef, UInt32 instance,
		const TreeItem* ownerFn, UInt32 ownerInstance, const std::map<TokenID, TokenID>* tok2owner,
		const TreeItem* itemScope)
	{
		DefType r;
		if (!posItem)
			return r;
		if (IsUnit(posItem))
		{
			r.kind = DefType::Kind::UnitVal;
			r.vc = AsUnit(posItem)->GetValueType();
			r.dom = DefType::Dom::Node;
			r.dNode = UNode(fnDef, instance, posItem->GetNameID(), r.vc);
			return r;
		}
		if (!IsDataItem(posItem))
			return r; // container/typed-by-example positions: deferred

		r.kind = DefType::Kind::Data;
		auto adi = AsDataItem(posItem);
		r.vcomp = adi->GetValueComposition();

		if (TokenID vTok = adi->ValuesUnitToken())
		{
			if (tok2owner)
				if (auto it = tok2owner->find(vTok); it != tok2owner->end())
				{
					vTok = it->second, r.vNode = ValNode(ownerFn, ownerInstance, vTok);
					if (IsDomainSortedVarOf(ownerFn, vTok))
						r.vuNode = UNode(ownerFn, ownerInstance, vTok); // K2 identity through the sig binding too
				}
			if (r.vNode == NO_TYPE_VAR)
			{
				// an own <...> clause shadows the origin's variables; unmapped tokens
				// of a type application (tok2owner set) belong to fnDef's own lexical
				// world and never resolve to the origin's variables.
				// batch U: a values token naming a DOMAIN-SORTED generic or a unit
				// PARAMETER additionally carries the unit's IDENTITY (vuNode) -- the
				// SAME node its domain role uses, which is the K2 bridge; a token
				// resolving to a concrete scope unit carries that unit (vUnit)
				if (IsOwnDeclaredVar(fnDef, vTok))
				{
					r.vNode = ValNode(fnDef, instance, vTok);
					if (IsDomainSortedVarOf(fnDef, vTok))
						r.vuNode = UNode(fnDef, instance, vTok);
				}
				else if (!tok2owner && ownerFn && IsGenericVarOf(ownerFn, vTok))
				{
					r.vNode = ValNode(ownerFn, ownerInstance, vTok);
					if (IsDomainSortedVarOf(ownerFn, vTok))
						r.vuNode = UNode(ownerFn, ownerInstance, vTok);
				}
				else if (IsGenericVarOf(fnDef, vTok))
				{
					r.vNode = ValNode(fnDef, instance, vTok);
					if (IsDomainSortedVarOf(fnDef, vTok))
						r.vuNode = UNode(fnDef, instance, vTok);
				}
				else if (auto vc = ValueClass::FindByScriptName(vTok))
					r.vc = vc;
				else if (itemScope && HasBodyShadower(vTok, itemScope))
					; // a body-local declaration shadows the outer name: defer
				else
				{
					// a unit parameter of fnDef in the VALUES role: per-instantiation
					// identity + the class its declaration pins (`unit<uint32> U`)
					const TreeItem* q = fnDef->_GetFirstSubItem();
					for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fnDef); j != n && q; ++j, q = q->GetNextItem())
						if (q->GetNameID() == vTok && IsUnit(q))
						{
							r.vc = AsUnit(q)->GetValueType();
							r.vuNode = UNode(fnDef, instance, vTok, r.vc);
							break;
						}
					if (r.vuNode == NO_TYPE_VAR && r.vc == nullptr)
						if (auto u = ResolveUnitInScope(vTok, fnDef))
						{
							r.vc = AsUnit(u.get())->GetValueType(); // kinds-level class
							r.vKeep = u; r.vUnit = AsUnit(u.get()); // + identity (batch U)
						}
					// else: unknown values class (checked per application)
				}
			}
		}

		if (TokenID dTok = adi->DomainUnitToken())
		{
			if (tok2owner)
				if (auto it = tok2owner->find(dTok); it != tok2owner->end())
					dTok = it->second, r.dom = DefType::Dom::Node, r.dNode = UNode(ownerFn, ownerInstance, dTok);
			if (r.dom == DefType::Dom::Unknown)
			{
				if (IsOwnDeclaredVar(fnDef, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok);
				}
				else if (!tok2owner && ownerFn && IsGenericVarOf(ownerFn, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(ownerFn, ownerInstance, dTok);
				}
				else if (IsGenericVarOf(fnDef, dTok))
				{
					r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok);
				}
				else if (itemScope && HasBodyShadower(dTok, itemScope))
				{ /* a body-local declaration shadows the outer name: defer */ }
				else
				{
					// a unit parameter of fnDef: its per-instantiation identity
					const TreeItem* q = fnDef->_GetFirstSubItem();
					for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(fnDef); j != n && q; ++j, q = q->GetNextItem())
						if (q->GetNameID() == dTok && IsUnit(q))
						{
							r.dom = DefType::Dom::Node; r.dNode = UNode(fnDef, instance, dTok, AsUnit(q)->GetValueType());
							break;
						}
					if (r.dom == DefType::Dom::Unknown)
						if (auto u = ResolveUnitInScope(dTok, fnDef))
						{
							if (AsUnit(u.get())->GetValueType()->GetValueClassID() == ValueClassID::VT_Void)
								r.dom = DefType::Dom::Void;
							else
							{
								r.dom = DefType::Dom::Concrete; r.domKeep = u; r.domUnit = AsUnit(u.get());
							}
						}
				}
			}
		}
		return r;
	}

	void FunctionChecker::UnifyData(const DefType& a, const DefType& b, const SharedStr& srcA, const SharedStr& srcB)
	{
		if (a.kind == DefType::Kind::Unknown || b.kind == DefType::Kind::Unknown)
			return; // deferred to per-application checking
		if (a.kind != b.kind || a.kind == DefType::Kind::Func)
			return; // kind confusion and function-value conformance are handled at binding sites

		// value positions
		if (a.vNode != NO_TYPE_VAR && b.vNode != NO_TYPE_VAR)
			m_Unifier.LinkValue(a.vNode, b.vNode, srcB);
		else if (a.vNode != NO_TYPE_VAR && b.vc)
			m_Unifier.BindValue(a.vNode, b.vc, srcB);
		else if (b.vNode != NO_TYPE_VAR && a.vc)
			m_Unifier.BindValue(b.vNode, a.vc, srcA);
		else if (a.vc && b.vc && a.vc != b.vc)
			throwErrorF("ExprParser", "the definition of '{}': {} ({}) does not match {} ({})"
				, m_FuncItem->GetFullName().c_str()
				, a.vc->GetNameID(), srcA.c_str(), b.vc->GetNameID(), srcB.c_str());

		// values-unit IDENTITY (batch U): declared identities through unit
		// parameters and domain-sorted generics -- the function-signature K2
		// bridge (`attribute<E> rel (D); attribute<V> vals (E)` flows both roles
		// of E through ONE unit node). Terms without identity information defer;
		// concrete pairs compare by defining-expression identity under the
		// checker's total-symmetric mode, exactly like domains below
		if (a.kind == DefType::Kind::Data && b.kind == DefType::Kind::Data)
		{
			if (a.vuNode != NO_TYPE_VAR && b.vuNode != NO_TYPE_VAR)
				m_Unifier.LinkUnit(a.vuNode, b.vuNode, srcB);
			else if (a.vuNode != NO_TYPE_VAR && b.vUnit)
				m_Unifier.BindUnit(a.vuNode, b.vKeep, b.vUnit, srcB);
			else if (b.vuNode != NO_TYPE_VAR && a.vUnit)
				m_Unifier.BindUnit(b.vuNode, a.vKeep, a.vUnit, srcA);
			// concrete-vs-concrete deliberately DEFERS (S1, review finding): reduction
			// checks values units by UnifyValues (class + metric, AllowDefaultLeft) --
			// two key-distinct metric-less units of one class unify there, so a
			// key-identity error here would reject configs that reduce fine. Identity
			// is enforced only through a declared unit-variable contract (the arms
			// above), a surface that did not resolve before batch U.
		}

		// domain positions (void broadcasts; unknown defers)
		using Dom = DefType::Dom;
		if (a.dom == Dom::Unknown || b.dom == Dom::Unknown || a.dom == Dom::Void || b.dom == Dom::Void)
			return;
		if (a.dom == Dom::Node && b.dom == Dom::Node)
			m_Unifier.LinkUnit(a.dNode, b.dNode, srcB);
		else if (a.dom == Dom::Node && b.dom == Dom::Concrete)
			m_Unifier.BindUnit(a.dNode, b.domKeep, b.domUnit, srcB);
		else if (b.dom == Dom::Node && a.dom == Dom::Concrete)
			m_Unifier.BindUnit(b.dNode, a.domKeep, a.domUnit, srcA);
		else if (a.dom == Dom::Concrete && b.dom == Dom::Concrete)
			if (!a.domUnit->UnifyDomain(b.domUnit, "", "", TypeUnifier::s_CheckerUM))
				throwErrorF("ExprParser", "the definition of '{}': the domain of {} differs from the domain of {}"
					, m_FuncItem->GetFullName().c_str(), srcA.c_str(), srcB.c_str());
	}

	// type one application: infer/validate all arguments, unify them against the
	// applied function's declared parameters under a fresh instance, and return the
	// declared result type under that instance
	DefType FunctionChecker::InferApplication(const TreeItem* refScope, const DefType& fnVal, LispPtr argsList, CharPtr headName)
	{
		std::vector<DefType> argTerms;
		UInt32 nrArgs = 0; bool anyHole = false;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right())
		{
			bool hole = a.Left().IsSymb() && a.Left().GetSymbID() == t_Hole;
			anyHole |= hole;
			argTerms.push_back(hole ? DefType{} : InferArg(refScope, a.Left()));
			++nrArgs;
		}
		if (fnVal.kind != DefType::Kind::Func || !fnVal.fn)
			return {};
		const TreeItem* fnDef = fnVal.fn;
		if (TreeItem_IsFunctionVariantSet(fnDef))
			return {}; // variant selection is argument-class-dependent: per application
		if (TreeItem_HasFunctionRestParam(fnDef))
			return {}; // '...x' variadic: the rest binding is per application (splice + fold)
		if (!TreeItem_GetFunctionResultName(fnDef))
			return {}; // no declared signature (bare 'name: function' values): per application
		UInt32 nrParams = TreeItem_GetFunctionParamCount(fnDef);
		if (!anyHole && nrArgs != nrParams)
			throwErrorF("ExprParser", "'{}': function '{}' expects {} argument(s); {} provided"
				, headName, fnDef->GetFullName().c_str(), nrParams, nrArgs);
		if (anyHole)
			return {}; // partial application: residual arity and types per application

		UInt32 instance = m_NextInstance++;
		const TreeItem* ownerFn = fnVal.varsOwner;
		UInt32 ownerInstance = fnVal.instance;
		const std::map<TokenID, TokenID>* t2o = fnVal.tok2owner.get();

		const TreeItem* p = fnDef->_GetFirstSubItem();
		for (UInt32 k = 0; k != nrParams && p; ++k, p = p->GetNextItem())
		{
			SharedStr pName(p->GetNameID().AsStrRangeLock()); // materialized: TokenStr temporaries must not span nested walks (token-registry lock)
			if (auto declaredSig = TreeItem_GetFunctionParamSignature(fnDef, k))
			{
				// signature-typed parameter: a plain function reference with a declared
				// signature is checked and linked here; passed-through values without
				// one (bare 'name: function') and partial bindings defer
				if (argTerms[k].kind == DefType::Kind::Func && argTerms[k].fn && !argTerms[k].varsOwner
					&& argTerms[k].fn->IsFunctionItem() && TreeItem_GetFunctionResultName(argTerms[k].fn))
				{
					const TreeItem* bound = argTerms[k].fn;
					CheckFunctionSignature(bound, declaredSig.get(), pName.c_str());
					auto sigVars = TreeItem_GetFunctionTypeVars(declaredSig.get());
					auto typeArgs = TreeItem_GetFunctionParamSigTypeArgs(fnDef, k);
					if (sigVars && typeArgs && typeArgs->size() == sigVars->size())
					{
						SharedStr bindSource = mySSPrintF("function '{}' bound to parameter '{}' of '{}'"
							, bound->GetFullName().c_str(), pName.c_str(), fnDef->GetFullName().c_str());
						// per type-application argument, the target variable may belong to
						// fnDef (fresh instance) or, through the value's origin, to ownerFn
						auto targetV = [&](TokenID t) -> SizeT
						{
							if (t2o) if (auto it = t2o->find(t); it != t2o->end()) return ValNode(ownerFn, ownerInstance, it->second);
							if (IsOwnDeclaredVar(fnDef, t)) return ValNode(fnDef, instance, t);
							if (!t2o && ownerFn && IsGenericVarOf(ownerFn, t)) return ValNode(ownerFn, ownerInstance, t);
							if (IsGenericVarOf(fnDef, t)) return ValNode(fnDef, instance, t);
							return NO_TYPE_VAR;
						};
						auto targetD = [&](TokenID t) -> SizeT
						{
							if (t2o) if (auto it = t2o->find(t); it != t2o->end()) return UNode(ownerFn, ownerInstance, it->second);
							if (IsOwnDeclaredVar(fnDef, t)) return UNode(fnDef, instance, t);
							if (!t2o && ownerFn && IsGenericVarOf(ownerFn, t)) return UNode(ownerFn, ownerInstance, t);
							if (IsGenericVarOf(fnDef, t)) return UNode(fnDef, instance, t);
							return NO_TYPE_VAR;
						};
						LinkSignatureBinding(m_Unifier, declaredSig.get(), bound, sigVars, typeArgs
							, targetV, targetD, m_NextInstance++, bindSource);
					}
				}
				else if (argTerms[k].kind != DefType::Kind::Unknown && argTerms[k].kind != DefType::Kind::Func)
					throwErrorF("ExprParser", "the definition of '{}': parameter '{}' of function '{}' requires a function argument"
						, m_FuncItem->GetFullName().c_str(), pName.c_str(), fnDef->GetFullName().c_str());
				continue;
			}
			DefType pT = PositionType(p, fnDef, instance, ownerFn, ownerInstance, t2o);
			SharedStr argSrc = mySSPrintF("argument {} of '{}'", k + 1, headName);
			SharedStr parSrc = mySSPrintF("parameter '{}' of function '{}'", pName.c_str(), fnDef->GetFullName().c_str());
			UnifyData(argTerms[k], pT, argSrc, parSrc);
		}

		auto resultChild = fnDef->GetConstSubTreeItemByID(TreeItem_GetFunctionResultName(fnDef));
		if (!resultChild)
			return {};
		if (resultChild->IsFunctionItem())
		{
			if (t2o || ownerFn)
				return {}; // deeper chains of function-valued results: per application
			DefType r; r.kind = DefType::Kind::Func; r.fn = resultChild.get();
			r.varsOwner = fnDef; r.instance = instance; // its positions reference fnDef's variables
			m_Keep.push_back(resultChild);
			return r;
		}
		return PositionType(resultChild.get(), fnDef, instance, ownerFn, ownerInstance, t2o);
	}

	// argument-position inference: function references become Func values (mirroring
	// ResolveBodyArg); container literals defer; everything else infers as expression
	DefType FunctionChecker::InferArg(const TreeItem* refScope, LispPtr argExpr)
	{
		if (argExpr.IsSymb())
		{
			TokenID sym = argExpr.GetSymbID();
			if (sym == t_Hole || token::isConst(sym) || ValueClass::FindByScriptName(sym))
				return {};
			SharedStr s(sym.AsStrRangeLock());
			if (std::find(s.begin(), s.send(), '/') == s.send())
			{
				// function-typed parameters short-circuit by name (mirroring the
				// reducer's binding lookup in ResolveBodyArg); DATA parameters do
				// NOT -- a nearer body local may shadow them (nearest-scope, exactly
				// like ResolveBodySymbol resolves the argument at reduction)
				if (auto headChild = m_FuncItem->GetConstSubTreeItemByID(sym))
					for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
						if (m_Params[i] == headChild.get()
							&& (TreeItem_GetFunctionParamSignature(m_FuncItem, i) || m_Params[i]->IsFunctionItem()))
							return ParamType(i);
				for (const TreeItem* scope = refScope; scope; scope = scope->GetTreeParent().get())
				{
					if (auto child = scope->GetConstSubTreeItemByID(sym))
					{
						for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
							if (m_Params[i] == child.get())
								return ParamType(i);
						if (child->IsFunctionItem())
						{
							DefType r; r.kind = DefType::Kind::Func; r.fn = child.get();
							m_Keep.push_back(child);
							return r;
						}
						return InferBodyItem(child.get());
					}
					if (scope == m_FuncItem)
						break;
				}
				auto fnRef = m_FuncItem->ResolveItemPath(s);
				if (!fnRef || !fnRef->IsFunctionItem())
					if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
						if (auto lex = defParent->ResolveItemPath(s); lex && lex->IsFunctionItem())
							fnRef = lex;
				if (!fnRef || !fnRef->IsFunctionItem())
					if (auto pf = FindPreludeFunction(sym); pf && pf->IsFunctionItem())
						fnRef = pf;
				if (fnRef && fnRef->IsFunctionItem())
				{
					DefType r; r.kind = DefType::Kind::Func; r.fn = fnRef.get();
					m_Keep.push_back(fnRef);
					return r;
				}
			}
		}
		if (argExpr.IsRealList() && argExpr.Left().IsSymb() && argExpr.Left().GetSymbID() == t_ContainerLiteral)
			return {}; // §5.9 literal: members ('.'-rebound) are resolved and checked at reduction
		return InferExpr(refScope, argExpr);
	}

	// ---- operator signatures, batch A: described group records type applications ----
	//
	// The walker consumes AbstrOperGroup::GetSignatures() (OperSignature.h). Per
	// application it (1) filters the group's members by arity and by the argument
	// classes it knows -- a concrete class, a node's binding, or a node's (hard)
	// feasible set, each an over-approximation of the classes any successful
	// reduction can present, so elimination is sound; (2) applies the unique
	// surviving merged record: the shared domain variable with void broadcast and
	// one class node per record variable; and (3) derives the cross-position class
	// relations from the record's member TUPLES -- positions on which ALL tuples
	// agree are LINKED (hard: exactly the old shared-node semantics, so e.g.
	// mul(x:V, y:W) with independent rigids still errors), and positions all
	// tuples pin to one class are BOUND, but never onto a rigid ∀-variable
	// (support sets are soft, see ConstraintRec). Mixed survivor sets, undescribed
	// survivors, and arity mismatches DEFER -- FindOper's widening escape hatches
	// and per-application checking stay in charge, so a description can only ADD
	// judgments where the membership is unambiguous.

	// the witness classes of one inferred argument term; result = could not tell
	enum class WitnessKind : UInt8 { None, Concrete, Feasible };
	WitnessKind ArgWitnesses(TypeUnifier& u, const DefType& t, ValueClassSet& r)
	{
		if (t.kind != DefType::Kind::Data && t.kind != DefType::Kind::UnitVal)
			return WitnessKind::None;
		if (t.vc)
		{
			r.reset(); r.set(UInt32(t.vc->GetValueClassID()));
			return WitnessKind::Concrete;
		}
		if (t.vNode != NO_TYPE_VAR)
		{
			const auto& n = u.m_ValueNodes[u.FindV(t.vNode)];
			if (n.bound)
			{
				r.reset(); r.set(UInt32(n.bound->GetValueClassID()));
				return WitnessKind::Concrete;
			}
			if (!n.feasible.all())
			{
				r = n.feasible;
				return WitnessKind::Feasible;
			}
		}
		return WitnessKind::None;
	}

	// can witness class `w` present itself to a member position registered as
	// `argCls`? (§18.2: item-free class synthesis; Find, never FindCertain)
	// a known composition restricts the synthesis to that composition's class
	bool WitnessMatchesArgClass(const ValueClass* w, bool isUnitArg, const Class* argCls, ValueComposition knownComp)
	{
		auto uc = UnitClass::Find(w);
		if (!uc)
			return false;
		if (isUnitArg)
			return uc->IsDerivedFrom(argCls);
		static const ValueComposition s_Comps[3] = { ValueComposition::Single, ValueComposition::Polygon, ValueComposition::Sequence };
		for (auto comp : s_Comps)
		{
			if (knownComp != ValueComposition::Unknown && comp != knownComp)
				continue;
			auto vt = uc->GetValueType(comp);
			if (!vt)
				continue;
			auto dic = DataItemClass::Find(vt);
			if (dic && dic->IsDerivedFrom(argCls))
				return true;
		}
		return false;
	}

	bool MemberAcceptsArity(const AbstrOperGroup* og, const Operator* m, arg_index nrArgs)
	{
		arg_index ns = m->NrSpecifiedArgs(), req = ns - m->NrOptionalArgs();
		if (og->AllowExtraArgs())
			return nrArgs >= req;
		return req <= nrArgs && nrArgs <= ns;
	}

	// sound elimination on the REGISTERED classes (described or not).
	// Survives: no known argument class rules the member out.
	// EliminatedConcrete: some position with a CONCRETE class rejects it -- the
	// same classes reach reduction, so FindOper is certain to reject it there too.
	// EliminatedFeasible: only feasible-SET witnesses reject it -- symbolic
	// knowledge, so the no-candidate verdict must defer, not error (a rejecting
	// concrete position elsewhere still upgrades the member to Concrete: the
	// scan continues past a feasible rejection looking for one).
	enum class MemberVerdict : UInt8 { Survives, EliminatedConcrete, EliminatedFeasible };
	MemberVerdict ClassifyMember(TypeUnifier& u, const Operator* m, const std::vector<DefType>& argTerms)
	{
		auto verdict = MemberVerdict::Survives;
		arg_index ns = m->NrSpecifiedArgs();
		for (arg_index i = 0, ie = std::min<arg_index>(ns, arg_index(argTerms.size())); i != ie; ++i)
		{
			ValueClassSet w;
			WitnessKind wk = ArgWitnesses(u, argTerms[i], w);
			if (wk == WitnessKind::None)
				continue;
			auto argCls = m->GetArgClass(i);
			if (!argCls)
				continue;
			bool isUnitArg = argTerms[i].kind == DefType::Kind::UnitVal;
			ValueComposition knownComp = argTerms[i].kind == DefType::Kind::Data ? argTerms[i].vcomp : ValueComposition::Unknown;
			if (knownComp == ValueComposition::MultiPoint)
				knownComp = ValueComposition::Sequence; // folded onto one sequence class (§18.2)
			bool any = false;
			for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count) && !any; ++v)
				if (w.test(v))
					if (auto wc = ValueClass::FindByValueClassID(ValueClassID(v)))
						any = WitnessMatchesArgClass(wc, isUnitArg, argCls, knownComp);
			if (!any)
			{
				if (wk == WitnessKind::Concrete)
					return MemberVerdict::EliminatedConcrete;
				verdict = MemberVerdict::EliminatedFeasible;
			}
		}
		return verdict;
	}

	// K11b: consume an ArgContainer position -- the members a container argument
	// contributes are unified against the shared domain/values variables the
	// description declares, so a container that disagrees with another argument
	// bound to the same variable (e.g. discrete_alloc's allocUnit) is a
	// DEFINITION-time error instead of a reduction-time one.
	//
	// THE CONSUMED SET IS NAME-DIRECTED (review finding, reproduced). Operators read
	// a SUBSET of the container: discrete_alloc looks each suitability up by type
	// NAME (`GetConstSubTreeItemByID(gg->m_NameID)`), so a container carrying further
	// members -- a per-type weight, a regional helper -- is legitimate and already
	// exercised in tst (`source/Compacted/SuitabilityMaps` has 3 members for 2 type
	// names). An "every member" claim therefore FALSELY rejects working configs, as a
	// same-file control proved: the top-level call reduces while the function-body
	// call was rejected. So the claim applies to the NAMED members only, and only
	// when those names are definition-time knowable -- the `namesPos` string array,
	// evaluated exactly like the §12.7 for_each tranche evaluates its name arrays.
	// No evaluable name array ⇒ NO claim (defer), which is the honest verdict when
	// the member set is data-directed.
	//
	// Two argument shapes are typed: a structured/by-example CONTAINER PARAMETER
	// (K11a-4 already built its member map) and a definition-scope CONTAINER item
	// (members enumerated concretely here). Anything else -- expressions, generated
	// containers, closure captures -- defers exactly as before.
	void FunctionChecker::LinkContainerArg(const SignatureRecord::Pos& p, const DefType& argTerm, LispPtr argExpr,
		const TreeItem* refScope, const SharedStr& argSrc, LispPtr argsList,
		const std::function<SizeT(sig_var)>& VN, const std::function<SizeT(sig_var)>& DN)
	{
		if (p.domain == no_sig_var && p.values == no_sig_var)
			return; // a purely descriptive container position: nothing to link
		if (p.namesPos == arg_index(-1) || !refScope)
			return; // the consumed member set is unknown: claim nothing
		// the names array: CLOSED over the formals, or nothing is claimed
		LispPtr namesExpr;
		{
			arg_index j = 0;
			for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
				if (j == p.namesPos)
				{
					namesExpr = a.Left();
					break;
				}
		}
		if (namesExpr.EndP())
			return;
		auto names = EvalClosedStrArray(refScope, namesExpr);
		if (!names || names->empty())
			return; // data-directed member set: defer, exactly as before K11b
		std::shared_ptr<const std::map<TokenID, DefType>> members;
		bool fromExternal = false;
		if (argTerm.kind == DefType::Kind::Container && argTerm.members)
			members = argTerm.members;                       // a structured container parameter
		else if (argExpr.IsSymb())
		{
			// a definition-scope container reference: enumerate its declared members
			const TreeItem* local = nullptr; UInt32 paramIdx = 0;
			SharedTreeItem ext; ExtRefKind extKind = ExtRefKind::DefScopeExternal; SharedStr genSubPath;
			int code = 0;
			try {
				code = ResolveName(refScope, argExpr.GetSymbID(), &local, &paramIdx, &ext, &extKind, &genSubPath);
			}
			catch (...) {
				return; // an unresolvable argument is reported by its own walk
			}
			const TreeItem* c = nullptr;
			if (code == 2 && extKind == ExtRefKind::DefScopeExternal && ext)
				c = ext.get();
			else if (code == 1)
				c = local;                                    // a body-local container
			if (!c || !IsPlainContainer(c) || !c->_GetFirstSubItem() || !ExemplarMemberSetIsClosed(c))
				return;                                       // not an enumerable container: defer
			if (code == 2 && ext)
				m_Keep.push_back(ext);
			members = BuildConcreteContainerMembers(c);
			fromExternal = true;
		}
		if (!members)
			return;

		// A definition-scope container's members are CONCRETE, and definition-scope
		// externals deliberately DEFER (their types are checked per application) --
		// binding the operator's shared variable to a concrete member unit would pin
		// a rigid unit parameter and falsely reject a body that today type-checks
		// (verified: `attribute<V> x (cells) := SomeExternal;` is accepted precisely
		// because the external defers). So for an external argument only the
		// INTRA-container fact is claimed: the NAMED member attributes must agree with
		// EACH OTHER on one domain. A container PARAMETER's members carry variables,
		// so there the full cross-argument link runs -- the ∀ payoff.
		SharedStr refName; const DefType* refMember = nullptr;
		for (const auto& nm : *names)
		{
			// nm is DATA (a name-array entry): probe with GetExisting so an unmatched
			// name creates no registry entry -- an absent token cannot name a member
			TokenID nmTok = GetExistingTokenID_mt(nm.begin(), nm.send());
			auto it = IsDefined(nmTok) ? members->find(nmTok) : members->end();
			if (it == members->end())
				continue; // a named member the argument does not declare: reduction reports it
			const DefType& mt = it->second;
			if (mt.kind != DefType::Kind::Data)
				continue; // only member ATTRIBUTES carry the shared domain/values
			SharedStr memberSrc = mySSPrintF("member '{}' of {}", nm.c_str(), argSrc.c_str());
			if (fromExternal)
			{
				if (p.domain == no_sig_var || mt.dom == DefType::Dom::Unknown)
					continue;
				if (!refMember)
				{
					refName = nm; refMember = &mt;
					continue;
				}
				DefType a; a.kind = DefType::Kind::Data;
				a.dom = refMember->dom; a.dNode = refMember->dNode; a.domKeep = refMember->domKeep; a.domUnit = refMember->domUnit;
				DefType b; b.kind = DefType::Kind::Data;
				b.dom = mt.dom; b.dNode = mt.dNode; b.domKeep = mt.domKeep; b.domUnit = mt.domUnit;
				UnifyData(a, b, mySSPrintF("member '{}' of {}", refName.c_str(), argSrc.c_str()), memberSrc);
				continue;
			}
			if (p.domain != no_sig_var && mt.dom != DefType::Dom::Unknown)
			{
				DefType want; want.kind = DefType::Kind::Data;
				want.dom = DefType::Dom::Node; want.dNode = DN(p.domain);
				DefType got; got.kind = DefType::Kind::Data;
				got.dom = mt.dom; got.dNode = mt.dNode; got.domKeep = mt.domKeep; got.domUnit = mt.domUnit;
				UnifyData(got, want, memberSrc, argSrc);
			}
			if (p.values != no_sig_var && (mt.vc || mt.vUnit || mt.vNode != NO_TYPE_VAR))
			{
				DefType want; want.kind = DefType::Kind::Data;
				want.vNode = VN(p.values);
				DefType got; got.kind = DefType::Kind::Data;
				got.vc = mt.vc; got.vNode = mt.vNode; got.vKeep = mt.vKeep; got.vUnit = mt.vUnit;
				UnifyData(got, want, memberSrc, argSrc);
			}
		}
	}

	// §12.7: type a reference INTO a rule-computed member set -- 'r/foo' where
	// r's rule is a meta application (the pseudo-expanded for_each container)
	// or a cacheable composite whose record describes its sub-items
	// (u := unique(x); u/Values -- the slSubItemCall tranche). An exact hit
	// (case-insensitive, like the engine's item lookup) yields the member's
	// type, a path prefix of a deeper member is an intermediate container (no
	// claim), a path BELOW a member defers (members may carry deeper
	// sub-structure), and a complete-set miss is reported honestly -- sound
	// because meta rules reject every inline member access and described-
	// complete cache results make SubItemOperator certain to reject the same
	// reference per application. Everything else defers as before.
	DefType FunctionChecker::InferGeneratedMember(TokenID sym, const TreeItem* genItem, const SharedStr& subPath)
	{
		DefType ct = InferBodyItem(genItem);
		if (!ct.members)
			return {};
		TokenID subTok = GetExistingTokenID_mt(subPath.begin(), subPath.send());
		if (IsDefined(subTok))
			if (auto it = ct.members->find(subTok); it != ct.members->end())
				return it->second;
		auto ciEq = [](char x, char y) { return AsciiTokenFold(x) == AsciiTokenFold(y); };
		SizeT sn = subPath.ssize();
		for (const auto& [pathTok, mt] : *ct.members)
		{
			SharedStr path(pathTok.AsStrRangeLock()); // materialized key text for the prefix tests
			SizeT pn = path.ssize();
			if (pn > sn && *(path.begin() + sn) == DELIMITER_CHAR
				&& std::equal(subPath.begin(), subPath.send(), path.begin(), ciEq))
				return {}; // an intermediate container on the way to a deeper member
			if (sn > pn && *(subPath.begin() + pn) == DELIMITER_CHAR
				&& std::equal(path.begin(), path.send(), subPath.begin(), ciEq))
				return {}; // a path BELOW a member: members may carry deeper
				           // sub-structure the member set makes no claim about
		}
		if (!ct.membersComplete)
			return {};
		if (ct.members->empty())
			throwErrorF("ExprParser", "'{}': the calculation rule of '{}' generates no members, so '{}' cannot exist (body of function '{}')"
				, sym, genItem->GetFullName().c_str()
				, subPath.c_str(), m_FuncItem->GetFullName().c_str());
		SharedStr listed; UInt32 nrListed = 0;
		for (const auto& [pathTok, mt] : *ct.members)
		{
			if (nrListed == 10)
			{
				listed += ", ...";
				break;
			}
			if (nrListed++)
				listed += ", ";
			listed += SharedStr(pathTok.AsStrRangeLock());
		}
		throwErrorF("ExprParser", "'{}': the calculation rule of '{}' generates member(s) {}; '{}' is not among them (body of function '{}')"
			, sym, genItem->GetFullName().c_str()
			, listed.c_str(), subPath.c_str(), m_FuncItem->GetFullName().c_str());
	}

	DefType FunctionChecker::InferOperatorApplication(const AbstrOperGroup* og, TokenID headID, const TreeItem* refScope, LispPtr argsList)
	{
		std::vector<DefType> argTerms;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right())
			argTerms.push_back(InferExpr(refScope, a.Left()));

		if (!og->MustCacheResult())
		{
			// §12.7 for_each tranche: a container-GENERATING meta application
			// whose meta-directing arguments are CLOSED over the formals is
			// processed at definition scan (the ruling); every failure inside
			// falls through to the wholesale deferral, byte-identical to before
			if (auto containerType = TryMetaContainerProcessing(og, headID, refScope, argsList, argTerms))
				return *containerType;
			return {}; // meta/selection groups: fluid effective arity, per-application checking
		}

		auto sigs = og->GetSignatures();
		if (!sigs)
			return {}; // no member describes itself: defer, as before the description layer

		arg_index nrArgs = arg_index(argTerms.size());
		Int32 theRecord = -2; // -2: no class survivor yet; -1: mixed/undescribed -> defer
		bool anyAritySurvivor = false, anyFeasibleOnlyElimination = false;
		std::vector<const Operator*> survivors; // §12.7: the members a spec-record may derive from
		std::vector<Int32> survivorRecords;     // §6.2 cross-record fallback: the DISTINCT records that survive
		for (const auto& me : sigs->members)
		{
			if (!MemberAcceptsArity(og, me.oper, nrArgs))
				continue;
			anyAritySurvivor = true;
			auto mv = ClassifyMember(m_Unifier, me.oper, argTerms);
			if (mv != MemberVerdict::Survives)
			{
				if (mv == MemberVerdict::EliminatedFeasible)
					anyFeasibleOnlyElimination = true;
				continue;
			}
			survivors.push_back(me.oper);
			if (std::find(survivorRecords.begin(), survivorRecords.end(), me.recordIdx) == survivorRecords.end())
				survivorRecords.push_back(me.recordIdx);
			if (theRecord != -1)
			{
				if (me.recordIdx < 0)
					theRecord = -1;
				else if (theRecord == -2)
					theRecord = me.recordIdx;
				else if (theRecord != me.recordIdx)
					theRecord = -1;
			}
		}
		if (!anyAritySurvivor)
			return {}; // arity outside every member: defer (a same-named function or FindOper's own widening may serve)

		SharedStr headName(headID.AsStrRangeLock()); // materialized: no TokenStr may span the unification calls
		if (theRecord == -2)
		{
			// every member rejected the known argument classes. Members rejected by a
			// concrete class fail at reduction with certainty; a member rejected only
			// through a feasible SET is symbolic knowledge, so the verdict defers
			// (soft support: the ∀-variable is not rejected).
			if (anyFeasibleOnlyElimination)
				return {};
			throwErrorF("ExprParser", "{}: the argument types of operator '{}' do not match any of its registered overloads"
				, m_Unifier.FullName().c_str(), headName.c_str());
		}
		if (theRecord < 0)
		{
			// §6.2 cross-record fallback: several congruence classes survive, so no
			// single record's VALUE claims apply -- but they may still AGREE on the
			// DOMAIN skeleton, and whichever member reduction picks, that part holds.
			// This un-gates the combining operators: `add`/`+` splits into three
			// records (two polygon families + the scalar family), all of which say
			// "both arguments and the result share ONE domain", so
			// `pcount(nw/F1) + pcount(nw/F2)` over different node units is now a
			// DEFINITION-time conflict instead of an instantiation-time one.
			if (auto skeleton = BuildDomainSkeletonRecord(sigs, survivorRecords))
				return ApplyOperRecord(*skeleton, headName, argTerms, refScope, argsList); // K11 leftover: container positions link here too
			return {}; // no agreed skeleton (or an undescribed member survives): defer
		}

		const auto& mr = sigs->records[theRecord];
		// §12.7: a DynamicShape record with a definition-time-knowable spec is
		// upgraded to the spec-derived concrete record; every failure inside
		// falls through to the DynamicShape deferral, byte-identical to today
		if (mr.shape.dynamicShape)
			if (auto specType = TrySpecProcessing(og, mr, survivors, headName, refScope, argsList, argTerms))
				return *specType;
		return ApplyOperRecord(mr, headName, argTerms, refScope, argsList); // K11b: enable ArgContainer linking
	}

	DefType FunctionChecker::InferExpr(const TreeItem* refScope, LispPtr expr)
	{
		if (expr.EndP())
			return {};
		if (!expr.IsRealList())
		{
			if (expr.IsSymb())
			{
				TokenID sym = expr.GetSymbID();
				if (sym == t_Hole || token::isConst(sym) || ValueClass::FindByScriptName(sym))
					return {};
				const TreeItem* local = nullptr; UInt32 paramIdx = 0;
				SharedStr genSubPath;
				ExtRefKind extKind = ExtRefKind::DefScopeExternal;
				switch (ResolveName(refScope, sym, &local, &paramIdx, nullptr, &extKind, &genSubPath))
				{
				case 0: return ParamType(paramIdx);
				case 1: return local ? InferBodyItem(local) : DefType{};
				case 2:
					// K11a-2: structured-parameter member access. Code 2 is OVERLOADED
					// (K11a-3 review finding): prelude refs, closure captures and
					// def-scope externals also return 2 WITHOUT touching paramIdx/
					// genSubPath -- only a genuine ParamMember may reach the member map
					// (the stale defaults falsely hit parameter 0 with an empty path)
					if (extKind == ExtRefKind::ParamMember)
						return InferParamMember(paramIdx, genSubPath);
					return {}; // prelude/capture/external: checked per application
				case 3: return local ? InferGeneratedMember(sym, local, genSubPath) : DefType{}; // §12.7: rule-generated member access
				default: return {}; // imports/externals: their types are checked per application
				}
			}
			return {}; // numeric / string literals: void-domain constants, class per application
		}

		TokenID headID = expr.Left().GetSymbID();
		if (headID == token::sourceDescr)
			return InferExpr(refScope, expr.Right().Left());
		if (headID == token::arrow || headID == token::scope || headID == token::subitem)
			throwErrorF("ExprParser", "the '{}' construct is not yet supported inside inlined function bodies"
				"; bind the function application to a container to use the instantiating form"
				, headID);

		// §5.10 applied call result: type the application when the inner value's
		// signature is known; residual arity is verified at reduction
		if (headID == t_ApplyValue)
		{
			DefType fnVal = InferArg(refScope, expr.Right().Left());
			return InferApplication(refScope, fnVal, expr.Right().Right(), "(...)");
		}

		const AbstrOperGroup* og = AbstrOperGroup::FindName(headID);
		if (og->IsTemplateCall() && !ValueClass::FindByScriptName(headID)) // value-type heads (float64(x)) are conversions, not function calls
		{
			if (headID == t_Map)
				throwErrorF("ExprParser", "map(...) can only appear as a whole calculation rule, not as a sub-expression");
			// the head name is materialized BEFORE the recursive walk: a TokenStr
			// temporary holds the token registry's shared lock, and the walk below
			// parses body expressions (token creation needs the exclusive lock)
			SharedStr headName(headID.AsStrRangeLock());
			for (UInt32 i = 0, n = m_Params.size(); i != n; ++i)
				if (m_Params[i]->GetNameID() == headID)
					return InferApplication(refScope, ParamType(i), expr.Right(), headName.c_str());

			// a direct function/import call: resolve, then type the application
			auto callee = m_FuncItem->ResolveItemPath(SharedStr(headID.AsStrRangeLock()));
			if (!callee || !callee->IsFunctionItem())
				if (auto defParent = m_FuncItem->GetTreeParent()) // lexical definition scope (§4.6)
					if (auto lex = defParent->ResolveItemPath(SharedStr(headID.AsStrRangeLock())); lex && lex->IsFunctionItem())
						callee = lex;
			if (!callee || !callee->IsFunctionItem())
				if (auto pf = FindPreludeFunction(headID); pf && pf->IsFunctionItem())
					callee = pf; // prelude: implicit outermost namespace for call heads
			// A lexical lookup that landed on a NON-function item -- typically the
			// enclosing function's function-valued parameter, which became visible here
			// once the definition namespace was injected -- must still fall through to
			// the closure environment. Testing only for a null callee misreports such a
			// head as a template instantiation.
			if (!callee || !callee->IsFunctionItem())
				if (auto env = FindEnclosingFunctionMember(headID))
				{
					// an enclosing function's parameter or local applied as a function:
					// bound through the closure environment at reduction; a declared
					// signature (an exemplar-cloned parameter) still types the call
					DefType envVal; envVal.kind = DefType::Kind::Func;
					envVal.fn = env->IsFunctionItem() ? env.get() : nullptr;
					m_Keep.push_back(env);
					return InferApplication(refScope, envVal, expr.Right(), headName.c_str());
				}
			if (!callee)
				throwErrorF("ExprParser", "'{}': unknown operator or function in body of function '{}'"
					, headName.c_str(), m_FuncItem->GetFullName().c_str());
			if (!callee->IsFunctionItem())
				throwErrorF("ExprParser", "'{}': template instantiations are not supported inside function bodies"
					, headName.c_str());
			DefType calleeVal; calleeVal.kind = DefType::Kind::Func; calleeVal.fn = callee.get();
			m_Keep.push_back(callee);
			return InferApplication(refScope, calleeVal, expr.Right(), headName.c_str());
		}

		// a value-class head is a conversion: the class is the head's, the domain
		// follows the (single) argument
		if (auto convVC = ValueClass::FindByScriptName(headID))
		{
			DefType argT; UInt32 n = 0;
			for (LispPtr a = expr.Right(); !a.EndP(); a = a.Right(), ++n)
				argT = InferExpr(refScope, a.Left());
			if (n != 1)
				return {};
			DefType r; r.kind = DefType::Kind::Data; r.vc = convVC;
			if (argT.kind == DefType::Kind::Data)
			{
				r.dom = argT.dom; r.domUnit = argT.domUnit; r.domKeep = argT.domKeep; r.dNode = argT.dNode;
				r.vcomp = argT.vcomp; // a value conversion preserves the geometric composition
			}
			return r;
		}

		// built-in operator: the group's described signature records (batch A)
		// type the application; groups without described members walk their
		// arguments and defer the result. Memoized per (refScope, interned expr)
		// so repeated applications -- crucially fresh-unit ones like unique(a) /
		// select(c) -- share ONE result node (batch D, §6.1). An error throws
		// (never cached); a cache hit skips re-walking the (identical) arguments,
		// whose own checks already ran on the first occurrence
		auto memoKey = std::make_pair(refScope, LispRef(expr));
		if (auto it = m_ApplTypes.find(memoKey); it != m_ApplTypes.end())
			return it->second;
		DefType applResult = InferOperatorApplication(og, headID, refScope, expr.Right());
		m_ApplTypes.emplace(std::move(memoKey), applResult);
		return applResult;
	}

	DefType FunctionChecker::InferBodyItem(const TreeItem* refItem)
	{
		if (auto it = m_ItemTypes.find(refItem); it != m_ItemTypes.end())
			return it->second;
		if (!m_InProgress.insert(refItem).second)
			return {}; // cycle guard; true circularity is caught by the reduction
		DefType inferred;
		SharedStr exprStr = refItem->GetExpr();
		if (!exprStr.empty())
		{
			if (AbstrCalculator::MustEvaluate(exprStr.c_str()))
				throwErrorF("ExprParser", "'{}': leading-'=' string indirection is not supported inside function bodies"
					, refItem->GetFullName().c_str());
			auto calc = AbstrCalculator::ConstructFromStr(refItem, exprStr, CalcRole::Calculator);
			auto refScope = refItem->GetTreeParent();
			inferred = InferExpr(refScope.get(), RewriteExpr(calc->GetLispExprOrg()));
		}
		DefType declared = DeclaredItemType(refItem);
		if (declared.kind != DefType::Kind::Unknown && inferred.kind != DefType::Kind::Unknown)
		{
			SharedStr itemName(refItem->GetNameID().AsStrRangeLock()); // TokenStr must not span UnifyData (token-registry lock)
			SharedStr ruleSrc = mySSPrintF("the calculation rule of '{}'", itemName.c_str());
			SharedStr declSrc = mySSPrintF("the declared type of '{}'", itemName.c_str());
			UnifyData(inferred, declared, ruleSrc, declSrc);
		}
		DefType itemType = declared.kind != DefType::Kind::Unknown ? declared : inferred;
		m_InProgress.erase(refItem);
		m_ItemTypes[refItem] = itemType;
		return itemType;
	}

	void CheckFunctionDefinition(const TreeItem* funcItem)
	{
		if (TreeItem_IsFunctionDefinitionChecked(funcItem))
		{
			// The definition was already checked. A wrong definition is a persistent error whose
			// verdict is recorded ON THE FUNCTION ITEM (below) -- re-raise it so this application
			// fails too, WITHOUT re-running the (failing) check at every application.
			if (funcItem->WasFailed(FailType::MetaInfo))
				funcItem->ThrowFail();
			return;
		}
		if (TreeItem_IsFunctionVariantSet(funcItem))
			return; // each variant is checked at its own application

		// §12.7: closed-spec evaluation can UpdateMetaInfo definition-scope items
		// whose expressions apply the function CURRENTLY being checked, and the
		// checked-flag is set only on completion -- guard against re-entry (the
		// outer invocation completes the verdict). Meta-thread-only, like the
		// whole checker, so a plain static set suffices
		static std::set<const TreeItem*> s_CheckInProgress;
		if (!s_CheckInProgress.insert(funcItem).second)
			return;
		struct Eraser
		{
			std::set<const TreeItem*>* s; const TreeItem* f;
			~Eraser() { s->erase(f); }
		} eraser{ &s_CheckInProgress, funcItem };
		try
		{
			TokenID resultName = TreeItem_GetFunctionResultName(funcItem);
			auto resultChild = funcItem->GetConstSubTreeItemByID(resultName);
			if (!resultChild)
				throwErrorF("ExprParser", "'{}': designated result '{}' not found"
					, funcItem->GetFullName().c_str(), resultName);
			if (!resultChild->GetExpr().empty()) // signature-only functions and nested-function results have no body expression here
			{
				FunctionChecker chk;
				chk.m_FuncItem = funcItem;
				chk.m_Unifier.m_ApplItem = funcItem;
				chk.m_Unifier.m_Phase = "the definition of ";
				chk.m_DeclSource = mySSPrintF("declared by function '{}'", funcItem->GetFullName().c_str());
				UInt32 nrParams = TreeItem_GetFunctionParamCount(funcItem);
				const TreeItem* p = funcItem->_GetFirstSubItem();
				for (UInt32 i = 0; i < nrParams && p; ++i, p = p->GetNextItem())
					chk.m_Params.push_back(p);
				chk.InferBodyItem(resultChild.get());
			}
		}
		catch (...)
		{
			// Record the verdict ON THE FUNCTION DEFINITION and cache it: a wrong definition
			// becomes a failed item (so it shows as failed, whether the check was triggered by
			// an application or by the audit), and every subsequent application re-raises the
			// same verdict (above) rather than re-running the failing check. CatchFail captures
			// the in-flight exception with its context; the re-throw fails the caller too.
			funcItem->CatchFail(FailType::MetaInfo);
			TreeItem_SetFunctionDefinitionChecked(funcItem);
			throw;
		}
		TreeItem_SetFunctionDefinitionChecked(funcItem);
	}


} // namespace hof
