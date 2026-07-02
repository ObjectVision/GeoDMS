// Copyright (C) 1998-2026 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_PTR_SHAREDTREEPTR_H)
#define __RTC_PTR_SHAREDTREEPTR_H

#include <memory>

#include "RtcBase.h"
#include "ptr/SharedPtr.h" // for the newly_obj / existing_obj / no_zombies tag types
#include "ptr/WeakPtr.h"   // for the WeakPtr<U> borrow ctor

//  -----------------------------------------------------------------------
//  shared_tree_ptr<T> / weak_tree_ptr<T>
//
//  Migration option B (see doc/development/std-ptr-migration-plan.md): thin wrappers over
//  std::shared_ptr / std::weak_ptr that re-add the in-repo SharedPtr/WeakPtr API surface
//  (get_ptr / is_null / has_ptr + the newly_obj / existing_obj / no_zombies construction tags),
//  so the TreeItem-family ownership migration to std::shared_ptr does not have to churn ~1000
//  call sites in one atomic cascade. Underlying ownership is a real std::shared_ptr control block,
//  giving real std::weak_ptr liveness detection (.lock()) -- the whole point of the migration.
//
//  PRECONDITION: the element type's most-derived enable_shared_from_this base is on TreeItem, i.e.
//  the object is managed by a std::shared_ptr, so existing_obj/no_zombies can recover the control
//  block via shared_from_this()/weak_from_this(). static_pointer_cast bridges the (const/derived)
//  gap between that base shared_ptr and the requested T.
//
//  FOLLOW-UP (migration plan §15.5): factor these wrappers out again toward pure std:: pointers.
//  -----------------------------------------------------------------------

template <class T>
struct shared_tree_ptr : std::shared_ptr<T>
{
	using base_type = std::shared_ptr<T>;
	using element_type = T;

	using base_type::base_type; // inherit std::shared_ptr ctors (nullptr, copy/move, converting from shared_ptr<U>)

	constexpr shared_tree_ptr() noexcept = default;
	shared_tree_ptr(const base_type& rhs) noexcept : base_type(rhs) {}
	shared_tree_ptr(base_type&& rhs) noexcept : base_type(std::move(rhs)) {}

	// NB: no implicit raw-pointer ctor. Raw<->shared conversion is explicit: use a construction tag
	// (existing_obj / newly_obj / no_zombies) to build a shared_tree_ptr, and .get() to obtain a raw pointer.
	// The inherited std::shared_ptr `template<class Y> explicit shared_ptr(Y*)` ctor (pulled in by the
	// `using base_type::base_type;` above) is DELETED here: constructing from a bare raw pointer would build a
	// SEPARATE control block with a delete-deleter, silently double-managing a tree-owned object (a rogue
	// control block that frees the item out from under its real owners at teardown -- a real UAF class). The
	// =delete makes every such accidental site a compile error; classify it with a tag instead.
	template <class Y> shared_tree_ptr(Y*) = delete;

	// Construction-tag ctors mapping the intrusive semantics onto std:: ownership.
	// Templated on the (possibly derived) source pointer so they are an EXACT match for a derived raw
	// pointer (e.g. AbstrUnit* -> shared_tree_ptr<const TreeItem>). A non-templated (T* p, tag) ctor would
	// need a derived->base conversion and so LOSE overload resolution to the inherited std::shared_ptr
	// (Y*, Deleter) ctor (pulled in by `using base_type::base_type;`); libstdc++ then HARD-errors there
	// (the tag is not an invocable deleter) instead of SFINAE-falling-back the way MSVC's STL does. Being
	// templated with a fixed tag parameter, partial ordering prefers these over std's deduced-Deleter ctor.
	template <class Y> requires std::is_convertible_v<Y*, T*>
	shared_tree_ptr(Y* p, newly_obj)   : base_type(static_cast<T*>(p)) {}                  // adopt a freshly created object
	template <class Y> requires std::is_convertible_v<Y*, T*>
	shared_tree_ptr(Y* p, existing_obj): base_type(dup_existing(static_cast<T*>(p))) {}    // borrow an already-owned object
	template <class Y> requires std::is_convertible_v<Y*, T*>
	shared_tree_ptr(Y* p, no_zombies)  : base_type(dup_no_zombies(static_cast<T*>(p))) {}  // safe weak->strong (null if expiring)

	/* REMOVE
	// Transition aids: borrow from an in-repo intrusive SharedPtr<U> / WeakPtr<U> (existence implies owned).
	template <typename U> requires std::is_convertible_v<U*, T*>
	shared_tree_ptr(const SharedPtr<U>& sp) : base_type(dup_existing(static_cast<T*>(sp.get_ptr()))) {}
	template <typename U> requires std::is_convertible_v<U*, T*>
	shared_tree_ptr(const WeakPtr<U>& wp) : base_type(dup_no_zombies(static_cast<T*>(wp.get_ptr()))) {}
	*/

	// in-repo SharedPtr API surface
	T*   get_ptr()  const noexcept { return this->get(); }
	bool is_null()  const noexcept { return this->get() == nullptr; }
	bool has_ptr()  const noexcept { return this->get() != nullptr; }


