// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "OperSignature.h"

#include "mci/ValueClass.h"
#include "utl/mySPrintF.h"

#include "OperGroups.h"
#include "Operator.h"
#include "AbstrDataItem.h" // SigUnitChecker: classify a registered arg/result class as data-item vs unit
#include "AbstrUnit.h"
#include "Metric.h"        // SigUnitChecker defense #1: recompute declared metric products/quotients
#include "TreeItemClass.h" // complete TreeItemClass so *::GetStaticClass() upcasts to const Class*

// *****************************************************************************
// Operator::DescribeSignature — the batch-0 vtable slot. The default is
// "undescribed": every consumer then defers, reproducing the pre-signature
// behavior exactly. Family bases (clc/geo) override it once per family.
// *****************************************************************************

bool Operator::DescribeSignature(AbstrSignatureBuilder& /*sb*/) const
{
	return false;
}

// §12.7: the closed-spec variant — default: no per-spec description, the walker
// keeps the DynamicShape deferral exactly as without spec knowledge
bool Operator::DescribeSpecSignature(AbstrSignatureBuilder& /*sb*/, CharPtr /*specValue*/) const
{
	return false;
}

// §12.7 for_each tranche — default: no describable generated container, the
// walker keeps the wholesale meta-group deferral exactly as before
bool Operator::DescribeMetaSignature(MetaMemberLayout& /*layout*/, CharPtr /*optSpecValue*/) const
{
	return false;
}

// *****************************************************************************
// SignatureRecorder
// *****************************************************************************

sig_var SignatureRecorder::NewVar(CharPtr role, UInt8 flags, const ValueClass* fixedCls)
{
	sig_var v = rec.NrVars();
	rec.varRoles.emplace_back(role ? role : "");
	rec.varFlags.push_back(flags);
	rec.varFixedCls.push_back(fixedCls);
	rec.varConstraints.push_back(TokenID());
	rec.memberClasses.push_back(nullptr);
	return v;
}

SignatureRecord::Pos& SignatureRecorder::PosAt(arg_index i)
{
	if (rec.args.size() <= i)
		rec.args.resize(i + 1);
	return rec.args[i];
}

sig_var SignatureRecorder::UnitVar(CharPtr role)              { return NewVar(role, SignatureRecord::VF_None); }
sig_var SignatureRecorder::VoidDomain()                       { return NewVar("void", SignatureRecord::VF_VoidDomain); }
sig_var SignatureRecorder::DefaultUnit(const ValueClass* vc)  { return NewVar("default", SignatureRecord::VF_DefaultUnit, vc); }
sig_var SignatureRecorder::GeneratedUnit(CharPtr role)        { return NewVar(role, SignatureRecord::VF_Generated); }

void SignatureRecorder::MemberValueClass(sig_var u, const ValueClass* vc)
{
	assert(u < rec.NrVars());
	// normalize to the walker's FIELD-class vocabulary (review finding, batch B):
	// sequence/polygon members register COMPOSED classes (float32seq, dpolygon),
	// but walker terms carry the field class with the composition separate
	// (§18.2) — raw composed classes in the tuples would falsely reject every
	// concrete sequence/polygon argument at the class bind and the tuple
	// narrowing. The position's ValueComposition keeps the composed-ness.
	if (vc && IsAcceptableValuesComposition(vc->GetValueComposition()))
		if (auto fc = vc->GetFieldClass())
			vc = fc;
	rec.memberClasses[u] = vc;
}
void SignatureRecorder::FixedValueClass(sig_var u, const ValueClass* vc)
{
	assert(u < rec.NrVars());
	rec.varFixedCls[u] = vc;
}
void SignatureRecorder::ConstrainValueClass(sig_var u, TokenID genericConstraint)
{
	assert(u < rec.NrVars());
	rec.varConstraints[u] = genericConstraint;
}

