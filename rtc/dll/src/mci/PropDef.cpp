// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "dbg/Debug.h"
#include "mci/PropDef.h"
#include "mci/PropDefEnums.h"
#include "mci/ValueClass.h"
#include "ptr/PersistentSharedObj.h"
#include "ser/StringStream.h"

#include <set>

/********** PropDef Interface **********/

#include "mci/AbstrValue.h"

AbstrPropDef::AbstrPropDef(CharPtr propName, 
		const Class* pc, const ValueClass* vt, 
		set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, chg_mode chgMode, bool isStored, bool canBeIndirect, bool addImplicitSuppl)
	:	m_PropNameID(GetTokenID_st(propName)), m_ValueClass(vt) ,m_Class(pc)
	,	m_SetMode(setMode), m_XmlMode(xmlMode), m_CpyMode(cpyMode), m_ChgMode(chgMode)
	,	m_PrevPD(0)
	,	m_PrevCopyablePD(0)
	,	m_CanBeIndirect(canBeIndirect)
	,	m_AddImplicitSuppl(addImplicitSuppl)
#if defined(MG_DEBUGDATA)
	,	md_Name(propName)
#endif
{
	dms_assert(setMode > set_mode::construction || cpyMode == cpy_mode::none);
	dms_assert(setMode > set_mode::construction || chgMode == chg_mode::none);
	dms_assert(setMode > set_mode::construction || !canBeIndirect);
	dms_assert(setMode > set_mode::construction || !addImplicitSuppl);
	dms_assert(!addImplicitSuppl          || canBeIndirect);

	AbstrPropDef** insertionPoint = &(pc->m_LastPD);
	while (*insertionPoint && stricmp((*insertionPoint)->GetID().c_str_st(), propName) < 0)
		insertionPoint = &((*insertionPoint)->m_PrevPD);
	dms_assert(!*insertionPoint || stricmp((*insertionPoint)->GetID().c_str_st(), propName) > 0); // no double names

	m_PrevPD = *insertionPoint;
	*insertionPoint = this;

	if (cpyMode > cpy_mode::none && !isStored)
	{
		m_PrevCopyablePD     = pc->m_LastCopyablePD;
		pc->m_LastCopyablePD = this;
	}
}

AbstrPropDef::~AbstrPropDef()
{
#ifdef MG_DEBUG
// remove this from list
	CheckPtr(m_Class, 0, "AbstrPropDef::Destructor");
	AbstrPropDef** pdPtr = &(m_Class->m_LastPD);
	while (*pdPtr != this && *pdPtr !=0) 
	{
		CheckPtr(*pdPtr, AbstrPropDef::GetStaticClass(), "AbstrPropDef::Destructor");
		pdPtr = &((*pdPtr)->m_PrevPD);
	}
	dms_assert(*pdPtr == this);
	*pdPtr = (*pdPtr)->m_PrevPD;
#endif
}

TokenID AbstrPropDef::GetID() const
{
	return m_PropNameID;
}
// non virtual helper functions
bool AbstrPropDef::HasNonDefaultValue(const Object* self) const
{
	return true; // best guess.
}

AbstrValue* AbstrPropDef::CreateValue() const
{
	return debug_cast<AbstrValue*>(m_ValueClass->CreateObj());
}

IMPL_RTTI_CLASS(AbstrPropDef)

/********** Property management for destroying Objects **********/

void AbstrPropDef::RemoveValue(Object* item)
{
	throwIllegalAbstract(MG_POS, "AbstrPropDef::RemoveValue");
}

/********** AbstrPropDef Properties **********/

namespace {

struct ItemClassProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	ItemClassProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("ItemClass", set_mode::construction, xml_mode::attribute) 
	{}

	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		MGD_PRECONDITION(pd);
		return pd->GetItemClass()->GetID();
	}
};

struct ValueClassProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	ValueClassProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("ValueClass", set_mode::construction, xml_mode::attribute) 
	{}

	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		MGD_PRECONDITION(pd);
		return pd->GetValueClass()->GetID();
	}
};