	// Comparison with a raw element pointer (C++20 synthesizes != and the reversed form). Constrained to
	// genuine related pointer types and deliberately NOT matching std::nullptr_t, so `p == nullptr` routes
	// unambiguously to std::shared_ptr's own nullptr_t overload instead of competing with this one.
	template <typename U>
		requires (std::is_convertible_v<U*, const T*> || std::is_convertible_v<const T*, U*>)
	bool operator==(const U* rhs) const noexcept { return this->get() == static_cast<const T*>(rhs); }

private:
	static base_type dup_existing(T* p)
	{
		if (!p) return {};
		return std::static_pointer_cast<T>(p->shared_from_this()); // asserts/throws bad_weak_ptr if not owned
	}
	static base_type dup_no_zombies(T* p)
	{
		if (!p) return {};
		return std::static_pointer_cast<T>(p->weak_from_this().lock()); // null if the object is expiring
	}
};

template <class T>
struct weak_tree_ptr : std::weak_ptr<T>
{
	using base_type = std::weak_ptr<T>;
	using element_type = T;

	using base_type::base_type; // inherit std::weak_ptr ctors

	constexpr weak_tree_ptr() noexcept = default;
	weak_tree_ptr(const base_type& rhs) noexcept : base_type(rhs) {}
	weak_tree_ptr(const std::shared_ptr<T>& rhs) noexcept : base_type(rhs) {}
	// raw-pointer borrow (graceful: empty weak if the object is not std-owned); matches the old WeakPtr(rawptr).
	weak_tree_ptr(T* p) : base_type(p ? std::static_pointer_cast<T>(p->weak_from_this().lock()) : std::shared_ptr<T>{}) {}

	// Drop-in for the old NON-owning WeakPtr<T>: raw access goes through a momentary lock() so a vanished
	// target reads as null instead of dangling. The target is owned elsewhere (the tree's sub-item link / the
	// producing DC), so the raw result stays valid for the immediate use -- matching the old WeakPtr.get()
	// contract, but liveness-checked. (For a result that must stay alive across an operation, hold lock().)
	T*   get()     const noexcept { return this->lock().get(); }
	T*   get_ptr() const noexcept { return this->lock().get(); }
	bool is_null() const noexcept { return this->expired(); }
	bool has_ptr() const noexcept { return !this->expired(); }
	std::shared_ptr<T> lock_ptr() const noexcept { return this->lock(); }
	explicit operator bool() const noexcept { return !this->expired(); }
	T* operator->() const noexcept { return this->lock().get(); }

	// raw-pointer assignment: store a weak borrow via the std control block (graceful null if not owned);
	// nullptr clears. enable_shared_from_this lives on TreeItem, so recover the base shared_ptr and re-cast.
	weak_tree_ptr& operator=(T* p)
	{
		base_type::operator=(p ? std::static_pointer_cast<T>(p->weak_from_this().lock()) : std::shared_ptr<T>{});
		return *this;
	}
	weak_tree_ptr& operator=(std::nullptr_t) noexcept { this->reset(); return *this; }
	weak_tree_ptr& operator=(const std::shared_ptr<T>& rhs) noexcept { base_type::operator=(rhs); return *this; }

	template <typename U>
		requires (std::is_convertible_v<U*, const T*> || std::is_convertible_v<const T*, U*>)
	bool operator==(const U* rhs) const noexcept { return this->lock().get() == static_cast<const T*>(rhs); }
};

// MakeSharedFromBorrowedObjectPtr for the TreeItem family: borrow an already-owned object as an OWNING
// shared_tree_ptr (recovers the std control block via shared_from_this). This overload is constrained to
// std::enable_shared_from_this-backed types (the TreeItem family), so it is MORE specialized than the
// intrusive SharedPtr<T> overload in SharedPtr.h and is selected for those types; non-family (intrusive)
// types lack shared_from_this and fall back to the SharedPtr<T> version. Same name/signature on purpose.
template <typename T> requires requires(T* p) { p->shared_from_this(); }
auto MakeSharedFromBorrowedObjectPtr(T* ptr) -> shared_tree_ptr<T>
{
	return shared_tree_ptr<T>(ptr, existing_obj{});
}

// pointer_traits so InterestPtr<shared_tree_ptr<T>> / weak_tree_ptr work (mirrors the std::shared_ptr/weak_ptr specializations in RtcBase.h)
template <typename T> struct pointer_traits<shared_tree_ptr<T>> : pointer_traits_helper<T> {
	static T* get_ptr(const shared_tree_ptr<T>& ptr) { return ptr.get(); }
};
template <typename T> struct pointer_traits<weak_tree_ptr<T>> : pointer_traits_helper<T> {
	static T* get_ptr(const weak_tree_ptr<T>& ptr) { return ptr.lock().get(); } // momentary lock; null if expired
};

// Lock a STORED weak_tree_ptr for use and return an owning shared_tree_ptr valid for the immediate operation.
// Policy: every pointer to a TreeItem-family object STORED beyond a stack frame is a weak_tree_ptr; on use it must be
// lock()ed and checked. An expired target means the config tree is being (or has been) destroyed, so the current
// operation can no longer complete -> throw the task cancellation object (caught by the enclosing OC/CalcResult
// CancelableFrame). Do NOT use for transient (stack-local) borrows -- those stay raw.
template <class T>
shared_tree_ptr<T> lock_or_cancel(const weak_tree_ptr<T>& w)
{
	auto p = w.lock();
	if (!p)
		throwTaskCanceled();
	return shared_tree_ptr<T>(std::move(p));
}

#endif // __RTC_PTR_SHAREDTREEPTR_H
