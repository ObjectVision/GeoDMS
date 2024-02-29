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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/CheckPtr.h"
#include "dbg/DebugCast.h"
#include "dbg/DmsCatch.h"
#include "ser/ValueTypeStream.h"
#include "mci/ValueWrap.h"

#include "AbstrDataItem.h"
#include "DataItemClass.h"
#include "TreeItemClass.h"
#include "AbstrDataObject.h"
#include "UnitClass.h"
#include "Unit.h"

//----------------------------------------------------------------------
// class  : DataItemClass
//----------------------------------------------------------------------

DataItemClass::DataItemClass(
	Constructor       cFunc, 
	TokenID           typeID,
	const ValueClass* valuesType)
	: 	Class(cFunc, AbstrDataObject::GetStaticClass(), typeID),
		m_ValuesType(valuesType)
{
	dms_assert(valuesType && !valuesType->m_DataItemClass);
	valuesType->m_DataItemClass = this;
}

DataItemClass::~DataItemClass()
{
	m_ValuesType->m_DataItemClass = nullptr;
}

bool DataItemClass::IsDataObjType() const
{
	return true;
}

// static
TIC_CALL AbstrDataItem* CreateAbstrDataItem(
	TreeItem*        parent, 
	TokenID          nameID, 
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc)
{
	AbstrDataItem* 
		result = debug_cast<AbstrDataItem*>(
			parent->CreateItem(nameID, AbstrDataItem::GetStaticClass())
		);
	dms_assert(result);
	result->InitAbstrDataItem(tDomainUnit, tValuesUnit, vc);
	return result;
}

TIC_CALL AbstrDataItem* CreateAbstrDataItemFromPath(
	TreeItem*        parent,
	CharPtr          path,
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc)
{
	AbstrDataItem*
		result = debug_cast<AbstrDataItem*>(
			parent->CreateItemFromPath(path, AbstrDataItem::GetStaticClass())
		);
	dms_assert(result);
	result->InitAbstrDataItem(tDomainUnit, tValuesUnit, vc);
	return result;
}

// static
TIC_CALL AbstrDataItem* CreateDataItem(
	TreeItem*        context, 
	TokenID          nameID, 
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc)
{
	dms_assert(valuesUnit);
	dms_assert(vc != ValueComposition::Unknown);
	return 
		DataItemClass::FindCertain(
			valuesUnit->GetValueType(vc)
		,	context
		)->CreateDataItem(
				context 
			,	nameID 
			,	domainUnit 
			,	valuesUnit
			,	vc 
			);
}

TIC_CALL AbstrDataItem* CreateDataItemFromPath(
	TreeItem*        context,
	CharPtr          path,
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc)
{
	dms_assert(valuesUnit);
	dms_assert(vc != ValueComposition::Unknown);
	return
		DataItemClass::FindCertain(
			valuesUnit->GetValueType(vc)
			, context
		)->CreateDataItemFromPath(
			context
			, path
			, domainUnit
			, valuesUnit
			, vc
		);
}

AbstrDataItem* CreateCacheDataItem(
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc)
{
	AbstrDataItem* adi = CreateDataItem(nullptr, TokenID::GetEmptyID(), domainUnit, valuesUnit, vc);
	adi ->SetPassor();
	return adi;
}


AbstrDataItem* DataItemClass::CreateDataItem(
	TreeItem*        context, 
	TokenID          nameID, 
	const AbstrUnit* domainUnit, // Default unit will be selected
	const AbstrUnit* valuesUnit,
	ValueComposition vc) const
{
	dms_assert(vc != ValueComposition::Unknown);
	dms_assert((vc == ValueComposition::Single) == (GetValuesType()->GetValueComposition() == ValueComposition::Single));

	AbstrDataItem* 
		result
			=	CreateAbstrDataItem(
					context
				,	nameID 
				,	TokenID::GetEmptyID()
				,	TokenID::GetEmptyID()
				,	vc
				);

	dms_assert(result);
	result->m_DomainUnit = domainUnit;
	result->m_ValuesUnit = valuesUnit;
	return result;
}

AbstrDataItem* DataItemClass::CreateDataItemFromPath(
	TreeItem*        context,
	CharPtr          path,
	const AbstrUnit* domainUnit, // Default unit will be selected
	const AbstrUnit* valuesUnit,
	ValueComposition vc) const
{
	dms_assert(vc != ValueComposition::Unknown);
	dms_assert((vc == ValueComposition::Single) == (GetValuesType()->GetValueComposition() == ValueComposition::Single));

	AbstrDataItem* result = CreateAbstrDataItemFromPath(context, path
			, TokenID::GetEmptyID()
			, TokenID::GetEmptyID()
			, vc
		);

	dms_assert(result);
	result->m_DomainUnit = domainUnit;
	result->m_ValuesUnit = valuesUnit;
	return result;
}

