// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "StxPCH.h"
#include "TreeItemFunctionSpec.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ConfigFileName.h"
#include "ConfigProd.h"
#include "StxInterface.h"

#include "dbg/SeverityType.h"
#include "mci/PropDef.h"
#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/StrFormat.h"

#include "stg/StorageInterface.h"

#include "AbstrDataItem.h"
#include "AbstrUnit.h"
#include "DataItemClass.h"
#include "ExprRewrite.h"
#include "OperGroups.h"
#include "PropDefInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include <algorithm>
#include <cctype>
#include <string>

#include <stdarg.h>

// The typed-function (HOF) language machinery of ConfigProd: type variables
// and their constraints, function literals and pending-lambda
// materialization/hoisting, item headings/aliases/type refs, and the
// function-declaration productions (OnFunctionHeading .. OnFunctionDeclEnd).
// Split from ConfigProd.cpp (2026-08); the classic item/property/storage
// productions stay there.

static const TreeItem* FindSubItemRaw(const TreeItem* parent, TokenID id); // defined below (raw, parse-safe)

bool ConfigProd::IsTopLevelFunctionParam() const
{
	// a top-level parameter is a direct child of the function item; a member of a
	// structured parameter (e.g. 'attribute<V> flow' inside 'unit<uint32> Net { ... }')
	// is one level deeper and must not be recorded as a parameter-indexed generic/signature
	return !m_FuncStates.empty() && m_pCurrent
		&& m_pCurrent->GetTreeParent().get() == m_FuncStates.back().funcItem.get();
}

TokenID ConfigProd::FindActiveTypeVarConstraint(TokenID varName) const
{
	// lexical: a nested function sees the type variables of its enclosing function
	// declarations, nearest first (§4.6/§5.10)
	if (varName)
		for (auto fsIt = m_FuncStates.rbegin(); fsIt != m_FuncStates.rend(); ++fsIt)
			for (const auto& typeVar : fsIt->typeVars)
				if (typeVar.first == varName)
					return typeVar.second;
	return TokenID::GetEmptyID();
}

void ConfigProd::OnTypeVarName()
{
	m_PendingTypeVarName = m_strIdentifierID;
}

void ConfigProd::OnTypeVarConstraint()
{
	if (!IsKnownGenericConstraint(m_strIdentifierID))
		throwSemanticError(mgFormat2string("unknown type-variable constraint '{}'; known constraints: any, numerics, integers, floats, uints/unsigned_ints, sints/signed_ints, domains, points, domain_points"
			, GetTokenStr(m_strIdentifierID)).c_str());
	m_PendingTypeVars.emplace_back(m_PendingTypeVarName, m_strIdentifierID);
}

StaticLateTokenID st_Arc("arc");
StaticLateTokenID st_Single("single");
StaticLateTokenID st_Sequence("sequence");
StaticLateTokenID st_Poly("poly");
StaticLateTokenID st_Polygon("polygon");
StaticLateTokenID st_MultiPoint("multipoint");

void ConfigProd::DoEntityParam()
{
	if      (m_strIdentifierID == st_Arc     ) SetVC(ValueComposition::Sequence);
	else if (m_strIdentifierID == st_Single  ) SetVC(ValueComposition::Single);
	else if (m_strIdentifierID == st_Sequence) SetVC(ValueComposition::Sequence);
	else if (m_strIdentifierID == st_Poly    ) SetVC(ValueComposition::Polygon);
	else if (m_strIdentifierID == st_Polygon ) SetVC(ValueComposition::Polygon);
	else if (m_strIdentifierID == st_MultiPoint) SetVC(ValueComposition::MultiPoint);
	else
		m_pParamEntity = m_strIdentifierID;
}

void ConfigProd::SetVC (ValueComposition    vc)
{
	if (m_eParamVC != ValueComposition::Unknown)
		throwErrorD("ConfigProd::SetValueComposition", "duplicate specification of ValueComposition not allowed");

	m_eParamVC = vc;
}


// *****************************************************************************
// Function/Procedure:DoUnitRange
// Description:       Set range of the current unit
// *****************************************************************************

