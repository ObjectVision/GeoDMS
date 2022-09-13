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
#ifndef __SYM_LISPLIST_H
#define __SYM_LISPLIST_H

#include "LispRef.h"

/****************** LispList class definition *******************/

// List = LispList<LispRef> is afgeleid van LispRef 
// List x geeft de garantie dat x.IsRealList() == true
// wat impliceert dat x.IsNumb() == x.false en IsSymb() == false.
// Ieder element van LispList<T> is gegarandeerd van type T.
// Dus LispList<T> y garandeert dat indien ! y.EndP()
//	 y.Head() ( == y.Left() ) heeft de garanties van type T en 
//   y.Tail() ( == y.Right()) heeft de garanties van type LispList<T>

template <class T = LispRef> struct LispList;

template <class T = LispRef>
struct LispListPtr : LispPtr
{
	typedef LispPtr::pointer     pointer;
    typedef LispList<T>          list_type;
	typedef typename T::ptr_type elem_ptr;
    typedef LispListPtr<T>       ptr_type;

	LispListPtr() {}
	LispListPtr(LispPtr src) : LispPtr(src) { if (!Check()) LispError("Invalid cast to LispList", src); }

    list_type Delete(elem_ptr e) const;
    list_type Concat(ptr_type l) const;

    elem_ptr Head() const { return reinterpret_cast<elem_ptr&>(Left());  }
    ptr_type Tail() const { return reinterpret_cast<ptr_type&>(Right()); }

    elem_ptr First () const {return Head(); };
    elem_ptr Second() const {return Tail().First ();};
    elem_ptr Third () const {return Tail().Second();};
    elem_ptr Fourth() const {return Tail().Third ();};
    elem_ptr Fifth () const {return Tail().Fourth();};

    bool IsEmpty() const { return EndP(); }

    bool   LengthIs(UInt32 n) const;
    UInt32 Length()  const;

    bool Check() const;

protected:
	explicit LispListPtr(LispObj* obj) : LispPtr(obj) {}
};

template <class T>
struct LispList : SharedPtrWrap<LispListPtr<T> >
{
	using base_type = SharedPtrWrap<LispListPtr<T> >;
	typedef typename T::ptr_type elem_ptr;
	typedef LispListPtr<T>       ptr_type;

    LispList()
	{}

    LispList(elem_ptr head, ptr_type tail) 
		: base_type(LispRef(head, tail).get_ptr()) 
	{}

	LispList(ptr_type listPtr) 
		: base_type(listPtr) 
	{}

	LispList(LispPtr  lispPtr) 
		: base_type(ptr_type(lispPtr) )  // invoke Check
	{} 

	operator ptr_type() const { return ptr_type(this->get_ptr()); }

};

typedef LispListPtr<LispRef> LispRefListPtr;
typedef LispList<LispRef>    LispRefList;

/*********** function implementations ********************/

template <class T> bool
LispListPtr<T>::Check() const
{
	for (LispListPtr cursor = *this; !cursor.EndP(); cursor = cursor.Tail())
	{
		if (!cursor.IsList())
			return false;
		if (!cursor.Head().Check())
			return false;
	}
	return true;
}

// the following functions are easy RECURSIVE

template <class T>
bool LispListPtr<T>::LengthIs(UInt32 n) const
{
	if (IsEmpty()) return (n == 0);
	if (n==0) return false;
	return Tail().LengthIs(n-1);
}

template <class T>
UInt32 LispListPtr<T>::Length() const
{
	UInt32 argCount = 0;
	for (LispListPtr cursor = *this; !cursor.EndP(); cursor = cursor.Tail())
		++argCount;
	return argCount;
}

// the following functions are hard RECURSIVE

template <class T> LispList<T>
LispListPtr<T>::Delete(elem_ptr e) const
{
	if (IsEmpty())
		return LispList<T>();
	elem_ptr first = First();
	if (first == e)
		return Tail();
	return LispList(first, Tail().Delete(e));
}

template <class T> LispList<T>
LispListPtr<T>::Concat(ptr_type b) const
{
	if (IsEmpty()) return b;
	return LispList<T>(Head(), Tail().Concat(b));
}

/************* Named constructors *************************/

//* REMOVE TODO G8.5

template <class T> inline LispList<T>
List1(typename T::ptr_type e1)
{
	return LispList<T>(e1,LispList<T>());
}

template <class T> inline LispList<T>
List2(typename T::ptr_type e1, typename T::ptr_type e2)
{
	return LispList<T>(e1, List1<T>(e2));
}

template <class T> inline LispList<T>
List3(typename T::ptr_type e1, typename T::ptr_type e2, typename T::ptr_type e3)
{
	return LispList<T>(e1, List2<T>(e2,e3) );
}

template <class T> inline LispList<T>
List4(typename T::ptr_type e1, typename T::ptr_type e2, typename T::ptr_type e3, typename T::ptr_type e4)
{
	return LispList<T>(e1, List3<T>(e2,e3,e4) );
}

template <class T> inline LispList<T>
List5(typename T::ptr_type e1, typename T::ptr_type e2, typename T::ptr_type e3, typename T::ptr_type e4, typename T::ptr_type e5)
{
	return LispList<T>(e1, List4<T>(e2,e3,e4,e5) );
}

template <class T> inline LispList<T>
List6(typename T::ptr_type e1, typename T::ptr_type e2, typename T::ptr_type e3, typename T::ptr_type e4, typename T::ptr_type e5, typename T::ptr_type e6)
{
	return LispList<T>(e1, List5<T>(e2,e3,e4,e5, e6) );
}

//*/

/************* Generic Lists*************************/

template <class T = LispRef> inline
auto List()
{
	return LispList<T>();
}

template <class T = LispRef, typename... Args>
auto List(typename T::ptr_type e1, Args&&... args)
{
	return LispList<T>(e1, List<T>(std::forward<Args>(args)...));
}

SYM_CALL bool IsExpr(LispPtr e);
SYM_CALL bool IsExprList(LispPtr e);

template <typename... Args>
auto ExprList(TokenID t, Args&&... args)
{
//	dms_assert(IsExpr(args) && ...);

	return List(LispRef(t), std::forward<Args>(args)...);
}

/**************** Operators ************************/
#include "ser/FormattedStream.h"

template <class T>
FormattedOutStream& operator <<(FormattedOutStream& output, LispListPtr<T> list)
{
	output << "{";
	while (!list.IsEmpty())
	{
		output << list.First();
		list = list.Tail();
		if (!list.IsEmpty())
			output << " ";
	}
	output << "}";
	return output;
}

#endif // __SYM_LISPLIST_H
