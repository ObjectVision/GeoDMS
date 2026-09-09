// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// The function-item specification: the side-assoc that records what a user-defined
// function item declares (parameter count, result sub-item, signature exemplars,
// generic type variables, variant sets) and the queries the parser and the calculator
// run against it. Keyed by the function definition item, NOT a TreeItem member --
// which is why it lives in its own translation unit rather than in TreeItem.cpp.

#include "TreeItemFunctionSpec.h"

#include "RtcInterface.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "mci/ValueComposition.h"
#include "xct/DmsException.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "TreeItem.h"
#include "UsingCache.h"

#include "set/StaticQuickAssoc.h"
#include "utl/StrFormat.h"  // mgFormat2SharedStr, for the #1261 FunctionSpec text
#include "vt/CharPtrRange.h"

#include <algorithm>
#include <bitset>
#include <tuple>
#include <vector>

// user-defined function items: declared parameter count + designated result sub-item
// + optional per-parameter function-signature exemplars, kept in a side-assoc keyed by
// the function definition item (set by the config parser, read at call dispatch;
// erased in ~TreeItem).
namespace {
	struct FunctionSpecData
	{
		UInt32 nrParams = 0;
		TokenID resultName;
		std::vector<std::tuple<UInt32, std::weak_ptr<const TreeItem>, std::vector<TokenID>>> paramSigs; // (param index, signature exemplar, type-application args)
		std::vector<std::pair<UInt32, TokenID>> paramSigNames = {}; // #1252: (param index, the reference as the source wrote it)
		std::vector<std::tuple<UInt32, TokenID, std::vector<TokenID>>> pendingParamSigs = {}; // #1252: not resolved while parsing; resolved at UpdateMetaInfo
		// the `= {}` on the members below keeps `FunctionSpecData{ nrParams, resultName, {} }`
		// out of -Wmissing-field-initializers, like the bool members further down
		std::vector<std::pair<UInt32, std::weak_ptr<const TreeItem>>> paramTypeExemplars = {}; // K11a by-example: (param index, UNIT exemplar whose declared members type the parameter)
		std::vector<std::tuple<UInt32, TokenID, TokenID, bool>> genericParams = {}; // (param index, type variable, constraint, isDomainVar)
		std::vector<std::pair<TokenID, TokenID>> typeVars = {}; // the declaration's own ordered <var: constraint> list (WP4.1)
		std::vector<UInt32> metaRefParams = {}; // 'item x' parameters: bound as raw item references (sourceDescr), not calculation keys
		bool hasRestParam = false;      // '...x' rest parameter (always the LAST param): binds ONE OR MORE trailing arguments
		bool definitionChecked = false; // WP3.4: body scope/shape validated once
		bool isVariantSet = false;      // §5.7: a function that dispatches to variant sub-functions by argument type
		bool signatureOnly = false;     // 'alias = function<...>(...) -> ...;' -- a signature-only function item (declared type, no body)
		bool resultIsFunction = false;  // §5.10: '-> function' / '-> sigAlias' -- the result is function-valued
		bool resultIsGenericUnit = false; // '-> unit<V>': represented as TreeItem until the application binds V
		std::weak_ptr<const TreeItem> resultSig = {}; // the '-> sigAlias<...>' result-signature exemplar, if any (else expired)
		std::vector<TokenID> resultSigTypeArgs = {};  // the result signature's type-application args
		TokenID resultSigName = {};                   // #1252: the result signature reference as the source wrote it
		TokenID pendingResultSigName = {};            // #1252: idem, not resolved while parsing
		std::vector<TokenID> pendingResultSigTypeArgs = {};
	};
	bool IsDefaultValue(const FunctionSpecData& v) { return v.nrParams == 0 && !v.resultName && v.paramSigs.empty() && v.genericParams.empty() && v.typeVars.empty() && v.metaRefParams.empty() && !v.hasRestParam && !v.definitionChecked && !v.isVariantSet && !v.signatureOnly && !v.resultIsFunction && !v.resultIsGenericUnit && v.resultSig.expired() && v.resultSigTypeArgs.empty() && v.paramSigNames.empty() && !v.resultSigName && v.pendingParamSigs.empty() && !v.pendingResultSigName; }
	static_quick_assoc<const TreeItem*, FunctionSpecData> s_FunctionSpecAssoc;

	static TokenID t_gcAny          = GetTokenID_st("any");
	static TokenID t_gcNumerics     = GetTokenID_st("numerics");
	static TokenID t_gcIntegers     = GetTokenID_st("integers");
	static TokenID t_gcFloats       = GetTokenID_st("floats");
	static TokenID t_gcUInts        = GetTokenID_st("uints");
	static TokenID t_gcUnsignedInts = GetTokenID_st("unsigned_ints");
	static TokenID t_gcSInts        = GetTokenID_st("sints");
	static TokenID t_gcSignedInts   = GetTokenID_st("signed_ints");
	static TokenID t_gcDomains      = GetTokenID_st("domains");
	static TokenID t_gcPoints       = GetTokenID_st("points");
	static TokenID t_gcDomainPoints = GetTokenID_st("domain_points");
	static TokenID t_gcSignedDomainPoints = GetTokenID_st("signed_domain_points");
	static TokenID t_gcUnsignedDomainPoints = GetTokenID_st("unsigned_domain_points");
}

