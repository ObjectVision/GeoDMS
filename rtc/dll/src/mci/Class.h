// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_MCI_CLASS_H
#define __RTC_MCI_CLASS_H

#include "mci/Object.h"
#include "ptr/SharedPtr.h"
#include "act/Actor.h" // for SharedActor (= SharedObjWrap<Actor>): the owning return type of the CreateFromXml factories

//----------------------------------------------------------------------
// Macro's for RunTimeTypeInfo (introspection) and dynamic creation
//----------------------------------------------------------------------

// IMPL

#define IMPL_ABSTR(OBJ, CLASS) \
	const CLASS* s_register##OBJ = OBJ::GetStaticClass();

#define IMPL_RTTI(OBJ, CLASS) \
	const Class* OBJ::GetDynamicClass() const { return GetStaticClass(); }  \
	IMPL_ABSTR(OBJ, CLASS)

// IMPL_T1

#define IMPL_RTTI_T1(cls, CLASS, T) \
	const Class* cls<T>::GetDynamicClass() const { return GetStaticClass(); }  \
	static const CLASS*  g_##T##cls##Cls = cls<T>::GetStaticClass();

//**********  dynamic creation                           **********

using Constructor = Object* (*)();

/********** Class Class Definition (non-polymorphic) **********/

class MetaClass;

class Class : public Object
{
	using base_type = Object;

public:
	// migration (a): TreeItem-family classes also carry a std::make_shared-based creator (sFunc); nullptr otherwise.
	using SharedConstructor = std::shared_ptr<Object>(*)();
	RTC_CALL Class(Constructor cFunc, const Class* baseCls, TokenID typeID, SharedConstructor sFunc = nullptr);
	RTC_CALL ~Class();

	RTC_CALL TokenID GetID() const override { return m_TypeID; }

	RTC_CALL bool IsDerivedFrom(const Class* base) const;
	RTC_CALL Object*         CreateObj() const;
	RTC_CALL std::shared_ptr<Object> CreateSharedObj() const; // migration (a): make_shared-based; null if !HasSharedCreator
	bool HasSharedCreator() const { return m_SharedConstructor != nullptr; }
	RTC_CALL         bool    HasDefaultCreator() const;
	RTC_CALL virtual bool    IsDataObjType() const;

	RTC_CALL static const Class*     Find(TokenID id);
	RTC_CALL static UInt32           Size(); // # in ClassKernel
	RTC_CALL static const ClassCPtr* Begin();
	RTC_CALL static const ClassCPtr* End();

	class  AbstrPropDef* GetLastPropDef        () const { return m_LastPD;         }
	class  AbstrPropDef* GetLastCopyablePropDef() const { return m_LastCopyablePD; }
	RTC_CALL class  AbstrPropDef* FindPropDef(TokenID nameID) const;

	const Class* GetBaseClass()    const { return m_BaseClass;    }
	const Class* GetLastSubClass() const { return m_LastSubClass; }
	const Class* GetPrevClass()    const { return m_PrevClass;    }

	DECL_RTTI(RTC_CALL, MetaClass)

private:
	mutable const Class*  m_BaseClass;
	mutable const Class*  m_LastSubClass;
	mutable const Class*  m_PrevClass;

	mutable AbstrPropDef* m_LastPD; friend class  AbstrPropDef;
	mutable AbstrPropDef* m_LastCopyablePD; 
	Constructor           m_Constructor;
	SharedConstructor     m_SharedConstructor; // migration (a): make_shared creator for TreeItem-family; else nullptr
	TokenID               m_TypeID;
};

// IMPL_CLASS

#define IMPL_CLASS(cls, CreateFunc) \
	const Class* cls::GetStaticClass() \
	{ \
		static Class s_Cls(CreateFunc, cls::base_type::GetStaticClass(), GetTokenID_st(#cls) ); \
		return &s_Cls; \
	} 

#define IMPL_ABSTR_CLASS(OBJ) IMPL_ABSTR(OBJ, Class) IMPL_CLASS(OBJ, nullptr)
#define IMPL_RTTI_CLASS(OBJ)  IMPL_RTTI(OBJ, Class)  IMPL_CLASS(OBJ, nullptr)

// IMPL_CLASS_T1

#define IMPL_CLASS_T1(cls, T, cFunc) \
	const Class* cls<T>::GetStaticClass() \
	{ \
		static Class s_StaticCls(cFunc, base_type::GetStaticClass(), GetTokenID_st(#cls "<" #T ">") ); \
		return &s_StaticCls; \
	} 

/********** Class MetaClass Definition (non-polymorphic) **********/

class MetaClass : public Class
{
	typedef Class base_type;
public:
	// Factories own their freshly created product from birth and hand it back as an owning shared_ptr,
	// so no parentless item is ever exposed as a raw, unowned pointer (orphanage detected at the source).
	// The element type is Actor: every factory product is TreeItem-derived, and TreeItem now derives from
	// Actor directly (the TreeItem family carries its own std control block via enable_shared_from_this; it
	// is no longer a SharedObjWrap<Actor>/SharedActor). Returns the std::shared_ptr control block of the
	// freshly created (make_shared'd) object, upcast to Actor. std ownership flows through here so a
	// brand-new parentless root survives until the caller (XmlTreeParser) captures it.
	typedef std::shared_ptr<Actor> (*createFromXmlFuncType)(Object* currObj, struct XmlElement& elem);

	RTC_CALL MetaClass(
		TokenID scriptID,
		const Class* baseCls,
		createFromXmlFuncType xmlFunc);
	RTC_CALL ~MetaClass();

	RTC_CALL static MetaClass* Find(TokenID id);
	RTC_CALL std::shared_ptr<Actor> CreateFromXml(Object* currObj, struct XmlElement& elem) const;

	DECL_RTTI(RTC_CALL, MetaClass)

private:
	createFromXmlFuncType m_XmlFunc;
};


// IMPL_METACLASS

#define IMPL_METACLASS(cls, SCRIPTNAME, XML_FUNC) \
	const MetaClass*  cls::GetStaticClass() \
	{ \
		static MetaClass s_Cls(GetTokenID_st(SCRIPTNAME), base_type::GetStaticClass(), XML_FUNC); \
		return &s_Cls; \
	} 

#define IMPL_RTTI_METACLASS(cls, SCRIPTNAME, XML_FUNC) \
	IMPL_RTTI(cls, MetaClass) \
	IMPL_METACLASS(cls, SCRIPTNAME, XML_FUNC) 


#endif // __RTC_MCI_CLASS_H
