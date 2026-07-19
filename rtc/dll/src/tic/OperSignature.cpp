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

// *****************************************************************************
// Operator::DescribeSignature — the batch-0 vtable slot. The default is
// "undescribed": every consumer then defers, reproducing the pre-signature
// behavior exactly. Family bases (clc/geo) override it once per family.
// *****************************************************************************

bool Operator::DescribeSignature(AbstrSignatureBuilder& /*sb*/) const
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
	if (shape.dynamicShape)
		r += mySSPrintF(" [shape: {}]", shape.dynamicNote.c_str());

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