void SignatureRecorder::ArgName(arg_index i, CharPtr name) { PosAt(i).name = SharedStr(name); }
void SignatureRecorder::ArgAttr(arg_index i, sig_var values, sig_var domain, ValueComposition vc, SigArgTraits traits)
{
	auto& p = PosAt(i);
	p.kind = SignatureRecord::PosKind::Attr; p.values = values; p.domain = domain; p.vc = vc; p.traits = traits;
}
void SignatureRecorder::ArgUnit(arg_index i, sig_var u)
{
	auto& p = PosAt(i);
	p.kind = SignatureRecord::PosKind::Unit; p.values = u;
}
void SignatureRecorder::ArgMetaValue(arg_index i, const ValueClass* vc, CharPtr meaning)
{
	auto& p = PosAt(i);
	p.kind = SignatureRecord::PosKind::MetaValue; p.metaCls = vc; p.name = SharedStr(meaning);
}
void SignatureRecorder::ArgContainer(arg_index i, CharPtr memberPattern, sig_var sharedMemberDomain)
{
	auto& p = PosAt(i);
	p.kind = SignatureRecord::PosKind::Container; p.domain = sharedMemberDomain; p.name = SharedStr(memberPattern);
}
void SignatureRecorder::ArgDeferred(arg_index i, CharPtr note)
{
	auto& p = PosAt(i);
	p.kind = SignatureRecord::PosKind::Deferred; p.name = SharedStr(note);
}
void SignatureRecorder::RepeatArgs(arg_index fromPos, sig_var values, sig_var domain, ValueComposition vc)
{
	rec.repeat.active = true; rec.repeat.fromPos = fromPos;
	rec.repeat.values = values; rec.repeat.domain = domain; rec.repeat.vc = vc;
}

void SignatureRecorder::SameValueClass(sig_var a, sig_var b)   { rec.rels.push_back({ SignatureRecord::RelKind::SameValueClass, a, b }); }
void SignatureRecorder::CompatibleValues(sig_var a, sig_var b) { rec.rels.push_back({ SignatureRecord::RelKind::CompatibleValues, a, b }); }
void SignatureRecorder::MetricProduct(sig_var r, sig_var a, sig_var b)     { rec.rels.push_back({ SignatureRecord::RelKind::MetricProduct, r, a, b }); }
void SignatureRecorder::MetricQuotient(sig_var r, sig_var num, sig_var den){ rec.rels.push_back({ SignatureRecord::RelKind::MetricQuotient, r, num, den }); }
void SignatureRecorder::MetricPower(sig_var r, sig_var base, int exponent)
{
	SignatureRecord::Rel rel{ SignatureRecord::RelKind::MetricPower, r, base };
	rel.power = exponent;
	rec.rels.push_back(std::move(rel));
}
void SignatureRecorder::Dimensionless(sig_var u) { rec.rels.push_back({ SignatureRecord::RelKind::Dimensionless, u }); }
void SignatureRecorder::CastOf(sig_var r, sig_var src, const ValueClass* toCls)
{
	SignatureRecord::Rel rel{ SignatureRecord::RelKind::CastOf, r, src };
	rel.cls = toCls;
	rec.rels.push_back(std::move(rel));
}
void SignatureRecorder::DeferredRelation(CharPtr note)
{
	SignatureRecord::Rel rel{ SignatureRecord::RelKind::Deferred };
	rel.note = SharedStr(note);
	rec.rels.push_back(std::move(rel));
}