const DataItemClass* DataItemClass::Find(const ValueClass* valuesType)
{
	dms_assert(valuesType);
	return valuesType->m_DataItemClass;
}

const DataItemClass* DataItemClass::FindCertain(
	const ValueClass* valuesType,
	const TreeItem* context)
{
	assert(valuesType); // PRECONDITION
	const DataItemClass* dic = Find(valuesType);
	if (!dic)
		context->throwItemErrorF(
			"Cannot find DataItemClass for ValuesType %s"
		,	valuesType->GetName().c_str()
		);
	return dic;
}

#include "xml/XmlParser.h"

static TokenID nameTokenID = GetTokenID_st("name");
static TokenID domainUnitTokenID = GetTokenID_st("DomainUnit");
static TokenID valuesUnitTokenID = GetTokenID_st("ValuesUnit");
static TokenID featureTypeID = GetTokenID_st("ValueComposition");

Object* DataItemClass::CreateFromXml(Object* context, XmlElement& elem)
{
	CheckPtr(context, TreeItem::GetStaticClass(), "DataItemClass::CreateFromXml");
	TreeItem* container = debug_cast<TreeItem*>(context);

	return CreateAbstrDataItem(container, 
		GetTokenID_mt(elem.GetAttrValue(nameTokenID)),
		GetTokenID_mt(elem.GetAttrValue(domainUnitTokenID)),
		GetTokenID_mt(elem.GetAttrValue(valuesUnitTokenID)),
		DetermineValueComposition(elem.GetAttrValue(featureTypeID))
	);
}

//----------------------------------------------------------------------
// streaming utility functions
//----------------------------------------------------------------------

#include "ser/FormattedStream.h"

FormattedOutStream& operator <<(FormattedOutStream& os, const DataItemClass& sc)
{
	return
	os << "DataItemClass " << sc.GetID()
	   << ", " << *(sc.GetValuesType())
	   << ") ";
}

IMPL_RTTI_METACLASS(DataItemClass, "DATAITEM", DataItemClass::CreateFromXml)

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

#include "TicInterface.h"

#include "AbstrUnit.h"
#include "utl/mySPrintF.h"

TIC_CALL AbstrDataItem* DMS_CONV DMS_CreateDataItem(
	TreeItem*        context, 
	CharPtr          name, 
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc)
{
	DMS_CALL_BEGIN

		dms_assert(name);
		dms_assert(vc >= ValueComposition::Single);
		dms_assert(vc < ValueComposition::Count);

		CheckPtr(context, TreeItem ::GetStaticClass(), "DMS_CreateDataItem");

		if (!valuesUnit)
			context->throwItemErrorF(
				"DMS_CreateDataItem('%s') called without ValuesUnit", 
				name
			);

//		dms_assert(!context->IsCacheItem()); ReportFunctions genereert wel in cache

		CheckPtr(valuesUnit, AbstrUnit::GetStaticClass(), "DMS_CreateDataItem");
		if (domainUnit)
			CheckPtr(domainUnit, AbstrUnit::GetStaticClass(), "DMS_CreateDataItem");
		else if (IsUnit(context)) // Try the parent as a unit
			domainUnit = AsUnit(context);
		else if (IsDataItem(context))
			domainUnit = AsDataItem(context)->GetAbstrValuesUnit();
		else
			throwDmsErrF("DMS_CreateDataItem [%s] called without DomainUnit", name);
		dms_assert(domainUnit);

		return CreateDataItem(
			context, 
			GetTokenID_mt(name),
			domainUnit, valuesUnit, 
			vc
		);

	DMS_CALL_END
	return 0;
}

TIC_CALL AbstrParam* DMS_CONV DMS_CreateParam(
	TreeItem*  context, CharPtr name, 
	const AbstrUnit* valueUnit, ValueComposition vc)
{
	DMS_CALL_BEGIN

		CheckPtr(context,   TreeItem::GetStaticClass(),  "DMS_CreateParam");
		CheckPtr(valueUnit, AbstrUnit::GetStaticClass(), "DMS_CreateParam");

		AbstrDataItem* param = DMS_CreateDataItem(
				context, name, 
				Unit<Void>::GetStaticClass()->CreateDefault(), 
				valueUnit, vc);
		dms_assert(param);

		return param;

	DMS_CALL_END
	return 0;
}

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

TIC_CALL const DataItemClass* DMS_CONV DMS_DataItemClass_Find(const UnitClass* vuc, ValueComposition vc)
{
	DMS_CALL_BEGIN

		CheckPtr(vuc, UnitClass::GetStaticClass(), "DMS_DataItemClass_Find");

		return  DataItemClass::FindCertain(vuc->GetValueType(vc), 0);

	DMS_CALL_END
	return 0;
}