// REMOVE COMMENT: Integreer met DoNrOfRowsProp()
void ConfigProd::DoUnitRangeProp(bool isCategorical)
{
	assert(m_pCurrent);
	AbstrUnit* unit = AsCheckedUnit(m_pCurrent.get());
	dms_assert(unit);
	const ValueClass* vc = unit->GetValueType();
	dms_assert(vc);
	UInt32 nrDims = vc->GetNrDims();
	if (nrDims != 2 && !vc->IsNumeric())
		throwSemanticError(mgFormat2string("DoUnitRangeProp: this unit does not allow range assignment because its ValueType is {}", vc->GetName()).c_str());

	const ValueClass* ic = ValueClass::FindByValueClassID(m_eAssignmentDomainType);
	if (ic->GetNrDims() != nrDims || (nrDims == 1 && !ic->IsNumeric()))
		throwSemanticError(mgFormat2string("DoUnitRangeProp: the provided range is incompatible with the ValueType {} of this unit", vc->GetName()).c_str());

	switch (nrDims) {
		case 1:
			dms_assert(vc->IsNumeric() && ic->IsNumeric());
			unit->SetRangeAsFloat64(m_FloatInterval.first, m_FloatInterval.second);
			break;

		case 2:
			unit->SetRangeAsDPoint(
				Top   (m_DPointInterval), Left (m_DPointInterval),
				Bottom(m_DPointInterval), Right(m_DPointInterval)
			);
			break;

		default:
			throwDmsErrF("DoUnitRangeProp: cannot set range of units with ValueType {} ", vc->GetName().c_str());
	}
	unit->SetTSF(USF_HasConfigRange);
}

// *****************************************************************************
// Procedure:DoStorageProp
// Description:       storage property setting for treeitem
// Parameters:        
//     char* p_pFilename: pointer to the file name 
// *****************************************************************************

void ConfigProd::DoStorageProp()
{
	DMS_TreeItem_SetStorageManager(
		m_pCurrent.get(), 
		m_strIdentifierID.GetStr().c_str(),
		m_sPropFileTypeID.GetStr().c_str(),
		StorageReadOnlySetting::Default
	);
}

void ConfigProd::DoFileType()
{
	m_sPropFileTypeID = m_strIdentifierID;
}

// *****************************************************************************
// Function/Procedure:DoUsingProp
// Description:       using property setting for a Treeitem
// Parameters:        
//     char* p_pPath: searchpath to be added for this treeitem
// *****************************************************************************

void ConfigProd::DoUsingProp()
{
	dms_assert(m_pCurrent);
	m_pCurrent->AddUsingUrl(m_strIdentifierID); // splits up on ';' so will look into Str anyway
}

// *****************************************************************************
// Function/Procedure:DoExpressionProp
// Description:       expression property for this tree item
// *****************************************************************************

void ConfigProd::OnExprFunctionLiteral(CharPtr first, CharPtr last)
{
	// a backtracked-and-reparsed context may fire the same literal twice: dedup
	for (const auto& p : m_PendingLambdas)
		if (p.litFirst == first && p.litLast == last)
			return;
	// a literal nested inside an enclosing literal is re-parsed (and lifted) by the
	// enclosing literal's own declaration parse: keep outermost extents only
	while (!m_PendingLambdas.empty()
		&& m_PendingLambdas.back().spliceFirst >= first && m_PendingLambdas.back().spliceLast <= last)
		m_PendingLambdas.pop_back();
	m_PendingLambdas.push_back(PendingLambda{ first, last, first, last });
}

void ConfigProd::OnWidenFunctionLiteral(CharPtr first, CharPtr last)
{
	// '(function ...)' as a parenthesized group: the splice swallows the parentheses,
	// so the lifted name can take call suffixes ('(function ...)(x)' -> '_lambda_n(x)')
	dms_assert(!m_PendingLambdas.empty());
	auto& e = m_PendingLambdas.back();
	dms_assert(first <= e.spliceFirst && e.spliceLast <= last);
	e.spliceFirst = first;
	e.spliceLast = last;
}

SharedStr ConfigProd::MaterializePendingLambdas(CharPtr first, CharPtr last)
{
	if (m_PendingLambdas.empty())
		return SharedStr(CharPtrRange(first, last));

	// lifting a literal from a PARAMETER declaration would insert the hidden item
	// between the parameters, breaking the params-are-the-first-N-sub-items binding
	if (!m_FuncStates.empty() && m_FuncStates.back().inParamList)
		throwSemanticError("a function literal is not supported in a parameter declaration");

	auto lambdas = std::move(m_PendingLambdas);
	m_PendingLambdas.clear();

	std::string acc;
	acc.reserve(last - first);
	CharPtr cursor = first;
	for (const auto& p : lambdas)
	{
		if (p.spliceFirst < cursor || p.spliceLast > last)
			throwSemanticError("ambiguous function-literal capture in this calculation rule; parenthesize the literal or simplify the expression");
		SharedStr name = HoistFunctionLiteral(p.litFirst, p.litLast);
		acc.append(cursor, p.spliceFirst);
		acc.append(name.begin(), name.send());
		cursor = p.spliceLast;
	}
	acc.append(cursor, last);
	return SharedStr(acc.c_str());
}

