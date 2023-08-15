//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#include "StxPch.h"
#pragma hdrstop

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
#include "PropDefInterface.h"
#include "TreeItemClass.h"
#include "Unit.h"
#include "UnitClass.h"

#include <stdarg.h>

// ============================= CLASS: ConfigProd

ConfigProd::ConfigProd(TreeItem* context)
:	m_pCurrent(nullptr)
,	m_ResultCommitted(false)
#if defined(MG_DEBUG)
,	md_IsIncludedFile(context)
#endif
{
	if (context)
		m_stackContexts.push_back(context);

	MG_DEBUGCODE( ClearSignature(); )
}

ConfigProd::~ConfigProd()
{
	if (!m_ResultCommitted)
	{
		if (m_pCurrent)
		{
			m_pCurrent->EnableAutoDelete();
			m_pCurrent = nullptr;
		}
		while( m_stackContexts.size() )
		{
			TreeItem* ti = m_stackContexts.back();
			if (ti)
				ti->EnableAutoDelete();
			m_stackContexts.pop_back();
		}
	}
}

TreeItem* ConfigProd::GetContextItem() const
{
	return CurrentIsRoot() 
		?	0
		:	m_stackContexts.back1(); 
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
			fileName.c_str(), 
			CurrentIsRoot() 
				? m_pCurrent.get_ptr()
				: GetContextItem()
		);
	if (!m_pCurrent)
		throwSemanticError(mgFormat2string("Parse error in included config file %s", GetTokenStr(m_strIdentifierID)).c_str());
	dms_assert(m_pCurrent);
	dbg_assert(!CurrentIsTop());
}

// *****************************************************************************
// Function/Procedure:DoBeginBlock
// Description:       pushes current parent TreeItem on the block stack
// *****************************************************************************

void ConfigProd::DoBeginBlock()
{
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
	CreateItem(m_ItemNameID, first);

	MG_DEBUGCODE( ClearSignature(); )
	ClearPropData();
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

#if defined(MG_DEBUG)
// syntax guarantees the processing and resetting of the correct signature for data-items and units
// thus checking for invalid signatures is redundant
void ConfigProd::ClearSignature()
{
	m_eValueClass    = 0;
	m_pSignatureUnit = TokenID::GetEmptyID();
}
#endif

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
		context->throwItemErrorF("SubItem '%s' is already defined", name);
	}
}


void ConfigProd::CreateItem(TokenID nameID, const iterator_t& loc)
{
	if (CurrentIsRoot())
	{
		if(m_eSignatureType != SignatureType::TreeItem)
			throwSemanticError("root of configuration tree must be a container");

		dms_assert( m_stackContexts.empty() );
		if (m_pCurrent)
		{
			dms_assert(m_pCurrent->GetID() == nameID);
		}
		else
			m_pCurrent = TreeItem::CreateConfigRoot(nameID);
	}
	else // stackContexts not empty
	{
		dms_assert(GetContextItem()); // only nonnulls in stackContexts
			
		CheckIsNew(GetContextItem(), nameID);

		switch (m_eSignatureType) {
			case SignatureType::TreeItem: CreateContainer(nameID); break;
			case SignatureType::Template: CreateTemplate (nameID); break;
			case SignatureType::Unit:     CreateUnit     (nameID); break;
			case SignatureType::Attribute:CreateAttribute(nameID); break;
			case SignatureType::Parameter:CreateParameter(nameID); break;
			default: dms_assert(0); // syntax only produces CreateItem with valid signature types
		}
	}

	dms_assert(m_pCurrent);
	
	position_t const& pos = loc.get_position(); 

	m_pCurrent->SetLocation(
		new SourceLocation(
			ConfigurationFilenameLock::GetCurrentFileDescrFromConfigLoadDir(), 
			pos.line, 
			pos.column
		)
	);
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
			m_pCurrent = CreateAbstrDataItem(
				GetContextItem(),
				nameID,
				domainUnit,
				valuesUnit,
				m_eParamVC);
}