void SignatureRecorder::ResultAttr(sig_var values, sig_var domain, ValueComposition vc)
{
	rec.result.kind = SignatureRecord::PosKind::Attr;
	rec.result.values = values; rec.result.domain = domain; rec.result.vc = vc;
}
void SignatureRecorder::ResultUnit(sig_var u)
{
	rec.result.kind = SignatureRecord::PosKind::Unit;
	rec.result.values = u;
}
void SignatureRecorder::ResultContainer(CharPtr memberPattern, sig_var sharedMemberDomain)
{
	rec.result.kind = SignatureRecord::PosKind::Container;
	rec.result.domain = sharedMemberDomain; rec.result.name = SharedStr(memberPattern);
}
void SignatureRecorder::ResultDeferred(CharPtr note)
{
	rec.resultDeferred = true; rec.resultNote = SharedStr(note);
}
void SignatureRecorder::ResultContainerMember(CharPtr path, sig_var values, sig_var domain, ValueComposition vc)
{
	SignatureRecord::ResultMember rm;
	rm.path = SharedStr(path); rm.values = values; rm.domain = domain; rm.vc = vc;
	rec.resultMembers.push_back(std::move(rm));
}
void SignatureRecorder::ResultMembersComplete()
{
	rec.resultMembersComplete = true;
}
void SignatureRecorder::DynamicShape(CharPtr why)
{
	rec.dynamicShape = true; rec.dynamicNote = SharedStr(why);
}

// *****************************************************************************
// shape equality — everything except memberClasses
// *****************************************************************************

bool SignatureRecord::SameShape(const SignatureRecord& rhs) const
{
	return varRoles == rhs.varRoles
		&& varFlags == rhs.varFlags
		&& varFixedCls == rhs.varFixedCls
		&& varConstraints == rhs.varConstraints
		&& args == rhs.args
		&& result == rhs.result
		&& resultMembers == rhs.resultMembers
		&& resultMembersComplete == rhs.resultMembersComplete
		&& resultDeferred == rhs.resultDeferred
		&& dynamicShape == rhs.dynamicShape
		&& resultNote == rhs.resultNote
		&& dynamicNote == rhs.dynamicNote
		&& repeat == rhs.repeat
		&& rels == rhs.rels;
}

// *****************************************************************************
// the per-group cache: record every member once, merge congruent records
// *****************************************************************************

const OperGroupSignatures* AbstrOperGroup::GetSignatures() const
{
	if (!m_SignaturesValid)
	{
		std::unique_ptr<OperGroupSignatures> sigs;
		for (const Operator* m = GetFirstMember(); m; m = m->GetNextGroupMember())
		{
			if (!sigs)
				sigs = std::make_unique<OperGroupSignatures>();
			SignatureRecorder recorder;
			Int32 recordIdx = -1;
			if (m->DescribeSignature(recorder))
			{
				// merge-time structural audit (light): described positions may not
				// outnumber the registered ones unless a repeat tail covers the rest
				assert(arg_index(recorder.rec.args.size()) <= m->NrSpecifiedArgs() || recorder.rec.repeat.active);
#if defined(MG_DEBUG)
				// SigUnitChecker (op-sig §9 defense #2): a described position's item KIND must not
				// CONTRADICT the member's REGISTERED class — an 'attribute' position registered as a
				// unit (or a 'unit' position registered as a data item) means the hand-written
				// DescribeSignature drifted from the operator itself. Only a definite contradiction
				// fires: a generic/base registration (neither strictly data nor unit) passes, so this
				// never false-positives on a genuinely polymorphic argument.
				{
					auto kindContradicts = [](SignatureRecord::PosKind k, ClassCPtr cls) -> bool
					{
						if (!cls)
							return false;
						bool isData = cls->IsDerivedFrom(AbstrDataItem::GetStaticClass());
						bool isUnit = cls->IsDerivedFrom(AbstrUnit::GetStaticClass());
						if (k == SignatureRecord::PosKind::Attr) return isUnit && !isData;
						if (k == SignatureRecord::PosKind::Unit) return isData && !isUnit;
						return false;
					};
					arg_index nChk = std::min<arg_index>(arg_index(recorder.rec.args.size()), m->NrSpecifiedArgs());
					for (arg_index i = 0; i != nChk; ++i)
						assert(!kindContradicts(recorder.rec.args[i].kind, m->GetArgClass(i))
							&& "SigUnitChecker: a described argument's item kind (attribute/unit) contradicts the operator's registered argument class");
					assert(!kindContradicts(recorder.rec.result.kind, m->GetResultClass())
						&& "SigUnitChecker: the described result's item kind contradicts the operator's registered result class");
				}
#endif

				sigs->anyDescribed = true;
				for (Int32 r = 0, n = Int32(sigs->records.size()); r != n; ++r)
					if (sigs->records[r].shape.SameShape(recorder.rec))
					{
						recordIdx = r;
						break;
					}
				if (recordIdx < 0)
				{
					recordIdx = Int32(sigs->records.size());
					auto& mr = sigs->records.emplace_back();
					mr.shape = recorder.rec;
					mr.shape.memberClasses.clear();
				}
				auto& mr = sigs->records[recordIdx];
				mr.tuples.push_back(std::move(recorder.rec.memberClasses));
				mr.members.push_back(m);
			}
			sigs->members.push_back({ m, recordIdx });
		}
		if (sigs && !sigs->anyDescribed)
			sigs.reset(); // groups without any described member: nullptr, walker fast-exits
		m_Signatures = std::move(sigs);
		m_SignaturesValid = true; // Register() invalidates on late member registration
	}
	return m_Signatures.get();
}

