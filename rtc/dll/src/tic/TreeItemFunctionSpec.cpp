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
	};
	bool IsDefaultValue(const FunctionSpecData& v) { return v.nrParams == 0 && !v.resultName && v.paramSigs.empty() && v.genericParams.empty() && v.typeVars.empty() && v.metaRefParams.empty() && !v.hasRestParam && !v.definitionChecked && !v.isVariantSet && !v.signatureOnly && !v.resultIsFunction && !v.resultIsGenericUnit && v.resultSig.expired() && v.resultSigTypeArgs.empty(); }
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
	s_FunctionSpecAssoc.assoc(functionItem, FunctionSpecData{ nrParams, resultName, {} });
}

TIC_CALL void TreeItem_AddFunctionParamSignature(const TreeItem* functionItem, UInt32 paramIndex, const TreeItem* signatureExemplar, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	assert(signatureExemplar && signatureExemplar->IsFunctionItem());
	s_FunctionSpecAssoc[functionItem].paramSigs.emplace_back(paramIndex, signatureExemplar->weak_from_this(), std::move(typeArgs));
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
TIC_CALL void TreeItem_SetFunctionResultSig(const TreeItem* functionItem, bool resultIsFunction, const TreeItem* resultSigExemplar, std::vector<TokenID> typeArgs)
{
	assert(functionItem && functionItem->IsFunctionItem());
	auto& spec = s_FunctionSpecAssoc[functionItem];
	spec.resultIsFunction = resultIsFunction;
	if (resultSigExemplar)
		spec.resultSig = resultSigExemplar->weak_from_this();
	spec.resultSigTypeArgs = std::move(typeArgs);
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
					, setItem->GetFullName().c_str(), a.variant->GetID(), b.variant->GetID());
			if (cmp == 2)
				throwDmsErrF("variant set '{}': variants '{}' and '{}' overlap without one being more specific than the other; split their parameter types"
					, setItem->GetFullName().c_str(), a.variant->GetID(), b.variant->GetID());
		}
}

void TreeItem_CopyFunctionSpec(const TreeItem* dstFunctionItem, const TreeItem* srcFunctionItem)
{
	assert(dstFunctionItem && dstFunctionItem->IsFunctionItem());
	auto specPtr = s_FunctionSpecAssoc.get_value_ptr(srcFunctionItem);
	if (specPtr)
		s_FunctionSpecAssoc.assoc(dstFunctionItem, *specPtr);
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
