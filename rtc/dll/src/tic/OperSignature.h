// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_OPERSIGNATURE_H)
#define __TIC_OPERSIGNATURE_H

#include "TicBase.h"
#include "mci/ValueComposition.h"
#include "ptr/SharedStr.h"

#include <vector>

// *****************************************************************************
// Operator unit-constraint signatures (doc/development/operator-signature-interface.md)
//
// Operators DESCRIBE their unit/class constraints declaratively through the
// AbstrSignatureBuilder vocabulary (batch 0). The description is recorded once
// per member into a plain-data SignatureRecord; congruent member records are
// merged per group into an OperGroupSignatures cache whose per-member class
// TUPLES carry the cross-position co-variance exactly (each member binds its
// variables consistently, so "all members agree on positions i,j" is a sound
// family fact). Consumers: the definition-time typed walker (AbstrCalculator),
// the printer below, and (later) the §9 debug drift-verifier.
//
// A sig_var is ONE sort of unit variable, usable in both values and domain
// roles (the K2 bridge). Until the UnitNode tranche (batch U) lands, the
// walker interprets variables at the CLASS level only: values roles become
// value-class nodes, domain roles become domain-identity nodes, and no unit
// identity is claimed across the two — so descriptions stay honest (e.g. mul's
// result var is NOT its argument var: the metric product is a different unit
// even where the classes coincide).
// *****************************************************************************

using sig_var = UInt32;
constexpr sig_var no_sig_var = sig_var(-1);

enum class SigArgTraits : UInt8 { none = 0, no_void_broadcast = 1, categorical = 2 };

struct AbstrSignatureBuilder
{
	// --- unit variables: ONE sort, usable in BOTH domain and values roles (the K2 fix)
	virtual sig_var UnitVar(CharPtr role) = 0;             // "D", "E2", "V"
	virtual sig_var VoidDomain() = 0;                      // K15, parameters
	virtual sig_var DefaultUnit(const ValueClass* vc) = 0; // boolean/default_unit_creator
	virtual sig_var GeneratedUnit(CharPtr role) = 0;       // K6 existential

	// --- value-class constraints on a unit variable
	virtual void MemberValueClass(sig_var u, const ValueClass* vc) = 0; // this member's own class; merged as tuples
	virtual void FixedValueClass (sig_var u, const ValueClass* vc) = 0; // K14; identical across members
	virtual void ConstrainValueClass(sig_var u, TokenID genericConstraint) = 0; // "floats"; intersected

	// --- argument positions (0-based, aligned with GetArgClass)
	virtual void ArgName(arg_index i, CharPtr name) = 0;   // diagnostics
	virtual void ArgAttr(arg_index i, sig_var values, sig_var domain,
	                     ValueComposition vc, SigArgTraits traits = SigArgTraits::none) = 0;
	virtual void ArgUnit(arg_index i, sig_var u) = 0;
	virtual void ArgMetaValue(arg_index i, const ValueClass* vc, CharPtr meaning) = 0; // K13
	// K11: a CONTAINER argument. `sharedMemberDomain` / `sharedMemberValues` are the
	// domain / values unit the CONSUMED members share. `namesPos` is the argument
	// position holding the STRING ARRAY that names them: operators read a
	// name-directed SUBSET (discrete_alloc looks up suitabilities by the type names),
	// so a container may legitimately carry further members. Without a `namesPos`
	// the consumed set is unknown at definition time and K11b claims NOTHING — an
	// "every member" claim would reject the blessed superset-container pattern.
	virtual void ArgContainer(arg_index i, CharPtr memberPattern,
	                          sig_var sharedMemberDomain = no_sig_var,
	                          sig_var sharedMemberValues = no_sig_var,
	                          arg_index namesPos = arg_index(-1)) = 0;                 // K11
	virtual void ArgDeferred(arg_index i, CharPtr note) = 0;
	virtual void RepeatArgs(arg_index fromPos, sig_var values, sig_var domain,
	                        ValueComposition vc) = 0;                                  // variadic tails

