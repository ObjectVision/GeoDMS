// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Internal to the calculator component: the definition-time type term (DefType) and the
 *  walker that derives one for every body expression (FunctionChecker). Split over two
 *  translation units -- HofTypeChecker.cpp (name and expression inference) and
 *  HofOperSignatureInfer.cpp (how an operator group's signature records type an
 *  application) -- which is the only reason these are declared in a header.
 *
 *  Not part of the Tic interface -- do not include this outside rtc/dll/src/tic.
 */

#if !defined(__TIC_HOFDEFTYPE_H)
#define __TIC_HOFDEFTYPE_H

#include "HofTypeUnifier.h"

#include "OperSignature.h"

#include <optional>

namespace hof {

	// §12.7: generated/computed member maps are keyed by the TokenID of the full
	// relative member path -- TreeItem lookup (and hence SubItemOperator's
	// GetCurrItem) accepts either case (with a deprecation warning), and token
	// equality IS the tree's fixed-ASCII case folding, so keying by token gives
	// the case-insensitive map for free (a hand-rolled folding comparator used
	// to duplicate it here). Look up with GetExistingTokenID: every key was
	// interned when the map was built, so an absent token cannot name a present
	// member (and data-derived probe names then create no registry entries).
	// The fold survives only for the string-PREFIX scans over materialized key
	// text, which token identity cannot express; FIXED ASCII, matching token
	// equality -- locale-dependent folding (stricmp/tolower) could diverge
	// under a changed CRT locale (review finding)
	inline char AsciiTokenFold(char ch) { return (ch >= 'A' && ch <= 'Z') ? char(ch - 'A' + 'a') : ch; }

	// the definition-time type of a body expression (kinds-level, §5 terms)
	struct DefType
	{
		enum class Kind : UInt8 { Unknown, Data, UnitVal, Func, Container } kind = Kind::Unknown;

		// value position (Data: the values class; UnitVal: the unit's class):
		// concrete class XOR unifier node XOR unknown
		const ValueClass* vc = nullptr;
		SizeT vNode = NO_TYPE_VAR;

		// value composition of a Data term when known (§18.4); Unknown = no claim.
		// Consumed by candidate elimination only (a Single-composition argument
		// cannot serve a sequence-registered member and vice versa)
		ValueComposition vcomp = ValueComposition::Unknown;

		// values-unit IDENTITY of a Data term (batch U, §8): a unit node when the
		// declared values token names a unit parameter or domain-sorted generic
		// (the function-signature K2 bridge -- the SAME node its domain role uses),
		// or the concrete scope unit it resolves to. Class reasoning stays on
		// vc/vNode; identity is compared by UnifyData's values-identity block.
		// Neither set = no identity claim (defers)
		SizeT vuNode = NO_TYPE_VAR;
		const AbstrUnit* vUnit = nullptr; SharedTreeItem vKeep;

		// domain position (Data: the domain; UnitVal: the unit's own identity)
		enum class Dom : UInt8 { Unknown, Void, Concrete, Node } dom = Dom::Unknown;
		const AbstrUnit* domUnit = nullptr; SharedTreeItem domKeep; // Concrete
		SizeT dNode = NO_TYPE_VAR;                                  // Node

		// Func: the function or signature item whose declared positions type an
		// application of this value; tokens naming varsOwner's generic variables
		// resolve to (varsOwner, instance) nodes; tok2owner adds the type-application
		// translation (sig var -> varsOwner var)
		const TreeItem* fn = nullptr;
		const TreeItem* varsOwner = nullptr;
		UInt32 instance = 0;
		std::shared_ptr<std::map<TokenID, TokenID>> tok2owner;

		// K11 member map (§12.7): the member set of a composite result -- the
		// pseudo-expanded members of a container-GENERATING meta application
		// (for_each, Kind::Container) OR the DESCRIBED sub-items of a
		// cacheable operator's cache result (unique/Values & co., any kind;
		// slSubItemCall tranche) -- keyed by the TokenID of the sub-item PATH (a
		// name-array entry may contain '/'); token equality is case-folded, as
		// TreeItem lookup is. membersComplete marks a definitively-known set --
		// the only state in which a missing member may be reported (sound: meta
		// rules reject every inline member access; described-complete cache
		// results make SubItemOperator certain to reject the same reference)
		std::shared_ptr<const std::map<TokenID, DefType>> members;
		bool membersComplete = false;
	};

