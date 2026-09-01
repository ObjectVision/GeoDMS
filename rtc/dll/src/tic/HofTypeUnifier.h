// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Internal to the calculator component: the unification store over an application's
 *  type variables, and the contracts a bound argument must satisfy against a declared
 *  signature or a structured parameter. Both the reduction (HofApplication.cpp) and the
 *  definition-time checker (HofTypeChecker.cpp, HofOperSignatureInfer.cpp) run over
 *  this store, which is why it lives in a header rather than in one of them.
 *
 *  Not part of the Tic interface -- do not include this outside rtc/dll/src/tic.
 */

#if !defined(__TIC_HOFTYPEUNIFIER_H)
#define __TIC_HOFTYPEUNIFIER_H

#include "HofClosure.h"

#include "mci/ValueClassID.h"

#include <bitset>
#include <optional>

class AbstrUnit;

namespace hof {

	// ---- WP4.1 tranche 2: unification store over an application's type variables ----
	//
	// Robinson unification specialized to the shallow type terms of §5: value-class
	// variables and domain variables (concrete units are opaque, compared by
	// UnifyDomain -- key identity, §2). Variables are identified by (owner function,
	// name), so a signature-typed parameter can LINK the bound generic function's OWN
	// variable to one of the applied function's variables -- a genuine
	// variable-variable link, kept as a union-find equivalence class. Every class
	// carries at most one concrete binding and, for value variables, the intersection
	// of all member constraints as an acceptance set over the closed value-class
	// universe (the §5.7 v2 mechanism), so conflicts surface at the application that
	// creates them -- with attribution -- even when no member of the class is ever
	// bound concretely.

	using ValueClassSet = std::bitset<UInt32(ValueClassID::VT_Count)>;

// the acceptance set of a named generic constraint over the closed value-class universe
ValueClassSet GenericConstraintSet(TokenID constraintName);

// the declared constraint of `var`: fn's ordered type-variable list, else the
// generic-parameter records (which carry the same `<var: constraint>` pairs)
TokenID DeclaredConstraintOf(const TreeItem* fn, TokenID var);

	constexpr SizeT NO_TYPE_VAR = SizeT(-1);

	struct TypeUnifier
	{
		// tranche 3: variables are additionally keyed by an INSTANCE number, so every
		// binding/instantiation of a generic function gets its own copies of that
		// function's variables (two independent bindings of one generic function must
		// not link through a shared node). Instance 0 with owner == the checked
		// function marks that function's own variables; the definition-time walker
		// creates those as RIGID (skolem) variables: a rigid variable must hold for
		// EVERY instantiation, so it can never be bound to a concrete type, forced
		// equal to another rigid variable, or narrowed below its declared constraint.
		const TreeItem* m_ApplItem = nullptr; // the applied/checked function, for error attribution
		CharPtr m_Phase = "";                 // "" at application; "the definition of " at definition time

		// soft: an operator-support set (derived from the group's registered members,
		// OperSignature.h). A concrete class outside the set is a definition-time
		// error (reduction would find no member), but soft sets never narrow or
		// reject a rigid ∀-variable and stay out of `feasible`: operator support is
		// not a declared promise -- an unsupported instantiation fails at its own
		// reduction (S1), and the prelude's <T: any> null-aware predicates over
		// eq/lt depend on passing through them symbolically.
		struct ConstraintRec { TokenID name, constraint; SharedStr source; ValueClassSet set; bool soft = false; SharedStr setText = {}; };
		struct ValueNode
		{
			SizeT parent; // union-find: parent == own index at a root
			TokenID name; // the user-visible variable name (roots keep the rigid/outer one)
			bool rigid = false;
			const ValueClass* bound = nullptr;
			SharedStr boundSource;
			ValueClassSet feasible; // invariant: the intersection of all constraint sets
			std::vector<ConstraintRec> constraints;
		};
		// batch U (§8): the former DomainNode, generalized to ONE pool of unit-
		// identity nodes serving BOTH roles a unit can play -- the domain of a data
		// term AND (new) the values-unit identity of a data term. A unit variable
		// (unit parameter, domain-sorted generic) used in a values position of one
		// signature slot and a domain position of another therefore flows through a
		// single node -- the K2 bridge. Every unit node carries a companion CLASS
		// node (the ValueNode keyed by the same (owner, instance, name), so all
		// existing class-side resolution converges on it); the BindUnit/LinkUnit
		// invariant keeps unit identity and class reasoning consistent.
		struct UnitNode
		{
			SizeT parent;
			TokenID name;
			bool rigid = false;
			SharedTreeItem keepAlive; // owns the liveness of `bound`
			const AbstrUnit* bound = nullptr;
			SharedStr boundSource;
			SizeT classNode = NO_TYPE_VAR; // companion ValueNode: class-of(this unit)
		};
		// the `= {}` on these four keeps `TypeUnifier{ m_FuncItem }` (which names only the
		// first member) out of -Wmissing-field-initializers, like the members above
		std::vector<ValueNode> m_ValueNodes = {};
		std::vector<UnitNode>  m_UnitNodes = {};
		using VarKey = std::tuple<const TreeItem*, UInt32, TokenID>;
		std::map<VarKey, SizeT> m_ValueVarIndex = {}, m_UnitVarIndex = {};