TIC_CALL bool IsKnownGenericConstraint(TokenID constraintName)
{
	return constraintName == t_gcAny
		|| constraintName == t_gcNumerics
		|| constraintName == t_gcIntegers
		|| constraintName == t_gcFloats
		|| constraintName == t_gcUInts
		|| constraintName == t_gcUnsignedInts
		|| constraintName == t_gcSInts
		|| constraintName == t_gcSignedInts
		|| constraintName == t_gcDomains
		|| constraintName == t_gcPoints
		|| constraintName == t_gcDomainPoints
		|| constraintName == t_gcSignedDomainPoints
		|| constraintName == t_gcUnsignedDomainPoints;
}

bool MatchesGenericConstraint(const ValueClass* vc, TokenID constraintName)
{
	if (!vc)
		return false;
	if (constraintName == t_gcAny)      return true;
	if (constraintName == t_gcNumerics) return vc->IsNumeric();
	if (constraintName == t_gcIntegers) return vc->IsIntegral();
	if (constraintName == t_gcFloats)   return vc->IsNumeric() && !vc->IsIntegral();
	if (constraintName == t_gcUInts || constraintName == t_gcUnsignedInts)
		return vc->IsIntegral() && !vc->IsSigned();
	if (constraintName == t_gcSInts || constraintName == t_gcSignedInts)
		return vc->IsIntegral() && vc->IsSigned();
	if (constraintName == t_gcDomains)  return vc->IsCountable();
	if (constraintName == t_gcPoints)   return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single;
	if (constraintName == t_gcDomainPoints)
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable();
	if (constraintName == t_gcSignedDomainPoints)
		// domain_points restricted to SIGNED coordinates (spoint, ipoint): only those
		// can carry a negative cell offset. Signedness lives on the coordinate type,
		// not on the point value class -- is_signed<Point<T>> is false for every T --
		// so consult the scalar class, exactly as IsCountable() does for integrality.
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable()
			&& vc->GetScalarClass() && vc->GetScalarClass()->IsSigned();
	if (constraintName == t_gcUnsignedDomainPoints)
		// the complement within domain_points (wpoint, upoint): see the note above
		return vc->GetNrDims() == 2 && vc->GetValueComposition() == ValueComposition::Single && vc->IsCountable()
			&& vc->GetScalarClass() && !vc->GetScalarClass()->IsSigned();
	return false;
}

TIC_CALL void TreeItem_SetFunctionSpec(const TreeItem* functionItem, UInt32 nrParams, TokenID resultName)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc.assocOrErase(functionItem, FunctionSpecData{ nrParams, resultName, {} });
}

TIC_CALL void TreeItem_AddFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* signatureExemplar, std::vector<TokenID> typeArgs, TokenID sourceName)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(signatureExemplar && signatureExemplar->IsFunctionItem());
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.paramSigs.emplace_back(paramIndex, signatureExemplar->weak_from_this(), std::move(typeArgs));
	if (sourceName)
		spec.paramSigNames.emplace_back(paramIndex, sourceName);
}

TokenID TreeItem_GetFunctionParamSigName(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& sigName : specPtr->paramSigNames)
			if (sigName.first == paramIndex)
				return sigName.second;
	return {};
}

SharedTreeItem TreeItem_GetFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& paramSig : specPtr->paramSigs)
			if (std::get<0>(paramSig) == paramIndex)
				return SharedTreeItem(std::get<1>(paramSig).lock());
	return {};
}

// K11a by-example: a 'p: exemplar' parameter whose exemplar is a UNIT -- the
// exemplar's declared sub-items serve as the parameter's member block for the
// definition-time checker (the parse-time clone carries only the class).
TIC_CALL void TreeItem_AddFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* exemplar)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(exemplar);
	s_FunctionSpecAssoc[functionItem].paramTypeExemplars.emplace_back(paramIndex, exemplar->weak_from_this());
}

SharedTreeItem TreeItem_GetFunctionParamTypeExemplar(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& pe : specPtr->paramTypeExemplars)
			if (pe.first == paramIndex)
				return SharedTreeItem(pe.second.lock());
	return {};
}

TIC_CALL void TreeItem_SetFunctionRestParam(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].hasRestParam = true;
}

TIC_CALL bool TreeItem_HasFunctionRestParam(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->hasRestParam;
}

TIC_CALL void TreeItem_AddFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].metaRefParams.push_back(paramIndex);
}

bool TreeItem_IsFunctionMetaRefParam(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (UInt32 idx : specPtr->metaRefParams)
			if (idx == paramIndex)
				return true;
	return false;
}

const std::vector<TokenID>* TreeItem_GetFunctionParamSigTypeArgs(const TreeItem* functionItem, UInt32 paramIndex)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		for (const auto& paramSig : specPtr->paramSigs)
			if (std::get<0>(paramSig) == paramIndex && !std::get<2>(paramSig).empty())
				return &std::get<2>(paramSig);
	return nullptr;
}

