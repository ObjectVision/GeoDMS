// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Name        : mci/Object.h
 *  Description : provides streaming, rtti, serialisation
 *
 *	Instance : Class relations are as follows:
 * 
 *	SharedBase: AddRef, DelRef, m_RefCount
 *	NameBase: GetNameID()->TokenID, m_ID
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

//#include "sym/Token.h"

struct ErrMsg;
struct SourceLocation;
using ErrMsgPtr = std::shared_ptr<ErrMsg>;

#include "throwItemError.h"

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
//   - namespace: GetNameID()->TokenID; m_ID in thing or ID as key in a dictionary of things.
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
//   - namespace: GetNameID()->TokenID; m_ID in thing or ID as key in a dictionary of things.
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
	// Callee contracts (#1227): these virtuals are what the error-reporting path calls to name an
	// item (see AbstrMsgGenerator in dbg/DebugContext.h), so their base declarations state what
	// any override may enter. GetID and GetLocation are pure reads -- every override returns a
	// member or a preregistered token, and an override that starts resolving or registering
	// breaks the reporting path (#1225 was exactly such a re-resolve). The class-introspection
	// trio may search the class register (ObjectRegister, inner) and may throw -- e.g.
	// AbstrDataItem::GetDynamicObjClass via DataItemClass::FindCertain -- which stays
	// registry-shared. GetSourceName may format names, nothing outer.
	RTC_CALL virtual TokenID GetNameID() const DMS_CALLEE_ENTERS_NOTHING;

	RTC_CALL virtual SharedStr GetSourceName() const DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	RTC_CALL virtual const SourceLocation* GetLocation() const DMS_CALLEE_ENTERS_NOTHING;

	RTC_CALL virtual const Class* GetDynamicClass() const DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);

	// TODO G8: Move to AbstrDataItem
	RTC_CALL virtual const Class* GetDynamicObjClass() const DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	RTC_CALL virtual const Class* GetCurrentObjClass() const DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);

	RTC_CALL virtual const Object* _GetAs(const Class* cls) const;
	const Object* GetAs(const Class* cls) const { return _GetAs(cls); }

	// XML Support; what does it depend on, how does it vary
	RTC_CALL virtual void XML_Dump(OutStreamBase* xmlOutStr) const;
	RTC_CALL virtual void XML_DumpData(OutStreamBase* xmlOutStr) const;

	[[nodiscard]] SharedStr GetXmlClassName() const;
	[[nodiscard]] TokenStr  GetXmlClassNameLock() const;
	RTC_CALL virtual TokenID GetXmlClassID() const;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE
	//
	// The plain name materializes; the ...Lock variant hands out a TokenStr, which HOLDS A SHARED
	// USAGE OF THE TOKEN REGISTRY for as long as it lives -- and, as an unnamed argument temporary,
	// that is until the end of the full expression. Registering any token in the meantime (which
	// most non-trivial calls can reach) is a self-deadlock: see sym/Token.h and issue #1227.
	// Use ...Lock only where the string is consumed immediately and the call cannot tokenize, or
	// where a CharPtr into the registry must outlive the expression (the DMS_* C interface).
	[[nodiscard]] RTC_CALL SharedStr GetName() const;
	[[nodiscard]] RTC_CALL TokenStr  GetNameLock() const;
	[[nodiscard]] RTC_CALL bool    IsKindOf(const Class* cls) const;
	[[nodiscard]] RTC_CALL SharedStr GetClsName() const; // Warning: GetClsName is already a #defined symbol in WINUSER.H
	[[nodiscard]] RTC_CALL TokenStr  GetClsNameLock() const;
	[[nodiscard]] RTC_CALL TokenID GetClsID() const;

	/// Return the full configuration name; default maps to GetName().
	/// Override to include configuration-specific qualifiers.
	///
	/// Contract: DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v). Naming an item is what
	/// the error-reporting path does for a living (see AbstrMsgGenerator in dbg/DebugContext.h),
	/// so GetFullName must stay inside that ceiling: it reads the token registry -- which is
	/// cheap and allowed -- and takes nothing outer to it. An override may not evaluate a
	/// property, prepare data, or reach a storage or tile lock. Both bodies declare it (#1227).
	[[nodiscard]] RTC_CALL virtual auto GetFullName() const -> SharedStr DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	[[nodiscard]] RTC_CALL virtual auto GetFullCfgName() const -> SharedStr DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v);

	/// Throw a contextualized item error with a pre-wrapped WeakStr message.
	[[noreturn]] RTC_CALL void throwItemError(WeakStr msgStr) const;

	/// Throw a contextualized item error from a raw CharPtr by building a SharedStr.
	/// Uses MG_DEBUG_ALLOCATOR_SRC tag for debug allocation tracking.
	/// TODO: Prefer strongly-typed string view equivalents
	[[noreturn]] RTC_CALL void throwItemError(CharPtr msg) const;

	/// Format string error helper; forwards args to ::throwItemErrorF.
	/// Safety: Ensure msg is a safe format string and args match placeholders.
	/// TODO: Consider type-safe formatting wrappers or compile-time format checks.
	template<typename ...Args>
	[[noreturn]] void throwItemErrorF(CharPtr msg, Args&&... args) const {
		::throwItemErrorF(this, msg, std::forward<Args>(args)...);
	}
	DECL_ABSTR(RTC_CALL, Class)
};

//**********  dynamic creation                           **********

//	CreateFunc can be used in definition of Obj::GetStaticClass()

template <typename CLS> inline
Object* CreateFunc() { return new CLS(); }

//**********  dynamic creation via std::shared_ptr (migration (a)) **********
// SharedCreateFunc<CLS>() builds a std::shared_ptr-managed instance for factory-only types. It reuses the
// existing CreateFunc<CLS> friend (which can reach CLS's PRIVATE/PROTECTED ctor via `new CLS`), then wraps
// the raw pointer in a std::shared_ptr. The pointer is handed to shared_ptr as CLS* (not Object*) so that,
// when CLS derives std::enable_shared_from_this, the control block's weak-this gets wired -- a separate
// control-block allocation (vs make_shared) is the deliberate price for reaching non-public ctors without
// per-subclass friendship/churn.
#include <memory>

template <typename CLS> inline
std::shared_ptr<Object> SharedCreateFunc()
{
	CLS* p = static_cast<CLS*>(CreateFunc<CLS>()); // CreateFunc returns Object*; recover the concrete type
	return std::shared_ptr<CLS>(p);                // wires enable_shared_from_this for CLS's base; upcasts to shared_ptr<Object>
}


#endif // __RTC_MCI_OBJECT_H
