// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "StxPch.h"

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
#include "utl/mySPrintF.h"

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

// ============================= CLASS: ConfigProd

ConfigProd::ConfigProd(TreeItem* context, bool rootIsFirstItem)
:	m_pCurrent(nullptr)
,	m_ResultCommitted(false)

#if defined(MG_DEBUG)
,	md_ContextWasGiven(context != nullptr)
#endif

{
	m_MergeIntoExisting = rootIsFirstItem;

	if (context)
	{
		if (rootIsFirstItem)
			m_pCurrent = make_shared_tree(context, existing_obj{}); // existing (owned) context item
		else
			m_stackContexts.push_back(make_shared_tree(context, existing_obj{}));
	}

	ClearSignature();
}

ConfigProd::~ConfigProd()
{
	if (!m_ResultCommitted && !m_MergeIntoExisting)
	{
		if (m_pCurrent)
		{
			m_pCurrent->EnableAutoDelete();
			m_pCurrent = nullptr;
		}
		while( m_stackContexts.size() )
		{
			TreeItem* ti = m_stackContexts.back().get();
			if (ti)
				ti->EnableAutoDelete();
			m_stackContexts.pop_back();
		}
	}
}

TreeItem* ConfigProd::GetContextItem() const
{
	return CurrentIsRoot() 
		?	nullptr
		:	m_stackContexts.back().get(); 
}

TreeItem* ConfigProd::GetContextOrRootItem(TokenID& nameID) const
{
	auto contextItem = GetContextItem();
	if (!contextItem && m_MergeIntoExisting)
	{
		contextItem = m_pCurrent.get();
		nameID = {};
	}
	return contextItem;
}

void ConfigProd::ProdIdentifier(iterator_t first, iterator_t last)
{
	m_strIdentifierID = GetTokenID_mt(&*first, &*last);
}

void ConfigProd::ProdQuotedIdentifier()
{
	m_strIdentifierID = GetTokenID_mt(m_StringVal.c_str());
}

void ConfigProd::DoInclude()
{
	if (m_stackContexts.size() && !GetContextItem())
		return;

	SharedStr fileName = SharedStr(m_strIdentifierID);
	m_pCurrent =
		AppendTreeFromConfiguration(
			fileName.c_str()
		,	CurrentIsRoot()
				? m_pCurrent.get()
				: GetContextItem()
		,	false
		);
	if (!m_pCurrent)
		throwSemanticError(mgFormat2string("Parse error in included config file {}", GetTokenStr(m_strIdentifierID)).c_str());
	dms_assert(m_pCurrent);
//	dbg_assert(!CurrentIsTop());
}

// *****************************************************************************
// Function/Procedure:DoBeginBlock
// Description:       pushes current parent TreeItem on the block stack
// *****************************************************************************

void ConfigProd::DoBeginBlock()
{
	if (m_LastDeclNameCount > 1)
		throwSemanticError("an item block cannot follow a multi-name declaration");

	m_stackContexts.push_back(m_pCurrent);

	m_pCurrent = nullptr;
}

// *****************************************************************************
// class/module:      ConfigProd
// Function/Procedure:DoEndBlock
// Description:       pops the parent from the block stack
// *****************************************************************************

void ConfigProd::DoEndBlock()
{
	dms_assert(GetContextItem());

	m_pCurrent     = m_stackContexts.back1();
	m_stackContexts.pop_back();
}

void ConfigProd::DoItemHeading(iterator_t first, iterator_t last)
{
	m_LastDeclNameCount = 1;
	m_LastDeclSiblings.clear();

	TokenID genericVar = ConsumeGenericParamMarker();

	CreateItem(m_ItemNameID, first);
	if (genericVar && !m_FuncStates.empty() && m_FuncStates.back().inParamList)
		m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount, genericVar, FindActiveTypeVarConstraint(genericVar));

	ClearSignature();
	ClearPropData();
}