		SizeT FindV(SizeT i) { while (m_ValueNodes[i].parent != i) i = m_ValueNodes[i].parent = m_ValueNodes[m_ValueNodes[i].parent].parent; return i; }
		SizeT FindU(SizeT i) { while (m_UnitNodes[i].parent != i) i = m_UnitNodes[i].parent = m_UnitNodes[m_UnitNodes[i].parent].parent; return i; }

		SharedStr FullName() const { return mySSPrintF("{}'{}'", m_Phase, m_ApplItem->GetFullName().c_str()); }

		// get-or-create the node for owner's variable; a freshly created node seeds its
		// acceptance set from the variable's declared constraint, attributed to
		// `declSource`; `fallbackConstraint` covers variables declared by an ENCLOSING
		// function (lexically visible, not in owner's own lists)
		SizeT ValueVar(const TreeItem* owner, UInt32 instance, TokenID name, const SharedStr& declSource, bool rigid = false, TokenID fallbackConstraint = TokenID())
		{
			auto [it, isNew] = m_ValueVarIndex.try_emplace(VarKey{ owner, instance, name }, m_ValueNodes.size());
			if (isNew)
			{
				ValueNode n; n.parent = m_ValueNodes.size(); n.name = name; n.rigid = rigid;
				n.feasible.set();
				TokenID cons = DeclaredConstraintOf(owner, name);
				if (!cons)
					cons = fallbackConstraint;
				if (cons)
				{
					ConstraintRec rec{ name, cons, declSource, GenericConstraintSet(cons) };
					n.feasible = rec.set;
					n.constraints.push_back(std::move(rec));
				}
				m_ValueNodes.push_back(std::move(n));
			}
			return it->second;
		}
		// get-or-create the unit-identity node for owner's variable, with its
		// companion class node created eagerly under the SAME (owner, instance,
		// name) key -- the one moment the key is known -- so any class-side path
		// (ValNode, signature bindings) resolves to the same node. `declaredCls`
		// is the DECLARED value class of a unit parameter (`unit<uint32> U`): the
		// identity varies per instantiation (rigid), but the class is pinned by
		// the declaration itself, so the companion binds concretely instead of
		// staying rigid; without it the companion follows the unit node's rigidity
		// and seeds from the variable's declared constraint (e.g. '<D: domains>').
		SizeT UnitVar(const TreeItem* owner, UInt32 instance, TokenID name, bool rigid = false, const ValueClass* declaredCls = nullptr, TokenID fallbackConstraint = TokenID())
		{
			auto [it, isNew] = m_UnitVarIndex.try_emplace(VarKey{ owner, instance, name }, m_UnitNodes.size());
			if (isNew)
			{
				// push the node BEFORE anything that can throw (the companion's
				// declared-class bind may): the index entry must never dangle
				SizeT idx = it->second;
				UnitNode n; n.parent = idx; n.name = name; n.rigid = rigid;
				m_UnitNodes.push_back(std::move(n));
				SharedStr declSource = mySSPrintF("the declaration of '{}'", name.GetStrLock().c_str());
				SizeT comp = ValueVar(owner, instance, name, declSource, rigid && !declaredCls, fallbackConstraint);
				m_UnitNodes[idx].classNode = comp;
				if (declaredCls)
					BindDeclaredClass(comp, declaredCls, declSource);
			}
			else if (declaredCls)
			{
				// a later caller may know the declared class the creating path did
				// not (get-or-create runs once; type applications and sig bindings
				// can reach a unit parameter's node before ParamType does) --
				// reconcile rather than silently drop the pin
				SizeT comp = m_UnitNodes[FindU(it->second)].classNode;
				if (comp != NO_TYPE_VAR)
					BindDeclaredClass(comp, declaredCls
						, mySSPrintF("the declaration of '{}'", name.GetStrLock().c_str()));
			}
			return it->second;
		}

