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

	using base_type::base_type; // inherit std::shared_ptr ctors (nullptr, raw-adopt, copy/move, converting)

	constexpr shared_tree_ptr() noexcept = default;
	shared_tree_ptr(const base_type& rhs) noexcept : base_type(rhs) {}
	shared_tree_ptr(base_type&& rhs) noexcept : base_type(std::move(rhs)) {}

	// Construction-tag ctors mapping the intrusive semantics onto std:: ownership:
	shared_tree_ptr(T* p, newly_obj) : base_type(p) {}                     // adopt a freshly created object
	shared_tree_ptr(T* p, existing_obj) : base_type(dup_existing(p)) {}    // borrow an already-owned object
	shared_tree_ptr(T* p, no_zombies)  : base_type(dup_no_zombies(p)) {}   // safe weak->strong (null if expiring)

	// in-repo SharedPtr API surface
	T*   get_ptr()  const noexcept { return this->get(); }
	bool is_null()  const noexcept { return this->get() == nullptr; }
	bool has_ptr()  const noexcept { return this->get() != nullptr; }

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

	// Note: unlike the in-repo raw WeakPtr, dereferencing goes through lock(); call sites that need a
	// live pointer must hold the lock() result. has_ptr/is_null reflect expiry.
	std::shared_ptr<T> lock_ptr() const noexcept { return this->lock(); }
	bool is_null() const noexcept { return this->expired(); }
	bool has_ptr() const noexcept { return !this->expired(); }
};

// pointer_traits so InterestPtr<shared_tree_ptr<T>> / weak_tree_ptr work (mirrors the std::shared_ptr/weak_ptr specializations in RtcBase.h)
template <typename T> struct pointer_traits<shared_tree_ptr<T>> : pointer_traits_helper<T> {
	static T* get_ptr(const shared_tree_ptr<T>& ptr) { return ptr.get(); }
};
template <typename T> struct pointer_traits<weak_tree_ptr<T>> : pointer_traits_helper<T> {};

#endif // __RTC_PTR_SHAREDTREEPTR_H