TokenID ConfigProd::ConsumeGenericParamMarker()
{
	// unit<V>: DoBasicType left the variable pending; the item becomes a plain binder
	if (m_PendingGenericUnitVar)
	{
		TokenID genericVar = m_PendingGenericUnitVar;
		m_PendingGenericUnitVar = TokenID::GetEmptyID();
		SetSignature(SignatureType::TreeItem);
		return genericVar;
	}
	// attribute<V> / parameter<V>: the values-unit reference names a type variable;
	// the declaration stays as-is (the token is never resolved on inert definitions)
	if (m_eSignatureType == SignatureType::Attribute || m_eSignatureType == SignatureType::Parameter)
		if (FindActiveTypeVarConstraint(m_pSignatureUnit))
			return m_pSignatureUnit;
	return TokenID::GetEmptyID();
}

void ConfigProd::SetSignature(SignatureType type)
{
	m_eSignatureType = type;

	m_pParamEntity   = TokenID::GetEmptyID();
	m_eParamVC       = ValueComposition::Unknown;
}

void ConfigProd::DoEntitySignature()
{
	SetSignature(SignatureType::Unit);
	m_eValueClass = ValueWrap<UInt32>::GetStaticClass();
}

void ConfigProd::ClearSignature()
{
	m_eValueClass    = 0;
	m_pSignatureUnit = TokenID::GetEmptyID();
	m_PendingTypeExemplar = nullptr;
}

void ConfigProd::DoAttrSignature()
{
	SetSignature(SignatureType::Attribute);
}

// *****************************************************************************
// Function/Procedure:DoUnitIdentifier
// Description:       Search for the Unit if it is already in the tree, if not 
//					  an exception will be thrown
// *****************************************************************************

void ConfigProd::DoUnitIdentifier()
{
	m_pSignatureUnit = m_strIdentifierID;
}

// *****************************************************************************
// Function/Procedure:DoItemCreate
// Description:       Search for the Unit if it is already in the tree, if not 
//					  an exception will be thrown
// *****************************************************************************

void CheckIsNew(TreeItem* context, TokenID nameID)
{
	dms_assert(context);
	if (context->GetSubTreeItemByID(nameID))
	{
		auto name = SharedStr(GetTokenStr(nameID));
		context->throwItemErrorF("SubItem '{}' is already defined", name);
	}
}


void ConfigProd::CreateItem(TokenID nameID, const iterator_t& loc)
{
	if (CurrentIsRoot())
	{
		assert( m_stackContexts.empty() );
		if (m_pCurrent)
		{
			if (m_pCurrent->GetID() != nameID)
			{
				if (!m_MergeIntoExisting)
					throwDmsErrD("Illegal 2nd item after root of configuration tree.");
				reportF(MsgCategory::storage_read, SeverityTypeID::ST_Warning
					, "Configuration file {}: root item '{}' was already provided with name '{}'"
					, ConfigurationFilenameLock::GetCurrentFileDescrFromConfigLoadDir()->GetFileName().c_str()
					, AsString(m_pCurrent->GetID()).c_str()
					, AsString(nameID).c_str()
				);
			}
		}
		else
		{
			if (m_eSignatureType != SignatureType::TreeItem)
				throwSemanticError("root of configuration tree must be a container");
			m_pCurrent = TreeItem::CreateConfigRoot(nameID); // sole-owning parentless root; keep the std::shared_ptr
			goto setLocation;
		}
	}
		
	if (!m_MergeIntoExisting)
	{
		// stackContexts not empty
		assert(GetContextItem()); // only non-nulls in stackContexts
		CheckIsNew(GetContextItem(), nameID);
	}

	switch (m_eSignatureType) {
		case SignatureType::TreeItem: CreateContainer(nameID); break;
		case SignatureType::Template: CreateTemplate (nameID); break;
		case SignatureType::Function: CreateFunction (nameID); break;
		case SignatureType::Unit:     CreateUnit     (nameID); break;
		case SignatureType::Attribute:CreateAttribute(nameID); break;
		case SignatureType::Parameter:CreateParameter(nameID); break;
		default: dms_assert(0); // syntax only produces CreateItem with valid signature types
	}

setLocation:
	assert(m_pCurrent);
	position_t const& pos = loc.get_position();

	m_pCurrent->SetLocation(
		new SourceLocation(
			ConfigurationFilenameLock::GetCurrentFileDescrFromConfigLoadDir(),
			pos.line,
			pos.column
		)
	);

	CloneAliasRefinement(); // if declared as 'name: refined_alias', clone the alias's IntegrityCheck
}