	struct FunctionChecker
	{
		const TreeItem*              m_FuncItem = nullptr;
		std::vector<const TreeItem*> m_Params;
		std::set<const TreeItem*>    m_InProgress;

		TypeUnifier                  m_Unifier;
		SharedStr                    m_DeclSource;
		UInt32                       m_NextInstance = 1; // 0 = the checked function's own (rigid) variables
		std::map<const TreeItem*, DefType> m_ItemTypes;  // memoized body-item types
		// batch D (§6.1): memoize operator-application results per (refScope,
		// hash-consed application expr). LispRefs are interned, so two textually
		// identical applications share one LispObj; keying on it makes repeated
		// subexpressions denote ONE result node -- a K6 SOUNDNESS prerequisite (two
		// `unique(a)`/`select(c)` occurrences reduce to a single DataController, so
		// their fresh existential units must be the same node), and a diagnostics
		// de-duplicator for every repeated subexpression as a bonus. refScope is in
		// the key because symbol resolution inside the application depends on it.
		// The key holds a STRONG LispRef (§12.7 review): the map entry itself pins
		// the interned node, so the pointer-ordered key can never dangle or ABA
		std::map<std::pair<const TreeItem*, LispRef>, DefType> m_ApplTypes;
		// K11a-3.1 review: memoize the checked function's own parameter types -- every
		// nw/member reference re-derived ParamType (and for a structured parameter
		// rebuilt the whole member map incl. per-member FindItem scope walks). Nodes
		// are get-or-create so this only removes rework, never changes a verdict.
		std::map<UInt32, DefType>    m_ParamTypes;
		std::vector<SharedTreeItem>  m_Keep;             // liveness of resolved callees

		DefType InferBodyItem(const TreeItem* refItem);
		DefType InferExpr(const TreeItem* refScope, LispPtr expr);
		DefType InferArg(const TreeItem* refScope, LispPtr argExpr);
		DefType InferApplication(const TreeItem* refScope, const DefType& fnVal, LispPtr argsList, CharPtr headName);
		DefType InferOperatorApplication(const AbstrOperGroup* og, TokenID headID, const TreeItem* refScope, LispPtr argsList); // op-sig batch A
		DefType ApplyOperRecord(const OperGroupSignatures::MergedRecord& mr, const SharedStr& headName, const std::vector<DefType>& argTerms
			, const TreeItem* refScope = nullptr, LispPtr argsList = LispPtr()); // K11b: refScope+argsList enable ArgContainer member enumeration
		// §6.2 cross-record fallback: the DOMAIN skeleton all surviving records agree on
		std::optional<OperGroupSignatures::MergedRecord> BuildDomainSkeletonRecord(const OperGroupSignatures* sigs, const std::vector<Int32>& recordIdxs);
		DefType ParamType(UInt32 idx);     // memoized front (m_ParamTypes)
		DefType ParamTypeImpl(UInt32 idx);
		// K11a-1: the declared member set of a structured unit parameter (a member
		// block on a `unit<...> P { … }` parameter), each member typed by its declared
		// value class over the parameter's own domain. Enables def-time typing of
		// `P/member` access (InferExpr case 2 / InferParamMember).
		std::shared_ptr<const std::map<TokenID, DefType>>
			BuildParamMembers(const TreeItem* p, SizeT paramDomNode, const TreeItem* memberSrc = nullptr);
		// K11b: the CONCRETE members of a definition-scope container argument
		std::shared_ptr<const std::map<TokenID, DefType>>
			BuildConcreteContainerMembers(const TreeItem* c);
		// K11b: link an ArgContainer position's shared domain/values against the actual members
		void LinkContainerArg(const SignatureRecord::Pos& p, const DefType& argTerm, LispPtr argExpr,
			const TreeItem* refScope, const SharedStr& argSrc, LispPtr argsList,
			const std::function<SizeT(sig_var)>& VN, const std::function<SizeT(sig_var)>& DN);
		DefType InferParamMember(UInt32 paramIdx, const SharedStr& memberPath);
		DefType DeclaredItemType(const TreeItem* item);
		DefType PositionType(const TreeItem* posItem, const TreeItem* fnDef, UInt32 instance,
			const TreeItem* ownerFn, UInt32 ownerInstance, const std::map<TokenID, TokenID>* tok2owner,
			const TreeItem* itemScope = nullptr);
		void UnifyData(const DefType& a, const DefType& b, const SharedStr& srcA, const SharedStr& srcB);

