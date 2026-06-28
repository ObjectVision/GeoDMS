// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
TIC_CALL SharedMutableDataItem CreateAbstrDataItem(
	TreeItem*        parent,
	TokenID          nameID,
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc)
{
	// Keep the owning std::shared_ptr: when parent==nullptr (cache items) it is the SOLE owner, so
	// returning a raw pointer would free the item immediately (the old OwningPtr.release() left it in
	// refcount-0 limbo; std::shared_ptr deletes deterministically). Callers that assign into a
	// TreeItemDualRef adopt ownership via its owning operator=; parented callers may use .get().
	SharedMutableTreeItem item = parent->CreateItem(nameID, AbstrDataItem::GetStaticClass());
	auto* result = debug_cast<AbstrDataItem*>(item.get());
	dms_assert(result);
	result->InitAbstrDataItem(tDomainUnit, tValuesUnit, vc);
	return SharedMutableDataItem(std::static_pointer_cast<AbstrDataItem>(std::move(item)));
}

TIC_CALL SharedMutableDataItem CreateAbstrDataItemFromPath(
	TreeItem*        parent,
	CharPtr          path,
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc)
{
	SharedMutableTreeItem item = parent->CreateItemFromPath(path, AbstrDataItem::GetStaticClass());
	auto* result = debug_cast<AbstrDataItem*>(item.get());
	assert(result);
	result->InitAbstrDataItem(tDomainUnit, tValuesUnit, vc);
	return SharedMutableDataItem(std::static_pointer_cast<AbstrDataItem>(std::move(item)));
}

// static
TIC_CALL SharedMutableDataItem CreateDataItem(
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

TIC_CALL SharedMutableDataItem CreateDataItemFromPath(
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

SharedMutableDataItem CreateCacheDataItem(
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc)
{
	SharedMutableDataItem adi = CreateDataItem(nullptr, TokenID::GetEmptyID(), domainUnit, valuesUnit, vc);
	adi ->SetPassor();
	return adi;
}


SharedMutableDataItem DataItemClass::CreateDataItem(
	TreeItem*        context,
	TokenID          nameID,
	const AbstrUnit* domainUnit, // Default unit will be selected
	const AbstrUnit* valuesUnit,
	ValueComposition vc) const
{
	dms_assert(vc != ValueComposition::Unknown);
	dms_assert((vc == ValueComposition::Single) == (GetValuesType()->GetValueComposition() == ValueComposition::Single));

	SharedMutableDataItem
		result
			=	CreateAbstrDataItem(
					context
				,	nameID
				,	TokenID::GetEmptyID()
				,	TokenID::GetEmptyID()
				,	vc
				);

	dms_assert(result);
	result->m_DomainUnit = SharedUnit( domainUnit, existing_obj{} ); // store as weak back-ref (unit owned by tree)
	result->m_ValuesUnit = SharedUnit( valuesUnit, existing_obj{} );
	return result;
}

SharedMutableDataItem DataItemClass::CreateDataItemFromPath(
	TreeItem*        context,
	CharPtr          path,
	const AbstrUnit* domainUnit, // Default unit will be selected
	const AbstrUnit* valuesUnit,
	ValueComposition vc) const
{
	dms_assert(vc != ValueComposition::Unknown);
	dms_assert((vc == ValueComposition::Single) == (GetValuesType()->GetValueComposition() == ValueComposition::Single));

	SharedMutableDataItem result = CreateAbstrDataItemFromPath(context, path
			, TokenID::GetEmptyID()
			, TokenID::GetEmptyID()
			, vc
		);

	dms_assert(result);
	result->m_DomainUnit = SharedUnit( domainUnit, existing_obj{} ); // store as weak back-ref (unit owned by tree)
	result->m_ValuesUnit = SharedUnit( valuesUnit, existing_obj{} );
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

std::shared_ptr<Actor> DataItemClass::CreateFromXml(Object* context, XmlElement& elem)
{
	CheckPtr(context, TreeItem::GetStaticClass(), "DataItemClass::CreateFromXml");
	TreeItem* container = debug_cast<TreeItem*>(context);

	// CreateAbstrDataItem returns the owning std::shared_ptr (SharedMutableDataItem); return it upcast to
	// Actor so the std control block flows through (co-owned with the parent container).
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
		).get(); // item owned by context (parent); raw stays valid for the C-API caller

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

