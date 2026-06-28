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

	// Construction-tag ctors mapping the intrusive semantics onto std:: ownership:
	shared_tree_ptr(T* p, newly_obj) : base_type(p) {}                     // adopt a freshly created object
	shared_tree_ptr(T* p, existing_obj) : base_type(dup_existing(p)) {}    // borrow an already-owned object
	shared_tree_ptr(T* p, no_zombies)  : base_type(dup_no_zombies(p)) {}   // safe weak->strong (null if expiring)

	// Transition aids: borrow from an in-repo intrusive SharedPtr<U> / WeakPtr<U> (existence implies owned).
	template <typename U> requires std::is_convertible_v<U*, T*>
	shared_tree_ptr(const SharedPtr<U>& sp) : base_type(dup_existing(static_cast<T*>(sp.get_ptr()))) {}
	template <typename U> requires std::is_convertible_v<U*, T*>
	shared_tree_ptr(const WeakPtr<U>& wp) : base_type(dup_no_zombies(static_cast<T*>(wp.get_ptr()))) {}

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

// pointer_traits so InterestPtr<shared_tree_ptr<T>> / weak_tree_ptr work (mirrors the std::shared_ptr/weak_ptr specializations in RtcBase.h)
template <typename T> struct pointer_traits<shared_tree_ptr<T>> : pointer_traits_helper<T> {
	static T* get_ptr(const shared_tree_ptr<T>& ptr) { return ptr.get(); }
};
template <typename T> struct pointer_traits<weak_tree_ptr<T>> : pointer_traits_helper<T> {
	static T* get_ptr(const weak_tree_ptr<T>& ptr) { return ptr.lock().get(); } // momentary lock; null if expired
};

#endif // __RTC_PTR_SHAREDTREEPTR_H