SharedStr ConfigProd::HoistFunctionLiteral(CharPtr litFirst, CharPtr litLast)
{
	// synthesize a hidden declaration at the literal's lexical position:
	// 'function _lambda_<n> <literal-after-keyword>' -- the keyword is 8 chars
	dms_assert(litLast - litFirst > 8);
	TreeItem* context = GetContextItem();
	if (!context)
		throwSemanticError("a function literal cannot be lifted beside the configuration root; declare the rule inside a container");
	// probe for a free synthesized name: collision-proof against user items, other
	// (#include'd) parse sessions, and earlier lambdas in the same container
	SharedStr name;
	for (;;)
	{
		name = mySSPrintF("_lambda_{}", ++m_LambdaCounter);
		if (!FindSubItemRaw(context, GetTokenID_mt(name.c_str())))
			break;
	}
	SharedStr declStr = mySSPrintF("function {} {}", name.c_str()
		, SharedStr(CharPtrRange(litFirst + 8, litLast)).c_str());

	// the nested declaration parse creates the item under the CURRENT block context
	// (a sibling of the item under construction; a body item inside a function):
	// save the members that parse clobbers
	auto savedCurrent   = m_pCurrent;
	auto savedNameID    = m_ItemNameID;
	auto savedDeclCount = m_LastDeclNameCount;
	auto savedSiblings  = std::move(m_LastDeclSiblings);
	m_LastDeclSiblings.clear();
	m_pCurrent = nullptr; // block-content position

	try
	{
		ParseNestedDeclaration(declStr.c_str());
		if (m_pCurrent)
			m_pCurrent->SetIsHidden(true); // an endogenous helper, not part of the authored tree
	}
	catch (...)
	{
		m_pCurrent = savedCurrent;
		m_ItemNameID = savedNameID;
		m_LastDeclNameCount = savedDeclCount;
		m_LastDeclSiblings = std::move(savedSiblings);
		throw;
	}
	m_pCurrent = savedCurrent;
	m_ItemNameID = savedNameID;
	m_LastDeclNameCount = savedDeclCount;
	m_LastDeclSiblings = std::move(savedSiblings);
	return name;
}

///////////////////////////////////////////////////////////////////////////////
//
//  tmp funcs
//
///////////////////////////////////////////////////////////////////////////////

void ConfigProd::DoItemName()
{
	m_ItemNameID = m_strIdentifierID;
}

void ConfigProd::OnBareExprHeading(iterator_t first)
{
	// §5.12 'name := expr;' -- an auto-typed plain item: class and domain follow
	// from the calculation (the DC layer derives them; the definition-time walker
	// infers them inside function bodies)
	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();
	DoItemName();
	SetSignature(SignatureType::TreeItem);
	CreateItem(m_ItemNameID, first);
	ClearSignature();
	ClearPropData();
}

// ============================= name:type declaration style

void ConfigProd::StartPendingNames()
{
	m_PendingNames.clear();
	m_PendingNames.push_back(m_strIdentifierID);
}

void ConfigProd::AddPendingName()
{
	m_PendingNames.push_back(m_strIdentifierID);
}

// raw (non-updating) child lookup: parse-time code runs under the
// no-UpdateMetaInfo lock, so the updating GetConstSubTreeItemByID is off-limits
static const TreeItem* FindSubItemRaw(const TreeItem* parent, TokenID id)
{
	for (const TreeItem* c = parent->_GetFirstSubItem(); c; c = c->GetNextItem())
		if (c->GetID() == id)
			return c;
	return nullptr;
}

const TreeItem* ConfigProd::ResolveTypeRef(TokenID refID) const
{
	// parse-time type resolution: declared-before-use, walking the parent chain of the
	// current context; using-directives are deliberately not consulted (their lazy
	// resolution must not be forced mid-parse)
	SharedStr refStr(GetTokenStr(refID));
	CharPtr b = refStr.begin(), e = refStr.send();
	if (b == e || *b == '.' || *b == '/')
		return nullptr;

	CharPtr slash = std::find(b, e, '/');
	TokenID firstTok = (slash == e) ? refID : GetTokenID_mt(b, slash);

	for (const TreeItem* scope = GetContextItem(); scope; scope = scope->GetTreeParent().get())
	{
		const TreeItem* found = FindSubItemRaw(scope, firstTok);
		if (!found)
			continue;
		CharPtr segBegin = slash;
		const TreeItem* cursor = found;
		while (cursor && segBegin != e)
		{
			++segBegin; // skip '/'
			CharPtr segEnd = std::find(segBegin, e, '/');
			cursor = FindSubItemRaw(cursor, GetTokenID_mt(segBegin, segEnd));
			segBegin = segEnd;
		}
		return cursor; // nearest scope wins; no fall-through on partial resolution
	}
	return nullptr;
}

