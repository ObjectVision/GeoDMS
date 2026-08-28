// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// How a built-in operator group types an application at definition time: selecting and
// merging the group's signature records, the domain-skeleton fallback when several
// records survive, and the spec / meta-container special cases.

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

	// §6.2 cross-record fallback. When several congruence records survive, the
	// walker used to defer wholesale: no single record's class tuples apply. But a
	// group's records often still AGREE on the DOMAIN SKELETON -- which positions are
	// attributes/units and which of them share a domain -- and that part holds no
	// matter which member reduction ultimately selects, so claiming it is sound.
	//
	// This is the gate the K11a-1b note called out: `add`/`+` splits into three
	// records (spolygon/ipolygon, dpolygon/fpolygon, and the scalar family), and
	// with classless arguments none can be eliminated. All three nevertheless say
	// "both arguments and the result live on ONE domain", so a body adding two
	// attributes over DIFFERENT domains is now rejected at the definition.
	//
	// The synthesized record claims ONLY that structure: every position gets a FRESH
	// values variable (no cross-position class identity, no metric or value-class
	// relations), the domain variables are the canonical ones all records share, and
	// a value composition is claimed only where every record agrees. The empty tuple
	// list makes ApplyOperRecord's soft support sets, class pins and agreement links
	// no-ops, so the application reduces to pure domain unification.
	std::optional<OperGroupSignatures::MergedRecord>
	FunctionChecker::BuildDomainSkeletonRecord(const OperGroupSignatures* sigs, const std::vector<Int32>& recordIdxs)
	{
		if (recordIdxs.size() < 2)
			return std::nullopt;
		for (Int32 ri : recordIdxs)
			if (ri < 0)
				return std::nullopt; // an UNDESCRIBED member survives: nothing is known

		// canonicalize one record: per position (kind, domain-slot, composition),
		// where a domain slot numbers the domain variables in first-seen order and
		// carries the variable's flags (void/generated domains are distinct claims).
		// K11 leftover (2026-07-29): Container positions are canonicalized too (their
		// shared member domain slot, whether a shared member VALUES var exists, and
		// the naming argument), so multi-record groups with a container argument can
		// still agree on the skeleton; Deferred/MetaValue positions canonicalize as
		// kind-only (no claim) instead of vetoing the whole skeleton.
		struct Slot { UInt32 id = UInt32(-1); UInt8 flags = 0; };
		struct PosSkel
		{
			SignatureRecord::PosKind kind{}; Slot dom; ValueComposition vc{};
			bool hasVal = false;                  // Container: a shared member VALUES var exists
			arg_index namesPos = arg_index(-1);   // Container: the naming argument
		};
		auto canon = [](const SignatureRecord& s, std::vector<PosSkel>& args, PosSkel& res, std::vector<TokenID>& roles) -> bool
		{
			if (s.dynamicShape || s.resultDeferred || s.repeat.active || !s.resultMembers.empty() || !s.resultMemberSets.empty())
				return false; // deferred/variadic/composite shapes: not a plain skeleton
			std::map<sig_var, UInt32> seen;
			auto slotOf = [&](sig_var v) -> Slot
			{
				Slot r;
				if (v == no_sig_var || v >= s.NrVars())
					return r;
				auto [it, isNew] = seen.try_emplace(v, UInt32(seen.size()));
				if (isNew)
					roles.push_back(s.varRoles[v]);
				r.id = it->second; r.flags = s.varFlags[v];
				return r;
			};
			auto posOf = [&](const SignatureRecord::Pos& p) -> PosSkel
			{
				PosSkel ps; ps.kind = p.kind; ps.vc = p.vc;
				// an Attr's domain, and a Unit position's own identity variable
				if (p.kind == SignatureRecord::PosKind::Attr)
					ps.dom = slotOf(p.domain);
				else if (p.kind == SignatureRecord::PosKind::Unit)
					ps.dom = slotOf(p.values);
				else if (p.kind == SignatureRecord::PosKind::Container)
				{
					ps.dom = slotOf(p.domain);
					ps.hasVal = p.values != no_sig_var;
					ps.namesPos = p.namesPos;
				}
				return ps;
			};
			for (const auto& p : s.args)
				args.push_back(posOf(p));
			res = posOf(s.result);
			return true;
		};

		std::vector<PosSkel> refArgs; PosSkel refRes; std::vector<TokenID> refRoles;
		if (!canon(sigs->records[recordIdxs[0]].shape, refArgs, refRes, refRoles))
			return std::nullopt;
		std::vector<bool> vcAgrees(refArgs.size(), true);
		bool resVcAgrees = true;
		for (SizeT k = 1; k != recordIdxs.size(); ++k)
		{
			std::vector<PosSkel> a; PosSkel r; std::vector<TokenID> roles;
			if (!canon(sigs->records[recordIdxs[k]].shape, a, r, roles))
				return std::nullopt;
			if (a.size() != refArgs.size())
				return std::nullopt;
			for (SizeT i = 0; i != a.size(); ++i)
			{
				if (a[i].kind != refArgs[i].kind || a[i].dom.id != refArgs[i].dom.id || a[i].dom.flags != refArgs[i].dom.flags)
					return std::nullopt; // the skeletons disagree: nothing is shared
				if (a[i].kind == SignatureRecord::PosKind::Container
					&& (a[i].hasVal != refArgs[i].hasVal || a[i].namesPos != refArgs[i].namesPos))
					return std::nullopt; // container claims must agree exactly
				if (a[i].vc != refArgs[i].vc)
					vcAgrees[i] = false;
			}
			if (r.kind != refRes.kind || r.dom.id != refRes.dom.id || r.dom.flags != refRes.dom.flags)
				return std::nullopt;
			if (r.vc != refRes.vc)
				resVcAgrees = false;
		}

		// synthesize: canonical domain vars first, then one fresh values var per position
		OperGroupSignatures::MergedRecord mr;
		auto& shape = mr.shape;
		UInt32 nDom = UInt32(refRoles.size());
		auto addVar = [&](TokenID role, UInt8 flags) -> sig_var
		{
			sig_var v = shape.NrVars();
			shape.varRoles.push_back(role);
			shape.varFlags.push_back(flags);
			shape.varFixedCls.push_back(nullptr);
			shape.varConstraints.push_back(TokenID());
			return v;
		};
		std::vector<UInt8> domFlags(nDom, 0);
		for (const auto& p : refArgs)
			if (p.dom.id != UInt32(-1))
				domFlags[p.dom.id] = p.dom.flags;
		if (refRes.dom.id != UInt32(-1))
			domFlags[refRes.dom.id] = refRes.dom.flags;
		for (UInt32 d = 0; d != nDom; ++d)
			addVar(refRoles[d], domFlags[d]);

		auto emitPos = [&](const PosSkel& ps, bool vcOk, CharPtr posName) -> SignatureRecord::Pos
		{
			SignatureRecord::Pos p;
			p.kind = ps.kind;
			p.vc = vcOk ? ps.vc : ValueComposition::Unknown;
			if (ps.kind == SignatureRecord::PosKind::Attr)
			{
				// A DISTINCT role name per position is essential: TypeUnifier keys
				// variables by (owner, instance, NAME), so same-named fresh variables
				// would collapse into ONE class node and falsely equate the positions'
				// value classes (that turned `cond ? 0 : 1` into a bool-vs-uint32
				// conflict between the condition and the result).
				p.values = addVar(GetTokenID_mt(posName), 0);
				p.domain = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id);
			}
			else if (ps.kind == SignatureRecord::PosKind::Unit)
				p.values = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id); // the unit's own identity
			else if (ps.kind == SignatureRecord::PosKind::Container)
			{
				// the agreed container claim: shared member domain slot, an intra-
				// container shared VALUES var when every record declares one (fresh --
				// no cross-position class identity), and the agreed naming argument
				p.domain = ps.dom.id == UInt32(-1) ? no_sig_var : sig_var(ps.dom.id);
				if (ps.hasVal)
					p.values = addVar(GetTokenID_mt(posName), 0);
				p.namesPos = ps.namesPos;
			}
			else
				p.kind = SignatureRecord::PosKind::None; // MetaValue & co: no claim
			return p;
		};
		for (SizeT i = 0; i != refArgs.size(); ++i)
			// spelled-out synthetic role names: the label space is case-folded like any
			// other token space, so a one-letter synthetic ('sig_r') would fold onto a
			// describe-side label ('R' -> 'sig_R') and report an engine-internal mix-up
			shape.args.push_back(emitPos(refArgs[i], vcAgrees[i], mySSPrintF("sig_arg{}", UInt32(i)).c_str()));
		shape.result = emitPos(refRes, resVcAgrees, "sig_res");
		// no tuples and no members: ApplyOperRecord's class machinery becomes a no-op
		return mr;
	}

	DefType FunctionChecker::ApplyOperRecord(const OperGroupSignatures::MergedRecord& mr, const SharedStr& headName, const std::vector<DefType>& argTerms
		, const TreeItem* refScope, LispPtr argsList)
	{
		const auto& shape = mr.shape;
		UInt32 inst = m_NextInstance++;
		SharedStr src = mySSPrintF("operator '{}'", headName.c_str());

		UInt32 nv = shape.NrVars();
		std::vector<SizeT> valNode(nv, NO_TYPE_VAR), domNode(nv, NO_TYPE_VAR);

		auto VN = [&](sig_var v) -> SizeT
		{
			if (valNode[v] == NO_TYPE_VAR)
			{
				valNode[v] = m_Unifier.ValueVar(nullptr, inst, shape.varRoles[v], src, false, shape.varConstraints[v]);
				if (shape.varFixedCls[v])
					m_Unifier.BindValue(valNode[v], shape.varFixedCls[v], src);
				else
				{
					// soft support set: the union of the congruent members' classes at v
					ValueClassSet set; bool covered = !mr.tuples.empty();
					SharedStr setText; UInt32 nrClasses = 0;
					for (const auto& tuple : mr.tuples)
					{
						const ValueClass* mc = v < tuple.size() ? tuple[v] : nullptr;
						if (!mc)
						{
							covered = false; // some member leaves v unconstrained: no set
							break;
						}
						if (set.test(UInt32(mc->GetValueClassID())))
							continue;
						set.set(UInt32(mc->GetValueClassID()));
						if (nrClasses++)
							setText += ", ";
						setText += SharedStr(mc->GetName());
					}
					if (covered)
						m_Unifier.AddSoftConstraint(valNode[v], set, shape.varRoles[v], src, setText);
				}
			}
			return valNode[v];
		};
		auto DN = [&](sig_var v) -> SizeT
		{
			if (domNode[v] == NO_TYPE_VAR)
				domNode[v] = m_Unifier.UnitVar(nullptr, inst, shape.varRoles[v]);
			return domNode[v];
		};

		// the record variables used in a values role by any position
		std::vector<sig_var> posVars;
		auto notePosVar = [&](sig_var v)
		{
			if (v == no_sig_var)
				return;
			for (sig_var q : posVars)
				if (q == v)
					return;
			posVars.push_back(v);
		};
		for (const auto& p : shape.args)
			if (p.kind == SignatureRecord::PosKind::Attr || p.kind == SignatureRecord::PosKind::Unit)
				notePosVar(p.values);
		if (shape.repeat.active)
			notePosVar(shape.repeat.values);
		if (shape.result.kind == SignatureRecord::PosKind::Attr || shape.result.kind == SignatureRecord::PosKind::Unit)
			notePosVar(shape.result.values);
		for (const auto& rm : shape.resultMembers) // §12.7: member values participate like positions
			notePosVar(rm.values);
		for (sig_var v : posVars)
			VN(v); // create + attach soft sets before any linking

		// tuple agreement over a tuple subset: equal-class pairs and single-class pins
		auto agreeEqual = [&](const std::vector<const std::vector<const ValueClass*>*>& tuples, sig_var v, sig_var q) -> bool
		{
			if (tuples.empty())
				return false;
			for (auto t : tuples)
			{
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				const ValueClass* cq = q < t->size() ? (*t)[q] : nullptr;
				if (!cv || cv != cq)
					return false;
			}
			return true;
		};
		auto agreeClass = [&](const std::vector<const std::vector<const ValueClass*>*>& tuples, sig_var v) -> const ValueClass*
		{
			const ValueClass* c = nullptr;
			for (auto t : tuples)
			{
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				if (!cv || (c && c != cv))
					return nullptr;
				c = cv;
			}
			return c;
		};

		// batch B: a record variable used in BOTH a values role and a domain role
		// claims unit IDENTITY across those positions (the K2 pattern: lookup's E2
		// in org_rel's VALUES role and values' DOMAIN role; rlookup/invert result
		// values borrowing an argument's domain). Values-only variables stay
		// class-level: their runtime discharge is UnifyValues (class + metric),
		// where a key-identity claim would over-reject (S1, review finding)
		std::vector<bool> inDomainRole(nv, false);
		auto noteDomainRole = [&](sig_var v) { if (v != no_sig_var) inDomainRole[v] = true; };
		for (const auto& p : shape.args)
			if (p.kind == SignatureRecord::PosKind::Attr)
				noteDomainRole(p.domain);
		if (shape.repeat.active)
			noteDomainRole(shape.repeat.domain);
		if (shape.result.kind == SignatureRecord::PosKind::Attr)
			noteDomainRole(shape.result.domain);
		for (const auto& rm : shape.resultMembers) // §12.7: member domains are real domain roles
			noteDomainRole(rm.domain);

		std::vector<const std::vector<const ValueClass*>*> allTuples;
		for (const auto& t : mr.tuples)
			allTuples.push_back(&t);

		// pre-unification links: positions on which EVERY member agrees carry the
		// old shared-node semantics exactly (hard: rigid-rigid conflicts must error)
		SharedStr agreeSrc = mySSPrintF("the registered overloads of operator '{}'", headName.c_str());
		for (SizeT i = 0; i != posVars.size(); ++i)
			for (SizeT j = i + 1; j != posVars.size(); ++j)
				if (agreeEqual(allTuples, posVars[i], posVars[j]))
					m_Unifier.LinkValue(VN(posVars[i]), VN(posVars[j]), agreeSrc);

		// unify each argument against its described position
		auto posType = [&](const SignatureRecord::Pos& p) -> DefType
		{
			DefType r;
			if (p.kind == SignatureRecord::PosKind::Attr)
			{
				r.kind = DefType::Kind::Data;
				r.vcomp = p.vc;
				if (p.values != no_sig_var)
				{
					r.vNode = VN(p.values);
					if (inDomainRole[p.values])
						r.vuNode = DN(p.values); // K2: the SAME node the domain role uses
				}
				if (p.domain != no_sig_var)
				{
					if (shape.varFlags[p.domain] & SignatureRecord::VF_VoidDomain)
						r.dom = DefType::Dom::Void;
					else
					{
						r.dom = DefType::Dom::Node;
						r.dNode = DN(p.domain);
					}
				}
			}
			else if (p.kind == SignatureRecord::PosKind::Unit)
			{
				r.kind = DefType::Kind::UnitVal;
				r.vNode = VN(p.values);
				r.dom = DefType::Dom::Node;
				r.dNode = DN(p.values); // the unit's own identity, in the same var
			}
			return r; // MetaValue/Container/Deferred: Unknown (argument stays walked)
		};

		for (SizeT k = 0; k != argTerms.size(); ++k)
		{
			const SignatureRecord::Pos* p = nullptr;
			SignatureRecord::Pos repeatPos;
			if (k < shape.args.size())
				p = &shape.args[k];
			else if (shape.repeat.active && k >= shape.repeat.fromPos)
			{
				repeatPos.kind = SignatureRecord::PosKind::Attr;
				repeatPos.values = shape.repeat.values; repeatPos.domain = shape.repeat.domain; repeatPos.vc = shape.repeat.vc;
				p = &repeatPos;
			}
			if (!p || p->kind == SignatureRecord::PosKind::None)
				continue;
			SharedStr argSrc = mySSPrintF("argument {} of operator '{}'", k + 1, headName.c_str());
			// K11b: a described CONTAINER argument -- unify its actual members against
			// the shared domain/values variables the description declares
			if (p->kind == SignatureRecord::PosKind::Container)
			{
				LispPtr argExpr;
				if (refScope)
				{
					SizeT j = 0;
					for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
						if (j == k)
						{
							argExpr = a.Left();
							break;
						}
				}
				LinkContainerArg(*p, argTerms[k], argExpr, refScope, argSrc, argsList, VN, DN);
				continue;
			}
			DefType posT = posType(*p);
			if (posT.kind == DefType::Kind::Unknown)
				continue;
			UnifyData(argTerms[k], posT, argSrc, src);
		}

		// narrow the tuples by what the arguments bound; an empty remainder means
		// no registered member matches the (concrete) classes -- reduction is bound
		// to fail on the same FindOper this record was derived from
		std::vector<const std::vector<const ValueClass*>*> compatible;
		for (auto t : allTuples)
		{
			bool ok = true;
			for (sig_var v : posVars)
			{
				if (valNode[v] == NO_TYPE_VAR)
					continue;
				const auto& n = m_Unifier.m_ValueNodes[m_Unifier.FindV(valNode[v])];
				if (!n.bound)
					continue;
				const ValueClass* cv = v < t->size() ? (*t)[v] : nullptr;
				if (cv && cv != n.bound)
				{
					ok = false;
					break;
				}
			}
			if (ok)
				compatible.push_back(t);
		}
		if (compatible.empty() && !allTuples.empty())
			throwErrorF("ExprParser", "{}: the argument types of operator '{}' do not match any of its registered overloads"
				, m_Unifier.FullName().c_str(), headName.c_str());

		// post-narrowing propagation: facts every REMAINING member agrees on.
		// Pins and narrowed links never touch rigid ∀-variables (soft support);
		// flexible nodes take them as ordinary bindings/links.
		for (sig_var v : posVars)
		{
			if (auto c = agreeClass(compatible, v))
			{
				const auto& n = m_Unifier.m_ValueNodes[m_Unifier.FindV(valNode[v])];
				if (!n.rigid && !n.bound)
					m_Unifier.BindValue(valNode[v], c, agreeSrc);
			}
		}
		for (SizeT i = 0; i != posVars.size(); ++i)
			for (SizeT j = i + 1; j != posVars.size(); ++j)
				if (agreeEqual(compatible, posVars[i], posVars[j]))
				{
					SizeT ri = m_Unifier.FindV(valNode[posVars[i]]), rj = m_Unifier.FindV(valNode[posVars[j]]);
					if (ri == rj)
						continue;
					if (m_Unifier.m_ValueNodes[ri].rigid && m_Unifier.m_ValueNodes[rj].rigid)
						continue; // narrowed-set knowledge stays soft on rigid pairs
					m_Unifier.LinkValue(valNode[posVars[i]], valNode[posVars[j]], agreeSrc);
				}

		// the result, in the same variables
		if (shape.dynamicShape || shape.resultDeferred)
			return {};
		DefType r = posType(shape.result);
		// §12.7 slSubItemCall tranche: typed sub-items of a composite result --
		// consumed by InferGeneratedMember when the body references INTO the
		// result (u/Values). Member values stay class-level unless the var is
		// also in a domain role (the batch-B K2 rule); member domains claim
		// identity through the same DN nodes the positions bound -- so
		// unique's Values rides the SAME existential node as the result unit.
		// The completeness flag licenses definition-time missing-member errors
		// (SubItemOperator is certain to reject the same reference) -- and a
		// complete-EMPTY set (the plain select_* groups) must attach too, or
		// its promised definition-time report never fires (review finding).
		if (!shape.resultMembers.empty() || shape.resultMembersComplete || !shape.resultMemberSets.empty())
		{
			auto members = std::make_shared<std::map<TokenID, DefType>>();
			auto memberTypeOf = [&](sig_var values, sig_var domain, ValueComposition vc) -> DefType
			{
				DefType m; m.kind = DefType::Kind::Data; m.vcomp = vc;
				if (values != no_sig_var)
				{
					m.vNode = VN(values);
					if (inDomainRole[values])
						m.vuNode = DN(values);
				}
				if (domain != no_sig_var)
				{
					if (shape.varFlags[domain] & SignatureRecord::VF_VoidDomain)
						m.dom = DefType::Dom::Void;
					else
					{
						m.dom = DefType::Dom::Node;
						m.dNode = DN(domain);
					}
				}
				return m;
			};
			for (const auto& rm : shape.resultMembers)
				(*members)[rm.path] = memberTypeOf(rm.values, rm.domain, rm.vc);

			// NAME-DIRECTED result member families: one member per entry of the
			// declared names array -- discrete_alloc's shadow_prices/<type> and
			// total_allocated/<type>. The array must be definition-time evaluable
			// (the §12.7 / K11b closedness test); otherwise this family contributes
			// nothing, leaving the set exactly as incomplete as before.
			for (const auto& rms : shape.resultMemberSets)
			{
				if (rms.namesPos == arg_index(-1) || !refScope)
					continue;
				LispPtr namesExpr;
				{
					arg_index j = 0;
					for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++j)
						if (j == rms.namesPos)
						{
							namesExpr = a.Left();
							break;
						}
				}
				if (namesExpr.EndP())
					continue;
				auto names = EvalClosedStrArray(refScope, namesExpr);
				if (!names)
					continue; // data-directed: no claim, as before
				DefType mt = memberTypeOf(rms.values, rms.domain, rms.vc);
				SharedStr prefixStr(rms.prefix.AsStrRange()); // materialized: the loop below CREATES tokens
				for (const auto& nm : *names)
				{
					if (!nm.IsDefined() || nm.empty())
						continue;
					(*members)[GetTokenID_mt((prefixStr + "/" + nm).c_str())] = mt;
				}
			}
			r.members = std::move(members);
			r.membersComplete = shape.resultMembersComplete;
		}
		return r;
	}

	// §12.7: attempt definition-time K13 spec processing for a DynamicShape record
	// whose single string-valued ArgMetaValue position holds a spec CLOSED over
	// the formals. On success the surviving members' DescribeSpecSignature records
	// (merged; NO DynamicShape) are applied -- including the ruled honest ARITY
	// check: the derived position count IS the CalcNrArgs predicate CreateResult
	// applies, the sole exemption from §6.2's arity-always-defers rule. Every
	// other outcome returns nullopt and the caller defers exactly as today.
	std::optional<DefType> FunctionChecker::TrySpecProcessing(const AbstrOperGroup* og,
		const OperGroupSignatures::MergedRecord& mr, const std::vector<const Operator*>& survivors,
		const SharedStr& headName, const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms)
	{
		if (survivors.empty())
			return std::nullopt;
		// review finding: a trailing '...rest' symbol counts as ONE syntactic term
		// here but splices to its captured argument count at reduction -- both the
		// positional mapping and the arity verdict would misalign. Rest-having
		// functions defer the whole spec path (their effective arity is per
		// application)
		if (TreeItem_HasFunctionRestParam(m_FuncItem))
			return std::nullopt;
		SizeT metaPos = SizeT(-1);
		for (SizeT k = 0; k != mr.shape.args.size(); ++k)
			if (mr.shape.args[k].kind == SignatureRecord::PosKind::MetaValue)
			{
				if (metaPos != SizeT(-1))
					return std::nullopt; // several meta-directing positions: not this tranche
				metaPos = k;
			}
		if (metaPos == SizeT(-1))
			return std::nullopt;
		const ValueClass* mc = mr.shape.args[metaPos].metaCls;
		if (!mc || mc->GetValueClassID() != ValueClassID::VT_SharedStr)
			return std::nullopt; // non-string meta values (name arrays): await K11

		LispPtr specExpr; SizeT k = 0;
		for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++k)
			if (k == metaPos)
			{
				specExpr = a.Left();
				break;
			}
		if (specExpr.EndP())
			return std::nullopt;
		auto specValue = EvalClosedSpec(refScope, specExpr);
		if (!specValue)
			return std::nullopt;

		// derive the per-spec record from THIS application's survivors (review
		// finding: the survivor set varies per application with the argument
		// classes, so a cross-application memo keyed on the spec alone would
		// reuse the wrong tuples; derivation is cheap and the LispPtr application
		// memo already de-duplicates identical call sites)
		auto merged = std::make_shared<OperGroupSignatures::MergedRecord>();
		for (const Operator* m : survivors)
		{
			SignatureRecorder rec;
			// a throw here is the member's OWN spec validation (ParseDijkstraString/
			// CheckFlags -- the very predicates CreateResult applies first), reached
			// only for a CLEANLY EVALUATED closed spec: reporting it at definition
			// is honest (the §12.7 ruling's sanctioned upgrade from the v1 deferral)
			bool described = m->DescribeSpecSignature(rec, specValue->c_str());
			if (!described)
				return std::nullopt;
			if (merged->members.empty())
			{
				merged->shape = rec.rec;
				merged->shape.memberClasses.clear();
			}
			else if (!merged->shape.SameShape(rec.rec))
				return std::nullopt; // incongruent per-spec records across survivors: defer
			merged->tuples.push_back(std::move(rec.rec.memberClasses));
			merged->members.push_back(m);
		}
		const auto& derived = merged;

		// the ruled arity exemption -- strictly OUTSIDE every defer-catch above
		if (argTerms.size() != derived->shape.args.size())
			throwErrorF("ExprParser", "{}: number of given arguments to operator '{}' doesn't match the specification '{}': {} arguments given (including the specification), but {} expected"
				, m_Unifier.FullName().c_str(), headName.c_str(), specValue->c_str()
				, argTerms.size(), derived->shape.args.size());

		return ApplyOperRecord(*derived, headName, argTerms, refScope, argsList); // K11b: spec records may carry container positions too
	}

	// §12.7 for_each tranche: definition-time processing of a container-
	// GENERATING meta application (a dont_cache_result group). The group's
	// member(s) describe their argument LAYOUT (MetaMemberLayout); when the
	// name array -- and, for for_each_ind, the field spec directing the layout --
	// is CLOSED over the checked function's formals, it is EVALUATED at
	// definition scan (the ruling: storage-backed sources included) and the
	// application types as a Container carrying the pseudo-expanded member set.
	// Member types come from the layout's unit positions: a direct unit
	// argument types every member uniformly (a formal unit parameter
	// contributes its unifier node -- the K2 bridge; a closed external unit its
	// concrete identity), a (container, name-array) pair resolves a unit per
	// member inside a closed external container. An unresolvable unit defers
	// that MEMBER's type, never the member set. Every other failure -- open
	// arguments, evaluation failure, duplicate names, heterogeneous layouts --
	// returns nullopt and the caller defers exactly as before the tranche.
	std::optional<DefType> FunctionChecker::TryMetaContainerProcessing(const AbstrOperGroup* og, TokenID headID,
		const TreeItem* refScope, LispPtr argsList, const std::vector<DefType>& argTerms)
	{
		// a trailing '...rest' splices at reduction: positions misalign (§12.7 review rule)
		if (TreeItem_HasFunctionRestParam(m_FuncItem))
			return std::nullopt;
		if (!og->GetFirstMember())
			return std::nullopt;

		// for a layout directed by the first argument's value (for_each_ind),
		// that value must itself be closed and evaluable
		std::optional<SharedStr> specValue;
		if (og->HasDynamicArgPolicies())
		{
			if (argsList.EndP())
				return std::nullopt;
			specValue = EvalClosedSpec(refScope, argsList.Left());
			if (!specValue)
				return std::nullopt;
		}

		// every member must describe the SAME layout (each for_each group holds one).
		// A throw is the member's OWN spec validation (ScanFirstArg -- the predicate
		// CreateResult applies first), reached only for a CLEANLY EVALUATED closed
		// spec: reporting it at definition is honest (the §12.7 ruling's sanctioned
		// upgrade from the v1 deferral)
		MetaMemberLayout layout;
		bool anyDescribed = false;
		for (const Operator* m = og->GetFirstMember(); m; m = m->GetNextGroupMember())
		{
			MetaMemberLayout ml;
			bool described = m->DescribeMetaSignature(ml, specValue ? specValue->c_str() : nullptr);
			if (!described)
				return std::nullopt; // an undescribed member could serve the application: defer
			if (anyDescribed && !(ml == layout))
				return std::nullopt;
			layout = ml;
			anyDescribed = true;
		}
		if (!anyDescribed || layout.namesPos == no_meta_pos)
			return std::nullopt;

		// arity: outside the group's accepted range, a same-named function may
		// serve the call -- defer (§6.2). Within it, a spec-derived width is
		// CreateResult's own predicate: its violation is the ruled honest error
		// (for_each_ind's own message shape), strictly outside every defer-catch.
		arg_index nrGiven = arg_index(argTerms.size());
		if (!og->AcceptsArity(nrGiven))
			return std::nullopt;
		if (nrGiven != layout.nrArgs)
		{
			if (!specValue)
				return std::nullopt; // layout-static groups: arity always defers (§6.2)
			SharedStr headName(headID.AsStrRange());
			throwErrorF("ExprParser", "{}: number of given arguments to operator '{}' doesn't match the specification '{}': {} arguments given (including the specification), but {} expected"
				, m_Unifier.FullName().c_str(), headName.c_str(), specValue->c_str()
				, nrGiven, layout.nrArgs);
		}

		auto argExprAt = [&](arg_index p) -> LispPtr
		{
			arg_index k = 0;
			for (LispPtr a = argsList; !a.EndP(); a = a.Right(), ++k)
				if (k == p)
					return a.Left();
			return {};
		};

		auto names = EvalClosedStrArray(refScope, argExprAt(layout.namesPos));
		if (!names)
			return std::nullopt;

		// one unit-providing source per layout role; FragAt yields a UnitVal
		// fragment per member, or Unknown where nothing is knowable
		struct UnitSource
		{
			DefType uniform;                                     // context-only mode
			SharedTreeItem container;                            // pair mode
			std::optional<std::vector<SharedStr>> perMemberNames;

			DefType FragAt(SizeT i) const
			{
				if (container && perMemberNames)
				{
					if (i >= perMemberNames->size())
						return {};
					try
					{
						auto u = container->ResolveItemPath((*perMemberNames)[i]);
						if (u && IsUnit(u.get()) && !u->InTemplate())
						{
							DefType f; f.kind = DefType::Kind::UnitVal;
							f.vc = AsUnit(u.get())->GetValueType();
							f.dom = DefType::Dom::Concrete; f.domKeep = u; f.domUnit = AsUnit(u.get());
							return f;
						}
					}
					catch (...) {}
					return {}; // an unresolvable unit name defers this member's type
				}
				return uniform;
			}
		};
		auto resolveUnitSource = [&](arg_index pos, arg_index namesPos) -> UnitSource
		{
			UnitSource s;
			if (pos == no_meta_pos || pos >= nrGiven)
				return s;
			try
			{
				if (namesPos == no_meta_pos)
				{
					if (argTerms[pos].kind == DefType::Kind::UnitVal)
						s.uniform = argTerms[pos]; // formal unit parameters ride their unifier node here
					else if (LispPtr ue = argExprAt(pos); ue.IsSymb())
					{
						if (auto vc = ValueClass::FindByScriptName(ue.GetSymbID()))
						{
							// the DEFAULT unit of a value-class name ('float64'):
							// class pinned, identity left unclaimed
							s.uniform.kind = DefType::Kind::UnitVal;
							s.uniform.vc = vc;
							return s;
						}
						// a def-scope external unit: closed by construction (§12.7)
						const TreeItem* local = nullptr; UInt32 pi = 0;
						SharedTreeItem ext; ExtRefKind ek = ExtRefKind::DefScopeExternal;
						if (ResolveName(refScope, ue.GetSymbID(), &local, &pi, &ext, &ek) == 2
							&& ek == ExtRefKind::DefScopeExternal && ext && IsUnit(ext.get()) && !ext->InTemplate())
						{
							s.uniform.kind = DefType::Kind::UnitVal;
							s.uniform.vc = AsUnit(ext.get())->GetValueType();
							s.uniform.dom = DefType::Dom::Concrete; s.uniform.domKeep = ext; s.uniform.domUnit = AsUnit(ext.get());
						}
					}
					return s;
				}
				// pair mode: a CLOSED external container + a closed per-member name array
				LispPtr ce = argExprAt(pos);
				if (!ce.IsSymb() || namesPos >= nrGiven)
					return s;
				const TreeItem* local = nullptr; UInt32 pi = 0;
				SharedTreeItem ext; ExtRefKind ek = ExtRefKind::DefScopeExternal;
				if (ResolveName(refScope, ce.GetSymbID(), &local, &pi, &ext, &ek) != 2
					|| ek != ExtRefKind::DefScopeExternal || !ext || ext->IsFunctionItem() || ext->InTemplate())
					return s;
				ext->UpdateMetaInfo(); // its own rule may generate the named units
				auto nm = EvalClosedStrArray(refScope, argExprAt(namesPos));
				if (!nm)
					return s;
				s.container = ext;
				s.perMemberNames = std::move(nm);
			}
			catch (...)
			{
				s.container = {};
				s.perMemberNames.reset(); // defer the member types, keep the member set
			}
			return s;
		};

		UnitSource duSrc = resolveUnitSource(layout.domainPos, layout.domainNamesPos);
		UnitSource vuSrc = resolveUnitSource(layout.valuesPos, layout.valuesNamesPos);
		UnitSource unSrc = resolveUnitSource(layout.unitPos, layout.unitNamesPos);

		auto memberTypeAt = [&](SizeT i) -> DefType
		{
			switch (layout.memberKind)
			{
			case MetaMemberLayout::MemberKind::Data:
			{
				DefType m; m.kind = DefType::Kind::Data; m.vcomp = layout.vcomp;
				DefType du = duSrc.FragAt(i);
				if (du.kind == DefType::Kind::UnitVal)
				{
					if (du.vc && du.vc->GetValueClassID() == ValueClassID::VT_Void)
						m.dom = DefType::Dom::Void;
					else
					{
						m.dom = du.dom; m.domUnit = du.domUnit; m.domKeep = du.domKeep; m.dNode = du.dNode;
					}
				}
				DefType vu = vuSrc.FragAt(i);
				if (vu.kind == DefType::Kind::UnitVal)
				{
					m.vc = vu.vc; m.vNode = vu.vNode;
					m.vuNode = vu.dNode; m.vUnit = vu.domUnit; m.vKeep = vu.domKeep;
				}
				return m;
			}
			case MetaMemberLayout::MemberKind::Unit:
			{
				DefType m; m.kind = DefType::Kind::UnitVal;
				DefType un = unSrc.FragAt(i);
				if (un.kind == DefType::Kind::UnitVal)
					m.vc = un.vc; // class only: each generated unit's IDENTITY is fresh per holder
				return m;
			}
			default:
				return {}; // TemplateCopy / plain items: member types per application
			}
		};

		auto members = std::make_shared<std::map<TokenID, DefType>>();
		for (SizeT i = 0; i != names->size(); ++i)
		{
			const SharedStr& nm = (*names)[i];
			if (!nm.IsDefined() || nm.empty())
				continue; // skipped rows, exactly as ForEach_CreateResult
			if (!members->emplace(GetTokenID_mt(nm.c_str()), memberTypeAt(i)).second)
				return std::nullopt; // duplicate generated names (token equality = the tree's case folding): not modeled
		}

		DefType r;
		r.kind = DefType::Kind::Container;
		r.members = std::move(members);
		r.membersComplete = true;
		return r;
	}


} // namespace hof