		// unifier nodes; the checked function's own variables (instance 0) are rigid.
		// A rigid variable may lexically belong to an ENCLOSING function (nested
		// declarations see the enclosing type-parameter clause) -- seed its constraint
		// from that declaration when the checked function's own lists lack it.
		SizeT ValNode(const TreeItem* owner, UInt32 inst, TokenID tok)
		{
			bool rigid = owner == m_FuncItem && inst == 0;
			TokenID fallback;
			if (rigid && !DeclaredConstraintOf(m_FuncItem, tok))
				for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
					if (TokenID c = DeclaredConstraintOf(enc.get(), tok))
					{
						fallback = c;
						break;
					}
			return m_Unifier.ValueVar(owner, inst, tok
				, rigid ? m_DeclSource : mySSPrintF("function '{}'", owner->GetFullName().c_str()), rigid, fallback);
		}
		SizeT UNode(const TreeItem* owner, UInt32 inst, TokenID tok, const ValueClass* declaredCls = nullptr)
		{
			// the token may name a unit PARAMETER of the owner whose declared class
			// must pin the companion regardless of WHICH path creates the node
			// first -- type applications and sig bindings reach here before
			// ParamType does, and creation is get-or-create-once (review finding:
			// an unpinned first creation froze the companion rigid/unconstrained,
			// making the verdict depend on the body's reference order)
			if (!declaredCls && owner && owner->IsFunctionItem())
			{
				const TreeItem* q = owner->_GetFirstSubItem();
				for (UInt32 j = 0, n = TreeItem_GetFunctionParamCount(owner); j != n && q; ++j, q = q->GetNextItem())
					if (q->GetNameID() == tok && IsUnit(q))
					{
						declaredCls = AsUnit(q)->GetValueType();
						break;
					}
			}
			// the companion class node needs the SAME enclosing-declaration
			// constraint fallback as ValNode: a rigid domain variable lexically
			// belonging to an ENCLOSING function (named here only through a type
			// application) must not seed an all-set acceptance -- the t3 defect-#3
			// rule, re-applied to the batch-U companions
			bool rigid = owner == m_FuncItem && inst == 0;
			TokenID fallback;
			if (rigid && !declaredCls && !DeclaredConstraintOf(m_FuncItem, tok))
				for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
					if (TokenID c = DeclaredConstraintOf(enc.get(), tok))
					{
						fallback = c;
						break;
					}
			return m_Unifier.UnitVar(owner, inst, tok, rigid, declaredCls, fallback);
		}
		SharedTreeItem ResolveUnitInScope(TokenID tok, const TreeItem* fnDef)
		{
			SharedStr s(tok.AsStrRangeLock());
			auto u = fnDef->ResolveItemPath(s);
			if (!u || !IsUnit(u.get()))
				if (auto defP = fnDef->GetTreeParent()) // lexical definition scope (§4.6)
					u = defP->ResolveItemPath(s);
			if (u && IsUnit(u.get()) && !u->InTemplate()) // body-local units stay deferred (their meta info is not available here)
				return u;
			return {};
		}
		// a nested function's body may reference the ENCLOSING function's parameters
		// and locals; those are bound through the closure environment at reduction and
		// have no definition-time value here -- resolve them only to defer them
		SharedTreeItem FindEnclosingFunctionMember(TokenID tok)
		{
			for (auto enc = m_FuncItem->GetTreeParent(); enc && enc->IsFunctionItem(); enc = enc->GetTreeParent())
				if (auto c = enc->GetConstSubTreeItemByID(tok))
					return c;
			return {};
		}
		// a body-local declaration in a sub-container between the declaring item and
		// the function root shadows outer names at reduction; its meta info is not
		// available at definition time, so such declared-type tokens must defer
		bool HasBodyShadower(TokenID tok, const TreeItem* fromScope)
		{
			for (const TreeItem* scope = fromScope; scope; scope = scope->GetTreeParent().get())
			{
				if (scope == m_FuncItem)
					return false; // direct children are seen (and deferred) by ResolveUnitInScope
				if (scope->GetConstSubTreeItemByID(tok))
					return true;
			}
			return false;
		}