void ConfigProd::DoRefTypeSignature()
{
	m_PendingFunctionParamSig = nullptr;
	m_PendingTypeArgs.clear(); // a fresh typeref: type-application args follow, if any

	if (auto exemplar = ResolveTypeRef(m_strIdentifierID))
	{
		// type by example / type alias: declare the new item with the exemplar's type
		if (IsUnit(exemplar))
		{
			SetSignature(SignatureType::Unit);
			m_eValueClass = AsUnit(exemplar)->GetValueType();
			m_PendingTypeExemplar = exemplar; // clone its refinement (IntegrityCheck) onto the new item
			return;
		}
		if (IsDataItem(exemplar))
		{
			auto adi = AsDataItem(exemplar);
			SetSignature(SignatureType::Attribute);
			m_pSignatureUnit = adi->ValuesUnitToken();
			m_pParamEntity   = adi->DomainUnitToken();
			auto vc = adi->GetValueComposition();
			if (vc != ValueComposition::Single)
				m_eParamVC = vc;
			m_PendingTypeExemplar = exemplar; // clone its refinement (IntegrityCheck) onto the new item
			return;
		}
		if (exemplar->IsFunctionItem())
		{
			// legal in function PARAMETER position and (§5.10 Stage 2) as a function
			// RESULT type; OnFunctionResultSig converts the pending sig accordingly
			if (m_FuncStates.empty())
				throwSemanticError("a function-signature type is only supported for function parameters and results");
			SetSignature(SignatureType::TreeItem);
			m_PendingFunctionParamSig = exemplar;
			return;
		}
		SetSignature(SignatureType::TreeItem); // container-like exemplar: plain item
		// K11a-4: a CONTAINER exemplar of a function parameter ('cfg: Settings')
		// supplies the parameter's declared member block, exactly like a unit
		// exemplar -- retain it for the definition-time checker (parameters only:
		// outside a param list the pending exemplar would wrongly feed
		// CloneAliasRefinement for plain item declarations)
		if (!m_FuncStates.empty() && m_FuncStates.back().inParamList)
			m_PendingTypeExemplar = exemplar;
		return;
	}

	// unresolved reference: allowed inside function declarations only (binds by
	// reference, e.g. 'f: function' or a composite type declared further down)
	if (m_FuncStates.empty())
		throwSemanticError("unknown type: an item reference used as type must resolve to a previously declared item (outside function declarations)");
	SetSignature(SignatureType::TreeItem);
}

void ConfigProd::DoColonItemHeading(iterator_t first, iterator_t last)
{
	if (m_PendingNames.empty())
		throwSemanticError("declaration without name");

	m_LastDeclNameCount = m_PendingNames.size();
	m_LastDeclSiblings.clear();
	if (m_PendingFunctionParamSig && (m_FuncStates.empty() || !m_FuncStates.back().inParamList))
		throwSemanticError("a function-signature type is only supported for function parameters");
	TokenID domainVar = FindActiveTypeVarConstraint(m_pParamEntity) ? m_pParamEntity : TokenID::GetEmptyID(); // §5.10 Stage 2
	TokenID genericVar = ConsumeGenericParamMarker();
	bool inParams = !m_FuncStates.empty() && m_FuncStates.back().inParamList;
	for (SizeT i = 0; i != m_PendingNames.size(); ++i)
	{
		CreateItem(m_PendingNames[i], first);
		if (i + 1 != m_PendingNames.size())
			m_LastDeclSiblings.push_back(m_pCurrent); // all but the last; props/expr also apply to these
		bool topLevel = inParams && IsTopLevelFunctionParam();
		if (m_PendingFunctionParamSig && topLevel)
			m_FuncStates.back().paramSigs.emplace_back(m_FuncStates.back().paramCount + i, m_PendingFunctionParamSig, m_PendingTypeArgs);
		// K11a by-example: 'nw: network_links' / 'cfg: Settings' -- retain the UNIT or
		// CONTAINER exemplar so the definition-time checker can type the parameter's
		// members from ITS declared sub-items (the class clone above carries no
		// member block). Data-item exemplars carry no member interface.
		if (m_PendingTypeExemplar && topLevel && !IsDataItem(m_PendingTypeExemplar))
			m_FuncStates.back().paramExemplars.emplace_back(m_FuncStates.back().paramCount + i, m_PendingTypeExemplar);
		if (genericVar && topLevel)
			m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount + i, genericVar, FindActiveTypeVarConstraint(genericVar), false);
		if (domainVar && topLevel)
			m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount + i, domainVar, FindActiveTypeVarConstraint(domainVar), true);
		if (m_eSignatureType == SignatureType::MetaRef && topLevel)
			m_FuncStates.back().metaRefParams.push_back(m_FuncStates.back().paramCount + i);
	}
	m_PendingNames.clear();
	m_PendingFunctionParamSig = nullptr;

	ClearSignature();
	ClearPropData();
}

