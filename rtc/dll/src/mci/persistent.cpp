// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "PropDefInterface.h"

#include "ptr/PersistentSharedObj.h"
#include "mci/Class.h"
#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"
#include "ptr/SharedStr.h"
#include "set/VectorFunc.h"
#include "xml/XMLOut.h"

#include "dbg/DmsCatch.h"
#include "dbg/debug.h"
#include "utl/splitPath.h"
#include "utl/mySPrintF.h"
#include "xct/DmsException.h"
#include "LockLevels.h"
#include <stdarg.h>

// *****************************************************************************
// Section:     Annotated Object register for CheckPtr
// *****************************************************************************

#if defined(MG_DEBUG) && 0
#include <crtdbg.h>
#	define MG_CHECKPTR
#endif

#if defined(MG_CHECKPTR)

	#if !defined(MG_DEBUG)
		#error "definition of MG_CHECKPTR should imply MG_DEBUG"
	#endif

	#include <set>
	typedef std::set<const Object*> objectRegisterType;
	leveled_std_section cs_ORT(item_level_type(0), ord_level_type::ObjectRegister, "ObjectRegister");

	objectRegisterType* g_ObjectRegister = nullptr;

	bool IsObjectRegistered(const Object* item)
	{
		leveled_std_section::scoped_lock sl_ORT(cs_ORT);
		return g_ObjectRegister->find(item) != g_ObjectRegister->end();

	}
	void CheckPtr(const Object* item, const Class* cls, CharPtr dmsFunc)
	{
		dms_assert(g_ObjectRegister);
		if (!item)
			throwErrorF("CheckPtr", "function %s: Invalid NULL pointer supplied", dmsFunc);
		if (!IsObjectRegistered(item)) 
			throwErrorF("CheckPtr", "function %s: Invalid pointer %lx supplied", dmsFunc, item);
		if (cls)
		{
			if (!item->IsKindOf(cls) && !item->GetDynamicObjClass()->IsDerivedFrom(cls))
				throwErrorF("CheckPtr", "function %s requires an item of type %s", 
					dmsFunc, cls->GetName()
				);
		}
	}

	std::vector<TokenID> GetObjectRegisterCopy()
	{
		leveled_std_section::scoped_lock sl_ORT(cs_ORT);
		if (!g_ObjectRegister)
			return {};
		std::vector<TokenID> result; result.reserve(g_ObjectRegister->size());
		for (auto poPtr : *g_ObjectRegister)
			result.push_back(poPtr->GetID());
		return result;
	}

	void ReportExistingObjects()
	{
		auto orc = GetObjectRegisterCopy();
		if (orc.size())
		{
			DMS_CALL_BEGIN

				_RPT1(_CRT_WARN, "\n\nMemory Leak of %d Objects after destructing all DMS Runtime components", orc.size());
				for (auto t : orc)
					_RPT1(_CRT_WARN, "\nMemoLeak: Object %s", t.GetStr().c_str());

			DMS_CALL_END
		}
	}

// *****************************************************************************
// **********         Object Implementation                           **********
// *****************************************************************************

	Object::Object()
	{
		leveled_std_section::scoped_lock sl_ORT(cs_ORT);
		if (!g_ObjectRegister)
			g_ObjectRegister = new objectRegisterType;
		g_ObjectRegister->insert(this);
	}

	Object::~Object()
	{
		leveled_std_section::scoped_lock sl_ORT(cs_ORT);
		dms_assert(g_ObjectRegister);
		g_ObjectRegister->erase(this);
		if (!g_ObjectRegister->size())
		{
			delete g_ObjectRegister;
			g_ObjectRegister = nullptr;
		}
	}