	// --- relations beyond shared variables
	virtual void SameValueClass  (sig_var a, sig_var b) = 0;
	virtual void CompatibleValues(sig_var a, sig_var b) = 0;   // K7/K16; def-time = class equality
	virtual void MetricProduct (sig_var r, sig_var a, sig_var b) = 0;  // K10: declared, def-time-deferred
	virtual void MetricQuotient(sig_var r, sig_var num, sig_var den) = 0;
	virtual void MetricPower   (sig_var r, sig_var base, int exponent) = 0;
	virtual void Dimensionless (sig_var u) = 0;
	virtual void CastOf(sig_var r, sig_var src, const ValueClass* toCls) = 0;
	virtual void DeferredRelation(CharPtr note) = 0;           // K12: real, unexpressible, printable

	// --- result derivation, in the same variables
	virtual void ResultAttr(sig_var values, sig_var domain, ValueComposition vc) = 0;
	virtual void ResultUnit(sig_var u) = 0;
	virtual void ResultContainer(CharPtr memberPattern, sig_var sharedMemberDomain = no_sig_var) = 0;
	virtual void ResultDeferred(CharPtr note) = 0;

	// --- §12.7 slSubItemCall tranche: TYPED sub-items of a composite result,
	// path relative to the result root. The values var claims the member's
	// value CLASS (identity only when the var is also used in a domain role —
	// the batch-B K2 rule); the domain var claims the member's domain
	// IDENTITY; vc must be Unknown unless member-fixed (a wrong composition
	// claim falsely eliminates downstream overloads). Emit
	// ResultMembersComplete() ONLY when CreateResult provably creates no
	// sub-items beyond those described: it licenses definition-time
	// missing-member errors (sound because SubItemOperator is certain to
	// reject the same reference per application).
	virtual void ResultContainerMember(CharPtr path, sig_var values, sig_var domain, ValueComposition vc) = 0;
	// NAME-DIRECTED result members: one member '<pathPrefix>/<name>' per entry of
	// the string array at argument `namesPos`, all of the same declared type
	// (discrete_alloc's shadow_prices/<type> and total_allocated/<type>). The
	// walker expands these per application, and only when that array is
	// definition-time evaluable — the same closedness test the §12.7 for_each
	// tranche and K11b apply; otherwise nothing is claimed.
	virtual void ResultContainerMemberSet(CharPtr pathPrefix, arg_index namesPos,
	                                      sig_var values, sig_var domain, ValueComposition vc) = 0;
	virtual void ResultMembersComplete() = 0;

	// --- whole-signature deferral: shape depends on a meta-read value (K13)
	virtual void DynamicShape(CharPtr why) = 0;
protected:
	~AbstrSignatureBuilder() = default;
};

// the recorded form of one member's description: plain data, comparable.
// memberClasses (per var) is the member's OWN concrete-class binding and is the
// ONLY part excluded from shape equality — equal-shape records merge, their
// class vectors become the merged record's tuples.
struct SignatureRecord
{
	enum class PosKind : UInt8 { None, Attr, Unit, MetaValue, Container, Deferred };
	struct Pos
	{
		PosKind kind = PosKind::None;
		sig_var values = no_sig_var, domain = no_sig_var; // Attr: both; Unit: 'values' identifies the unit var
		ValueComposition vc = ValueComposition::Single;
		SigArgTraits traits = SigArgTraits::none;
		const ValueClass* metaCls = nullptr;              // MetaValue
		SharedStr name;                                   // ArgName / prose
		arg_index namesPos = arg_index(-1);               // Container: the string-array argument naming the CONSUMED members (K11b)

		bool operator==(const Pos& rhs) const
		{
			return kind == rhs.kind && values == rhs.values && domain == rhs.domain
				&& vc == rhs.vc && traits == rhs.traits && metaCls == rhs.metaCls
				&& name == rhs.name && namesPos == rhs.namesPos;
		}
	};
	enum class RelKind : UInt8 { SameValueClass, CompatibleValues, MetricProduct, MetricQuotient, MetricPower, Dimensionless, CastOf, Deferred };
	struct Rel
	{
		RelKind kind;
		sig_var a = no_sig_var, b = no_sig_var, c = no_sig_var;
		int power = 0;
		const ValueClass* cls = nullptr;
		SharedStr note = {}; // the `= {}` keeps the many `Rel{kind, a, b}` aggregate inits
		                     // out of -Wmissing-field-initializers, like the members above