TIC_CALL void TreeItem_SetFunctionTypeVars(const TreeItem* functionItem, std::vector<std::pair<TokenID, TokenID>> typeVars)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].typeVars = std::move(typeVars);
}

const std::vector<std::pair<TokenID, TokenID>>* TreeItem_GetFunctionTypeVars(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr && !specPtr->typeVars.empty())
		return &specPtr->typeVars;
	return nullptr;
}

TIC_CALL void TreeItem_SetFunctionSignatureOnly(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].signatureOnly = true;
}

bool TreeItem_IsFunctionSignatureOnly(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->signatureOnly;
}

// §5.10: record a function-valued result. `resultSigExemplar` (the '-> sigAlias<...>' exemplar,
// may be null for a bare '-> function') + its type-application args enable faithful rendering.
TIC_CALL void TreeItem_SetFunctionResultSig(const TreeItem* functionItem, bool resultIsFunction, const TreeItem* resultSigExemplar, std::vector<TokenID> typeArgs, TokenID sourceName)
{
	assert(functionItem && functionItem->IsFunctionItem());
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.resultIsFunction = resultIsFunction;
	if (resultSigExemplar)
		spec.resultSig = resultSigExemplar->weak_from_this();
	spec.resultSigTypeArgs = std::move(typeArgs);
	spec.resultSigName = sourceName;
}

bool TreeItem_IsFunctionResultFunction(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->resultIsFunction;
}

TIC_CALL void TreeItem_SetFunctionResultGenericUnit(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].resultIsGenericUnit = true;
}

bool TreeItem_IsFunctionResultGenericUnit(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->resultIsGenericUnit;
}

SharedTreeItem TreeItem_GetFunctionResultSig(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr)
		return SharedTreeItem(specPtr->resultSig.lock());
	return {};
}

const std::vector<TokenID>* TreeItem_GetFunctionResultSigTypeArgs(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (specPtr && !specPtr->resultSigTypeArgs.empty())
		return &specPtr->resultSigTypeArgs;
	return nullptr;
}

TokenID TreeItem_GetFunctionResultSigName(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr ? specPtr->resultSigName : TokenID();
}

// ===================================== #1252: type references and deferred resolution

namespace {
	// raw (non-updating) child lookup: this runs under the parser's no-UpdateMetaInfo lock
	const TreeItem* FindSubItemRaw(const TreeItem* parent, TokenID id)
	{
		for (const TreeItem* c = parent->_GetFirstSubItem(); c; c = c->GetNextItem())
			if (c->GetNameID() == id)
				return c;
		return nullptr;
	}

	// follow the '/'-separated segments in [b, e) below cursor; an empty range yields cursor
	const TreeItem* FollowSubPathRaw(const TreeItem* cursor, CharPtr b, CharPtr e)
	{
		while (cursor && b != e)
		{
			CharPtr segEnd = std::find(b, e, '/');
			cursor = FindSubItemRaw(cursor, GetTokenID_mt(b, segEnd));
			b = (segEnd == e) ? e : segEnd + 1;
		}
		return cursor;
	}
}

const TreeItem* TreeItem_ResolveTypeRefRaw(const TreeItem* context, CharPtr b, CharPtr e)
{
	if (!context || b == e)
		return nullptr;

	if (*b == '/') // absolute: from the root of the tree that context lives in
	{
		const TreeItem* root = context;
		while (const TreeItem* parent = root->GetTreeParent().get())
			root = parent;
		return FollowSubPathRaw(root, b + 1, e);
	}

	if (*b == '.') // dots on the declaring namespace, as FollowDots reads them
	{
		CharPtr dotsEnd = b;
		while (dotsEnd != e && *dotsEnd == '.')
			++dotsEnd;
		const TreeItem* cursor = context;
		for (CharPtr dot = b + 1; dot != dotsEnd && cursor; ++dot) // the first dot is the namespace itself
			cursor = cursor->GetTreeParent().get();
		if (!cursor)
			return nullptr; // ascended above the root
		if (dotsEnd == e)
			return cursor;
		if (*dotsEnd != '/')
			return nullptr; // '..x' is not a path
		return FollowSubPathRaw(cursor, dotsEnd + 1, e);
	}

	CharPtr slash = std::find(b, e, '/');
	TokenID firstTok = GetTokenID_mt(b, slash);
	for (const TreeItem* scope = context; scope; scope = scope->GetTreeParent().get())
	{
		const TreeItem* found = FindSubItemRaw(scope, firstTok);
		if (!found)
			continue;
		return FollowSubPathRaw(found, (slash == e) ? e : slash + 1, e); // nearest scope wins, no fall-through
	}
	return nullptr;
}

TIC_CALL void TreeItem_AddPendingFunctionParamSig(const TreeItem* functionItem, UInt32 paramIndex, TokenID sourceName, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(sourceName);
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.pendingParamSigs.emplace_back(paramIndex, sourceName, std::move(typeArgs));
	spec.paramSigNames.emplace_back(paramIndex, sourceName); // the dump keeps writing what the source wrote
}