		// bind a companion class node to a DECLARED class, but never onto a rigid
		// or already-bound node: a same-named type variable may legitimately own
		// the key (pathological shadowing) and an existing binding is either
		// already consistent or a conflict the unit side reports better -- defer
		void BindDeclaredClass(SizeT comp, const ValueClass* declaredCls, const SharedStr& declSource)
		{
			auto& cn = m_ValueNodes[FindV(comp)];
			if (!cn.rigid && !cn.bound)
				BindValue(comp, declaredCls, declSource);
		}

		void CheckFeasible(const ValueNode& n, const ValueClass* vt, const SharedStr& source)
		{
			bool hardOk = n.feasible.test(UInt32(vt->GetValueClassID()));
			for (const auto& rec : n.constraints)
				if (!rec.set.test(UInt32(vt->GetValueClassID())))
				{
					if (rec.soft)
						throwErrorF("ExprParser", "{}: {} ({}) is not among the value types supported by {} ({})"
							, FullName().c_str()
							, vt->GetName().c_str(), source.c_str()
							, rec.source.c_str(), rec.setText.c_str());
					if (!hardOk)
						throwErrorF("ExprParser", "{}: {} ({}) does not satisfy '{}: {}' ({})"
							, FullName().c_str()
							, vt->GetName().c_str(), source.c_str()
							, rec.name.GetStrLock().c_str(), rec.constraint.GetStrLock().c_str(), rec.source.c_str());
				}
			if (!hardOk)
				throwErrorF("ExprParser", "{}: {} ({}) does not satisfy the combined constraints on type variable '{}'"
					, FullName().c_str(), vt->GetName().c_str(), source.c_str(), n.name.GetStrLock().c_str());
		}

		// attach an operator-support set (see ConstraintRec::soft); a node already
		// bound outside the set errors immediately, an unbound node records the set
		// for its eventual binding, and `feasible` stays untouched so rigid
		// ∀-reasoning keeps using declared constraints only
		void AddSoftConstraint(SizeT i, const ValueClassSet& set, TokenID roleName, const SharedStr& source, const SharedStr& setText)
		{
			auto& n = m_ValueNodes[FindV(i)];
			if (n.bound)
			{
				if (!set.test(UInt32(n.bound->GetValueClassID())))
					throwErrorF("ExprParser", "{}: {} ({}) is not among the value types supported by {} ({})"
						, FullName().c_str()
						, n.bound->GetName().c_str(), n.boundSource.c_str()
						, source.c_str(), setText.c_str());
				return;
			}
			ConstraintRec rec;
			rec.name = roleName; rec.source = source; rec.set = set;
			rec.soft = true; rec.setText = setText;
			n.constraints.push_back(std::move(rec));
		}

		void BindValue(SizeT i, const ValueClass* vt, const SharedStr& source)
		{
			auto& n = m_ValueNodes[FindV(i)];
			if (n.rigid)
				throwErrorF("ExprParser", "{}: the body requires type variable '{}' to be {} ({}), but '{}' must remain generic in the definition"
					, FullName().c_str(), n.name.GetStrLock().c_str()
					, vt->GetName().c_str(), source.c_str(), n.name.GetStrLock().c_str());
			if (n.bound)
			{
				if (n.bound != vt)
					throwErrorF("ExprParser", "{}: inconsistent instantiation of type variable '{}': {} ({}) vs {} ({})"
						, FullName().c_str(), n.name.GetStrLock().c_str()
						, n.bound->GetName().c_str(), n.boundSource.c_str()
						, vt->GetName().c_str(), source.c_str());
				return;
			}
			CheckFeasible(n, vt, source);
			n.bound = vt; n.boundSource = source;
		}