// ============================= type aliases: alias = type;

void ConfigProd::OnAliasName()
{
	m_AliasNameID = m_strIdentifierID;
}

void ConfigProd::DoAliasDecl(iterator_t first, iterator_t last)
{
	// an alias declares a hidden, inert exemplar item of the aliased type; later
	// 'name: alias' declarations clone its type (DoRefTypeSignature)
	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();
	if (m_PendingFunctionParamSig)
		throwSemanticError("a function-signature type cannot alias another one via '='; declare it directly as 'alias = (params) -> result;'");

	CreateItem(m_AliasNameID, first);
	m_pCurrent->SetIsTemplate(); // inert: an exemplar is a type, not a calculatable item

	ClearSignature();
	ClearPropData();
}

static bool IsIdentChar(char c) { return std::isalnum((unsigned char)c) || c == '_'; }

// replace every complete-identifier occurrence of [fb,fe) in [sb,se) with [tb,te)
static SharedStr SubstituteWholeIdentifier(CharPtr sb, CharPtr se, CharPtr fb, CharPtr fe, CharPtr tb, CharPtr te)
{
	std::string out;
	SizeT fromLen = fe - fb;
	for (CharPtr p = sb; p != se; )
	{
		if (fromLen != 0 && SizeT(se - p) >= fromLen && std::equal(fb, fe, p)
			&& (p == sb || !IsIdentChar(p[-1]))
			&& (p + fromLen == se || !IsIdentChar(p[fromLen])))
		{
			out.append(tb, te);
			p += fromLen;
		}
		else
		{
			out.push_back(*p);
			++p;
		}
	}
	return SharedStr(out.c_str());
}

void ConfigProd::CloneAliasRefinement()
{
	if (!m_PendingTypeExemplar || !m_pCurrent)
		return;
	static TokenID t_IntegrityCheck = GetTokenID_st("IntegrityCheck");
	auto srcPd = m_PendingTypeExemplar->GetDynamicClass()->FindPropDef(t_IntegrityCheck);
	if (!srcPd)
		return;
	SharedStr check = srcPd->GetValueAsSharedStr(m_PendingTypeExemplar);
	if (check.empty())
		return;

	// the exemplar's IntegrityCheck references the value by the alias's own name;
	// rebind it to the new item's name
	SharedStr fromName(m_PendingTypeExemplar->GetID());
	SharedStr toName(m_pCurrent->GetID());
	SharedStr renamed = SubstituteWholeIdentifier(
		check.begin(), check.send(),
		fromName.begin(), fromName.send(),
		toName.begin(), toName.send());

	auto dstPd = m_pCurrent->GetDynamicClass()->FindPropDef(t_IntegrityCheck);
	if (dstPd)
		dstPd->SetValueAsCharRange(m_pCurrent.get(), renamed.begin(), renamed.send());
}

// ============================= function declarations

void ConfigProd::OnFunctionHeading(iterator_t first)
{
	// the function name was captured by the identifier action (m_ItemNameID);
	// a type-variable clause may have overwritten m_strIdentifierID since

	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();

	SetSignature(SignatureType::Function);
	CreateItem(m_ItemNameID, first);
	ClearSignature();
	ClearPropData();

	m_FuncStates.emplace_back();
	m_FuncStates.back().funcItem = m_pCurrent;
	m_FuncStates.back().inParamList = true;
	m_FuncStates.back().typeVars = std::move(m_PendingTypeVars);
	m_PendingTypeVars.clear();
}

void ConfigProd::OnVariantSetHeading()
{
	// 'function name { variant ... }' -- a set that dispatches to variant sub-functions
	// by argument type. The set item has no parameters or body of its own.
	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();

	SetSignature(SignatureType::Function);
	CreateItem(m_ItemNameID, m_PendingNameLoc); // CreateFunction validates the name + flags IsFunctionItem
	ClearSignature();
	ClearPropData();

	TreeItem_SetFunctionSpec(m_pCurrent.get(), 0, TokenID::GetEmptyID());
	TreeItem_SetFunctionVariantSet(m_pCurrent.get());
	// no FuncState is pushed for the set; each 'variant' pushes its own
}

