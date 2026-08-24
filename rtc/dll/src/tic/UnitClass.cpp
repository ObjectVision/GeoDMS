// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "TicPCH.h"
#include "act/UpdateMark.h" // UpdateMarker

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "UnitClass.h"

#include "act/TriggerOperator.h"
#include "dbg/DmsCatch.h"
#include "dbg/DebugCast.h"
#include "dbg/DebugContext.h"
#include "mci/ValueComposition.h"
#include "ser/ValueTypeStream.h"
#include "utl/IncrementalLock.h"

#include "AbstrUnit.h"
#include "TreeItemClass.h"
#include "Unit.h"

//----------------------------------------------------------------------
// class  : UnitClass
//----------------------------------------------------------------------

#include "mci/register.h"

namespace {

	typedef StaticRegister<UnitClass, TokenID, CompareLtItemIdPtrs<UnitClass> > RegisterType;
	RegisterType g_UnitClassRegister;
}

UnitClass::UnitClass(Constructor cFunc, TokenID typeID, const ValueClass* valueType, SharedConstructor sFunc)
	: 	Class(cFunc, AbstrUnit::GetStaticClass(), typeID, sFunc)
	,	m_ValueType(valueType)
	,	m_DefaultUnit(0)
{
	g_UnitClassRegister.Register(this);
	dms_assert(valueType && !valueType->m_UnitClass);
	valueType->m_UnitClass = this;
	// CreateDefault() is deferred to first access (called lazily from CreateDefault()).
	// Eager creation here caused a static initialization order crash on Linux:
	// UnitClass ctor -> CreateDefault -> CreateTmpUnit -> TreeItem::CreateItem
	// -> TreeItem::GetStaticClass() which may not be initialized yet across TUs.
}

UnitClass::~UnitClass()
{
//	dms_assert(!m_DefaultUnit);
	DropDefault();
	g_UnitClassRegister.Unregister(this);
	m_ValueType->m_UnitClass = nullptr;
}

auto UnitClass::CreateUnit(TreeItem* context, TokenID id) const -> SharedMutableUnit
{
	if (ValueClass::FindByScriptName(id) )
	{
		throwErrorF("UnitClass", "Cannot create a {} with the name '{}', since this name indicates a basic type"
		,	GetName().c_str()
		,	GetTokenStr(id).c_str()
		);
	}
	return AsUnit(TreeItem_CreateItem(context, id, this));
}

auto UnitClass::CreateUnitFromPath(TreeItem* context, CharPtr path) const ->  SharedMutableUnit
{
	if (ValueClass::FindByScriptName(TokenID::GetExisting(path)))
	{
		throwErrorF("UnitClass", "Cannot create a {} with the name '{}', since this name indicates a basic type"
			, GetName().c_str()
			, path
		);
	}
	return AsUnit(context->CreateItemFromPath(path, this));
}

auto UnitClass::CreateResultUnit(TreeItem* context) const -> SharedMutableUnit
{
	if (context)
		return make_shared_tree(AsUnit(context), existing_obj{}); // context is an existing (owned) tree item
	auto result = CreateUnit(nullptr, TokenID::GetEmptyID());
	result->SetPassor();
	result->DisableStorage();
	return result;
}

auto UnitClass::CreateTmpUnit(TreeItem* context) const -> SharedMutableUnit
{
	auto result = CreateResultUnit(context);
	if (!context)
		result->SetMaxRange();
	result->SetKeepDataState(true);
	return result;
}
//std::mutex cs_DefaultUnit;

const AbstrUnit* UnitClass::CreateDefault() const
{
	if (!m_DefaultUnit)
	{
//		auto lock = std::scoped_lock(cs_DefaultUnit);

		if (!m_DefaultUnit)
		{
			StaticStIncrementalLock<TreeItem::s_MakeEndoLockCount> makeEndoLock;
			UpdateMarker::ChangeSourceLock changeStamp(UpdateMarker::tsBereshit, "CreateDefault");

#if defined(MG_DEBUG_INTERESTSOURCE)
			DemandManagement::IncInterestDetector incInterestLock("UnitClass::CreateDefault()");
#endif // MG_DEBUG_INTERESTSOURCE

			m_DefaultUnit = CreateTmpUnit(nullptr); // std::shared_ptr sole owner from birth
			assert(m_DefaultUnit);
		}
	}
	return m_DefaultUnit.get();
}