TIC_CALL void TreeItem_SetPendingFunctionResultSig(const TreeItem* functionItem, TokenID sourceName, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(sourceName);
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.pendingResultSigName = sourceName;
	spec.pendingResultSigTypeArgs = std::move(typeArgs);
	spec.resultSigName = sourceName;
}

bool TreeItem_HasPendingFunctionSigs(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && (!specPtr->pendingParamSigs.empty() || specPtr->pendingResultSigName);
}

void TreeItem_ResolvePendingFunctionSigs(const TreeItem* functionItem)
{
	if (!TreeItem_HasPendingFunctionSigs(functionItem))
		return;
	auto& spec = s_FunctionSpecAssoc[functionItem]; // mutable: the pending references are consumed here

	// the base is the function item: that is the namespace its parameters are declared in
	auto resolve = [functionItem](TokenID name) -> const TreeItem*
	{
		SharedStr nameStr(name); // materialized: the walk below takes the token registry lock itself
		return TreeItem_ResolveTypeRefRaw(functionItem, nameStr.begin(), nameStr.send());
	};

	auto pendingParams = std::move(spec.pendingParamSigs);
	spec.pendingParamSigs.clear(); // resolved or reported, never a second attempt
	for (const auto& pending : pendingParams)
	{
		auto name = std::get<1>(pending);
		auto found = resolve(name);
		if (!found)
			functionItem->throwItemErrorF("parameter {}: the type '{}' does not resolve to a declared item"
				, std::get<0>(pending) + 1, name);
		if (!found->IsFunctionItem())
			functionItem->throwItemErrorF("parameter {}: the type '{}' is not a function signature; a type by example must be declared before the function that uses it"
				, std::get<0>(pending) + 1, name);
		spec.paramSigs.emplace_back(std::get<0>(pending), found->weak_from_this(), std::get<2>(pending));
	}

	if (auto name = spec.pendingResultSigName)
	{
		spec.pendingResultSigName = {};
		auto found = resolve(name);
		if (!found)
			functionItem->throwItemErrorF("result type: '{}' does not resolve to a declared item", name);
		if (!found->IsFunctionItem())
			functionItem->throwItemErrorF("result type: '{}' is not a function signature", name);
		spec.resultSig = found->weak_from_this();
		spec.resultSigTypeArgs = std::move(spec.pendingResultSigTypeArgs);
		spec.resultIsFunction = true;
	}
}

// ===================================== §5.7 v2: variant specificity / disjointness

namespace {

	using VariantParamSet = std::bitset<UInt32(ValueClassID::VT_Count)>;

	// the set of value classes a variant parameter accepts, over the CLOSED value-class
	// universe: a generic values-variable -> its constraint's subset; a concrete
	// script-named class -> singleton; anything else (plain items, composite types,
	// function-typed parameters, item-spec units) -> everything ("soft" wildcard).
	// Token-based only: safe at parse time (no meta machinery).
	VariantParamSet VariantParamMatchSet(const TreeItem* variant, UInt32 paramIndex, const TreeItem* param, bool* isHard)
	{
		VariantParamSet s;
		UInt32 seqNr = 0, idx; TokenID var, cons; bool isDom;
		while (TreeItem_GetFunctionGenericParam(variant, seqNr++, &idx, &var, &cons, &isDom))
			if (idx == paramIndex && !isDom)
			{
				for (UInt32 v = 0; v != UInt32(ValueClassID::VT_Count); ++v)
					if (auto vc = ValueClass::FindByValueClassID(ValueClassID(v)))
						if (MatchesGenericConstraint(vc, cons))
							s.set(v);
				if (isHard) *isHard = true;
				return s;
			}
		const ValueClass* vc = nullptr;
		if (IsDataItem(param))
			vc = ValueClass::FindByScriptName(AsDataItem(param)->ValuesUnitToken());
		else if (IsUnit(param))
			vc = AsUnit(param)->GetValueType();
		if (vc)
		{
			s.set(UInt32(vc->GetValueClassID()));
			if (isHard) *isHard = true;
			return s;
		}
		s.set(); // wildcard
		if (isHard) *isHard = false;
		return s;
	}

	struct VariantMatchInfo
	{
		const TreeItem*              variant = nullptr;
		std::vector<VariantParamSet> sets;
		bool                         allHard = true;
	};

	VariantMatchInfo GetVariantMatchInfo(const TreeItem* variant)
	{
		VariantMatchInfo r;
		r.variant = variant;
		UInt32 np = TreeItem_GetFunctionParamCount(variant);
		r.sets.reserve(np);
		const TreeItem* param = variant->_GetFirstSubItem();
		for (UInt32 i = 0; i != np && param; ++i, param = param->GetNextItem())
		{
			bool hard = false;
			r.sets.push_back(VariantParamMatchSet(variant, i, param, &hard));
			r.allHard = r.allHard && hard;
		}
		return r;
	}