void ConfigProd::CreateContainer(TokenID nameID)
{
	dms_assert(!m_eValueClass);
	dms_assert(!m_pSignatureUnit);

	if (m_eParamVC != ValueComposition::Unknown)
		throwDmsErrD("Illegal ValueComposition at container definition");
	if (m_pParamEntity)
		throwDmsErrD("Illegal domain-unit at container definition");

	m_pCurrent = GetContextItem()->CreateItem(nameID);
}

void ConfigProd::CreateTemplate(TokenID nameID)
{
	CreateContainer(nameID);
	m_pCurrent->SetIsTemplate();
}

bool IsPolygonType(ValueClassID vid)
{
	return (vid >= ValueClassID::VT_FirstPolygon && vid < ValueClassID::VT_FirstAfterPolygon);
}

void ConfigProd::CreateUnit(TokenID nameID)
{
	dms_assert(!m_pSignatureUnit);

	if (m_pParamEntity)
		throwDmsErrD("Illegal domain-unit at unit definition");
	if (m_eParamVC != ValueComposition::Unknown)
		throwDmsErrD("Units with non-singular ValueComposition are now obsolete");


	dms_assert(m_eValueClass); // POSTCONDITION OF DoBasicType which the grammar guarantees to have been processed
	if (m_eValueClass->GetValueComposition() != ValueComposition::Single)
		throwDmsErrD("Illegal composite type at unit definition");

	dms_assert( m_eValueClass->GetValueComposition() == ValueComposition::Single);

	dms_assert(GetContextItem());
	const UnitClass* uc = UnitClass::Find(m_eValueClass);
	dms_assert(m_eValueClass);
	m_pCurrent = uc->CreateUnit(GetContextItem(), nameID);
	dms_assert(m_pCurrent);
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
	dms_assert(GetContextItem());
	dms_assert(!m_eValueClass); // grammar guaranteed that DoBasicType wasn't called after last call to ClearSignature

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

static TokenID t_Void = GetTokenID_st("void");

void ConfigProd::CreateParameter(TokenID nameID)
{
	if (m_pParamEntity && m_pParamEntity != t_Void)
		throwDmsErrF("Illegal domain-unit %s at parameter definition", m_pParamEntity);


	dms_assert(GetContextItem());
	dms_assert(!m_eValueClass); // grammar guaranteed that DoBasicType wasn't called after last call to ClearSignature

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
	dms_assert(m_pCurrent);

/*  REMOVE
	if (IsUnit(m_pCurrent.get_ptr()) && m_pCurrent->HasConfigData())
	{
		AbstrUnit* unit = AsUnit(m_pCurrent.get_ptr());
		const ValueClass* vc = unit->GetValueType();
		TokenStr valueClassName = vc->GetName();
		SharedStr expr = m_pCurrent->_GetExprStr();

		if (!expr.empty())
		{
			// m_pCurrent->throwItemError("Combination of range and Calculation rule depreciated");
			reportF(SignatureType::Warning, "%s: Combination of range and Calculation rule depreciated", m_pCurrent->GetSourceName().c_str());
		}
		switch (unit->GetValueType()->GetNrDims())
		{
			case 1: {
				Range<Float64> r = unit->GetRangeAsFloat64();

				if (!expr.empty())
					m_pCurrent->SetExpr(
						mySSPrintF("range(%s,%s(%g),%s(%g))",
							expr
							, valueClassName, r.first
							, valueClassName, r.second
						)
					);

				else
				{
					expr = mySSPrintF("BaseUnit(Left('+Quote(PropValue(., 'FullName')+'/%s')+',0),%s)", m_pCurrent->GetName(), valueClassName);
					m_pCurrent->SetExpr(
						mySSPrintF("='range(%s,%s(%g),%s(%g))'",
							expr
							, valueClassName, r.first
							, valueClassName, r.second
						)
					);
				}
				break;
			}
			case 2: {
				if (expr.empty())
					expr = valueClassName.c_str();

				valueClassName = vc->GetScalarClass()->GetName();
				DRect r = unit->GetRangeAsDRect();
				m_pCurrent->SetExpr(
					mySSPrintF("range(%s, point(%s(%g), %s(%g)), point(%s(%g), %s(%g)))",
						expr
						, valueClassName, dmsPoint_GetFirstCfgValue(r.first ), valueClassName, dmsPoint_GetSecondCfgValue(r.first )
						, valueClassName, dmsPoint_GetFirstCfgValue(r.second), valueClassName, dmsPoint_GetSecondCfgValue(r.second)
					)
				);
				break;
			}
		}
		unit->ClearData();
	}
*/

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
		throwErrorD( "ConfigProd::DoBasicType: Unknown ValueType", m_strIdentifierID.GetStr().c_str());
}

void ConfigProd::DoEntityParam()
{
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
	AbstrUnit* unit = AsCheckedUnit(m_pCurrent.get_ptr());
	dms_assert(unit);
	const ValueClass* vc = unit->GetValueType();
	dms_assert(vc);
	UInt32 nrDims = vc->GetNrDims();
	if (nrDims != 2 && !vc->IsNumeric())
		throwSemanticError(mgFormat2string("DoUnitRangeProp: this unit does not allow range assignment because its ValueType is %s", vc->GetName()).c_str());

	const ValueClass* ic = ValueClass::FindByValueClassID(m_eAssignmentDomainType);
	if (ic->GetNrDims() != nrDims || (nrDims == 1 && !ic->IsNumeric()))
		throwSemanticError(mgFormat2string("DoUnitRangeProp: the provided range is incompatible with the ValueType %s of this unit", vc->GetName()).c_str());

	switch (nrDims) {
		case 1:
			dms_assert(vc->IsNumeric() && ic->IsNumeric());
			unit->SetRangeAsFloat64(m_FloatInterval.first, m_FloatInterval.second);
			break;

		case 2:
			unit->SetRangeAsDPoint(
				_Top   (m_DPointInterval), _Left (m_DPointInterval),
				_Bottom(m_DPointInterval), _Right(m_DPointInterval)
			);
			break;

		default:
			throwDmsErrF("DoUnitRangeProp: cannot set range of units with ValueType %s ", vc->GetName().c_str());
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
		m_pCurrent, 
		m_strIdentifierID.GetStr().c_str(),
		m_sPropFileTypeID.GetStr().c_str(),
		false
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
			"Unknown property '%s'", 
			GetTokenStr(m_strIdentifierID).c_str()
		);
	pd->SetValueAsCharRange(m_pCurrent, m_StringVal.begin(), m_StringVal.send());
}

void ConfigProd::DoExprProp(iterator_t first, iterator_t last)
{
	dms_assert(m_pCurrent);

	m_pCurrent->SetExpr(SharedStr(&*first, &*last));
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

void ConfigProd::DoNrOfRowsProp()
{
	assert(m_eValueType == ValueClassID::VT_UInt64);
	assert(m_pCurrent);

	AbstrUnit* unit = AsCheckedUnit(m_pCurrent.get_ptr());
	assert(unit);
	const ValueClass* vc = unit->GetValueType();
	assert(vc);

	if (!vc->IsNumeric())
		throwSemanticError(mgFormat2string("DoUnitRangeProp: the provided range is incompatible with the ValueType %s of this unit", vc->GetName()).c_str());

	unit->SetTSF(USF_HasConfigRange | TSF_Categorical);
	unit->SetRangeAsUInt64(0, m_IntValAsUInt64);
}

void ConfigProd::throwSemanticError(CharPtr msg)
{
	TreeItem* curr = m_pCurrent;
	UInt32 i = m_stackContexts.size();
	while (!curr && i--)
		curr = m_stackContexts[i];

	Object::throwItemErrorF(curr, "Semantic error %s", msg);
}