TokenID GetXmlModeID(xml_mode m)
{
	static_assert(int(xml_mode::none) == 0);
	static_assert(int(xml_mode::name) == 1);
	static_assert(int(xml_mode::signature) == 2);
	static_assert(int(xml_mode::attribute) == 3);
	static_assert(int(xml_mode::element) == 4);
	if (UInt32(m) > UInt32(xml_mode::element))
		throwErrorD("PropDef", "Unknown xml_mode");

	static TokenID xmlModeID[5] = {
		TokenID("none", (mt_tag*) nullptr),
		TokenID("name", (mt_tag*) nullptr),
		TokenID("signature", (mt_tag*) nullptr),
		TokenID("attribute", (mt_tag*) nullptr),
		TokenID("element", (mt_tag*) nullptr)
	};
	return xmlModeID[UInt32(m)];
}

struct XmlModeProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	XmlModeProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("XmlMode", set_mode::construction, xml_mode::attribute) 
	{}

	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		MGD_PRECONDITION(pd);
		return GetXmlModeID(pd->GetXmlMode());
	}
};

TokenID GetSetModeID(set_mode m)
{
	static_assert(int(set_mode::none) == 0);
	static_assert(int(set_mode::construction) == 1);
	static_assert(int(set_mode::obligated) == 2);
	static_assert(int(set_mode::optional) == 3);

	if (UInt32(m) > UInt32(set_mode::optional))
		throwErrorD("PropDef", "Unknown set_mode");
	static TokenID setModeID[4] = {
		TokenID("none", (mt_tag*)nullptr),
		TokenID("construction", (mt_tag*)nullptr),
		TokenID("obligated", (mt_tag*)nullptr),
		TokenID("optional", (mt_tag*)nullptr)
	};
	return setModeID[UInt32(m)];
}

struct SetModeProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	SetModeProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("SetMode", set_mode::construction, xml_mode::attribute)
	{}

	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		dms_assert(pd);
		return GetSetModeID(pd->GetSetMode());
	}
};

TokenID GetCpyModeID(cpy_mode m)
{
	static_assert(int(cpy_mode::none) == 0);
	static_assert(int(cpy_mode::expr_noroot) == 1);
	static_assert(int(cpy_mode::expr) == 2);
	static_assert(int(cpy_mode::all) == 3);

	if (UInt32(m) > UInt32(cpy_mode::all))
		throwErrorD("PropDef", "Unknown cpy_mode");
	static TokenID cpyModeID[4] = {
		TokenID("none", (mt_tag*)nullptr),
		TokenID("subexpr", (mt_tag*)nullptr),
		TokenID("expr", (mt_tag*)nullptr),
		TokenID("all", (mt_tag*)nullptr)
	};
	return cpyModeID[UInt32(m)];
}

struct CpyModeProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	CpyModeProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("CpyMode", set_mode::construction, xml_mode::attribute) 
	{}
	// override base class
	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		MGD_PRECONDITION(pd);
		return GetCpyModeID(pd->GetCpyMode());
	}
};

TokenID GetChgModeID(chg_mode m)
{
	static_assert(int(chg_mode::none) == 0);
	static_assert(int(chg_mode::eval) == 1);
	static_assert(int(chg_mode::invalidate) == 2);

	if (UInt32(m) > UInt32(chg_mode::invalidate))
		throwErrorD("PropDef", "Unknown chg_mode");
	static TokenID chgModeID[3] = {
		TokenID("none", (mt_tag*)nullptr),
		TokenID("eval", (mt_tag*)nullptr),
		TokenID("invalidate", (mt_tag*)nullptr)
	};
	return chgModeID[UInt32(m)];
}

struct ChgModeProp: ReadOnlyPropDef<AbstrPropDef, TokenID>
{
	ChgModeProp()
		:	ReadOnlyPropDef<AbstrPropDef, TokenID>("ChgMode", set_mode::construction, xml_mode::attribute)
	{}
	// override base class
	TokenID GetValue(const AbstrPropDef* pd) const override
	{
		dms_assert(pd);
		return GetChgModeID(pd->GetChgMode());
	}
};