	// -1: a strictly more specific than b; +1: b strictly more specific; 0: identical
	// coverage; 2: incomparable. Requires equal arity.
	int CompareVariantInfo(const VariantMatchInfo& a, const VariantMatchInfo& b)
	{
		bool aLEb = true, bLEa = true;
		for (SizeT i = 0; i != a.sets.size(); ++i)
		{
			if ((a.sets[i] & ~b.sets[i]).any()) aLEb = false;
			if ((b.sets[i] & ~a.sets[i]).any()) bLEa = false;
		}
		if (aLEb && bLEa) return 0;
		if (aLEb) return -1;
		if (bLEa) return +1;
		return 2;
	}

} // anonymous namespace

bool TreeItem_VariantMatches(const TreeItem* variant, const std::vector<const ValueClass*>& argVCs)
{
	UInt32 np = TreeItem_GetFunctionParamCount(variant);
	// a '...x' rest variant binds one-or-more trailing arguments through its LAST
	// param: it matches any argument count >= its declared param count
	bool hasRest = TreeItem_HasFunctionRestParam(variant);
	if (hasRest ? argVCs.size() < np : argVCs.size() != np)
		return false;
	auto info = GetVariantMatchInfo(variant);
	for (SizeT i = 0; i != argVCs.size(); ++i)
	{
		const auto& acceptSet = info.sets[std::min<SizeT>(i, info.sets.size() - 1)]; // rest tail: the last param's set
		if (!argVCs[i])
		{
			if (!acceptSet.all())
				return false; // a non-class argument (function value, literal) only matches a wildcard position
			continue;
		}
		if (!acceptSet.test(UInt32(argVCs[i]->GetValueClassID())))
			return false;
	}
	return true;
}

int TreeItem_CompareVariantSpecificity(const TreeItem* a, const TreeItem* b)
{
	auto ia = GetVariantMatchInfo(a), ib = GetVariantMatchInfo(b);
	// unequal declared arity (possible when a rest variant and a fixed/longer variant
	// both match one call): the variant with MORE declared params is more specific
	if (ia.sets.size() != ib.sets.size())
		return ia.sets.size() > ib.sets.size() ? -1 : +1;
	return CompareVariantInfo(ia, ib);
}

TIC_CALL void TreeItem_CheckVariantSetDisjointness(const TreeItem* setItem)
{
	// definition-time (§5.7 v2): two variants whose acceptance sets overlap must be
	// specificity-ordered -- identical or incomparable overlapping coverage is an
	// error now instead of a per-call ambiguity later. Pairs with a "soft" position
	// (unresolvable/wildcard type) are left to the call-time ambiguity guard.
	std::vector<VariantMatchInfo> infos;
	for (const TreeItem* v = setItem->_GetFirstSubItem(); v; v = v->GetNextItem())
		if (v->IsFunctionItem())
			infos.push_back(GetVariantMatchInfo(v));

	for (SizeT i = 0; i != infos.size(); ++i)
		for (SizeT j = i + 1; j != infos.size(); ++j)
		{
			const auto& a = infos[i]; const auto& b = infos[j];
			if (a.sets.size() != b.sets.size() || !a.allHard || !b.allHard)
				continue;
			bool overlap = true;
			for (SizeT k = 0; k != a.sets.size() && overlap; ++k)
				if (!(a.sets[k] & b.sets[k]).any())
					overlap = false;
			if (!overlap)
				continue;
			int cmp = CompareVariantInfo(a, b);
			if (cmp == 0)
				throwDmsErrF("variant set '{}': variants '{}' and '{}' accept identical argument types"
					, setItem->GetFullName().c_str(), a.variant->GetNameID(), b.variant->GetNameID());
			if (cmp == 2)
				throwDmsErrF("variant set '{}': variants '{}' and '{}' overlap without one being more specific than the other; split their parameter types"
					, setItem->GetFullName().c_str(), a.variant->GetNameID(), b.variant->GetNameID());
		}
}

void TreeItem_CopyFunctionSpec(const TreeItem* dstFunctionItem, const TreeItem* srcFunctionItem)
{
	assert(dstFunctionItem && dstFunctionItem->IsFunctionItem());
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(srcFunctionItem);
	if (specPtr)
		s_FunctionSpecAssoc.assocOrErase(dstFunctionItem, *specPtr);
}

void TreeItem_EraseFunctionSpec(const TreeItem* functionItem)
{
	s_FunctionSpecAssoc.erase(functionItem);
}

TIC_CALL void TreeItem_AddFunctionGenericParam(const TreeItem* functionItem, UInt32 paramIndex, TokenID varName, TokenID constraintName, bool isDomainVar)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].genericParams.emplace_back(paramIndex, varName, constraintName, isDomainVar);
}

bool TreeItem_GetFunctionGenericParam(const TreeItem* functionItem, UInt32 seqNr, UInt32* paramIndex, TokenID* varName, TokenID* constraintName, bool* isDomainVar)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (!specPtr || seqNr >= specPtr->genericParams.size())
		return false;
	const auto& genericParam = specPtr->genericParams[seqNr];
	if (paramIndex)     *paramIndex     = std::get<0>(genericParam);
	if (varName)        *varName        = std::get<1>(genericParam);
	if (constraintName) *constraintName = std::get<2>(genericParam);
	if (isDomainVar)    *isDomainVar    = std::get<3>(genericParam);
	return true;
}