		void LinkValue(SizeT a, SizeT b, const SharedStr& source)
		{
			SizeT ra = FindV(a), rb = FindV(b);
			if (ra == rb)
				return;
			if (m_ValueNodes[rb].rigid && !m_ValueNodes[ra].rigid)
				std::swap(ra, rb); // the rigid (or first) side survives as the class representative
			auto& na = m_ValueNodes[ra];
			auto& nb = m_ValueNodes[rb];
			if (na.rigid && nb.rigid)
				throwErrorF("ExprParser", "{}: the body requires type variables '{}' and '{}' to be equal ({}), but they are independent generic parameters of the definition"
					, FullName().c_str(), na.name.GetStrLock().c_str(), nb.name.GetStrLock().c_str(), source.c_str());
			if (na.rigid)
			{
				assert(!na.bound); // rigid variables never carry a concrete binding
				if (nb.bound)
					throwErrorF("ExprParser", "{}: the body requires type variable '{}' to be {} ({}), but '{}' must remain generic in the definition"
						, FullName().c_str(), na.name.GetStrLock().c_str()
						, nb.bound->GetName().c_str(), nb.boundSource.c_str(), na.name.GetStrLock().c_str());
				// FOR-ALL semantics: every instantiation allowed for the rigid variable
				// must be accepted by the other side's DECLARED constraints; soft
				// operator-support sets do not reject rigid variables (see ConstraintRec)
				if ((na.feasible & ~nb.feasible).any())
					for (const auto& rec : nb.constraints)
						if (!rec.soft && (na.feasible & ~rec.set).any())
							throwErrorF("ExprParser", "{}: type variable '{}' must satisfy '{}: {}' ({}) for every instantiation, which its declaration does not guarantee"
								, FullName().c_str(), na.name.GetStrLock().c_str()
								, rec.name.GetStrLock().c_str(), rec.constraint.GetStrLock().c_str(), rec.source.c_str());
			}
			if (na.bound && nb.bound && na.bound != nb.bound)
				throwErrorF("ExprParser", "{}: inconsistent instantiation of type variable '{}': {} ({}) vs {} ({})"
					, FullName().c_str(), na.name.GetStrLock().c_str()
					, na.bound->GetName().c_str(), na.boundSource.c_str()
					, nb.bound->GetName().c_str(), nb.boundSource.c_str());
			if (na.bound && !nb.bound)
				CheckFeasible(nb, na.bound, na.boundSource);
			if (!na.bound && nb.bound)
				CheckFeasible(na, nb.bound, nb.boundSource);
			if (!na.bound && !nb.bound && (na.feasible & nb.feasible).none())
			{
				// attribute a mutually exclusive pair when one exists
				for (const auto& recA : na.constraints)
					for (const auto& recB : nb.constraints)
						if ((recA.set & recB.set).none())
							throwErrorF("ExprParser", "{}: no value type can instantiate type variable '{}': '{}: {}' ({}) conflicts with '{}: {}' ({})"
								, FullName().c_str(), na.name.GetStrLock().c_str()
								, recA.name.GetStrLock().c_str(), recA.constraint.GetStrLock().c_str(), recA.source.c_str()
								, recB.name.GetStrLock().c_str(), recB.constraint.GetStrLock().c_str(), recB.source.c_str());
				throwErrorF("ExprParser", "{}: no value type satisfies the combined constraints on type variable '{}'"
					, FullName().c_str(), na.name.GetStrLock().c_str());
			}
			// merge rb into ra: ra keeps its (rigid/outer) name; payload and constraints unite
			if (!na.bound && nb.bound)
			{
				na.bound = nb.bound; na.boundSource = nb.boundSource;
			}
			na.feasible &= nb.feasible;
			na.constraints.insert(na.constraints.end(), nb.constraints.begin(), nb.constraints.end());
			nb.parent = ra;
		}

		// Unit-identity comparisons in this checker pass UM_AllowRightExpansion: the
		// checker always runs on the meta thread (definition/application checking
		// during meta-info construction), so UnifyDomain may intern the right
		// operand's DC too, which makes the comparison total and symmetric -- no
		// two-direction retry needed. (UM_AllowVoidRight is vestigial here: Void
		// units never reach a UnitNode -- they become Dom::Void at PositionType and
		// short-circuit in UnifyData -- but it is kept defensively.)
		static constexpr UnifyMode s_CheckerUM = UnifyMode(UM_AllowVoidRight | UM_AllowRightExpansion);