void UnitClass::DropDefault() const
{
	if (!m_DefaultUnit)
		return;

	m_DefaultUnit->EnableAutoDelete();
	m_DefaultUnit = nullptr;
};

const AbstrUnit* UnitClass::GetUnitOrDefault(const TreeItem* context, TokenID id, ValueComposition* vcPtr)
{
	const ValueClass* vc = ValueClass::FindByScriptName(id);
	if (vc) {
		if (IsAcceptableValuesComposition(vc->m_ValueComposition) && vcPtr)
		{
			if (*vcPtr != ValueComposition::Single)
				throwDmsErrF("cannot combine ValueClass {} and composition specifier {}", vc->GetName(), GetValueCompositionID(*vcPtr));
			*vcPtr = vc->m_ValueComposition;
			vc = vc->m_FieldClass;
		}
		const UnitClass* uc = Find(vc);
		if (uc)
			return uc->CreateDefault();
	}
	SharedStr unitName(id);
	return AsDynamicUnit( context->ResolveItemPath(unitName).get());
}

const UnitClass* UnitClass::Find(const ValueClass* valueClass)
{
	assert(valueClass);
	return valueClass->m_UnitClass;
}

const ValueClass* UnitClass::GetValueType(ValueComposition vc) const
{ 
	if (vc == ValueComposition::Single)
		return m_ValueType;
	assert(vc == ValueComposition::Polygon || vc == ValueComposition::Sequence || vc == ValueComposition::MultiPoint);
	return m_ValueType->GetSequenceClass();
}

#include "xml/XmlParser.h"

static StaticTokenID nameTokenID("name");
static StaticTokenID valueTypeID("ValueType");

std::shared_ptr<Actor> UnitClass::CreateFromXml(Object* context, struct XmlElement& elem)
{
	CheckPtr(context, TreeItem::GetStaticClass(), "UnitClass::CreateFromXml");
	TreeItem* container = debug_cast<TreeItem*>(context);

	CharPtr itemName      = elem.GetAttrValue(nameTokenID);
	CharPtr valueTypeName = elem.GetAttrValue(valueTypeID);

	const ValueClass* vc = ValueClass::FindByScriptName(GetTokenID_mt(valueTypeName) );
	if (!vc) throwDmsErrF("Unknown ValueType '{}' for Unit '{}'", valueTypeName, itemName);
	const UnitClass* uc = UnitClass::Find(vc);
	if (!uc) throwDmsErrF("UnitClass for found for ValueType {}", vc->GetName());
	// the new unit is co-owned by its parent container; return its std::shared_ptr (control block flows through)
	return uc->CreateUnit(container, GetTokenID_mt(itemName));
}

//----------------------------------------------------------------------
// reflection
//----------------------------------------------------------------------

IMPL_RTTI_METACLASS(UnitClass, "Unit", UnitClass::CreateFromXml)

//----------------------------------------------------------------------
// destruction of default units
//----------------------------------------------------------------------

namespace {
	UInt32 s_nrLocks = 0;
}

UnitClassRegComponentLock::UnitClassRegComponentLock()
{
	if (s_nrLocks++)
		return;
	dms_assert(s_nrLocks); // no overflow
}

UnitClassRegComponentLock::~UnitClassRegComponentLock()
{
	if (--s_nrLocks)
		return;

	dms_assert(g_UnitClassRegister.Empty());
	if (!g_UnitClassRegister.Empty())
	{
		RegisterType::const_iterator
			b = g_UnitClassRegister.Begin(),
			e = g_UnitClassRegister.End();
		while (b != e)
			(*b++)->DropDefault();
	}
}

//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

#include "TicInterface.h"

//----------------------------------------------------------------------
// C style Interface functions for class id retrieval
//----------------------------------------------------------------------

TIC_CALL const UnitClass* DMS_CONV DMS_UnitClass_Find(const ValueClass* valueClass)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(valueClass, ValueClass::GetStaticClass(), "DMS_UnitClass_Find");
		
		const UnitClass* uc = UnitClass::Find(valueClass);
		if (!uc)
			throwErrorF("DMS_UnitClass_Find", 
				"UnitClass not found for ValueType {}", 
				valueClass->GetName()
			);

		return uc;

	DMS_CALL_END
	return nullptr;
}

TIC_CALL const AbstrUnit*  DMS_CONV DMS_GetDefaultUnit(const UnitClass* uc)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(uc, UnitClass::GetStaticClass(), "DMS_GetDefaultUnit");

		return uc->CreateDefault();

	DMS_CALL_END
	return nullptr;
}