namespace
{
	ItemClassProp  itemClassProp;
	ValueClassProp valueClassProp;
	XmlModeProp xmlModeProp;
	SetModeProp setModeProp;
	CpyModeProp cpyModeProp;
	ChgModeProp chgModeProp;
}



}
/********** PropDef Reporting Interface **********/

#include "PropDefInterface.h"
#include "dbg/DmsCatch.h"
//#include "string.h"

RTC_CALL CharPtr  DMS_CONV DMS_Class_GetName(const Class* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_GetName");
		return self->GetName().c_str();
	
	DMS_CALL_END
	return "unknown class";
}

RTC_CALL AbstrPropDef* DMS_CONV DMS_Class_FindPropDef(const Class* self, CharPtr propName)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_FindPropDef");
		return self->FindPropDef(GetTokenID_mt(propName));

	DMS_CALL_END
	return nullptr;
}

RTC_CALL AbstrPropDef* DMS_CONV DMS_Class_GetLastPropDef(const Class* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_GetLastPropDef");
		return self->GetLastPropDef();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL CharPtr  DMS_CONV DMS_PropDef_GetName(const AbstrPropDef* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_GetName");
		return self->GetName().c_str();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL AbstrPropDef* DMS_CONV DMS_PropDef_GetPrevPropDef(const AbstrPropDef* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_GetPrevPropDef");
		return self->GetPrevPropDef();

	DMS_CALL_END
	return nullptr;
}

/********** Property          Definition (non-polymorphic) **********/

RTC_CALL void         DMS_CONV DMS_PropDef_GetValue(const AbstrPropDef* self, const Object* me, AbstrValue* value)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_GetValue");
		CheckPtr(me,   self->GetItemClass(),           "DMS_PropDef_GetValue");
		MG_CHECK(self->GetValueClass() == value->GetValueClass());

		self->GetAbstrValue(me, value);

	DMS_CALL_END
}

RTC_CALL IStringHandle DMS_CONV DMS_PropDef_GetValueAsIString(const AbstrPropDef* self, const Object* me)
{
	DMS_CALL_BEGIN

 		CheckPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_GetValueAsCharArrayAsIString");
		CDebugContextHandle dch("DMS_PropDef_GetValueAsCharArrayAsIString", self->GetName().c_str(), false);
		CheckPtr(me,   self->GetItemClass(),           "DMS_PropDef_GetValueAsCharArrayAsIString");

		SharedStr value = self->GetValueAsSharedStr(me);
		return IString::Create(value);

	DMS_CALL_END
	return nullptr;
}


RTC_CALL AbstrValue*  DMS_CONV DMS_PropDef_CreateValue(const AbstrPropDef* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_CreateValue");

		return self->CreateValue();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL void         DMS_CONV DMS_PropDef_SetValue(AbstrPropDef* self, Object* me, const AbstrValue* value)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_SetValue");
		ObjectContextHandle checkPtr2(me,   self->GetItemClass(),           "DMS_PropDef_SetValue");

		MG_CHECK(self->GetValueClass() == value->GetValueClass());

		self->SetAbstrValue(me, value);

	DMS_CALL_END
}

RTC_CALL void DMS_CONV DMS_PropDef_SetValueAsCharArray(AbstrPropDef* self, Object* me, CharPtr buffer)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_SetValueAsCharArray");
		ObjectContextHandle checkPtr2(me, self->GetItemClass(), "DMS_PropDef_SetValueAsCharArray");

		self->SetValueAsCharArray(me, buffer);

	DMS_CALL_END
}


RTC_CALL const ValueClass* DMS_CONV DMS_PropDef_GetValueClass(const AbstrPropDef* self)
{
	DMS_CALL_BEGIN

		ObjectContextHandle checkPtr(self, AbstrPropDef::GetStaticClass(), "DMS_PropDef_GetValueClass");
		return self->GetValueClass();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL const Class* DMS_CONV DMS_PropDef_GetStaticClass()
{
	DMS_CALL_BEGIN

		return AbstrPropDef::GetStaticClass();

	DMS_CALL_END
	return nullptr;
}
