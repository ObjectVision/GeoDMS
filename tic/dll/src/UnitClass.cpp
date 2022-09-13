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
#pragma hdrstop

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

UnitClass::UnitClass(Constructor cFunc, TokenID typeID, const ValueClass* valueType)
	: 	Class(cFunc, AbstrUnit::GetStaticClass(), typeID)
	,	m_ValueType(valueType)
	,	m_DefaultUnit(0)
{
	g_UnitClassRegister.Register(this);
	dms_assert(valueType && !valueType->m_UnitClass);
	valueType->m_UnitClass = this;
	CreateDefault();
}

UnitClass::~UnitClass()
{
//	dms_assert(!m_DefaultUnit);
	DropDefault();
	g_UnitClassRegister.Unregister(this);
	m_ValueType->m_UnitClass = nullptr;
}

AbstrUnit* UnitClass::CreateUnit(TreeItem* context, TokenID id) const
{
	if (ValueClass::FindByScriptName(id) )
	{
		throwErrorF("UnitClass", "Cannot create a %s with the name '%s', since this name indicates a basic type"
		,	GetName().c_str()
		,	GetTokenStr(id).c_str()
		);
	}
	return AsUnit(context->CreateItem(id, this));
}

AbstrUnit* UnitClass::CreateUnitFromPath(TreeItem* context, CharPtr path) const
{
	if (ValueClass::FindByScriptName(TokenID::GetExisting(path)))
	{
		throwErrorF("UnitClass", "Cannot create a %s with the name '%s', since this name indicates a basic type"
			, GetName().c_str()
			, path
		);
	}
	return AsUnit(context->CreateItemFromPath(path, this));
}

AbstrUnit* UnitClass::CreateResultUnit(TreeItem* context) const
{
	if (context)
		return AsUnit(context);
	AbstrUnit* result = CreateUnit(nullptr, TokenID::GetEmptyID());
	result->SetPassor();
	result->DisableStorage();
	return result;
}

AbstrUnit* UnitClass::CreateTmpUnit(TreeItem* context) const
{
	AbstrUnit* result = CreateResultUnit(context);
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

			m_DefaultUnit = CreateTmpUnit(nullptr);
			dms_assert(m_DefaultUnit);
			m_DefaultUnit->DisableAutoDelete();
		}
	}
	return m_DefaultUnit;
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
				throwDmsErrF("cannot combine ValueClass %s and composition specifier %s", vc->GetName(), GetValueCompositionID(*vcPtr));
			*vcPtr = vc->m_ValueComposition;
			vc = vc->m_FieldClass;
		}
		const UnitClass* uc = Find(vc);
		if (uc)
			return uc->CreateDefault();
	}
	SharedStr unitName(id);
	return AsDynamicUnit( context->FindItem(unitName));
}

const UnitClass* UnitClass::Find(const ValueClass* valueClass)
{
	dms_assert(valueClass);
	return valueClass->m_UnitClass;
}

const ValueClass* UnitClass::GetValueType(ValueComposition vc) const
{ 
	if (vc == ValueComposition::Single)
		return m_ValueType;
	dms_assert(vc == ValueComposition::Polygon || vc == ValueComposition::Sequence);
	return m_ValueType->GetSequenceClass();
}

#include "xml/XmlParser.h"

static TokenID nameTokenID = GetTokenID_st("name");
static TokenID valueTypeID = GetTokenID_st("ValueType");

Object* UnitClass::CreateFromXml(Object* context, struct XmlElement& elem)
{
	CheckPtr(context, TreeItem::GetStaticClass(), "UnitClass::CreateFromXml");
	TreeItem* container = debug_cast<TreeItem*>(context);

	CharPtr itemName      = elem.GetAttrValue(nameTokenID); 
	CharPtr valueTypeName = elem.GetAttrValue(valueTypeID);

	const ValueClass* vc = ValueClass::FindByScriptName(GetTokenID_mt(valueTypeName) );
	if (!vc) throwDmsErrF("Unknown ValueType '%s' for Unit '%s'", valueTypeName, itemName);
	const UnitClass* uc = UnitClass::Find(vc);
	if (!uc) throwDmsErrF("UnitClass for found for ValueType %s", vc->GetName());
	return uc->CreateUnit(container, GetTokenID_mt(itemName));
}

//----------------------------------------------------------------------
// reflection
//----------------------------------------------------------------------

IMPL_RTTI_METACLASS(UnitClass, "UNIT", UnitClass::CreateFromXml)

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
				"UnitClass not found for ValueType %s", 
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
