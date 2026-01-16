// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SYM_ASSOC_H
#define __SYM_ASSOC_H

#include "LispRef.h"

/**************** Assoc with type security *****************/

template <class BasePtr>
struct AssocPtrWrap : LispPtrWrap<BasePtr>
{
	using base_type = LispPtrWrap<BasePtr>;

	AssocPtrWrap() {}
	AssocPtrWrap(LispPtr src)
		: base_type(std::move(src))
	{
		if (!Check())
			LispError("Invalid cast to Assoc", src);
	}
	template <class RhsPtr>
	AssocPtrWrap(const AssocPtrWrap<RhsPtr>& rhs)
		: base_type(rhs.get(), existing_obj())
	{}

	template <class RhsPtr>
	AssocPtrWrap(AssocPtrWrap<RhsPtr>&& rhs)
		: base_type(std::move(rhs))
	{
	}

	LispPtr Key()   const { return this->Left(); }
	LispPtr Val()   const { return this->Right(); }
	bool IsFailed() const { return this->EndP(); }

	static AssocPtrWrap failed() { return AssocPtrWrap(); }

	bool Check() const
	{ 
		return this->IsList(); 
	}

//	auto AsLispPtr() const -> LispPtr { return LispPtr(this->get_ptr()); }

//	AssocPtr(AssocPtr&& rhs) : LispPtr(std::move(rhs)) {}
//	AssocPtr(const AssocPtr& rhs) : AssocPtr(rhs.get_ptr()) {}

//protected: friend struct Assoc;
//	explicit AssocPtrWrap(BasePtr obj, existing_obj) : base_type(std::move(obj)) {}
};


using AssocPtr = AssocPtrWrap<WeakPtr<const LispObj>>;

struct Assoc : AssocPtrWrap<SharedPtr<const LispObj>>
{
	using base_type = AssocPtrWrap<SharedPtr<const LispObj>>;
	using ptr_type = AssocPtr;

	Assoc() {}					//	empty-assoc = Fail
	Assoc(LispPtr key, LispPtr val) : base_type(LispRef(key, val)) {}

	template<typename BasePtr>
	Assoc(AssocPtrWrap<BasePtr> rhs) : base_type(std::move(rhs)) {}

//	Assoc(ptr_type listPtr) : base_type(listPtr) {}
	operator ptr_type() const { return ptr_type(get()); }
};

/**************** Assoc lists with type security *****************/

#include "LispList.h"

template <class BasePtr> struct AssocListPtrWrap;
using AssocListPtr = AssocListPtrWrap<WeakPtr<const LispObj>>;

//struct AssocList;
template <class BasePtr>
struct AssocListPtrWrap : LispListPtrWrap<BasePtr, Assoc>
{
	using base_type = LispListPtrWrap<BasePtr, Assoc>;
	using base_type::base_type;
//	AssocListPtrWrap(LispPtr source = LispPtr()) : base_type(source) {} // base class does check

	// override interface of LispList with covariant return types
	bool IsFailed() const { return (!this->IsEmpty()) && this->First().IsFailed(); }

	AssocListPtrWrap Tail() const
	{
		return reinterpret_cast<AssocListPtrWrap&>(base_type::Tail());
	}

	// new methods
	AssocPtr FindByKey(::LispPtr key) const
	{
		AssocListPtr cursor = *this;
		while (!cursor.IsEmpty())
		{
			AssocPtr a=cursor.First();
			if (a.Key()==key)
				return a;
			cursor = cursor.Tail();
		}
		return AssocPtr::failed();
	}

	::LispPtr Lookup(::LispPtr x) const
	{
		AssocPtr found = FindByKey(x);
		if (found.IsFailed())
			return x;
		return found.Val();
	}

	::LispPtr LookupRecursive(::LispPtr x) const
	{
		while (true)
		{
			AssocPtr found = FindByKey(x);
			if (found.IsFailed())
				return x;
			x = found.Val();
		}
	}

	LispRef ApplyOnce(::LispPtr expr) const
	{
		AssocPtr found = FindByKey(expr);
		if (!found.IsFailed())
			return found.Val();
		if (!expr.IsRealList())
			return expr;
		return LispRef(ApplyOnce(expr.Left()), ApplyOnce(expr.Right()));
	}

	LispRef ApplyMany(::LispPtr expr) const
	{
		AssocPtr found = FindByKey(expr);
		if (!found.IsFailed())
			return ApplyMany(found.Val());
		if (!expr.IsRealList())
			return expr;
		return LispRef(ApplyMany(expr.Left()), ApplyMany(expr.Right()));
	}

//protected: friend struct AssocList;
//	explicit AssocListPtrWrap(BasePtr obj, existing_obj) : base_type(std::move(obj), existing_obj{}) {}
};

struct AssocList : AssocListPtrWrap<SharedPtr<const LispObj>>
{
	using base_type = AssocListPtrWrap<SharedPtr<const LispObj>>;
	using ptr_type = AssocListPtr;

	AssocList() {};	//	creates an empty AssocList
	AssocList(AssocList&& rhs) : base_type(std::move(rhs)) {}
//	AssocList(AssocListPtr&& rhs) : base_type(std::move(rhs)) {}
	AssocList(const AssocList& rhs) : base_type(rhs) {}
//	AssocList(const AssocListPtr& rhs) : base_type(rhs) {}
	AssocList(ptr_type list) : base_type(list) {}
	AssocList(LispPtr  list) : base_type(ptr_type(list)) {}  // invoke Check

	AssocList(AssocPtr a, ptr_type l) : base_type(LispRef(a.get(), l.get()))
	{
		assert(!l.IsFailed());
	}

	void operator =(AssocList&& rhs) { this->swap(rhs); }

	template<typename BasePtr>
	void operator =(const AssocListPtrWrap<BasePtr>& rhs) { SharedPtr<const LispObj>::operator=(rhs); }

	static auto empty () -> AssocListPtr;
	static auto failed() -> AssocListPtr;

	operator ptr_type() const { return ptr_type(get()); }
};

/**************** constructing functions *****************/

inline AssocList Add(AssocListPtr self, AssocPtr a)
{
	return AssocList(a, self); 
}

inline AssocList Insert(AssocListPtr self, AssocPtr a)
{
	AssocPtr found = self.FindByKey(a.Key());
	if (found.IsFailed())
		return Add(self, a);
	if (found.Val()==a.Val())
		return self;
	return AssocList::failed();
}

inline AssocList InsertRecursive(AssocListPtr self, AssocPtr a)
{
	::LispPtr foundVal = self.LookupRecursive(a.Val());
	AssocPtr foundAssoc = self.FindByKey(a.Key());
	if (foundAssoc.IsFailed())
		return Add(self, Assoc(a.Key(),foundVal));
	if (self.LookupRecursive(foundAssoc.Val()) == foundVal)
		return self;
	return AssocList::failed();
}

inline AssocList ReverseAssoc(AssocListPtr self)
{
	if (self.IsEmpty())
		return AssocList::empty();
	AssocPtr top = self.First();
	return AssocList(Assoc(top.Val(),top.Key()), ReverseAssoc(self.Tail()));
}

/**************** operators      *****************/

SYM_CALL FormattedOutStream& operator <<(FormattedOutStream& output, AssocPtr a);

#endif // __SYM_ASSOC_H
