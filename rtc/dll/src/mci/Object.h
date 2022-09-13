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

#include "set/token.h"
#include "ptr/SharedStr.h"
struct ErrMsg;
struct SourceLocation;

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

	TokenStr GetXmlClassName() const { return GetXmlClassID().GetStr(); }
	RTC_CALL virtual TokenID GetXmlClassID() const;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE
	TokenStr GetName() const { return GetID().GetStr(); }
	RTC_CALL bool    IsKindOf(const Class* cls) const;
	RTC_CALL TokenStr GetClsName() const; // Warning: GetClsName is already a #defined symbol in WINUSER.H
	RTC_CALL TokenID GetClsID() const;

public:
	//	====================== Why here, TODO G8 move to separate header
	[[noreturn]] static RTC_CALL void throwItemError(const ErrMsg& msg);

	[[noreturn]] static RTC_CALL void throwItemError(const Object* self, WeakStr msgStr);
	[[noreturn]] static RTC_CALL void throwItemError(const Object* self, CharPtr msg);

	template<typename ...Args>
	[[noreturn]] static void throwItemErrorF(const Object* self, CharPtr msg, Args&&... args) {
		throwItemError(self, mgFormat2SharedStr(msg, std::forward<Args>(args)...));
	}


	[[noreturn]] RTC_CALL void throwItemError(WeakStr msgStr) const { throwItemError(this, msgStr); }
	[[noreturn]] void throwItemError(CharPtr msg) const { throwItemError(this, SharedStr(msg)); }

	template<typename ...Args>
	[[noreturn]] void throwItemErrorF(CharPtr msg, Args&&... args) const {
		throwItemErrorF(this, msg, std::forward<Args>(args)...);
	}

	DECL_ABSTR(RTC_CALL, Class);
};

//**********  dynamic creation                           **********

//	CreateFunc can be used in definition of Obj::GetStaticClass()

template <typename CLS> inline
Object* CreateFunc() { return new CLS(); }


#endif // __RTC_MCI_OBJECT_H