		// 0=parameter (index via *paramIdx), 1=local (via *local), 2=import/external; throws on unknown.
		// §12.7: the bare code 2 conflates CLOSED and OPEN references -- extKindPtr
		// discriminates (see ExtRefKind); externalOut receives the resolved item for
		// the DefScopeExternal case (the only evaluable one).
		// 3 (only when genSubPathOut is passed) = a path miss below an item whose
		// rule may GENERATE sub-items at instantiation (§12.7 for_each tranche):
		// *local is that generating item, *genSubPathOut the remaining path -- the
		// caller resolves it against the container's pseudo-expanded member set
		enum class ExtRefKind : UInt32 { DefScopeExternal = 0, ParamMember, PreludeFunc, ClosureCapture };
		int ResolveName(const TreeItem* refScope, TokenID sym, const TreeItem** local, UInt32* paramIdx = nullptr,
			SharedTreeItem* externalOut = nullptr, ExtRefKind* extKindPtr = nullptr, SharedStr* genSubPathOut = nullptr);

		// §12.7 impedance tranche: definition-time K13 spec processing. A body
		// sub-expression CLOSED over the formals (references no parameter, rest
		// slice, closure capture, or parameter member -- transitively through body
		// locals; externals terminate the scan since formals are lexically
		// invisible outside the function) is REDUCED to its DataController key --
		// the same hash-consed key every application interns (β-substitution is
		// the identity on closed expressions) -- and EVALUATED at definition scan,
		// storage-backed sources included (the explicit ruling). Any failure at
		// any stage yields nullopt/empty = defer, exactly as without the spec.
		std::map<std::pair<const TreeItem*, LispRef>, LispRef> m_ClosedKeyMemo; // strong key AND value LispRefs: nothing dangles
		std::set<const TreeItem*> m_ScanBusy; // cycle guard for body-local recursion (distinct from m_InProgress)
		LispRef TryBuildClosedKeyExpr(const TreeItem* refScope, LispPtr expr);
		LispRef TryBuildClosedKeyExprImpl(const TreeItem* refScope, LispPtr expr);
		std::optional<SharedStr> EvalClosedSpec(const TreeItem* refScope, LispPtr specExpr);
		std::optional<DefType> TrySpecProcessing(const AbstrOperGroup* og,
			const OperGroupSignatures::MergedRecord& mr, const std::vector<const Operator*>& survivors,
			const SharedStr& headName, const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms);

		// §12.7 for_each tranche: the container-generating meta family. A
		// closed name array (and, for for_each_ind, the closed field spec) is
		// EVALUATED at definition scan -- storage-backed sources included, per
		// the ruling -- and the application types as a Container whose member
		// set is the pseudo-expansion of the names; member domain/values types
		// come from the layout's unit positions (a formal unit parameter
		// contributes its unifier node: the K2 bridge). Any failure at any
		// stage defers exactly as before the tranche.
		std::optional<std::vector<SharedStr>> EvalClosedStrArray(const TreeItem* refScope, LispPtr expr);
		std::optional<DefType> TryMetaContainerProcessing(const AbstrOperGroup* og, TokenID headID,
			const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms);
		DefType InferGeneratedMember(TokenID sym, const TreeItem* genItem, const SharedStr& subPath);
	};

} // namespace hof

#endif // __TIC_HOFDEFTYPE_H
