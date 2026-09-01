// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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

// ============================= CLASS: ConfigProd

ConfigProd::ConfigProd(TreeItem* context, bool rootIsFirstItem)
:	m_pCurrent(nullptr)
,	m_ResultCommitted(false)

#if defined(MG_DEBUG)
,	md_ContextWasGiven(context != nullptr)
#endif

{
	m_MergeIntoExisting = rootIsFirstItem;
	m_ExprProd.m_LiteralSink = this; // §5.11 tier B: lambda lifting of function literals

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
		throwSemanticError(mgFormat2string("Parse error in included config file {}", GetTokenStrLock(m_strIdentifierID)).c_str());
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

	// §5.10 Stage 2: a domain spec '(D)' naming a type variable makes D a domain
	// variable, bound from the argument's domain at reduction (capture before
	// ConsumeGenericParamMarker, which may reset the signature state)
	TokenID domainVar = FindActiveTypeVarConstraint(m_pParamEntity) ? m_pParamEntity : TokenID::GetEmptyID();
	TokenID genericVar = ConsumeGenericParamMarker();

	CreateItem(m_ItemNameID, first);
	if (!m_FuncStates.empty() && m_FuncStates.back().inParamList && IsTopLevelFunctionParam())
	{
		if (genericVar)
			m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount, genericVar, FindActiveTypeVarConstraint(genericVar), false);
		if (domainVar)
			m_FuncStates.back().genericParams.emplace_back(m_FuncStates.back().paramCount, domainVar, FindActiveTypeVarConstraint(domainVar), true);
		if (m_eSignatureType == SignatureType::MetaRef)
			m_FuncStates.back().metaRefParams.push_back(m_FuncStates.back().paramCount);
	}

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
		auto name = SharedStr(GetTokenStrLock(nameID));
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

				// Merging into an existing item means a storage dictionary is being read (the only
				// caller that passes rootIsFirstItem is AppendTreeFromDictionary). Such a dictionary
				// carries the name of the container that WROTE the store, while the reader merges it
				// into a container of its own naming, so in the decoupling pattern the two differ by
				// construction -- the writing project need not even be the same project (#1194).
				// A name equal to the writer's proves nothing about reading the right file either;
				// the merged structure and types do, and the dictionary restrictions (#1154) check
				// them. Kept as provenance, not as a warning, so it stops training readers to skim
				// past warnings.
				reportF(MsgCategory::storage_read, SeverityTypeID::ST_MinorTrace
					, "Configuration file {}: dictionary root '{}' merged into item '{}'"
					, ConfigurationFilenameLock::GetCurrentFileDescrFromConfigLoadDir()->GetFileName().c_str()
					, AsString(nameID).c_str()
					, AsString(m_pCurrent->GetID()).c_str()
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
		case SignatureType::MetaRef:
			// 'item x': a meta-reference parameter -- the argument binds as a raw item
			// reference (like PropValue's item argument), never as a calculation key.
			// Validate AFTER creation: IsTopLevelFunctionParam tests m_pCurrent, which
			// only becomes the declared item once CreateContainer ran
			CreateContainer(nameID);
			if (m_FuncStates.empty() || !m_FuncStates.back().inParamList || !IsTopLevelFunctionParam())
				throwSemanticError("an 'item' (meta-reference) declaration is only supported as a function parameter");
			break;
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

static StaticLateTokenID t_Dot(".");

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
	// RewriteExpr.lsp rule head would capture before dispatch. Built-in operator
	// names are validated ARITY-AWARE at declaration end (ValidateFunctionArityVsOperator):
	// a same-named function may serve argument counts the operator rejects.
	if (HasRewriteRuleForHead(nameID))
		throwSemanticError(mgFormat2string("function name '{}' collides with a RewriteExpr.lsp rule head; calls would be rewritten before this function is found"
			, GetTokenStrLock(nameID)).c_str());

	CreateContainer(nameID);
	m_pCurrent->SetIsFunction();
}

void ConfigProd::ValidateFunctionArityVsOperator(const TreeItem* func)
{
	// arity-aware coexistence (§5.14 successor): a function may share a built-in
	// operator's name iff it only declares argument counts NO operator member accepts;
	// arity-aware head dispatch then routes exactly those counts to the function
	auto og = AbstrOperGroup::FindName(func->GetID());
	if (!og || og->IsTemplateCall())
		return;

	arg_index gMin, gMax;
	bool acceptsEverything = !og->GetArityEnvelope(gMin, gMax);

	// per-declaration ranges: a variant set covers the UNION of its members' ranges
	// (possibly gappy: {1} + [3,inf) leaves 2 to the operator), so test each member
	auto overlaps = [&](const TreeItem* f) -> bool
	{
		if (acceptsEverything)
			return true;
		UInt32 lo = TreeItem_GetFunctionParamCount(f);
		UInt32 hi = TreeItem_HasFunctionRestParam(f) ? std::numeric_limits<UInt32>::max() : lo;
		return lo <= gMax && gMin <= hi;
	};

	bool collision = false;
	if (TreeItem_IsFunctionVariantSet(func))
	{
		for (const TreeItem* v = func->_GetFirstSubItem(); v && !collision; v = v->GetNextItem())
			if (v->IsFunctionItem())
				collision = overlaps(v);
	}
	else
		collision = overlaps(func);

	if (collision)
		throwSemanticError(mgFormat2string("function name '{}' collides with a built-in operator: it declares an argument count the operator also accepts; only argument counts the operator rejects may be served by a same-named function"
			, GetTokenStrLock(func->GetID())).c_str());
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

static StaticLateTokenID t_Void("void");

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
		throwErrorD( "ConfigProd::DoBasicType: Unknown ValueType", m_strIdentifierID.GetStrLock().c_str());
	}
}


void ConfigProd::DoAnyProp()
{
	dms_assert(m_pCurrent);

	AbstrPropDef* pd = m_pCurrent->GetDynamicClass()->FindPropDef(m_strIdentifierID);
	if (!pd) 
		m_pCurrent->throwItemErrorF(
			"Unknown property '{}'", 
			GetTokenStrLock(m_strIdentifierID).c_str()
		);
	pd->SetValueAsCharRange(m_pCurrent.get(), m_StringVal.begin(), m_StringVal.send());
}

void ConfigProd::DoExprProp(iterator_t first, iterator_t last)
{
	dms_assert(m_pCurrent);

	SharedStr exprStr = MaterializePendingLambdas(&*first, &*last); // §5.11 tier B
	m_pCurrent->SetExpr(exprStr);
	for (auto& sibling : m_LastDeclSiblings) // multi-name declaration: all names share the calculation rule
		sibling->SetExpr(exprStr);
}

// ============================= §5.11 tier B: lambda lifting

// (FindSubItemRaw moved to ConfigProd_functions.cpp with its users, 2026-08)


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