		bool operator==(const Rel& rhs) const
		{
			return kind == rhs.kind && a == rhs.a && b == rhs.b && c == rhs.c
				&& power == rhs.power && cls == rhs.cls && note == rhs.note;
		}
	};
	struct Repeat
	{
		bool active = false;
		arg_index fromPos = 0;
		sig_var values = no_sig_var, domain = no_sig_var;
		ValueComposition vc = ValueComposition::Single;

		bool operator==(const Repeat& rhs) const
		{
			return active == rhs.active && fromPos == rhs.fromPos
				&& values == rhs.values && domain == rhs.domain && vc == rhs.vc;
		}
	};

	// per-var facts (indexed by sig_var)
	enum VarFlags : UInt8 { VF_None = 0, VF_VoidDomain = 1, VF_Generated = 2, VF_DefaultUnit = 4 };
	std::vector<SharedStr>         varRoles;
	std::vector<UInt8>             varFlags;
	std::vector<const ValueClass*> varFixedCls;    // FixedValueClass / DefaultUnit class
	std::vector<TokenID>           varConstraints; // ConstrainValueClass

	// a typed sub-item of a composite result (§12.7 slSubItemCall tranche)
	struct ResultMember
	{
		SharedStr path;
		sig_var values = no_sig_var, domain = no_sig_var;
		ValueComposition vc = ValueComposition::Unknown;

		bool operator==(const ResultMember& rhs) const
		{
			return path == rhs.path && values == rhs.values && domain == rhs.domain && vc == rhs.vc;
		}
	};

	// a NAME-DIRECTED family of result members: '<prefix>/<name>' for every name
	// in the string array at `namesPos`, each of the declared type
	struct ResultMemberSet
	{
		SharedStr prefix;
		arg_index namesPos = arg_index(-1);
		sig_var values = no_sig_var, domain = no_sig_var;
		ValueComposition vc = ValueComposition::Unknown;

		bool operator==(const ResultMemberSet& rhs) const
		{
			return prefix == rhs.prefix && namesPos == rhs.namesPos
				&& values == rhs.values && domain == rhs.domain && vc == rhs.vc;
		}
	};

	std::vector<Pos> args;
	Pos     result;                                // kind None => no result described
	std::vector<ResultMember> resultMembers;       // typed members of a composite result
	std::vector<ResultMemberSet> resultMemberSets; // name-directed member families (K11b result side)
	bool    resultMembersComplete = false;         // licenses def-time missing-member errors
	bool    resultDeferred = false, dynamicShape = false;
	SharedStr resultNote, dynamicNote;
	Repeat  repeat;
	std::vector<Rel> rels;

	// the member's own concrete classes per var (nullptr = unconstrained by this member)
	std::vector<const ValueClass*> memberClasses;

	UInt32 NrVars() const { return UInt32(varRoles.size()); }
	bool SameShape(const SignatureRecord& rhs) const; // everything except memberClasses
};

// the per-group cache: which members describe, merged into congruence classes
struct OperGroupSignatures
{
	struct MergedRecord
	{
		SignatureRecord shape;                     // memberClasses cleared
		std::vector<std::vector<const ValueClass*>> tuples; // one class vector per congruent member
		std::vector<const Operator*> members;
	};
	struct MemberEntry
	{
		const Operator* oper = nullptr;
		Int32 recordIdx = -1;                      // -1: undescribed member
	};
	std::vector<MergedRecord> records;
	std::vector<MemberEntry>  members;
	bool anyDescribed = false;
};

// §12.7 for_each tranche: the argument layout of a container-GENERATING meta
// operator (a dont_cache_result group), consumed by the definition-time
// checker to pseudo-expand the generated member set when the meta-directing
// arguments are CLOSED over a checked function's formals. Unlike a
// SignatureRecord this describes tree-item generation, not data typing: the
// name array names the generated members/paths (K13); the domain/values/unit
// positions declare each member's type, either as a direct unit argument
// (context-only mode) or as a (container, name-array) pair resolving a unit
// per member. nrArgs is the exact arity the layout implies — for a group
// whose layout is directed by its first argument's value (for_each_ind) it
// is CreateResult's own predicate, so the walker may report its violation
// honestly (the ruled §12.7 arity exemption); for layout-static groups the
// §6.2 arity-always-defers rule stands.
constexpr arg_index no_meta_pos = arg_index(-1);
struct MetaMemberLayout
{
	enum class MemberKind : UInt8 { Untyped, Data, Unit, TemplateCopy };
	MemberKind memberKind = MemberKind::Untyped;
	ValueComposition vcomp = ValueComposition::Single; // Data members
	arg_index nrArgs = 0;
	arg_index namesPos  = no_meta_pos;                       // string array: the generated member names/paths
	arg_index domainPos = no_meta_pos, domainNamesPos = no_meta_pos; // Data members
	arg_index valuesPos = no_meta_pos, valuesNamesPos = no_meta_pos; // Data members
	arg_index unitPos   = no_meta_pos, unitNamesPos   = no_meta_pos; // Unit members
	arg_index templPos  = no_meta_pos, templNamesPos  = no_meta_pos; // TemplateCopy members