		// the batch-U invariant, confined to BindUnit/LinkUnit: binding a unit also
		// binds its companion class node to the unit's value class; linking two
		// unit nodes also links their companions. No caller ordering can then
		// desynchronize unit identity from class reasoning. The unit-side checks
		// run FIRST, so their (older, role-specific) diagnostics keep precedence.

		void BindUnit(SizeT i, SharedTreeItem keepAlive, const AbstrUnit* du, const SharedStr& source)
		{
			auto& n = m_UnitNodes[FindU(i)];
			if (n.rigid)
				throwErrorF("ExprParser", "{}: the body pins unit variable '{}' to a specific unit ({}); it must remain generic in the definition"
					, FullName().c_str(), n.name.GetStrLock().c_str(), source.c_str());
			if (n.bound)
			{
				if (!n.bound->UnifyDomain(du, "", "", s_CheckerUM))
					throwErrorF("ExprParser", "{}: inconsistent instantiation of unit variable '{}': the unit bound {} differs from the unit bound {}"
						, FullName().c_str(), n.name.GetStrLock().c_str()
						, n.boundSource.c_str(), source.c_str());
				return;
			}
			n.keepAlive = std::move(keepAlive); n.bound = du; n.boundSource = source;
			if (n.classNode != NO_TYPE_VAR)
				if (auto vt = du->GetValueType())
					BindValue(n.classNode, vt, source);
		}

		void LinkUnit(SizeT a, SizeT b, const SharedStr& source)
		{
			SizeT ra = FindU(a), rb = FindU(b);
			if (ra == rb)
				return;
			if (m_UnitNodes[rb].rigid && !m_UnitNodes[ra].rigid)
				std::swap(ra, rb);
			auto& na = m_UnitNodes[ra];
			auto& nb = m_UnitNodes[rb];
			if (na.rigid && nb.rigid)
				throwErrorF("ExprParser", "{}: the body requires unit variables '{}' and '{}' to be equal ({}), but they are independent in the definition"
					, FullName().c_str(), na.name.GetStrLock().c_str(), nb.name.GetStrLock().c_str(), source.c_str());
			if (na.rigid && nb.bound)
				throwErrorF("ExprParser", "{}: the body pins unit variable '{}' to a specific unit ({}); it must remain generic in the definition"
					, FullName().c_str(), na.name.GetStrLock().c_str(), nb.boundSource.c_str());
			if (na.bound && nb.bound && !na.bound->UnifyDomain(nb.bound, "", "", s_CheckerUM))
				throwErrorF("ExprParser", "{}: inconsistent instantiation of unit variable '{}': the unit bound {} differs from the unit bound {}"
					, FullName().c_str(), na.name.GetStrLock().c_str()
					, na.boundSource.c_str(), nb.boundSource.c_str());
			if (na.classNode != NO_TYPE_VAR && nb.classNode != NO_TYPE_VAR)
				LinkValue(na.classNode, nb.classNode, source);
			if (!na.bound && nb.bound)
			{
				na.keepAlive = nb.keepAlive; na.bound = nb.bound; na.boundSource = nb.boundSource;
			}
			nb.parent = ra;
		}
	};

// WP4.1: enforce one 'sig<...>'-typed binding -- the bound function's positions
// instantiate or LINK the target variables named by the type application
void LinkSignatureBinding(TypeUnifier& u, const TreeItem* sig, const TreeItem* boundFn,
	const std::vector<std::pair<TokenID, TokenID>>* sigVars, const std::vector<TokenID>* typeArgs,
	const std::function<SizeT(TokenID)>& targetValueNode,
	const std::function<SizeT(TokenID)>& targetUnitNode,
	UInt32 boundInstance, const SharedStr& bindSource);

// structural compatibility of a bound function against a declared signature exemplar
void CheckFunctionSignature(const TreeItem* boundFn, const TreeItem* sigExemplar, CharPtr paramName);

// K11a-3: validate one structured / by-example parameter contract at the call boundary,
// resp. all of them for an 'apply'/'instantiate' item call
void CheckStructuredParamContract(const TreeItem* funcItem, const TreeItem* paramItem,
	const TreeItem* memberSrc, const TreeItem* argRoot);
void CheckStructuredParamContracts(const TreeItem* applyItem, LispPtr argList, const TreeItem* target);

} // namespace hof

#endif // __TIC_HOFTYPEUNIFIER_H