void ConfigProd::OnVariantSetEnd()
{
	// the set container is closed; the variants have already been finalized
	DoEndBlock(); // pop the set container opened by DoBeginBlock after OnVariantSetHeading

	// §5.7 v2: definition-time disjointness -- overlapping variants must be
	// specificity-ordered (token-based match sets; parse-safe)
	TreeItem_CheckVariantSetDisjointness(m_pCurrent.get());

	// arity-aware operator-name coexistence: validated over the members' arity ranges
	ValidateFunctionArityVsOperator(m_pCurrent.get());
}

void ConfigProd::OnFunctionSigHeading(iterator_t first)
{
	// 'alias = (params) -> resultType;' -- a signature-only function item
	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();

	SetSignature(SignatureType::Function);
	CreateItem(m_AliasNameID, first);
	ClearSignature();
	ClearPropData();

	m_FuncStates.emplace_back();
	m_FuncStates.back().funcItem = m_pCurrent;
	m_FuncStates.back().signatureOnly = true;
	m_FuncStates.back().inParamList = true;
	m_FuncStates.back().typeVars = std::move(m_PendingTypeVars);
	m_PendingTypeVars.clear();
}

void ConfigProd::OnEndFunctionParams()
{
	dms_assert(!m_FuncStates.empty());
	m_FuncStates.back().inParamList = false;
}

void ConfigProd::OnFunctionParamDecl()
{
	dms_assert(!m_FuncStates.empty());
	if (m_FuncStates.back().hasRestParam)
		throwSemanticError("a '...' rest parameter must be the last parameter");
	m_FuncStates.back().paramCount += m_LastDeclNameCount;
}

void ConfigProd::OnRestParamDecl(iterator_t first)
{
	// '...x': a variadic rest parameter -- binds ONE OR MORE trailing arguments; in the
	// body it may only be passed on as the trailing argument of a (typically recursive)
	// function call, where it splices the captured arguments
	if (m_FuncStates.empty() || !m_FuncStates.back().inParamList)
		throwSemanticError("a '...' rest parameter is only supported in a function parameter list");
	if (m_FuncStates.back().signatureOnly)
		throwSemanticError("a '...' rest parameter is not supported in a function-signature type");
	if (m_FuncStates.back().hasRestParam)
		throwSemanticError("only one '...' rest parameter is allowed and it must be the last parameter");

	SetSignature(SignatureType::TreeItem); // the rest param item is a plain TreeItem (like a container param)
	CreateItem(m_ItemNameID, first);
	if (!IsTopLevelFunctionParam())
		throwSemanticError("a '...' rest parameter is only supported as a direct function parameter");
	ClearSignature();
	ClearPropData();

	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();
	m_FuncStates.back().paramCount += 1;
	m_FuncStates.back().hasRestParam = true;
}

void ConfigProd::OnAnonSigParam(iterator_t first)
{
	// §5.10 Stage 2: anonymous parameter in a signature alias, e.g.
	// 'nuf = function<V: numerics, D: domains>(attribute<V>(D)) -> ...;'
	// synthesize a positional name; the signature/domain state set by the type
	// productions applies as for a named declaration
	dms_assert(!m_FuncStates.empty());
	m_ItemNameID = GetTokenID_mt(mgFormat2string("_{}", m_FuncStates.back().paramCount + 1).c_str());
	DoItemHeading(first, first);
	OnFunctionParamDecl();
}

void ConfigProd::OnParamSigTypeArg()
{
	// §5.10 Stage 2 / WP4.1: an argument of a type application 'sig<V, D>' must name
	// an active type variable; collected for binding enforcement at reduction
	if (!FindActiveTypeVarConstraint(m_strIdentifierID))
		throwSemanticError(mgFormat2string("'{}': type application argument must name a type variable of the enclosing declaration"
			, GetTokenStr(m_strIdentifierID)).c_str());
	m_PendingTypeArgs.push_back(m_strIdentifierID);
}

void ConfigProd::DoFunctionUsing()
{
	dms_assert(!m_FuncStates.empty());
	m_FuncStates.back().funcItem->AddUsingUrl(m_strIdentifierID);
}

void ConfigProd::DoFunctionResultName()
{
	dms_assert(!m_FuncStates.empty());
	m_FuncStates.back().resultName = m_strIdentifierID;
}

