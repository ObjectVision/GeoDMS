// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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

template <class PtrBase, class T = LispRef> struct LispListPtrWrap;
template <class T> using LispListPtr = LispListPtrWrap<WeakPtr<const LispObj>, T>;


template <class PtrBase, class T>
struct LispListPtrWrap : LispPtrWrap<PtrBase>
{
	using base_type = LispPtrWrap<PtrBase>;
//	using pointer = LispPtr::pointer;
    using list_type = LispList<T>;
	using elem_ptr = typename T::ptr_type;
	using ptr_type = LispListPtrWrap<WeakPtr<const LispObj>, T>;

	LispListPtrWrap() {}
	LispListPtrWrap(LispPtr src) : base_type(src) 
	{ 
		if (!Check()) 
			LispError("Invalid cast to LispList", src); 
	}
//	LispListPtrWrap(PtrBase ht, existing_obj) : base_type(ht) {}

	template <class RhsPtr>
	LispListPtrWrap(const LispListPtrWrap<RhsPtr, T>& rhs) : LispPtrWrap<PtrBase>(rhs.get()) {}

    list_type Delete(elem_ptr e) const;
    list_type Concat(ptr_type l) const;

	elem_ptr Head() const { auto tmp = this->Left();  return reinterpret_cast<elem_ptr&>(tmp);  }
	ptr_type Tail() const { auto tmp = this->Right(); return reinterpret_cast<ptr_type&>(tmp); }

    elem_ptr First () const {return Head(); };
    elem_ptr Second() const {return Tail().First ();};
    elem_ptr Third () const {return Tail().Second();};
    elem_ptr Fourth() const {return Tail().Third ();};
    elem_ptr Fifth () const {return Tail().Fourth();};

    bool IsEmpty() const { return this->EndP(); }

    bool   LengthIs(UInt32 n) const;
    UInt32 Length()  const;

    bool Check() const
	{
		for (ptr_type cursor = *this; !cursor.EndP(); cursor = cursor.Tail())
		{
			if (!cursor.IsList())
				return false;
			if (!cursor.Head().Check())
				return false;
		}
		return true;
	}

//	auto AsLispPtr() const -> LispPtr { return LispPtr(this->get_ptr()); }
};

template <class T>
struct LispList : LispListPtrWrap<SharedPtr<const LispObj>, T>
{
	using base_type = LispListPtrWrap<SharedPtr<const LispObj>, T>;
	using elem_ptr = typename T::ptr_type;
	using ptr_type = LispListPtr<T>;

    LispList()
	{}

    LispList(elem_ptr head, ptr_type tail) 
		: base_type(LispRef(head.AsLispPtr(), tail.AsLispPtr()))
	{}

	LispList(ptr_type listPtr) 
		: base_type(listPtr) 
	{}

	LispList(LispPtr src)
		: base_type(src) // invoke Check
	{}

	operator ptr_type() const { return ptr_type(this->get_ptr()); }
};

using LispRefListPtr = LispListPtrWrap<WeakPtr<const LispObj>,LispRef>;
using LispRefList = LispList<LispRef>;

/*********** function implementations ********************/


// the following functions are easy RECURSIVE

template <class PtrBase, class T>
bool LispListPtrWrap<PtrBase, T>::LengthIs(UInt32 n) const
{
	if (IsEmpty())
		return (n == 0);
	if (n==0) 
		return false;
	return Tail().LengthIs(n-1);
}

template <class PtrBase, class T>
UInt32 LispListPtrWrap<PtrBase, T>::Length() const
{
	UInt32 argCount = 0;
	for (auto cursor = *this; !cursor.EndP(); cursor = cursor.Tail())
		++argCount;
	return argCount;
}

// the following functions are hard RECURSIVE

template <class PtrBase, class T>
auto LispListPtrWrap<PtrBase, T>::Delete(elem_ptr e) const -> LispList<T>
{
	if (IsEmpty())
		return LispList<T>();
	elem_ptr first = First();
	if (first == e)
		return Tail();
	return LispList(first, Tail().Delete(e));
}

template <class PtrBase, class T>
auto LispListPtrWrap<PtrBase, T>::Concat(ptr_type b) const -> LispList<T>
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
FormattedOutStream& operator <<(FormattedOutStream& output, const LispListPtr<T>& src)
{
	auto list = src;
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