// *****************************************************************************
// the printer — the second interpreter over the same records
// *****************************************************************************

static SharedStr RenderVar(const SignatureRecord& shape, sig_var v)
{
	if (v == no_sig_var)
		return SharedStr("?");
	if (shape.varFixedCls[v])
		return SharedStr(shape.varFixedCls[v]->GetName());
	return shape.varRoles[v];
}

static SharedStr RenderPos(const SignatureRecord& shape, const SignatureRecord::Pos& p)
{
	switch (p.kind)
	{
	case SignatureRecord::PosKind::Attr:
		return mySSPrintF("attribute<{}>({})", RenderVar(shape, p.values).c_str(), RenderVar(shape, p.domain).c_str());
	case SignatureRecord::PosKind::Unit:
		return mySSPrintF("unit<{}>", RenderVar(shape, p.values).c_str());
	case SignatureRecord::PosKind::MetaValue:
		return mySSPrintF("{} [meta: {}]", p.metaCls ? SharedStr(p.metaCls->GetName()).c_str() : "value", p.name.c_str());
	case SignatureRecord::PosKind::Container:
		return mySSPrintF("container [{}]", p.name.c_str());
	case SignatureRecord::PosKind::Deferred:
		return mySSPrintF("... [{}]", p.name.c_str());
	default:
		return SharedStr("?");
	}
}