void ConfigProd::OnFunctionResultSig()
{
	dms_assert(!m_FuncStates.empty());
	if (m_PendingGenericUnitVar)
	{
		// '-> unit<V>' generic-unit result: declare as plain item; the computed type
		// follows from the reduced expression
		m_PendingGenericUnitVar = TokenID::GetEmptyID();
		SetSignature(SignatureType::TreeItem);
	}
	if (m_PendingFunctionParamSig)
	{
		// §5.10 Stage 2: '-> some_signature_alias[<V, D>]' -- a function-valued result;
		// v1 records the function-ness (shape checks against the alias stay kind-level).
		// Retain the exemplar + type-application args so the config dump can render the
		// declared result signature faithfully ('-> nuf<V, D>' rather than '-> function').
		m_FuncStates.back().resultSigExemplar = m_PendingFunctionParamSig;
		m_FuncStates.back().resultSigTypeArgs = std::move(m_PendingTypeArgs);
		m_PendingTypeArgs.clear();
		m_PendingFunctionParamSig = nullptr;
		m_FuncStates.back().resultIsFunction = true;
	}
	auto& fs = m_FuncStates.back();
	fs.resultSig           = m_eSignatureType;
	fs.resultValueClass    = m_eValueClass;
	fs.resultSignatureUnit = m_pSignatureUnit;
	fs.resultParamEntity   = m_pParamEntity;
	fs.resultVC            = m_eParamVC;

	// reset signature state: body items are parsed before the result item is created
	ClearSignature();
	m_eSignatureType = SignatureType::Undefined;
	m_pParamEntity   = TokenID::GetEmptyID();
	m_eParamVC       = ValueComposition::Unknown;
}

void ConfigProd::OnFunctionResultExpr(iterator_t first, iterator_t last)
{
	dms_assert(!m_FuncStates.empty());
	// §5.11 tier B: literals lifted here become body items of the function under
	// construction (the context stack top), lexically where they appear
	m_FuncStates.back().resultExpr = MaterializePendingLambdas(&*first, &*last);
}

void ConfigProd::OnFunctionResultIsFunction()
{
	// §5.10 '-> function': the result is a nested function, designated by name
	// (default 'result') from the body block; no data signature applies
	dms_assert(!m_FuncStates.empty());
	m_FuncStates.back().resultIsFunction = true;
	SetSignature(SignatureType::TreeItem); // benign placeholder for OnFunctionResultSig
}

static StaticLateTokenID t_Result("result");

void ConfigProd::OnAnonResultFunction()
{
	// §5.11 ':= function(params) -> T := e;' -- the result is an ANONYMOUS nested
	// function, declared under the enclosing function's result name (default
	// 'result'); the enclosing declaration then designates it implicitly
	dms_assert(!m_FuncStates.empty());
	auto& fs = m_FuncStates.back();
	if (!fs.resultIsFunction)
		throwSemanticError("an anonymous function result requires a '-> function' or signature-typed result specification");
	m_ItemNameID = fs.resultName ? fs.resultName : t_Result;
}