#elif defined(MG_DEBUG) // and not defined(MG_CHECKPTR)

	void CheckPtr(const Object* item, const Class* cls, CharPtr dmsFunc)
	{
		if (!item) 
			throwErrorF("CheckPtr", "Invalid Null Pointer in %s", dmsFunc);

		if (cls && !item->IsKindOf(cls) && !item->GetDynamicObjClass()->IsDerivedFrom(cls))
			throwErrorF("CheckPtr", "Invalid Item in %s; %s expected" , dmsFunc, cls->GetName().c_str());
	}
	static UInt32 g_NrPersistentObjects = 0;
	
	void ReportExistingObjects()
	{
		if (g_NrPersistentObjects)
		{
			DMS_CALL_BEGIN

#if defined(_MSC_VER)
				_RPT1(_CRT_WARN, "\n\nMemory Leak of %d Objects after destructing all DMS Runtime components\nUse MG_CHECKPTR to determine which objects", 
					g_NrPersistentObjects
				);
#else //defined(_MSC_VER)

				// GNU TODO

#endif //defined(_MSC_VER)

			DMS_CALL_END
		}
	}

	Object::Object()
	{
		++g_NrPersistentObjects;
	}
	
	Object::~Object()
	{
		--g_NrPersistentObjects;
	}
#else
	void ReportExistingObjects()
	{
		// NOP, nothing to report in Release version
	}

#endif // defined(MG_CHECKPTR) or defined(MG_DEBUG)

// *****************************************************************************
// Section:     Object CODE
// *****************************************************************************


TokenID Object::GetID() const
{
	const Class* cls = GetDynamicClass();
#ifdef MG_CHECKPTR
	if (!cls || cls == this)
		return TokenID(); // Class::GetID returns m_TypeID, if Class object was destroyed, infinite recursion is at the corner; which happens at ReportExistingObjects()
#endif // MG_CHECKPTR
	dms_assert(cls && cls != this); 
	return cls->GetID();
}

TokenStr Object::GetClsName() const
{
	const Class* cls = GetCurrentObjClass();
#ifdef MG_CHECKPTR
	if (!cls) 
		return TokenID("#DestructedClass#", (mt_tag*)nullptr).GetStr();
#endif // MG_CHECKPTR
	return cls->GetName();
}

TokenID Object::GetClsID() const
{
	const Class* cls = GetCurrentObjClass();
#ifdef MG_CHECKPTR
	if (!cls) return TokenID(Undefined());
#endif // MG_CHECKPTR
return cls->GetID();
}

TokenStr Object::GetName() const
{
	return GetID().GetStr();
}

bool Object::IsKindOf(const Class* cls) const
{
	auto dynamic_class =  GetDynamicClass();
	return dynamic_class->IsDerivedFrom(cls);
}

TokenStr Object::GetXmlClassName() const
{ 
	return GetXmlClassID().GetStr(); 
}

TokenID Object::GetXmlClassID() const
{
	const Class* cls = GetDynamicClass();
	return cls->GetClsID();
}

// GetDynamicClass() is pure virtual, 
// but can be called at an object that threw an exception in the destructor of TreeItem.
// probably a compiler bug.

const Class* Object::GetDynamicClass() const
{
	return GetStaticClass();
}

const Class* Object::GetDynamicObjClass() const
{
	return GetDynamicClass();
}

const Class* Object::GetCurrentObjClass() const
{
	return GetDynamicObjClass();
}

const Object* Object::_GetAs(const Class* cls) const
{
	return IsKindOf(cls)
		? this
		: nullptr;
}

// *****************************************************************************
// Section:     PersistentSharedObj impl
// *****************************************************************************

const PersistentSharedObj* PersistentSharedObj::GetParent() const
{
	return nullptr;
}

const PersistentSharedObj* PersistentSharedObj::GetRoot() const
{
	const PersistentSharedObj* result = this;
	while (true)
	{
		const PersistentSharedObj* r2 = result->GetParent();
		if (!r2)
			return result;
		result = r2;
	}
}

bool PersistentSharedObj::DoesContain(const PersistentSharedObj* subItemCandidate) const
{
	while (subItemCandidate)
	{
		if (subItemCandidate == this)
			return true;
		subItemCandidate = subItemCandidate->GetParent();
	}
	return false;
}

SharedStr Object::GetSourceName() const
{
	return SharedStr(GetID());
}

const SourceLocation* Object::GetLocation() const
{
	return nullptr;
}

const SourceLocation* PersistentSharedObj::GetLocation() const
{ 
	auto parent = GetParent();
	if (parent)
		return parent->GetLocation();
	return nullptr;
}

