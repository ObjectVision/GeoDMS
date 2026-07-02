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
//  TreeItem-family std::shared_ptr / std::weak_ptr construction helpers
//
//  The TreeItem family is owned by real std::shared_ptr control blocks (enable_shared_from_this
//  lives on TreeItem). Members and locals use std::shared_ptr / std::weak_ptr DIRECTLY -- the
//  earlier shared_tree_ptr / weak_tree_ptr wrappers (migration option B) are gone, so smart-pointer
//  semantics are the standard ones, without surprising member API (no hidden lock in operator->).
//
//  What remains here is the CONSTRUCTION discipline: a raw pointer never becomes owning by itself.
//  Classify every raw->smart conversion with a tag helper below. In particular, NEVER write
//  std::shared_ptr<T>(rawPtr) for a tree-owned object: that creates a SECOND control block with a
//  delete-deleter, silently double-managing the object (frees it out from under its real owners at
//  teardown -- a real UAF class). The wrapper's =delete used to catch this at compile time; with
//  plain std types this is a convention, enforced by review/grep -- always use make_shared_tree.
//
//  PRECONDITION: the element type's enable_shared_from_this base is on TreeItem, so existing_obj /
//  no_zombies can recover the control block via shared_from_this() / weak_from_this();
//  static_pointer_cast bridges the (const/derived) gap to the requested type.
//  -----------------------------------------------------------------------

// adopt a FRESHLY CREATED object (first owner; starts the control block).
// GUARD: enable_shared_from_this (on TreeItem) records the first adoption in its weak_this member, so a live
// object whose weak_from_this() is not expired ALREADY has a control block; adopting it again here would create
// a rogue second one (double-delete/UAF at teardown -- the exact class the removed wrapper's =delete raw ctor
// used to catch at compile time). Detect it before the damage is done.
template <class Y>
auto make_shared_tree(Y* p, newly_obj) -> std::shared_ptr<Y>
{
	MG_CHECK2(!p || p->weak_from_this().expired(), "make_shared_tree(p, newly_obj{}): object is already owned by a std::shared_ptr control block; borrow it with existing_obj{} instead of adopting it as new");
	return std::shared_ptr<Y>(p);
}

// borrow an ALREADY-OWNED object as an additional owner (asserts/throws bad_weak_ptr if not owned)
template <class Y>
auto make_shared_tree(Y* p, existing_obj) -> std::shared_ptr<Y>
{
	if (!p) return {};
	return std::static_pointer_cast<Y>(p->shared_from_this());
}

// safe weak->strong from a raw pointer: null if the object is expiring or not std-owned
template <class Y>
auto make_shared_tree(Y* p, no_zombies) -> std::shared_ptr<Y>
{
	if (!p) return {};
	return std::static_pointer_cast<Y>(p->weak_from_this().lock());
}

// weak borrow from a raw pointer (graceful: empty weak if the object is not std-owned / expiring)
template <class Y>
auto make_weak_tree(Y* p) -> std::weak_ptr<Y>
{
	if (!p) return {};
	return std::static_pointer_cast<Y>(p->weak_from_this().lock());
}

// MakeSharedFromBorrowedObjectPtr for the TreeItem family: borrow an already-owned object as an
// OWNING std::shared_ptr. Constrained to std::enable_shared_from_this-backed types (the TreeItem
// family), so it is MORE specialized than the intrusive SharedPtr<T> overload in SharedPtr.h and is
// selected for those types; non-family (intrusive) types lack shared_from_this and fall back to the
// SharedPtr<T> version. Same name/signature on purpose.
template <typename T> requires requires(T* p) { p->shared_from_this(); }
auto MakeSharedFromBorrowedObjectPtr(T* ptr) -> std::shared_ptr<T>
{
	return make_shared_tree(ptr, existing_obj{});
}

// Lock a STORED std::weak_ptr for use and return an owning std::shared_ptr valid for the immediate operation.
// Policy: every pointer to a TreeItem-family object STORED beyond a stack frame is a std::weak_ptr; on use it must be
// lock()ed and checked. An expired target means the config tree is being (or has been) destroyed, so the current
// operation can no longer complete -> throw the task cancellation object (caught by the enclosing OC/CalcResult
// CancelableFrame). Do NOT use for transient (stack-local) borrows -- those stay raw.
template <class T>
std::shared_ptr<T> lock_or_cancel(const std::weak_ptr<T>& w)
{
	auto p = w.lock();
	if (!p)
		throwTaskCanceled();
	return p;
}

#endif // __RTC_PTR_SHAREDTREEPTR_H