void ConfigProd::OnFunctionDeclEnd(iterator_t first)
{
	dms_assert(!m_FuncStates.empty());
	auto& fs = m_FuncStates.back();
	TreeItem* func = fs.funcItem.get();
	dms_assert(func);

	// implicit designation (§5.10/§5.11): without ':= expr', the result name
	// (default 'result') designates a body item -- a nested function for
	// '-> function', or a declared body item for a data result type followed by
	// a body block ('-> restype { ... result ... }')
	bool implicitDesignation = fs.resultExpr.empty() && !fs.signatureOnly;
	if (implicitDesignation)
		fs.resultExpr = SharedStr(fs.resultName ? fs.resultName : t_Result);

	// designation: a result expression that is just the name of a function sub-item
	// designates that item as the result instead of creating a new one
	TokenID designated;
	CharPtr b = fs.resultExpr.begin(), e = fs.resultExpr.send();
	while (b != e && std::isspace((unsigned char)*b)) ++b;
	while (e != b && std::isspace((unsigned char)e[-1])) --e;
	bool simpleName = (b != e) && (std::isalpha((unsigned char)*b) || *b == '_');
	for (CharPtr p = b + 1; simpleName && p != e; ++p)
		if (!std::isalnum((unsigned char)*p) && *p != '_')
			simpleName = false;
	if (simpleName)
	{
		TokenID exprTok = GetTokenID_mt(b, e);
		// designation targets BODY items only; the first paramCount sub-items are the
		// parameters, and a bare parameter name (e.g. the identity ':= x') must become
		// the result's calculation rule, not designate the expression-less parameter
		const TreeItem* firstBodyItem = func->_GetFirstSubItem();
		for (UInt32 k = fs.paramCount; k && firstBodyItem; --k)
			firstBodyItem = firstBodyItem->GetNextItem();
		for (const TreeItem* sub = firstBodyItem; sub; sub = sub->GetNextItem())
			if (sub->GetID() == exprTok)
			{
				designated = exprTok;
				break;
			}
		if (!designated)
		{	// allow case-insensitive designation (':= Result' designating sub-item 'result')
			auto equalsCI = [](CharPtr a, CharPtr aEnd, CharPtr b, CharPtr bEnd) {
				if (aEnd - a != bEnd - b)
					return false;
				for (; a != aEnd; ++a, ++b)
					if (std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b))
						return false;
				return true;
			};
			const TreeItem* uniqueMatch = nullptr;
			for (const TreeItem* sub = firstBodyItem; sub; sub = sub->GetNextItem())
			{
				SharedStr subName = SharedStr(sub->GetID());
				if (equalsCI(subName.begin(), subName.send(), b, e))
				{
					if (uniqueMatch)
						throwSemanticError(mgFormat2string("function result '{}' matches multiple sub-items case-insensitively"
							, SharedStr(CharPtrRange(b, e))).c_str());
					uniqueMatch = sub;
				}
			}
			if (uniqueMatch)
				designated = uniqueMatch->GetID();
		}
	}

	if (fs.resultIsFunction)
	{
		if (!designated)
			throwSemanticError("'-> function' requires a nested function named 'result' (or a ':= name' designation) in the body");
		auto resultChild = func->GetSubTreeItemByID(designated);
		if (!resultChild || !resultChild->IsFunctionItem())
			throwSemanticError("'-> function': the designated result must be a nested function");
	}
	else if (designated)
	{
		// the inverse guard: a DATA result type must not silently designate a
		// function (e.g. ':= (function ...)' lifted to a bare '_lambda_n' name)
		auto resultChild = func->GetSubTreeItemByID(designated);
		if (resultChild && resultChild->IsFunctionItem())
			throwSemanticError("the designated result is a function; declare a '-> function' or signature-typed result");
	}

	if (!designated)
	{
		if (fs.resultSig == SignatureType::Undefined)
			throwSemanticError("function result type expected");
		if (implicitDesignation)
			throwSemanticError("function result expression expected after ':=', or a body item named 'result' (or the declared result name) to designate");
		if (fs.resultExpr.empty() && !fs.signatureOnly)
			throwSemanticError("function result expression expected after ':='");

		m_eSignatureType = fs.resultSig;
		m_eValueClass    = fs.resultValueClass;
		m_pSignatureUnit = fs.resultSignatureUnit;
		m_pParamEntity   = fs.resultParamEntity;
		m_eParamVC       = fs.resultVC;

		TokenID resultName = fs.resultName ? fs.resultName : t_Result;
		CreateItem(resultName, first);
		if (!fs.resultExpr.empty())
			m_pCurrent->SetExpr(fs.resultExpr);

		ClearSignature();
		ClearPropData();
		designated = resultName;
	}

	TreeItem_SetFunctionSpec(func, fs.paramCount, designated);
	for (const auto& paramSig : fs.paramSigs)
		TreeItem_AddFunctionParamSignature(func, std::get<0>(paramSig), std::get<1>(paramSig), std::get<2>(paramSig));
	for (const auto& paramEx : fs.paramExemplars)
		TreeItem_AddFunctionParamTypeExemplar(func, paramEx.first, paramEx.second); // K11a by-example member source
	for (const auto& genericParam : fs.genericParams)
		TreeItem_AddFunctionGenericParam(func, std::get<0>(genericParam), std::get<1>(genericParam), std::get<2>(genericParam), std::get<3>(genericParam));
	for (UInt32 metaRefIdx : fs.metaRefParams)
		TreeItem_AddFunctionMetaRefParam(func, metaRefIdx);
	if (fs.hasRestParam)
		TreeItem_SetFunctionRestParam(func);
	if (fs.signatureOnly)
		TreeItem_SetFunctionSignatureOnly(func); // config-dump: render 'nuf = function<...>(...) -> ...;'
	if (fs.resultIsFunction)
		TreeItem_SetFunctionResultSig(func, true, fs.resultSigExemplar, fs.resultSigTypeArgs); // config-dump: render '-> nuf<V, D>'
	// arity-aware operator-name coexistence check (variant members are not call heads;
	// their SET is validated at OnVariantSetEnd)
	if (!(func->GetTreeParent() && TreeItem_IsFunctionVariantSet(func->GetTreeParent().get())))
		ValidateFunctionArityVsOperator(func);
	if (!fs.typeVars.empty())
		TreeItem_SetFunctionTypeVars(func, fs.typeVars); // WP4.1: ordered <var: constraint> list
	TreeItem_MakeStrictScope(func); // definition-side strictness: inline reduction resolves through this scope
	m_FuncStates.pop_back();

	DoEndBlock(); // close the frame opened at '('; m_pCurrent = the function item
}
