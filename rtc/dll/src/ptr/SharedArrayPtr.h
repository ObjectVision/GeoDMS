// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__RTC_PTR_SHAREDARRAYPTR_H)
#define __RTC_PTR_SHAREDARRAYPTR_H

#include "ptr/SharedArray.h"
#include "ptr/SharedPtr.h"
#include "geo/StringBounds.h"

template<typename T>
struct SharedArrayPtr : SharedPtr<SharedArray<T> > 
{
	using base_type = SharedPtr<SharedArray<T> >;
	typedef typename sequence_traits<T>::const_pointer const_iterator;
	typedef typename sequence_traits<T>::pointer       iterator;
	typedef T                                          value_type;

	SharedArrayPtr() {}
	SharedArrayPtr(typename base_type::pointer arrayptr) : base_type(arrayptr) {}

	const_iterator begin() const { return this->has_ptr() ? this->get_ptr()->begin(): const_iterator(); }
	const_iterator end  () const { return this->has_ptr() ? this->get_ptr()->end()  : const_iterator(); }

	iterator       begin() { return this->has_ptr() ? this->get_ptr()->begin(): iterator(); }
	iterator       end  () { return  this->has_ptr() ? this->get_ptr()->end()  : iterator(); }

	T& operator [](UInt32 i) { return (*this)[i]; }

	UInt32 size () const { return this->has_ptr() ? this->get_ptr()->size() : 0;  }
	bool   empty() const { return this->has_ptr() ? this->get_ptr()->empty(): true; }

	bool operator < (const SharedArrayPtr& b) const { return lex_compare(begin(), end(), b.begin(), b.end()); }
	bool operator ==(const SharedArrayPtr& b) const { return  equal     (begin(), end(), b.begin(), b.end()); }
	bool operator !=(const SharedArrayPtr& b) const { return !equal     (begin(), end(), b.begin(), b.end()); }
};

template<typename T>
struct SharedConstArrayPtr : SharedPtr<const SharedArray<T> > 
{
	typedef typename sequence_traits<T>::const_pointer   const_iterator;
	typedef typename sequence_traits<T>::pointer         iterator;
	typedef T                                            value_type;

	SharedConstArrayPtr() {}
	SharedConstArrayPtr(const SharedArray<T>* arrayPtr) : SharedPtr<const SharedArray<T> > (arrayPtr) {}

	const_iterator begin() const { return this->has_ptr() ? this->get_ptr()->begin(): const_iterator(); }
	const_iterator end  () const { return this->has_ptr() ? this->get_ptr()->end()  : const_iterator(); }

	typename param_type<T>::type operator [](UInt32 i) const { return (*this)[i]; }

	UInt32 size () const { return this->has_ptr() ? this->get_ptr()->size(): 0;  }
	bool   empty() const { return this->has_ptr() ? this->get_ptr()->empty() : true; }

	bool operator <(const SharedConstArrayPtr& b) const
	{
		return lex_compare(begin(), end(), b.begin(), b.end());
	}
};


#endif // __RTC_PTR_SHAREDARRAYPTR_H
