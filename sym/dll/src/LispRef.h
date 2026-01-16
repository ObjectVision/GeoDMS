// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SYM_LISPREF_H
#define __SYM_LISPREF_H

#include "SymBase.h"

#include "ptr/SharedPtr.h"
#include "ptr/WeakPtr.h"
#include "ptr/PersistentSharedObj.h"
#include "ser/FormattedStream.h"

/****************** forward decls *******************/

struct LispRef;
class SymbObj;
class NumbObj;
class ListObj;
using ChroID = UInt32;
struct FormattedOutStream;

/****************** LispComponent *******************/

struct LispComponent
{
	LispComponent();
	~LispComponent();
};

/****************** LispRef class definition *******************/

// Reference: Symbolic Computing with LISP, R.D. Cameron & A.H. Dixon., 1992

struct LispRef;
struct LispObj;

template <typename BasePtr> struct LispPtrWrap;
using LispPtr = LispPtrWrap< WeakPtr<const LispObj> >;

template <typename BasePtr>
struct LispPtrWrap: BasePtr
{
	using BasePtr::BasePtr;

	auto get() const noexcept { return this->BasePtr::get(); }

	// delegate requests to the LispObj
	bool     IsNumb() const { return get() ? get()->IsNumb() : false; }
	Number   GetNumbVal() const { return get() ? get()->GetNumbVal() : Number(0.0); }

	bool     IsUI64() const { return get() ? get()->IsUI64() : false; }
	UInt64   GetUI64Val() const { return get() ? get()->GetUI64Val() : 0; }

	bool     IsSymb() const { return get() ? get()->IsSymb() : false; }
	TokenStr GetSymbStr() const { return get() ? get()->GetSymbStr() : TokenID::GetEmptyTokenStr(); }
	TokenStr GetSymbEnd() const { return get() ? get()->GetSymbEnd() : TokenID::GetEmptyTokenStr(); }
	TokenID  GetSymbID() const { return get() ? get()->GetSymbID() : TokenID::GetEmptyID(); }

	bool     IsVar() const { return get() ? get()->IsVar() : false; }
	ChroID   GetChroID() const { return get() ? get()->GetChroID() : ChroID(); }

	bool     IsStrn() const { return get() ? get()->IsStrn() : false; }
	CharPtr  GetStrnBeg() const { return get() ? get()->GetStrnBeg() : TokenID::GetEmptyStr(); }
	CharPtr  GetStrnEnd() const { return get() ? get()->GetStrnEnd() : TokenID::GetEmptyStr(); }

	bool     IsList() const { return get() ? get()->IsList() : true; }
	bool     EndP() const { return get() == nullptr; }
	bool     IsRealList() const { return get() ? get()->IsList() : false; }
	LispPtr  Left() const { MG_CHECK(get()); return get()->Left(); }
	LispPtr  Right() const { MG_CHECK(get()); return get()->Right(); }
	
	bool Check() const { return true; }
	auto AsLispPtr() const -> LispPtr { return LispPtr(this->get()); }

//	Crude printing
	void Print(FormattedOutStream& out, UInt32 level) const
	{
		if (get())
			get()->Print(out, level);
		else
			out << "() ";
	}

	void PrintAsFLisp(FormattedOutStream& out, UInt32 level) const
	{
		if (get())
			get()->PrintAsFLisp(out, level);
		else
			out << "() ";
	}

	SYM_CALL static UInt32 MAX_PRINT_LEVEL;

//	compare operators
	template <typename OthPtr> bool operator ==(const LispPtrWrap<OthPtr>& rhs) const { return this->get() == rhs.get(); }
	template <typename OthPtr> bool operator < (const LispPtrWrap<OthPtr>& rhs) const { return this->get() < rhs.get(); }
};

//template <> struct pointer_traits<LispPtr> : pointer_traits_helper<LispObj> {};

struct LispRef : LispPtrWrap<SharedPtr<const LispObj> >
{
	using base_type = LispPtrWrap<SharedPtr<const LispObj> >;

	using ptr_type = LispPtr;

//	constructors
	LispRef() noexcept {};  		// Makes a LispRef to NULL

	SYM_CALL LispRef(LispPtr lrb) noexcept;  // borrowed existing object
	SYM_CALL LispRef(LispObj* lrb) noexcept; // newly created object
	SYM_CALL LispRef(LispPtr lrb, no_zombies nz) noexcept;

	LispRef(const LispRef&) noexcept = default;

	SYM_CALL LispRef(SharedPtr<const LispObj>&& rhs) noexcept;

	LispRef& operator =(const LispRef& rhs) noexcept = default;
	LispRef& operator =(LispRef&& rhs) noexcept = default;

	operator LispPtr() const { return LispPtr(get()); }

	SYM_CALL LispRef(Number v);	                  // Makes a LispRef to NumbObj
	SYM_CALL LispRef(UInt64 u);	                  // Makes a LispRef to U64Obj
	SYM_CALL LispRef(CharPtr s, ChroID c= 0);	  // Makes a LispRef to SymbObj with tokenStr s and Chro c
	SYM_CALL LispRef(TokenID  t, ChroID c= 0);	  // Makes a LispRef to SymbObj with tokenID  t and Chro c
	SYM_CALL LispRef(CharPtr begin, CharPtr end); // Makes a LispRef to StrnObj
	SYM_CALL LispRef(LispPtr head, LispPtr tail); // Makes a LispRef to ListObj

//	static constants
	static const LispRef s_null;
};

//template <typename BasePtr> bool LispPtrWrap<BasePtr>::operator ==(const LispRef& rhs) const { return this->get() == rhs.get(); }
//template <typename BasePtr> bool LispPtrWrap<BasePtr>::operator < (const LispRef& rhs) const { return this->get() < rhs.get(); }

/****************** class LispObj                     *******************/

struct LispObj : public SharedObj
{
private: 
	using base_type = SharedObj;
public:
	// ctor, dtor
	         LispObj();
	virtual ~LispObj();

	// query members
	virtual bool     IsNumb()     const;
	virtual Number   GetNumbVal() const;

	virtual bool     IsUI64()     const;
	virtual UInt64   GetUI64Val() const;

	virtual bool     IsSymb()     const;
	virtual TokenStr GetSymbStr() const;
	virtual TokenStr GetSymbEnd() const;
	virtual TokenID  GetSymbID()  const;
	virtual ChroID   GetChroID()  const;

	virtual bool     IsVar()      const;

	virtual bool     IsStrn()     const;
	virtual CharPtr  GetStrnBeg() const;
	virtual CharPtr  GetStrnEnd() const;

	virtual bool     IsList()     const;
	virtual LispPtr  Left() const;
	virtual LispPtr  Right() const;

	virtual void Print(FormattedOutStream&, UInt32 level) const=0;
	virtual void PrintAsFLisp(FormattedOutStream&, UInt32 level) const;

	DECL_ABSTR(SYM_CALL, Class)
};

/****************** operators for LispPtr       *******************/

// delegate requests to the LispObj


/****************** operators for LispRef       *******************/

inline FormattedOutStream& operator <<(FormattedOutStream& output, LispPtr R)
{
	R.Print(output, 0);
	return output;
}

SYM_CALL SharedStr AsFLispSharedStr(LispPtr lispRef, FormattingFlags ff);
SYM_CALL void LispError(CharPtr msg, LispPtr ref);

#endif // __SYM_LISPREF_H