void ConfigProd::ClearPropData()
{
	m_sPropFileTypeID = TokenID::GetEmptyID();
	ResetDataBlock();
}

static TokenID t_Dot = GetTokenID_st(".");

TokenID ConfigProd::RetrieveEntity()
{
	if (!m_pParamEntity)
		m_pParamEntity = t_Dot;
	return m_pParamEntity;
}

void ConfigProd::CreateDataItem(TokenID nameID, TokenID domainUnit, TokenID valuesUnit)
{
	if (m_eParamVC == ValueComposition::Unknown)
		m_eParamVC = ValueComposition::Single;

	auto contextItem = GetContextOrRootItem(nameID);
	m_pCurrent = CreateAbstrDataItem(contextItem, nameID, domainUnit, valuesUnit, m_eParamVC); // SharedMutableDataItem -> cursor (co-owned with parent contextItem)
}

void ConfigProd::CreateContainer(TokenID nameID)
{
	assert(!m_eValueClass);
	assert(!m_pSignatureUnit);

	if (m_eParamVC != ValueComposition::Unknown)
		throwDmsErrD("Illegal ValueComposition at container definition");
	if (m_pParamEntity)
		throwDmsErrD("Illegal domain-unit at container definition");

	auto contextItem = GetContextOrRootItem(nameID);
	m_pCurrent = contextItem->CreateItem(nameID); // co-owned with parent (contextItem); cursor keeps a std::shared_ptr
}

void ConfigProd::CreateTemplate(TokenID nameID)
{
	CreateContainer(nameID);
	m_pCurrent->SetIsTemplate();
}

void ConfigProd::CreateFunction(TokenID nameID)
{
	// function names join the operator namespace at call sites; reject names that a
	// built-in operator or a RewriteExpr.lsp rule head would capture before dispatch
	auto og = AbstrOperGroup::FindName(nameID);
	if (og && !og->IsTemplateCall())
		throwSemanticError(mgFormat2string("function name '{}' collides with a built-in operator"
			, GetTokenStr(nameID)).c_str());
	if (HasRewriteRuleForHead(nameID))
		throwSemanticError(mgFormat2string("function name '{}' collides with a RewriteExpr.lsp rule head; calls would be rewritten before this function is found"
			, GetTokenStr(nameID)).c_str());

	CreateContainer(nameID);
	m_pCurrent->SetIsFunction();
}

void ConfigProd::CreateUnit(TokenID nameID)
{
	assert(!m_pSignatureUnit);

	if (m_pParamEntity)
		throwDmsErrD("Illegal domain-unit at unit definition");
	if (m_eParamVC != ValueComposition::Unknown)
		throwDmsErrD("Units with non-singular ValueComposition are now obsolete");


	assert(m_eValueClass); // POSTCONDITION OF DoBasicType which the grammar guarantees to have been processed
	if (m_eValueClass->GetValueComposition() != ValueComposition::Single)
		throwDmsErrD("Illegal composite type at unit definition");

	dms_assert( m_eValueClass->GetValueComposition() == ValueComposition::Single);

	auto contextItem = GetContextOrRootItem(nameID);
	assert(contextItem);

	const UnitClass* uc = UnitClass::Find(m_eValueClass);
	dms_assert(m_eValueClass);
	m_pCurrent = uc->CreateUnit(contextItem, nameID); // co-owned with parent (contextItem); cursor keeps a std::shared_ptr
	assert(m_pCurrent);
}

// *****************************************************************************
// Function/Procedure:CreateAttribute
// Description:       Creates attribute tree item and associates it with 
//					  possibly existing entity
// Parameters:        
//     char* p_pUrl: url identifier
// *****************************************************************************