	bool operator==(const MetaMemberLayout&) const = default;
};

// the recording builder: DescribeSignature writes through the abstract
// vocabulary; the result is the plain-data record above
struct SignatureRecorder final : AbstrSignatureBuilder
{
	SignatureRecord rec;

	sig_var UnitVar(CharPtr role) override;
	sig_var VoidDomain() override;
	sig_var DefaultUnit(const ValueClass* vc) override;
	sig_var GeneratedUnit(CharPtr role) override;

	void MemberValueClass(sig_var u, const ValueClass* vc) override;
	void FixedValueClass (sig_var u, const ValueClass* vc) override;
	void ConstrainValueClass(sig_var u, TokenID genericConstraint) override;

	void ArgName(arg_index i, CharPtr name) override;
	void ArgAttr(arg_index i, sig_var values, sig_var domain, ValueComposition vc, SigArgTraits traits) override;
	void ArgUnit(arg_index i, sig_var u) override;
	void ArgMetaValue(arg_index i, const ValueClass* vc, CharPtr meaning) override;
	void ArgContainer(arg_index i, CharPtr memberPattern, sig_var sharedMemberDomain, sig_var sharedMemberValues, arg_index namesPos) override;
	void ArgDeferred(arg_index i, CharPtr note) override;
	void RepeatArgs(arg_index fromPos, sig_var values, sig_var domain, ValueComposition vc) override;

	void SameValueClass  (sig_var a, sig_var b) override;
	void CompatibleValues(sig_var a, sig_var b) override;
	void MetricProduct (sig_var r, sig_var a, sig_var b) override;
	void MetricQuotient(sig_var r, sig_var num, sig_var den) override;
	void MetricPower   (sig_var r, sig_var base, int exponent) override;
	void Dimensionless (sig_var u) override;
	void CastOf(sig_var r, sig_var src, const ValueClass* toCls) override;
	void DeferredRelation(CharPtr note) override;

	void ResultAttr(sig_var values, sig_var domain, ValueComposition vc) override;
	void ResultUnit(sig_var u) override;
	void ResultContainer(CharPtr memberPattern, sig_var sharedMemberDomain) override;
	void ResultDeferred(CharPtr note) override;
	void ResultContainerMember(CharPtr path, sig_var values, sig_var domain, ValueComposition vc) override;
	void ResultContainerMemberSet(CharPtr pathPrefix, arg_index namesPos, sig_var values, sig_var domain, ValueComposition vc) override;
	void ResultMembersComplete() override;

	void DynamicShape(CharPtr why) override;

private:
	sig_var NewVar(CharPtr role, UInt8 flags, const ValueClass* fixedCls = nullptr);
	SignatureRecord::Pos& PosAt(arg_index i);
};

// the printer: the second interpreter over the same records
TIC_CALL SharedStr RenderMergedSignature(const AbstrOperGroup* og, const OperGroupSignatures::MergedRecord& mr);

#if defined(MG_DEBUG)
// op-sig §9 drift defense #1 (the strongest): after a successful
// CreateResultCaller in FuncDC — operator, args, and result all concrete — replay
// the member's described SignatureRecord against the ACTUAL units and check the
// record's claims (unit identity, value class, CompatibleValues; metric relations
// are log-only). Drift is a programming error in DescribeSignature, so a violation
// is REPORTED (ST_Warning, full detail) and NOT thrown — a debug-only cross-check
// must never break a legitimate config's run, and report-only surfaces EVERY
// drifting family in one battery pass rather than aborting on the first.
TIC_CALL void SigUnitChecker_VerifyApplication(const Operator* oper, const ArgSeqType& argItems, const TreeItem* result);
#endif

#endif // __TIC_OPERSIGNATURE_H
