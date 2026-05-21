// Copyright (C) 1998-2026 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __SYM_ASSOC_H
#define __SYM_ASSOC_H

#include "LispRef.h"

#include <vector>

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
			LispError("Invalid cast to Assoc", this->AsLispPtr());
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

	static auto failed() -> AssocPtr;

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
		auto tmp = base_type::Tail();
		return reinterpret_cast<AssocListPtrWrap&>(tmp);
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
		return Assoc::failed();
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

	// Iterative result-stitching walk over the Lisp expression tree. Each frame
	// represents an in-progress node: phase 0 enters (assoc-lookup, list-test
	// or leaf-return), phase 1 records the left child's result and dispatches
	// the right, phase 2 stitches the LispRef(left, right) and pops. Keeps
	// stack usage O(1) regardless of expression depth -- iterated-calc configs
	// can produce thousands of nested levels here, the recursive form was the
	// load-bearing source of stack growth in the rewrite/substitute path.
	LispRef ApplyOnce(::LispPtr expr) const
	{
		struct frame_t
		{
			::LispPtr node;
			LispRef   left;
			uint8_t   phase; // 0=enter, 1=left done, 2=right done (stitch)
		};
		std::vector<frame_t> stack;
		stack.push_back({expr, {}, 0});
		LispRef result;

		while (!stack.empty())
		{
			auto& f = stack.back();
			if (f.phase == 0)
			{
				AssocPtr found = FindByKey(f.node);
				if (!found.IsFailed())
				{
					result = found.Val();
					stack.pop_back();
					continue;
				}
				if (!f.node.IsRealList())
				{
					result = f.node;
					stack.pop_back();
					continue;
				}
				f.phase = 1;
				stack.push_back({f.node.Left(), {}, 0});
			}
			else if (f.phase == 1)
			{
				f.left = std::move(result);
				f.phase = 2;
				stack.push_back({f.node.Right(), {}, 0});
			}
			else // phase == 2
			{
				result = LispRef(f.left, result);
				stack.pop_back();
			}
		}
		return result;
	}

	// Same iterative walk as ApplyOnce, with the substitution-chain behavior:
	// an assoc hit feeds Val() back through the same processing instead of
	// returning it as-is. Implemented by replacing f.node with found.Val()
	// and re-entering phase 0 on the next iteration.
	LispRef ApplyMany(::LispPtr expr) const
	{
		struct frame_t
		{
			::LispPtr node;
			LispRef   left;
			uint8_t   phase; // 0=enter, 1=left done, 2=right done (stitch)
		};
		std::vector<frame_t> stack;
		stack.push_back({expr, {}, 0});
		LispRef result;

		while (!stack.empty())
		{
			auto& f = stack.back();
			if (f.phase == 0)
			{
				AssocPtr found = FindByKey(f.node);
				if (!found.IsFailed())
				{
					f.node = found.Val();
					continue; // re-enter phase 0 with the chased value
				}
				if (!f.node.IsRealList())
				{
					result = f.node;
					stack.pop_back();
					continue;
				}
				f.phase = 1;
				stack.push_back({f.node.Left(), {}, 0});
			}
			else if (f.phase == 1)
			{
				f.left = std::move(result);
				f.phase = 2;
				stack.push_back({f.node.Right(), {}, 0});
			}
			else // phase == 2
			{
				result = LispRef(f.left, result);
				stack.pop_back();
			}
		}
		return result;
	}

//protected: friend struct AssocList;
//	explicit AssocListPtrWrap(BasePtr obj, existing_obj) : base_type(std::move(obj), existing_obj{}) {}
};

struct AssocList : AssocListPtrWrap<SharedPtr<const LispObj>>
{
	using base_type = AssocListPtrWrap<SharedPtr<const LispObj>>;
	using ptr_type = AssocListPtr;

	AssocList()  noexcept {};	//	creates an empty AssocList
	AssocList(AssocList&& rhs) noexcept : base_type(std::move(rhs)) {}
	AssocList(const AssocList& rhs)  noexcept : base_type(rhs) {}
	AssocList(ptr_type list)  noexcept : base_type(list) {}
	AssocList(LispPtr  list) : base_type(ptr_type(list)) {}  // invoke Check

	AssocList(AssocPtr a, ptr_type l) : base_type(LispRef(a.get(), l.get()))
	{
		assert(!l.IsFailed());
	}

	void operator =(AssocList&& rhs)  noexcept { this->swap(rhs); }

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