bool TreeItem_IsFunctionDefinitionChecked(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->definitionChecked;
}

void TreeItem_SetFunctionDefinitionChecked(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].definitionChecked = true;
}

TIC_CALL void TreeItem_SetFunctionVariantSet(const TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].isVariantSet = true;
}

TIC_CALL bool TreeItem_IsFunctionVariantSet(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr && specPtr->isVariantSet;
}

TIC_CALL UInt32 TreeItem_GetFunctionParamCount(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr ? specPtr->nrParams : 0;
}

TokenID TreeItem_GetFunctionResultName(const TreeItem* functionItem)
{
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	return specPtr ? specPtr->resultName : TokenID::GetEmptyID();
}

TIC_CALL void TreeItem_MakeStrictScope(TreeItem* functionItem)
{
	assert(functionItem && functionItem->IsFunctionItem());
	functionItem->GetUsingCache(); // initialize own items, declared usings, then definition namespace
}

// ===================================== #1261: the spec as one text, for the XML notation

namespace {

	// The text is a space separated list of 'key' or 'key=value' fields. Values never contain a
	// space: they are token names, dotted or '/'-separated paths, and small numbers. That matters,
	// because the reader of the XML notation collapses every run of white space in element text to
	// one space (XmlParser::ReadText), so a field may not rely on any other layout.
	void AppendField(SharedStr& out, CharPtr field)
	{
		if (!out.empty())
			out += " ";
		out += field;
	}

	void AppendTypeArgs(SharedStr& out, const std::vector<TokenID>* typeArgs)
	{
		if (!typeArgs || typeArgs->empty())
			return;
		out += ":";
		bool first = true;
		for (auto arg : *typeArgs)
		{
			if (!first)
				out += ",";
			first = false;
			out += SharedStr(arg);
		}
	}

	// the reference to write for a signature or type exemplar: what the source wrote when that was
	// recorded (#1252), and otherwise a name that resolves from the function item itself
	auto ExemplarRef(const TreeItem* functionItem, TokenID sourceName, const TreeItem* exemplar) -> SharedStr
	{
		if (sourceName)
			return SharedStr(sourceName);
		if (exemplar)
			return functionItem->GetFindableName(exemplar);
		return {};
	}

	// [b, e) split on `sep`, without allocating a string per piece
	auto SplitOn(CharPtrRange range, char sep) -> std::vector<CharPtrRange>
	{
		std::vector<CharPtrRange> result;
		CharPtr b = range.begin(), e = range.end();
		while (b <= e)
		{
			CharPtr p = b;
			while (p != e && *p != sep)
				++p;
			result.emplace_back(b, p);
			if (p == e)
				break;
			b = p + 1;
		}
		return result;
	}

	auto ParseTypeArgs(CharPtrRange range) -> std::vector<TokenID>
	{
		std::vector<TokenID> result;
		if (range.empty())
			return result;
		for (auto piece : SplitOn(range, ','))
			if (!piece.empty())
				result.push_back(GetTokenID_mt(SharedStr(piece).c_str()));
		return result;
	}

	UInt32 ParseIndex(const TreeItem* functionItem, CharPtrRange range, CharPtr fieldName)
	{
		SharedStr text(range);
		CharPtr b = text.c_str();
		if (!*b)
			functionItem->throwItemErrorF("FunctionSpec: '{}' has no parameter index", fieldName);
		UInt32 result = 0;
		for (CharPtr p = b; *p; ++p)
		{
			if (*p < '0' || *p > '9')
				functionItem->throwItemErrorF("FunctionSpec: '{}' has a parameter index that is not a number: '{}'", fieldName, b);
			result = result * 10 + UInt32(*p - '0');
		}
		return result;
	}

} // anonymous namespace