void ConfigProd::CreateAttribute(TokenID nameID)
{
	assert(!m_eValueClass); // grammar guaranteed that DoBasicType wasn't called after last call to ClearSignature

	CreateDataItem(
		nameID, 
		RetrieveEntity(),
		m_pSignatureUnit
	);
}

// *****************************************************************************
// Function/Procedure:CreateParameter
// Description:       Creates attribute tree item and associates it with 
//					  possibly existing entity
// *****************************************************************************

static TokenID t_Void = GetTokenID_st("Void");

void ConfigProd::CreateParameter(TokenID nameID)
{
	if (m_pParamEntity && m_pParamEntity != t_Void)
		throwDmsErrF("Illegal domain-unit {} at parameter definition", m_pParamEntity);


	assert(GetContextItem() || m_MergeIntoExisting);
	assert(!m_eValueClass); // grammar guaranteed that DoBasicType wasn't called after last call to ClearSignature

	CreateDataItem(
		nameID,
		t_Void,
		m_pSignatureUnit
	);
}

// *****************************************************************************
// Function/Procedure:OnItemCompleted
// Description:       Creates attribute tree item and associates it with 
//					  possibly existing entity
// *****************************************************************************

void ConfigProd::OnItemDecl()
{
	assert(m_pCurrent);
}

// *****************************************************************************
// Function/Procedure:DoEntityParam
// Description:       Retrieves pointer to existing entity otherwise exception
// Parameters:        
//     char* p_pUrl: url identifier
// *****************************************************************************


void ConfigProd::DoBasicType()
{
	m_eValueClass = ValueClass::FindByScriptName(m_strIdentifierID);
	if (!m_eValueClass)
	{
		if (FindActiveTypeVarConstraint(m_strIdentifierID))
		{
			m_PendingGenericUnitVar = m_strIdentifierID; // unit<V> with V a generic type variable
			return;
		}
		throwErrorD( "ConfigProd::DoBasicType: Unknown ValueType", m_strIdentifierID.GetStr().c_str());
	}
}

TokenID ConfigProd::FindActiveTypeVarConstraint(TokenID varName) const
{
	if (varName && !m_FuncStates.empty())
		for (const auto& typeVar : m_FuncStates.back().typeVars)
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

TokenID st_Arc        = GetTokenID_st("arc");
TokenID st_Single     = GetTokenID_st("single");
TokenID st_Sequence   = GetTokenID_st("sequence");
TokenID st_Poly       = GetTokenID_st("poly");
TokenID st_Polygon    = GetTokenID_st("polygon");
TokenID st_MultiPoint = GetTokenID_st("multipoint");

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

void ConfigProd::DoAnyProp()
{
	dms_assert(m_pCurrent);

	AbstrPropDef* pd = m_pCurrent->GetDynamicClass()->FindPropDef(m_strIdentifierID);
	if (!pd) 
		m_pCurrent->throwItemErrorF(
			"Unknown property '{}'", 
			GetTokenStr(m_strIdentifierID).c_str()
		);
	pd->SetValueAsCharRange(m_pCurrent.get(), m_StringVal.begin(), m_StringVal.send());
}

void ConfigProd::DoExprProp(iterator_t first, iterator_t last)
{
	dms_assert(m_pCurrent);

	SharedStr exprStr(CharPtrRange(&*first, &*last));
	m_pCurrent->SetExpr(exprStr);
	for (auto& sibling : m_LastDeclSiblings) // multi-name declaration: all names share the calculation rule
		sibling->SetExpr(exprStr);
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
		auto found = scope->GetConstSubTreeItemByID(firstTok);
		if (!found)
			continue;
		CharPtr segBegin = slash;
		const TreeItem* cursor = found.get();
		while (cursor && segBegin != e)
		{
			++segBegin; // skip '/'
			CharPtr segEnd = std::find(segBegin, e, '/');
			cursor = cursor->GetConstSubTreeItemByID(GetTokenID_mt(segBegin, segEnd)).get();
			segBegin = segEnd;
		}
		return cursor; // nearest scope wins; no fall-through on partial resolution
	}
	return nullptr;
}