SharedStr RenderMergedSignature(const AbstrOperGroup* og, const OperGroupSignatures::MergedRecord& mr)
{
	const auto& shape = mr.shape;
	SharedStr r = mySSPrintF("{}(", og->GetNameStr());
	for (SizeT i = 0; i != shape.args.size(); ++i)
	{
		if (i)
			r += "; ";
		if (!shape.args[i].name.empty() && shape.args[i].kind == SignatureRecord::PosKind::Attr)
			r += mySSPrintF("{}: ", shape.args[i].name.c_str());
		r += RenderPos(shape, shape.args[i]);
	}
	if (shape.repeat.active)
		r += mySSPrintF("; attribute<{}>({}) ...", RenderVar(shape, shape.repeat.values).c_str(), RenderVar(shape, shape.repeat.domain).c_str());
	r += ")";
	if (shape.result.kind != SignatureRecord::PosKind::None)
		r += mySSPrintF(" -> {}", RenderPos(shape, shape.result).c_str());
	else if (shape.resultDeferred && !shape.resultNote.empty())
		r += mySSPrintF(" -> ... [{}]", shape.resultNote.c_str()); // batch F: deferred-result prose was invisible
	// §12.7 slSubItemCall tranche: typed sub-items of a composite result
	if (!shape.resultMembers.empty())
	{
		r += " { ";
		for (SizeT i = 0; i != shape.resultMembers.size(); ++i)
		{
			const auto& rm = shape.resultMembers[i];
			if (i)
				r += "; ";
			r += mySSPrintF("{}: attribute<{}>({})", rm.path.c_str()
				, rm.values != no_sig_var ? RenderVar(shape, rm.values).c_str() : "..."
				, rm.domain != no_sig_var ? RenderVar(shape, rm.domain).c_str() : "...");
		}
		r += shape.resultMembersComplete ? " }" : "; ... }";
	}
	if (shape.dynamicShape)
		r += mySSPrintF(" [shape: {}]", shape.dynamicNote.c_str());

	// batch F: deferred-relation prose (K12/K16 contracts the walker cannot
	// check) — the printer is these notes' only consumer, so render them
	SharedStr deferredRels;
	for (const auto& rel : shape.rels)
		if (rel.kind == SignatureRecord::RelKind::Deferred && !rel.note.empty())
		{
			if (!deferredRels.empty())
				deferredRels += "; ";
			deferredRels += rel.note;
		}
	if (!deferredRels.empty())
		r += mySSPrintF(" [deferred: {}]", deferredRels.c_str());

	// per-var feasible sets: the union of the congruent members' concrete classes
	bool anyWhere = false;
	for (sig_var v = 0, nv = shape.NrVars(); v != nv; ++v)
	{
		SharedStr classes;
		UInt32 nrClasses = 0;
		for (const auto& tuple : mr.tuples)
		{
			const ValueClass* vc = v < tuple.size() ? tuple[v] : nullptr;
			if (!vc)
				continue;
			SharedStr vcName(vc->GetName());
			if (!classes.empty() && strstr(classes.c_str(), vcName.c_str()))
				continue; // cheap dedup on rendered names
			if (nrClasses++)
				classes += ", ";
			classes += vcName;
		}
		if (!nrClasses)
			continue;
		r += anyWhere ? "; " : " where ";
		anyWhere = true;
		r += mySSPrintF("{} in [{}]", shape.varRoles[v].c_str(), classes.c_str());
	}
	return r;
}

#if defined(MG_DEBUG)

// *****************************************************************************
// SigUnitChecker — op-sig §9 drift defense #1 (the strongest of the four).
//
// Runs after a successful CreateResultCaller in FuncDC, where the operator, its
// arguments, and the result are all CONCRETE. It re-derives, from the member's
// hand-written DescribeSignature, which units the description CLAIMS are related,
// and checks those claims against the units the operator actually produced:
//
//   * unit IDENTITY — a sig_var used in ANY domain role is ONE sort of unit
//     spanning its positions (the K2 bridge): all the actual units it names must
//     UnifyDomain-agree. A values-ONLY var stays class-level (identity there
//     would over-reject — S1), so it is checked for value class only. Void-domain
//     (broadcast), generated (existential), default-unit and void-flagged vars
//     never claim identity.
//   * value CLASS — each var's representative values-unit must carry the member's
//     own recorded value class (memberClasses / varFixedCls).
//   * SameValueClass / CompatibleValues — the two vars' values-units must agree on
//     value class.
//   * METRIC relations (product/quotient/dimensionless) — recomputed from the
//     operand units and compared to GetCurrMetric. LOG ONLY: §9 rules metrics the
//     deferred part, and the checker must never reject over them.
//
// Because it runs only AFTER the operator's own CreateResult succeeded, every
// unit here already satisfies the operator's real constraints; a firing check
// therefore means the DESCRIPTION over-claims a relation the operator does not
// enforce (the exact drift §9 targets). Under-claims are invisible here — the
// soundness bias.
//
// ARMED: identity (CHECK 1), value class (CHECK 2) and SameValueClass/
// CompatibleValues (CHECK 3-class) each emit a detailed ST_Error line and then
// ASSERT — drift in a hand-written DescribeSignature is a programming error and
// must abort the debug run (headless: logged detail + exit(3)). This never breaks
// a LEGITIMATE run: the checker runs only after CreateResult succeeded, so a fired
// assert means the description genuinely over-claims. Metric relations stay
// LOG-ONLY (§9: the deferred part), and a checker-INTERNAL exception is caught and
// logged (ST_Warning), never asserted. The battery (137/137) and a 22-agent audit
// showed zero false-fires across the described families before arming; a family
// NOT exercised there could still surface a first-run assert in tst-Debug — that is
// the intended drift signal, to be fixed at its description, not silenced here.
//
// The ST_Error/warning reports MUST use reportF_without_cancellation_check: plain
// reportF routes through ASyncContinueCheck(), which THROWS when a host cancel is
// pending (GUI / any cancellable host). A throw before the assert would escape into
// FuncDC_CreateResult's result-building catch and mark an already-successfully-
// created cache result as FAILED — a spurious failure on a run that is merely being
// cancelled. The call site additionally wraps this in its own catch(...).
//
// v1 coverage gaps (report nothing, never false-fire — follow-ups, not drift
// coverage): (a) rec.resultMembers (§12.7/§12.8 composite sub-items — unique
// Values, union UnionData, connect_info/discrete_alloc/dijkstra member sets) are
// NOT replayed, because resolving a member path post-CreateResult risks meta-info
// recursion; (b) addValRep is first-wins, so a RepeatArgs/variadic tail's value
// class is checked at its first position only.
// *****************************************************************************