TIC_CALL auto TreeItem_GetFunctionSpecAsStr(const TreeItem* functionItem) -> SharedStr
{
	assert(functionItem && functionItem->IsFunctionItem());
	SharedStr result;
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(functionItem);
	if (!specPtr)
		return SharedStr("params=0"); // a function with no declaration of its own still is one
	const auto& spec = *specPtr;

	AppendField(result, mgFormat2SharedStr("params={}", spec.nrParams).c_str());
	if (spec.resultName)
		AppendField(result, mgFormat2SharedStr("result={}", spec.resultName).c_str());
	if (spec.isVariantSet)
		AppendField(result, "variantset");
	if (spec.signatureOnly)
		AppendField(result, "sigonly");
	if (spec.hasRestParam)
		AppendField(result, "rest");
	if (spec.resultIsFunction)
		AppendField(result, "resultfn");
	if (spec.resultIsGenericUnit)
		AppendField(result, "resultgenunit");

	for (const auto& tv : spec.typeVars)
		AppendField(result, mgFormat2SharedStr("typevar={}:{}", tv.first, tv.second).c_str());

	for (const auto& gp : spec.genericParams)
		AppendField(result, mgFormat2SharedStr("generic={}:{}:{}:{}"
			, std::get<0>(gp), std::get<1>(gp), std::get<2>(gp), std::get<3>(gp) ? 1 : 0).c_str());

	for (auto idx : spec.metaRefParams)
		AppendField(result, mgFormat2SharedStr("metaref={}", idx).c_str());

	// A parameter signature is written by REFERENCE, never by the exemplar's identity: the reader
	// re-resolves it from the function item, so a signature declared further down the same file
	// resolves exactly as a forward reference in .dms syntax does.
	for (const auto& ps : spec.paramSigs)
	{
		UInt32 idx = std::get<0>(ps);
		auto ref = ExemplarRef(functionItem, TreeItem_GetFunctionParamSigName(functionItem, idx), std::get<1>(ps).lock().get());
		if (ref.empty())
			continue;
		SharedStr field = mgFormat2SharedStr("paramsig={}:{}", idx, ref);
		AppendTypeArgs(field, &std::get<2>(ps));
		AppendField(result, field.c_str());
	}
	// a signature that has not resolved yet carries no exemplar, so it is written from its name
	for (const auto& pps : spec.pendingParamSigs)
	{
		SharedStr field = mgFormat2SharedStr("paramsig={}:{}", std::get<0>(pps), std::get<1>(pps));
		AppendTypeArgs(field, &std::get<2>(pps));
		AppendField(result, field.c_str());
	}

	for (const auto& pe : spec.paramTypeExemplars)
	{
		auto ref = ExemplarRef(functionItem, TokenID(), pe.second.lock().get());
		if (!ref.empty())
			AppendField(result, mgFormat2SharedStr("paramex={}:{}", pe.first, ref).c_str());
	}

	if (auto resultRef = ExemplarRef(functionItem, spec.resultSigName, spec.resultSig.lock().get()); !resultRef.empty())
	{
		SharedStr field = mgFormat2SharedStr("resultsig={}", resultRef);
		AppendTypeArgs(field, spec.pendingResultSigName ? &spec.pendingResultSigTypeArgs : &spec.resultSigTypeArgs);
		AppendField(result, field.c_str());
	}
	return result;
}

