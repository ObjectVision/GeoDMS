// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

/*
 *  Name        : mci/Object.h
 *  Description : provides streaming, rtti, serialisation
 *
 *	Instance : Class relations are as follows:
 * 
 *	SharedBase: AddRef, DelRef, m_RefCount
 *	NameBase: GetID()->TokenID, m_ID
 *
 *	FileDescr: SharedBase
 *	SourceLocation: SharedBase
 *	Object:             Class
 *		Value
 *			PropDef:        PropDefClass(Class), NameBase
 *			Class:          MetaClass(Class), NameBase
 *				MetaClass:      MetaClass(MetaClass)
 *				ItemClass:      MetaClass(ItemClass)
 *				DataItemClass:  MetaClass(DataItemClass)
 *				PropDefClass:   MetaClass(PropDefClass)
 *		SharedObj : ..., SharedBase
 *			TreeObject, aka PersistentSharedObj: ..., GetFirst, GetNext, GetParent(); TODO G8: Rename and separate containers from anchestor relations with share owned relations.
 *          ===================
 *				TreeItem:         ItemClass(TreeItem), NamedBase
 *					AbstrDataItem:  DataItemClass(AbstrDataItem)
 *					AbstrUnitItem:  DataItemClass(AbstrDataItem)
 */

#ifndef __RTC_MCI_OBJECT_H
#define __RTC_MCI_OBJECT_H

#include "RtcBase.h"

//#include "set/Token.h"

struct ErrMsg;
struct SourceLocation;
using ErrMsgPtr = std::shared_ptr<ErrMsg>;

//----------------------------------------------------------------------
// Macro's for RunTimeTypeInfo (introspection) and dynamic creation
//----------------------------------------------------------------------

//	DECL_ABSTR 
//	used in interfaces of classes who's instances are always of a derived type
//	(abstract base classes). Only GetStaticClass() is defined.

#	define DECL_ABSTR(CALLTYPE, CLASS) \
	public: CALLTYPE static const CLASS* GetStaticClass();

//	DECL_RTTI 
//	used in interfaces of classes that can actually be instantiated.
//	The code-unit that defines the non-inline members of the class
//	is responsible for implementing GetStaticClass() & GetDynamicClass()
//	See macro's in mci/Class.h for implementation support.
//	The implementation of GetStaticClass registers a static instance 
//	of (a derived type of) Class. It can register a virtual constructor.

#define DECL_RTTI(CALLTYPE, CLASS)   \
	DECL_ABSTR(CALLTYPE, CLASS) \
	CALLTYPE const Class* GetDynamicClass() const override;

// *****************************************************************************
// Section:     Persistent Object Interface
// *****************************************************************************

// TODO: Separation of responsibilities: 
// - Named Tree of things:
//	 - Tree of things: GetParent()
//	 - Traversable: GetFirst(), GetNext()
//   - namespace: GetID()->TokenID; m_ID in thing or ID as key in a dictionary of things.
//	 - who owns who: now=subitems shared-own their parents; consider=containment=parents shared-own their children and have an id-ed linked list of them
// - reflection: GetDynamicClass()
// - dynamic construction: CreateFunc()
// - polymorphic serialisation (requires reflection and dynamic construction)
// - runtime polymorphy or efficiency (no vtable, known size)
// - shared ownership: m_RefCount, IncCount(), DecCount(), visible destructor
// 
// Containers
// - dynamic or const size, known at contruction
// - context known size or size needs to be stored with or next to array, such as with shared-owne
// - element type polymophic (size) or fixed
// - contained or share-owned
// solutions


// Separation of responsibilities, reordered:
// 1. Object: runtime polymorphism
// - reflection: GetDynamicClass()
// - optional dynamic construction: CreateFunc()
// - polymorphic serialisation (requires reflection and dynamic construction)
// 2. SharedBase: non-virtual shared ownership support, base for non-runtime polymorphic share-owned structs: _RefCount, IncCount(), DecCount(), visible destructor
// 3. SharedObject: Object, SharedBase: Release
// 4. PersistentSharedObj: SharedObject
// - Named Tree of things:
//	 - Tree of things: GetParent()
//	 - Traversable: GetFirst(), GetNext()
//   - namespace: GetID()->TokenID; m_ID in thing or ID as key in a dictionary of things.
//	 - TODO G8.5: who owns who: now=subitems shared-own their parents; consider=containment=parents shared-own their children and have an id-ed linked list of them

/********** Object Interface: runtime polymorphism, adds a bit to the vtables **********/

class Object
{
protected:
#if defined(MG_DEBUG)

	RTC_CALL Object();
	RTC_CALL virtual ~Object();

#else // inline ctor/dtor in release version

	Object() {}
	virtual ~Object() {}

#endif
	Object(const Object&) = delete;
	Object(Object&&) = delete;

	Object& operator =(const Object&) = delete;
	Object& operator =(Object&&) = delete;

public:
	RTC_CALL virtual TokenID GetID() const;

	RTC_CALL virtual SharedStr GetSourceName() const;
	RTC_CALL virtual const SourceLocation* GetLocation() const;

	RTC_CALL virtual const Class* GetDynamicClass() const;

	// TODO G8: Move to AbstrDataItem
	RTC_CALL virtual const Class* GetDynamicObjClass() const;
	RTC_CALL virtual const Class* GetCurrentObjClass() const;

	RTC_CALL virtual const Object* _GetAs(const Class* cls) const;
	const Object* GetAs(const Class* cls) const { return _GetAs(cls); }

	// TODO G8: Move to AbstrDataItem
	// serialization
	RTC_CALL virtual void ReadObj (PolymorphInpStream&);
	RTC_CALL virtual void WriteObj(PolymorphOutStream&) const;

	// XML Support; what does it depend on, how does it vary
	RTC_CALL virtual void XML_Dump(OutStreamBase* xmlOutStr) const;
	RTC_CALL virtual void XML_DumpData(OutStreamBase* xmlOutStr) const;

	RTC_CALL TokenStr GetXmlClassName() const;
	RTC_CALL virtual TokenID GetXmlClassID() const;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE
	RTC_CALL TokenStr GetName() const;
	RTC_CALL bool    IsKindOf(const Class* cls) const;
	RTC_CALL TokenStr GetClsName() const; // Warning: GetClsName is already a #defined symbol in WINUSER.H
	RTC_CALL TokenID GetClsID() const;

	DECL_ABSTR(RTC_CALL, Class);
};

//**********  dynamic creation                           **********

//	CreateFunc can be used in definition of Obj::GetStaticClass()

template <typename CLS> inline
Object* CreateFunc() { return new CLS(); }


#endif // __RTC_MCI_OBJECT_H