void ConfigProd::DoRefTypeSignature()
{
	m_PendingFunctionParamSig = nullptr;

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
			if (m_FuncStates.empty() || !m_FuncStates.back().inParamList)
				throwSemanticError("a function-signature type is only supported for function parameters");
			SetSignature(SignatureType::TreeItem);
			m_PendingFunctionParamSig = exemplar;
			return;
		}
		SetSignature(SignatureType::TreeItem); // container-like exemplar: plain item
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
	TokenID genericVar = ConsumeGenericParamMarker();
	bool recordGenerics = genericVar && !m_FuncStates.empty() && m_FuncStates.back().inParamList;
	for (SizeT i = 0; i != m_PendingNames.size(); ++i)
	{
		CreateItem(m_PendingNames[i], first);
		if (i + 1 != m_PendingNames.size())
			m_LastDeclSiblings.push_back(m_pCurrent); // all but the last; props/expr also apply to these
		if (m_PendingFunctionParamSig)
			m_FuncStates.back().paramSigs.emplace_back(m_FuncStates.back().paramCount + i, m_PendingFunctionParamSig);
		if (recordGenerics)
			m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount + i, genericVar, FindActiveTypeVarConstraint(genericVar));
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
	// 'function name { variant ... }' — a set that dispatches to variant sub-functions
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
}

void ConfigProd::OnFunctionSigHeading(iterator_t first)
{
	// 'alias = (params) -> resultType;' — a signature-only function item
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
	m_FuncStates.back().paramCount += m_LastDeclNameCount;
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
	m_FuncStates.back().resultExpr = SharedStr(CharPtrRange(&*first, &*last));
}

static TokenID t_Result = GetTokenID_st("result");

void ConfigProd::OnFunctionDeclEnd(iterator_t first)
{
	dms_assert(!m_FuncStates.empty());
	auto& fs = m_FuncStates.back();
	TreeItem* func = fs.funcItem.get();
	dms_assert(func);

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
		if (func->GetSubTreeItemByID(exprTok))
			designated = exprTok;
		else
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
			for (const TreeItem* sub = func->_GetFirstSubItem(); sub; sub = sub->GetNextItem())
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

	if (!designated)
	{
		if (fs.resultSig == SignatureType::Undefined)
			throwSemanticError("function result type expected");
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
		TreeItem_AddFunctionParamSignature(func, paramSig.first, paramSig.second);
	for (const auto& genericParam : fs.genericParams)
		TreeItem_AddFunctionGenericParam(func, std::get<0>(genericParam), std::get<1>(genericParam), std::get<2>(genericParam));
	TreeItem_MakeStrictScope(func); // definition-side strictness: inline reduction resolves through this scope
	m_FuncStates.pop_back();

	DoEndBlock(); // close the frame opened at '('; m_pCurrent = the function item
}

void ConfigProd::DoNrOfRowsProp()
{
	assert(m_eValueType == ValueClassID::VT_UInt64);
	assert(m_pCurrent);

	AbstrUnit* unit = AsCheckedUnit(m_pCurrent.get());
	assert(unit);
	const ValueClass* vc = unit->GetValueType();
	assert(vc);

	if (!vc->IsNumeric())
		throwSemanticError(mgFormat2string("DoUnitRangeProp: the provided range is incompatible with the ValueType {} of this unit", vc->GetName()).c_str());

	unit->SetTSF(USF_HasConfigRange | TSF_Categorical);
	unit->SetRangeAsUInt64(0, m_IntValAsUInt64);
}

void ConfigProd::throwSemanticError(CharPtr msg)
{
	TreeItem* curr = m_pCurrent.get();
	UInt32 i = m_stackContexts.size();
	while (!curr && i--)
		curr = m_stackContexts[i].get();

	throwItemErrorF(curr, "Semantic error {}", msg);
}