void SigUnitChecker_VerifyApplication(const Operator* oper, const ArgSeqType& argItems, const TreeItem* result)
{
	if (!oper)
		return;
	SignatureRecorder recorder;
	if (!oper->DescribeSignature(recorder))
		return; // undescribed member: nothing to replay
	const SignatureRecord& rec = recorder.rec;
	if (rec.dynamicShape || rec.resultDeferred)
		return; // whole-shape deferral: the concrete units are the operator's business

	const UInt32 nv = rec.NrVars();
	if (!nv)
		return;

	try {
		SharedStr where = mySSPrintF("operator '{}'", oper->GetGroup()->GetNameStr());

		// which vars are ever used in a domain role — only those claim unit identity
		std::vector<bool> inDomainRole(nv, false);
		auto noteDomainRole = [&](sig_var v) { if (v != no_sig_var && v < nv) inDomainRole[v] = true; };
		for (const auto& p : rec.args)
		{
			if (p.kind == SignatureRecord::PosKind::Attr)      noteDomainRole(p.domain);
			else if (p.kind == SignatureRecord::PosKind::Unit) noteDomainRole(p.values); // a unit position's own identity
		}
		if (rec.repeat.active) noteDomainRole(rec.repeat.domain);
		if (rec.result.kind == SignatureRecord::PosKind::Attr)      noteDomainRole(rec.result.domain);
		else if (rec.result.kind == SignatureRecord::PosKind::Unit) noteDomainRole(rec.result.values);

		// per var: the units that must be identical (an equivalence bucket) and a
		// representative values-unit driving the class + metric checks
		struct VarUnits { std::vector<const AbstrUnit*> idUnits; const AbstrUnit* valRep = nullptr; };
		std::vector<VarUnits> vu(nv);

		auto skipIdentity = [&](sig_var v) -> bool
		{
			return v == no_sig_var || v >= nv
				|| (rec.varFlags[v] & (SignatureRecord::VF_Generated | SignatureRecord::VF_VoidDomain | SignatureRecord::VF_DefaultUnit)) != 0;
		};
		auto addToIdBucket = [&](sig_var v, const AbstrUnit* u, bool voidBroadcast)
		{
			if (skipIdentity(v) || !u || voidBroadcast || u->IsDefaultUnit())
				return;
			vu[v].idUnits.push_back(u);
		};
		auto addValRep = [&](sig_var v, const AbstrUnit* u)
		{
			if (v == no_sig_var || v >= nv || !u)
				return;
			if (!vu[v].valRep)
				vu[v].valRep = u;
		};

		auto collectPos = [&](const SignatureRecord::Pos& p, const TreeItem* item)
		{
			if (!item)
				return;
			if (p.kind == SignatureRecord::PosKind::Attr)
			{
				if (!IsDataItem(item))
					return; // kind mismatch: reported by the result-class check / defense #2
				auto adi = AsDataItem(item);
				bool voidDom = adi->HasVoidDomainGuarantee();
				if (auto avu = adi->GetAbstrValuesUnit())
				{
					addValRep(p.values, avu);
					// the K2 bridge: a values var ALSO used in a domain role carries unit
					// identity, so its values-unit joins the identity bucket
					if (p.values != no_sig_var && p.values < nv && inDomainRole[p.values])
						addToIdBucket(p.values, avu, false);
				}
				if (auto adu = adi->GetAbstrDomainUnit())
					addToIdBucket(p.domain, adu, voidDom);
			}
			else if (p.kind == SignatureRecord::PosKind::Unit)
			{
				if (!IsUnit(item))
					return;
				auto au = AsUnit(item);
				addValRep(p.values, au);
				addToIdBucket(p.values, au, false); // a unit argument IS its own identity
			}
			// MetaValue / Container / Deferred: nothing to bind
		};

		for (SizeT k = 0; k != argItems.size(); ++k)
		{
			const SignatureRecord::Pos* p = nullptr;
			SignatureRecord::Pos repeatPos;
			if (k < rec.args.size())
				p = &rec.args[k];
			else if (rec.repeat.active && k >= rec.repeat.fromPos)
			{
				repeatPos.kind = SignatureRecord::PosKind::Attr;
				repeatPos.values = rec.repeat.values; repeatPos.domain = rec.repeat.domain; repeatPos.vc = rec.repeat.vc;
				p = &repeatPos;
			}
			if (p)
				collectPos(*p, argItems[k]);
		}
		if (rec.result.kind == SignatureRecord::PosKind::Attr || rec.result.kind == SignatureRecord::PosKind::Unit)
			collectPos(rec.result, result);

		auto roleName = [&](sig_var v) -> SharedStr { return (v != no_sig_var && v < nv) ? rec.varRoles[v] : SharedStr("?"); };
		auto valUnitOf = [&](sig_var v) -> const AbstrUnit* { return (v != no_sig_var && v < nv) ? vu[v].valRep : nullptr; };

		// (1) unit identity within each var's bucket (UnifyDomain, symmetric on the meta thread)
		for (sig_var v = 0; v != nv; ++v)
		{
			const AbstrUnit* ref = nullptr;
			for (auto u : vu[v].idUnits)
			{
				if (!ref) { ref = u; continue; }
				if (!ref->UnifyDomain(u, "", "", UnifyMode(UM_AllowVoidRight | UM_AllowRightExpansion)))
				{
					reportF_without_cancellation_check(SeverityTypeID::ST_Error
						, "SigUnitChecker drift: {}: unit variable '{}' should identify all its positions, but units {} and {} do not unify"
						, where.c_str(), roleName(v).c_str()
						, ref->GetFullName().c_str(), u->GetFullName().c_str());
					assert(!"SigUnitChecker: a described unit variable's positions are not unit-identical (see the preceding ST_Error log line)");
				}
			}
		}

		// (2) value class of each var's representative values-unit vs the member's recorded class
		for (sig_var v = 0; v != nv; ++v)
		{
			const ValueClass* want = v < rec.memberClasses.size() ? rec.memberClasses[v] : nullptr;
			if (!want)
				want = rec.varFixedCls[v];
			if (!want || !vu[v].valRep)
				continue;
			auto got = vu[v].valRep->GetValueType();
			if (got && got != want)
			{
				SharedStr wantName(want->GetName()), gotName(got->GetName());
				reportF_without_cancellation_check(SeverityTypeID::ST_Error
					, "SigUnitChecker drift: {}: unit variable '{}' recorded value class {}, but the actual unit {} has value class {}"
					, where.c_str(), roleName(v).c_str()
					, wantName.c_str(), vu[v].valRep->GetFullName().c_str(), gotName.c_str());
				assert(!"SigUnitChecker: a described unit variable's recorded value class differs from the actual unit's (see the preceding ST_Error log line)");
			}
		}

		// (3) relations
		for (const auto& rel : rec.rels)
		{
			switch (rel.kind)
			{
			case SignatureRecord::RelKind::SameValueClass:
			case SignatureRecord::RelKind::CompatibleValues:
			{
				auto ua = valUnitOf(rel.a), ub = valUnitOf(rel.b);
				if (!ua || !ub)
					break;
				auto ca = ua->GetValueType(), cb = ub->GetValueType();
				if (ca && cb && ca != cb)
				{
					SharedStr caName(ca->GetName()), cbName(cb->GetName());
					reportF_without_cancellation_check(SeverityTypeID::ST_Error
						, "SigUnitChecker drift: {}: {} of '{}' and '{}' violated: value classes {} vs {}"
						, where.c_str()
						, rel.kind == SignatureRecord::RelKind::SameValueClass ? "SameValueClass" : "CompatibleValues"
						, roleName(rel.a).c_str(), roleName(rel.b).c_str(), caName.c_str(), cbName.c_str());
					assert(!"SigUnitChecker: a described SameValueClass/CompatibleValues relation is violated by the actual units (see the preceding ST_Error log line)");
				}
				break;
			}
			case SignatureRecord::RelKind::MetricProduct:
			case SignatureRecord::RelKind::MetricQuotient:
			{
				auto ur = valUnitOf(rel.a), u1 = valUnitOf(rel.b), u2 = valUnitOf(rel.c);
				if (!ur || !u1 || !u2)
					break;
				UnitMetric got;
				if (rel.kind == SignatureRecord::RelKind::MetricProduct)
					got.SetProduct(u1->GetCurrMetric(), u2->GetCurrMetric());
				else
					got.SetQuotient(u1->GetCurrMetric(), u2->GetCurrMetric());
				if (!AreEqual(&got, ur->GetCurrMetric())) // LOG ONLY (§9: metrics are the deferred part)
					reportF_without_cancellation_check(SeverityTypeID::ST_Warning
						, "SigUnitChecker metric note: {}: declared metric relation on '{}' yields {}, but the actual unit {} has metric {}"
						, where.c_str(), roleName(rel.a).c_str()
						, got.AsString(FormattingFlags::None).c_str()
						, ur->GetFullName().c_str(), ur->GetCurrMetricStr(FormattingFlags::None).c_str());
				break;
			}
			case SignatureRecord::RelKind::Dimensionless:
			{
				auto u = valUnitOf(rel.a);
				if (u && !IsEmpty(u->GetCurrMetric())) // LOG ONLY
					reportF_without_cancellation_check(SeverityTypeID::ST_Warning
						, "SigUnitChecker metric note: {}: '{}' declared dimensionless, but the actual unit {} carries metric {}"
						, where.c_str(), roleName(rel.a).c_str()
						, u->GetFullName().c_str(), u->GetCurrMetricStr(FormattingFlags::None).c_str());
				break;
			}
			default:
				break; // MetricPower / CastOf / Deferred: not replayed in v1 (log-only, low value)
			}
		}
	}
	catch (...)
	{
		// a cross-check must never break the run; surface that it itself misbehaved
		reportF_without_cancellation_check(SeverityTypeID::ST_Warning, "SigUnitChecker: internal exception while verifying operator '{}'"
			, oper->GetGroup()->GetNameStr());
	}
}

#endif // MG_DEBUG
