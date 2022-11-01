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
#ifndef __SYM_LISPREF_H
#define __SYM_LISPREF_H

#include "SymBase.h"

#include "ptr/SharedPtr.h"
#include "ptr/WeakPtr.h"
#include "ptr/PersistentSharedObj.h"

/****************** forward decls *******************/

struct LispRef;
class SymbObj;
class NumbObj;
class ListObj;
typedef UInt32 ChroID;

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

struct LispPtr: WeakPtr<LispObj>
{
	LispPtr() {}
//	LispPtr(const LispRef& src);

// delegate requests to the LispObj
	inline bool     IsNumb    () const;
	inline Number   GetNumbVal() const;

	inline bool     IsUI64() const;
	inline UInt64   GetUI64Val() const;

	inline bool     IsSymb    () const;
	inline TokenStr GetSymbStr() const;
	inline TokenStr GetSymbEnd() const;
	inline TokenID  GetSymbID () const;

	inline bool     IsVar     () const;
	inline ChroID   GetChroID () const;

	inline bool     IsStrn    () const;
	inline CharPtr  GetStrnBeg() const;
	inline CharPtr  GetStrnEnd() const;

	inline bool     IsList    () const;
	inline bool     EndP      () const;
	inline bool     IsRealList() const;
	inline LispPtr  Left      () const;
	inline LispPtr  Right     () const;

//	Crude printing
	SYM_CALL void Print(FormattedOutStream& out, UInt32 level) const;
	SYM_CALL void PrintAsFLisp(FormattedOutStream& out, UInt32 level) const;
	SYM_CALL static UInt32 MAX_PRINT_LEVEL;

//	compare operators
	inline bool operator ==(LispPtr rhs) const { return get_ptr() == rhs.get_ptr(); }
	inline bool operator < (LispPtr rhs) const { return get_ptr()  < rhs.get_ptr(); }

	bool Check() const { return true; }

	explicit LispPtr(LispObj* obj) : WeakPtr(obj) {}
};

template <> struct pointer_traits<LispPtr> : pointer_traits_helper<LispObj> {};

struct LispRef : SharedPtrWrap<LispPtr>
{
	typedef LispPtr ptr_type;
  public:
//	constructors
	LispRef() noexcept {};  		// Makes a LispRef to NULL
	LispRef(LispPtr lrb) noexcept : SharedPtrWrap(lrb) {}

	LispRef(const LispRef&) = default;
	LispRef(LispRef&& rhs) noexcept : SharedPtrWrap(std::move(rhs)) {}

	LispRef& operator =(const LispRef& rhs) = default;
	LispRef& operator =(LispRef&& rhs) = default;

	operator ptr_type() const { return ptr_type(get_ptr()); }

	SYM_CALL LispRef(Number v);	                  // Makes a LispRef to NumbObj
	SYM_CALL LispRef(UInt64 u);	                  // Makes a LispRef to U64Obj
	SYM_CALL LispRef(CharPtr s, ChroID c= 0);	  // Makes a LispRef to SymbObj with tokenStr s and Chro c
	SYM_CALL LispRef(TokenID  t, ChroID c= 0);	  // Makes a LispRef to SymbObj with tokenID  t and Chro c
	SYM_CALL LispRef(CharPtr begin, CharPtr end); // Makes a LispRef to StrnObj
	SYM_CALL LispRef(LispPtr head, LispPtr tail); // Makes a LispRef to ListObj

//	static constants
	static const LispRef s_null;
};

/****************** class LispObj                     *******************/

struct LispObj : public PersistentSharedObj
{
private: typedef PersistentSharedObj base_type;
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

inline bool     LispPtr::IsNumb    () const { return get_ptr() ? get_ptr()->IsNumb()    : false; }
inline Number   LispPtr::GetNumbVal() const { return get_ptr() ? get_ptr()->GetNumbVal(): Number(0.0);     }

inline bool     LispPtr::IsUI64    () const { return get_ptr() ? get_ptr()->IsUI64() : false; }
inline UInt64   LispPtr::GetUI64Val() const { return get_ptr() ? get_ptr()->GetUI64Val() : 0; }

inline bool     LispPtr::IsSymb    () const { return get_ptr() ? get_ptr()->IsSymb()    : false; }
inline TokenStr LispPtr::GetSymbStr() const { return get_ptr() ? get_ptr()->GetSymbStr(): TokenID::GetEmptyTokenStr(); }
inline TokenStr LispPtr::GetSymbEnd() const { return get_ptr() ? get_ptr()->GetSymbEnd(): TokenID::GetEmptyTokenStr(); }
inline TokenID  LispPtr::GetSymbID () const { return get_ptr() ? get_ptr()->GetSymbID() : TokenID::GetEmptyID();  }

inline bool     LispPtr::IsVar     () const { return get_ptr() ? get_ptr()->IsVar()     : false;    }
inline ChroID   LispPtr::GetChroID () const { return get_ptr() ? get_ptr()->GetChroID() : ChroID(); }

inline bool     LispPtr::IsStrn    () const { return get_ptr() ? get_ptr()->IsStrn()    : false; }
inline CharPtr  LispPtr::GetStrnBeg() const { return get_ptr() ? get_ptr()->GetStrnBeg(): TokenID::GetEmptyStr(); }
inline CharPtr  LispPtr::GetStrnEnd() const { return get_ptr() ? get_ptr()->GetStrnEnd(): TokenID::GetEmptyStr(); }

inline bool     LispPtr::IsList    () const { return get_ptr() ? get_ptr()->IsList()    : true;  }
inline bool     LispPtr::EndP      () const { return get_ptr() == nullptr; }
inline bool     LispPtr::IsRealList() const { return get_ptr() ? get_ptr()->IsList()    : false; }
inline LispPtr  LispPtr::Left      () const { return get_ptr() ? get_ptr()->Left () : LispPtr(); }
inline LispPtr  LispPtr::Right     () const { return get_ptr() ? get_ptr()->Right() : LispPtr(); }

/****************** operators for LispRef       *******************/

inline FormattedOutStream& operator <<(FormattedOutStream& output, LispPtr R)
{
	R.Print(output, 0);
	return output;
}

SYM_CALL SharedStr AsFLispSharedStr(LispPtr lispRef);
SYM_CALL void LispError(CharPtr msg, LispPtr ref);

#endif // __SYM_LISPREF_H