SharedStr PersistentSharedObj::GetFullName() const
{
	dms_assert(this);
	// calc size
	UInt32 nameSz = 1;
	auto item = this;
	while (auto parent = item->GetParent()) {
		nameSz += item->GetID().GetStrLen() + 1;
		item = parent;
	}
	dms_assert(nameSz);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(nameSz);
	SharedStr resultStr(result);

	char* nameConcatBufferPtr = result->end();

	// reset and fill buffer
	item = this;
	*--nameConcatBufferPtr = char(0);
	while (auto parent = item->GetParent()) {
		TokenID nameToken = item->GetID();
		auto nameLen = nameToken.GetStrLen();
		nameConcatBufferPtr -= nameLen;
		auto range = nameToken.AsStrRange();
		fast_copy(range.m_CharPtrRange.begin(), range.m_CharPtrRange.end(), nameConcatBufferPtr);
		*--nameConcatBufferPtr = DELIMITER_CHAR;
		item = parent;
	}

	dms_assert(nameConcatBufferPtr == result->begin());

	return resultStr;
}

SharedStr PersistentSharedObj::GetPrefixedFullName() const
{
	auto result = GetFullName();
	auto rootID = GetRoot()->GetID();
	if (rootID)
		result = SharedStr(rootID) + ":" + result;
 	return result;

}

SharedStr PersistentSharedObj::GetRelativeName(const PersistentSharedObj* context) const
{
	MGD_PRECONDITION(context);
	MGD_PRECONDITION(context->DoesContain(this));

	if (this == context)
		return SharedStr();
	// calc size
	UInt32 nameSz = 0;
	auto item = this;
	while (item && item != context) {
		nameSz += item->GetID().GetStrLen()+1;
		item = item->GetParent();
	}
	dms_assert(nameSz);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(nameSz);
	SharedStr resultStr(result);

	char* nameConcatBufferPtr = result->end();

	// reset and fill buffer
	item = this;
	*--nameConcatBufferPtr = char(0);
	while (true) {
		TokenID nameToken= item->GetID();
		auto nameLen = nameToken.GetStrLen();
		nameConcatBufferPtr -= nameLen;
		fast_copy(nameToken.GetStr().c_str(), nameToken.GetStrEnd().c_str(), nameConcatBufferPtr);
		item = item->GetParent();
		if (item == context)
			break;
		*--nameConcatBufferPtr = DELIMITER_CHAR;
	}

	dms_assert(nameConcatBufferPtr == result->begin());

	return resultStr;
}

static TokenComponent tokenService;
static SharedStr str_Dot(".");

SharedStr PersistentSharedObj::GetFindableName(const PersistentSharedObj* subItem) const
{
	dms_assert(IsMetaThread());
	dms_check_not_debugonly;

	dms_assert(this);
	dms_assert(subItem);
	dms_assert(GetRoot() == subItem->GetRoot());
	if (subItem == this)
		return str_Dot;

	UInt32 upCount = 0;
	auto self = this;
	while (!self->DoesContain(subItem))
	{
		++upCount;
		self = self->GetParent();
		dms_assert(self);
	}
//	if (!self->GetParent())
//		return subItem->GetFullName();

	SharedStr result = subItem->GetRelativeName(self); // returns volatile pointer within GetFullName token str
	if (upCount)
	{
		++upCount;
		return DelimitedConcat(RepeatedDots(upCount), result.c_str());
	}
	return result;
}

void Object::ReadObj (PolymorphInpStream&)
{}

void Object::WriteObj(PolymorphOutStream&) const
{}

SharedStr PersistentSharedObj::GetSourceName() const
{
	if (GetParent())
		return
			mySSPrintF("%s: %s"
			,	GetFullName().c_str()
			,	GetClsName().c_str()
			);
	TokenStr nameID = GetName();
	return 
		mySSPrintF("%s: %s"
		,	nameID.c_str()
		,	GetClsName().c_str()
		);
}

void throwItemError(ErrMsgPtr msg)
{
	DmsException::throwMsg(msg);
}

void throwItemError(const PersistentSharedObj* self, WeakStr msgStr)
{ 
	throwItemError(std::make_shared<ErrMsg>( msgStr, self ) ); 
}

