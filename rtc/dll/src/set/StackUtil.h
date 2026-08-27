// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if !defined(__RTC_SET_STACK_UTIL_H)
#define __RTC_SET_STACK_UTIL_H

#include "dbg/Diagnostics.h"
#include "vt/BaseBounds.h"
#include <vector>

///////////////////////////////////////////////////////////////////////////////
//
//  stack with quick pops
//
///////////////////////////////////////////////////////////////////////////////

template<typename T>
struct quick_replace_stack : std::vector<T>
{
	using std::vector<T>::size;
	using std::vector<T>::end;
	using std::vector<T>::push_back;
	using std::vector<T>::pop_back;

	// no ctors since we always start with an empty stack

	// like back() the following functions give access to last elements of stack; back1() is equivalent with back()
	const T& back1() const { dms_assert(size() > 0); return end()[-1]; }
	const T& back2() const { dms_assert(size() > 1); return end()[-2]; }
	const T& back3() const { dms_assert(size() > 2); return end()[-3]; }
	const T& back4() const { dms_assert(size() > 3); return end()[-4]; }

	T& back1() { dms_assert(size() > 0); return end()[-1]; }
	T& back2() { dms_assert(size() > 1); return end()[-2]; }
	T& back3() { dms_assert(size() > 2); return end()[-3]; }
	T& back4() { dms_assert(size() > 3); return end()[-4]; }

	// alternative names provided for slisp-list like access
	const T& First () const { return back1(); }
	const T& Second() const { return back2(); }
	const T& Third () const { return back3(); }
	const T& Fourth() const { return back4(); }

	// push_back(const T& newVal) is already defined by base class, but repeated here for clarity
	template <typename Other> void repl_back0(Other&& newVal) { push_back(std::forward<Other>(newVal)); }
	template <typename Other> void repl_back1(Other&& newVal) { back1() = std::forward<Other>(newVal); }
	template <typename Other> void repl_back2(Other&& newVal) { back2() = std::forward<Other>(newVal); pop_back(); }
	template <typename Other> void repl_back3(Other&& newVal) { back3() = std::forward<Other>(newVal); pop_back(); pop_back(); }
	template <typename Other> void repl_back4(Other&& newVal) { back4() = std::forward<Other>(newVal); pop_back(); pop_back(); pop_back(); }
};


#endif // __RTC_SET_STACK_UTIL_H