TIC_CALL void TreeItem_SetFunctionSpecFromStr(TreeItem* functionItem, CharPtr specStr)
{
	assert(functionItem);
	assert(specStr);

	// first, so that every setter below meets its IsFunctionItem precondition. SetIsFunction implies
	// SetIsTemplate, so it does not matter whether the IsTemplate property element came first.
	functionItem->SetIsFunction();

	UInt32 nrParams = 0;
	TokenID resultName;
	bool variantSet = false, signatureOnly = false, hasRest = false;
	bool resultIsFunction = false, resultIsGenericUnit = false;
	std::vector<std::pair<TokenID, TokenID>> typeVars;
	std::vector<std::tuple<UInt32, TokenID, TokenID, bool>> genericParams;
	std::vector<UInt32> metaRefParams;
	std::vector<std::tuple<UInt32, TokenID, std::vector<TokenID>>> paramSigs;
	std::vector<std::pair<UInt32, SharedStr>> paramExemplars;
	TokenID resultSigName;
	std::vector<TokenID> resultSigTypeArgs;

	for (auto field : SplitOn(CharPtrRange(specStr, specStr + StrLen(specStr)), ' '))
	{
		if (field.empty())
			continue;
		CharPtr eq = field.begin();
		while (eq != field.end() && *eq != '=')
			++eq;
		CharPtrRange key(field.begin(), eq);
		CharPtrRange value(eq == field.end() ? eq : eq + 1, field.end());
		SharedStr keyStr(key);

		if (keyStr == "params")
			nrParams = ParseIndex(functionItem, value, "params");
		else if (keyStr == "result")
			resultName = GetTokenID_mt(SharedStr(value).c_str());
		else if (keyStr == "variantset")
			variantSet = true;
		else if (keyStr == "sigonly")
			signatureOnly = true;
		else if (keyStr == "rest")
			hasRest = true;
		else if (keyStr == "resultfn")
			resultIsFunction = true;
		else if (keyStr == "resultgenunit")
			resultIsGenericUnit = true;
		else if (keyStr == "typevar")
		{
			auto parts = SplitOn(value, ':');
			if (parts.size() != 2)
				functionItem->throwItemErrorF("FunctionSpec: 'typevar' expects '<var>:<constraint>', got '{}'", SharedStr(value));
			typeVars.emplace_back(GetTokenID_mt(SharedStr(parts[0]).c_str()), GetTokenID_mt(SharedStr(parts[1]).c_str()));
		}
		else if (keyStr == "generic")
		{
			auto parts = SplitOn(value, ':');
			if (parts.size() != 4)
				functionItem->throwItemErrorF("FunctionSpec: 'generic' expects '<index>:<var>:<constraint>:<isDomainVar>', got '{}'", SharedStr(value));
			genericParams.emplace_back(ParseIndex(functionItem, parts[0], "generic")
				, GetTokenID_mt(SharedStr(parts[1]).c_str())
				, GetTokenID_mt(SharedStr(parts[2]).c_str())
				, SharedStr(parts[3]) == "1");
		}
		else if (keyStr == "metaref")
			metaRefParams.push_back(ParseIndex(functionItem, value, "metaref"));
		else if (keyStr == "paramsig")
		{
			auto parts = SplitOn(value, ':');
			if (parts.size() < 2)
				functionItem->throwItemErrorF("FunctionSpec: 'paramsig' expects '<index>:<name>[:<typeargs>]', got '{}'", SharedStr(value));
			paramSigs.emplace_back(ParseIndex(functionItem, parts[0], "paramsig")
				, GetTokenID_mt(SharedStr(parts[1]).c_str())
				, parts.size() > 2 ? ParseTypeArgs(parts[2]) : std::vector<TokenID>{});
		}
		else if (keyStr == "paramex")
		{
			auto parts = SplitOn(value, ':');
			if (parts.size() != 2)
				functionItem->throwItemErrorF("FunctionSpec: 'paramex' expects '<index>:<name>', got '{}'", SharedStr(value));
			paramExemplars.emplace_back(ParseIndex(functionItem, parts[0], "paramex"), SharedStr(parts[1]));
		}
		else if (keyStr == "resultsig")
		{
			auto parts = SplitOn(value, ':');
			if (parts.empty() || parts[0].empty())
				functionItem->throwItemErrorF("FunctionSpec: 'resultsig' expects '<name>[:<typeargs>]', got '{}'", SharedStr(value));
			resultSigName = GetTokenID_mt(SharedStr(parts[0]).c_str());
			if (parts.size() > 1)
				resultSigTypeArgs = ParseTypeArgs(parts[1]);
		}
		else
			functionItem->throwItemErrorF("FunctionSpec: unknown field '{}'", keyStr);
	}

	// the order of ConfigProd::OnFunctionDeclEnd: the spec first, because SetFunctionSpec replaces
	// the whole record, then everything that adds to it
	TreeItem_SetFunctionSpec(functionItem, nrParams, resultName);
	if (variantSet)
		TreeItem_SetFunctionVariantSet(functionItem);

	// Resolve now when the reference already names a declared signature, and defer only when it does
	// not, which is the order ConfigProd works in. It matters for more than speed: only a RESOLVED
	// signature carries its type-application arguments where the config dump reads them
	// (TreeItem_GetFunctionParamSigTypeArgs looks in paramSigs), so a spec that deferred everything
	// wrote 'f: nuf' for what the source declared as 'f: nuf<Vx, Dx>'.
	auto resolveSig = [functionItem](TokenID name) -> const TreeItem*
	{
		SharedStr nameStr(name); // materialized: the walk takes the token registry lock itself
		auto found = TreeItem_ResolveTypeRefRaw(functionItem, nameStr.begin(), nameStr.send());
		return (found && found->IsFunctionItem()) ? found : nullptr;
	};
	for (const auto& ps : paramSigs)
	{
		if (auto found = resolveSig(std::get<1>(ps)))
			TreeItem_AddFunctionParamSignature(functionItem, std::get<0>(ps), found, std::get<2>(ps), std::get<1>(ps));
		else
			TreeItem_AddPendingFunctionParamSig(functionItem, std::get<0>(ps), std::get<1>(ps), std::get<2>(ps));
	}
	for (const auto& pe : paramExemplars)
	{
		// K11a by-example: resolved here rather than deferred, as ConfigProd resolves it; the dump
		// preserves declaration order, so an exemplar that resolved for the .dms source resolves here
		auto exemplar = TreeItem_ResolveTypeRefRaw(functionItem, pe.second.begin(), pe.second.send());
		if (!exemplar)
			functionItem->throwItemErrorF("FunctionSpec: parameter {}: the type example '{}' does not resolve to a declared item"
				, pe.first + 1, pe.second);
		TreeItem_AddFunctionParamTypeExemplar(functionItem, pe.first, exemplar);
	}
	for (const auto& gp : genericParams)
		TreeItem_AddFunctionGenericParam(functionItem, std::get<0>(gp), std::get<1>(gp), std::get<2>(gp), std::get<3>(gp));
	for (auto idx : metaRefParams)
		TreeItem_AddFunctionMetaRefParam(functionItem, idx);
	if (hasRest)
		TreeItem_SetFunctionRestParam(functionItem);
	if (signatureOnly)
		TreeItem_SetFunctionSignatureOnly(functionItem);
	if (resultSigName)
	{
		if (auto found = resolveSig(resultSigName))
			TreeItem_SetFunctionResultSig(functionItem, true, found, resultSigTypeArgs, resultSigName);
		else
			TreeItem_SetPendingFunctionResultSig(functionItem, resultSigName, resultSigTypeArgs);
	}
	else if (resultIsFunction)
		TreeItem_SetFunctionResultSig(functionItem, true, nullptr, {}, TokenID()); // a bare '-> function'
	if (resultIsGenericUnit)
		TreeItem_SetFunctionResultGenericUnit(functionItem);
	if (!typeVars.empty())
		TreeItem_SetFunctionTypeVars(functionItem, std::move(typeVars));
	TreeItem_MakeStrictScope(functionItem);
}