void throwItemError(const PersistentSharedObj* self, CharPtr msg)
{
	throwItemError(self, SharedStr(msg));
}

void Object::XML_Dump(OutStreamBase* xmlOutStr) const
{ 
	XML_OutElement xmlElem(*xmlOutStr, GetXmlClassName().c_str(), GetName().c_str(), ClosePolicy::pairedOnNewline);

	xmlOutStr->DumpPropList(this);
	xmlOutStr->DumpSubTags(this);
}

void Object::XML_DumpData(OutStreamBase* xmlOutStr) const
{ 
	throwIllegalAbstract(MG_POS, "PersistentSharedObj::XML_DumpData");
}

const Class* Object::GetStaticClass()
{
	static Class s_StaticCls(0, 0, GetTokenID_st("Object"));
	return &s_StaticCls;
}

const Class* PersistentSharedObj::GetStaticClass()
{
	static Class s_StaticCls(0, 0, GetTokenID_st("PersistentSharedObj") );
	return &s_StaticCls;
} 

/**********  PersistentSharedObj Properties ********************/

namespace {

	struct ClassProp : ReadOnlyPropDef < PersistentSharedObj, TokenID >
	{
		ClassProp()
			: ReadOnlyPropDef("Class", set_mode::none, xml_mode::attribute)
		{}

		// override base class
		TokenID GetValue(const PersistentSharedObj* pd) const override
		{
			MGD_PRECONDITION(pd);
			return pd->GetClsID();
		}
	};

	struct XmlClassProp : ReadOnlyPropDef < PersistentSharedObj, TokenID >
	{
		XmlClassProp()
			: ReadOnlyPropDef("XmlClass", set_mode::construction, xml_mode::name)
		{}
		// override base class
		TokenID GetValue(const PersistentSharedObj* pd) const override
		{
			MGD_PRECONDITION(pd);
			return pd->GetXmlClassID();
		}
	};

	namespace { 
		ClassProp classProp;
		XmlClassProp xmlClassProp;
	}

} // end anonymous namespace

/**********  PersistentSharedObj Interface ********************/

RTC_CALL const Class* DMS_CONV DMS_GetRootClass()
{
	return PersistentSharedObj::GetStaticClass();
}

// *****************************************************************************
// Section:     Class CODE
// *****************************************************************************

#include "mci/register.h"

StaticRegister<const Class, TokenID, CompareLtItemIdPtrs<Class> > g_ClassKernel;

/**********  Class CODE ********************/

Class::Class(Constructor cFunc, const Class* baseCls, TokenID typeID)
	: m_Constructor(cFunc), m_TypeID(typeID)
	, m_BaseClass(baseCls), m_LastSubClass(0), m_PrevClass(0)
	, m_LastPD(0) // DON'T INITIALIZE TWICE
	, m_LastCopyablePD(0)
{
	if (baseCls)
	{
		m_PrevClass = baseCls->m_LastSubClass;
		baseCls->m_LastSubClass = this;
	}
	g_ClassKernel.Register(this);
}

Class::~Class()
{
	g_ClassKernel.Unregister(this);
}

Object* Class::CreateObj() const
{
	if (!m_Constructor)
		throwIllegalAbstract(MG_POS, "Class::Create()");
	return m_Constructor(); 
}

bool Class::IsDataObjType() const
{
	return false;
}

bool Class::HasDefaultCreator() const
{
	return m_Constructor != 0;
}

UInt32 Class::Size() // static
{
	return g_ClassKernel.Size();
}

const ClassCPtr* Class::Begin() // static
{
	dms_assert( g_ClassKernel.s_Register );
	return begin_ptr( * g_ClassKernel.s_Register );
}

const ClassCPtr* Class::End() // static
{
	dms_assert( g_ClassKernel.s_Register );
	return end_ptr( * g_ClassKernel.s_Register );
}

const Class* Class::Find(TokenID id) // static
{
	return g_ClassKernel.Find(id);
}

// Single inheritance version as inline non member func because self/this is changed in loop 
// to avoid the recursion as seen in multiple inheritance version


