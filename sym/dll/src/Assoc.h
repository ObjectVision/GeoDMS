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
#ifndef __SYM_ASSOC_H
#define __SYM_ASSOC_H

#include "LispRef.h"

/**************** Assoc with type security *****************/

struct AssocPtr : LispPtr
{
	AssocPtr() {}
	AssocPtr(LispPtr a): LispPtr(a)
	{
	  	dms_assert(Check());
	}

	LispPtr Key()   const { return Left(); }
	LispPtr Val()   const { return Right(); }
	bool IsFailed() const { return EndP(); }

	SYM_CALL static AssocPtr failed();

	bool Check() const { return IsList(); }

//	AssocPtr(AssocPtr&& rhs) : LispPtr(std::move(rhs)) {}
//	AssocPtr(const AssocPtr& rhs) : AssocPtr(rhs.get_ptr()) {}

protected: friend struct Assoc;
	explicit AssocPtr(LispObj* obj) : LispPtr(obj) {}
};

struct Assoc : SharedPtrWrap<AssocPtr>
{
	typedef AssocPtr ptr_type;

//	Assoc() {}					//	empty-assoc = Fail
	Assoc(LispPtr key, LispPtr val): SharedPtrWrap(LispRef(key, val).get_ptr()) {}

	Assoc(ptr_type listPtr) : SharedPtrWrap(listPtr) {}
	operator ptr_type() const { return ptr_type(get_ptr()); }
};

/**************** Assoc lists with type security *****************/

#include "LispList.h"

//struct AssocList;

struct AssocListPtr : LispListPtr<Assoc>
{
	AssocListPtr(::LispPtr source = ::LispPtr()) : LispListPtr(source) {} // base class does check

	// override interface of LispList with covariant return types
	bool IsFailed() const { return (!IsEmpty()) && First().IsFailed(); }

	AssocListPtr Tail() const
	{
		return reinterpret_cast<AssocListPtr&>(LispListPtr<Assoc>::Tail());
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

	static AssocListPtr failed();
	static AssocListPtr empty();

protected: friend struct AssocList;
	explicit AssocListPtr(LispObj* obj) : LispListPtr(obj) {}
};


struct AssocList : SharedPtrWrap<AssocListPtr>
{
	typedef AssocListPtr ptr_type;

	AssocList() {};	//	creates an empty AssocList
	AssocList(ptr_type list) : SharedPtrWrap(list) {}
	AssocList(LispPtr  list) : SharedPtrWrap(ptr_type(list)) {}  // invoke Check

	AssocList(AssocPtr a, AssocListPtr l):	SharedPtrWrap(LispRef(a, l))
	{
		dms_assert(!l.IsFailed());
	}

	operator ptr_type() const { return ptr_type(get_ptr()); }
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
	return AssocListPtr::failed();
}

inline AssocList InsertRecursive(AssocListPtr self, AssocPtr a)
{
	::LispPtr foundVal = self.LookupRecursive(a.Val());
	AssocPtr foundAssoc = self.FindByKey(a.Key());
	if (foundAssoc.IsFailed())
		return Add(self, Assoc(a.Key(),foundVal));
	if (self.LookupRecursive(foundAssoc.Val()) == foundVal)
		return self;
	return AssocListPtr::failed();
}

inline AssocList ReverseAssoc(AssocListPtr self)
{
	if (self.IsEmpty())
		return AssocListPtr::empty();
	AssocPtr top = self.First();
	return AssocList(Assoc(top.Val(),top.Key()), ReverseAssoc(self.Tail()));
}

/**************** operators      *****************/

SYM_CALL FormattedOutStream& operator <<(FormattedOutStream& output, AssocPtr a);

#endif // __SYM_ASSOC_H