bool Class::IsDerivedFrom(const Class* base) const
{
	const Class* self = this;
	dms_assert(self);
	do
	{
		if (self == base)
			return true;
		self = self->GetBaseClass();
	} while (self);
	return false;
}

AbstrPropDef* Class::FindPropDef(TokenID nameID) const
{
	const Class* self = this;

	dms_assert(self);
	do
	{
		AbstrPropDef* currPropDef = self->m_LastPD;
		while (currPropDef)
		{
			if (currPropDef->GetID() == nameID)
				return currPropDef;
			currPropDef = currPropDef->GetPrevPropDef();
		}
		self = self->GetBaseClass();
	} while (self);
	return nullptr;
}

IMPL_RTTI_METACLASS(Class, "OBJECT", nullptr)


RTC_CALL const Class* DMS_CONV DMS_Class_GetStaticClass()
{
	DMS_CALL_BEGIN

		return Class::GetStaticClass();

	DMS_CALL_END
	return nullptr;
}

/**********  MetaClass CODE ********************/

StaticRegister<MetaClass, TokenID, CompareLtItemIdPtrs<MetaClass> > g_MetaClassKernel;

MetaClass::MetaClass(
		TokenID scriptID, 
		const Class* baseCls, 
		createFromXmlFuncType xmlFunc)
		: Class(0, baseCls, scriptID)
		, m_XmlFunc(xmlFunc)
{
	g_MetaClassKernel.Register(this);
}


MetaClass::~MetaClass()
{
	g_MetaClassKernel.Unregister(this);
}

MetaClass* MetaClass::Find(TokenID id)
{
	return g_MetaClassKernel.Find(id);
}

Object* MetaClass::CreateFromXml(Object* context, struct XmlElement& elem) const
{
	dms_assert(m_XmlFunc);
	return m_XmlFunc(context, elem);
}

IMPL_RTTI_METACLASS(MetaClass, "CLASS", nullptr)

/********** Interface Implementation **********/

// PersistentSharedObj Composition Interface
RTC_CALL const PersistentSharedObj* DMS_CONV DMS_Object_GetParent(const PersistentSharedObj* self, UInt32 index)
{
	DMS_CALL_BEGIN

		CheckPtr(self, NULL, "DMS_Object_GetParent");

		return self->GetParent();

	DMS_CALL_END
	return nullptr;
}


// PersistentSharedObj Naming Interface
RTC_CALL CharPtr       DMS_CONV DMS_Object_GetName    (const PersistentSharedObj* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, NULL, "DMS_Object_GetName");

		return self->GetName().c_str();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL CharPtr DMS_CONV DMS_Object_GetFullName(const PersistentSharedObj* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, NULL, "DMS_Object_GetFullName");

		return self->GetFullName().c_str();

	DMS_CALL_END
	return nullptr;
}

// PersistentSharedObj Dynamic Typing Interface
RTC_CALL const Class*  DMS_CONV DMS_Object_GetDynamicClass(const PersistentSharedObj* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, NULL, "DMS_Object_GetDynamicClass");

		return self->GetDynamicObjClass();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL Object* DMS_CONV DMS_Object_QueryInterface(Object* self, const Class* requiredClass)
{
	DMS_CALL_BEGIN

		CheckPtr(self, NULL, "DMS_Object_QueryInterface");
		CheckPtr(requiredClass, Class::GetStaticClass(), "DMS_Object_QueryInterface");

		return self->IsKindOf(requiredClass) ? self : 0;

	DMS_CALL_END
	return nullptr;
}


/********** Inheritance Class Definition (non-polymorphic) **********/

RTC_CALL const Class* DMS_CONV DMS_Class_GetBaseClass(const Class* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_GetBaseClass");

		return self->GetBaseClass();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL const Class* DMS_CONV DMS_Class_GetLastSubClass(const Class* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_GetLastSubClass");

		return self->GetLastSubClass();

	DMS_CALL_END
	return nullptr;
}

RTC_CALL const Class* DMS_CONV DMS_Class_GetPrevSubClass(const Class* self)
{
	DMS_CALL_BEGIN

		CheckPtr(self, Class::GetStaticClass(), "DMS_Class_GetPrevSubClass");

		return self->GetPrevClass();

	DMS_CALL_END
	return nullptr;
}

