// Copyright (C) 1998-2023 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__PTR_PERSISTENTSHAREDOBJ_H)
#define __PTR_PERSISTENTSHAREDOBJ_H

#include "ptr/SharedObj.h"
#include "ptr/SharedStr.h"
#include "throwItemError.h"

/// Interface for a shareable, persistent object that participates in a parent/child hierarchy
/// and exposes stable, user-visible naming and source location.
/// Notes:
/// - Naming is provided via ref-counted SharedStr.
/// - Hierarchy navigation is via GetParent() and helpers like GetRoot(), DoesContain().
/// - Error helpers report errors annotated with this item's context.
/// Ownership through SharedPtr; each object manages the lifetime of its parent by increasing its ref-count.
class PersistentObject : public Object
{
public:
	/// Return the logical parent in the persistent hierarchy, or nullptr if this is the root.
	/// Ownership: non-owning raw pointer.
	/// Thread-safety: depends on implementation.
	RTC_CALL virtual [[nodiscard]] const PersistentObject* GetParent() const noexcept;

	/// Return the local source/name identifier of this object (without path/ancestor context).
	/// Contract: Should be stable and suitable for composition by GetFullName().
	/// Overrides SharedObj::GetSourceName.
	RTC_CALL [[nodiscard]] SharedStr GetSourceName() const override;

	// NON VIRTUAL ROUTINES BASED ON THE ABOVE INTERFACE

	/// Compose the hierarchical full name by walking parents and joining source names.
	/// Format and separators are implementation-defined.
	/// Precondition: Parent chain must be acyclic.
	/// TODO: Consider [[nodiscard]] and noexcept if building uses only non-throwing ops.
	/// TODO: Consider caching full name if immutable to reduce repeated allocations.
	RTC_CALL auto GetFullName() const -> SharedStr;

	/// Return a prefixed variant of the full name (e.g., with type or scope prefix).
	/// TODO: Define/Document the prefix semantics in the implementation.
	/// TODO: Consider [[nodiscard]] and noexcept.
	RTC_CALL auto GetPrefixedFullName() const->SharedStr;

	/// Return the full configuration name; default maps to GetFullName().
	/// Override to include configuration-specific qualifiers.
	/// TODO: Consider [[nodiscard]] and noexcept.
	auto GetFullCfgName() const -> SharedStr override { return GetFullName(); }

	/// Ascend to the top-most ancestor (first node with no parent).
	/// Returns this if already at root.
	/// TODO: Consider [[nodiscard]] and noexcept.
	RTC_CALL auto GetRoot() const ->const PersistentObject*;

	/// Return true if this object is an ancestor (or equal to) the given subItemCandidate.
	RTC_CALL bool DoesContain(const PersistentObject* subItemCandidate) const noexcept;

	/// Return optional source location metadata for diagnostics.
	/// May return nullptr if location is unknown.
	/// Overrides SharedObj::GetLocation.
	RTC_CALL const SourceLocation* GetLocation() const override ;

	/// Compute the name of this object relative to the provided context.
	/// E.g., if context is an ancestor, the result may be a shorter path.
	/// Precondition: context must be within the same hierarchy.
	/// returs a full path when context is unrelated; define fallback (full name?).
	RTC_CALL [[nodiscard]] SharedStr GetRelativeName(const PersistentObject* context) const;

	/// Produce a name for subItem that is "findable" from this object (e.g., a relative path).
	/// Typically used to generate references between items.
	/// Precondition: subItem should be in the same hierarchy.
	/// TODO: Document the exact naming rules and escaping strategy.
	RTC_CALL [[nodiscard]] SharedStr GetFindableName(const PersistentObject* subItem) const;

	/// Declare abstract RTTI for this class (macro from project infrastructure).
	/// Typically provides meta-class registration and introspection facilities.
	DECL_ABSTR(RTC_CALL, Class);
};

/*
Suggestions for improvements (non-breaking API suggestions):
- Consider marking getters as [[nodiscard]] to encourage consumption of results:
  - GetParent(), GetSourceName(), GetFullName(), GetPrefixedFullName(), GetFullCfgName(),
    GetRoot(), GetLocation(), GetRelativeName(), GetFindableName()
- Consider noexcept where implementations guarantee no exceptions (especially simple ascents/lookups).
- Document and enforce acyclic parent chain invariants to prevent infinite loops.
- If names are immutable, cache computed full names to avoid repeated allocations; invalidate on rename.
- Prefer returning references or optional-like wrappers over raw pointers for GetLocation().
- For error helpers, unify overloads by funneling through one implementation; consider std::format or project’s formatting utilities.
- Ensure thread-safety guarantees (if any) are documented, especially for parent traversal and name computations.
- If ABI stability matters, keep RTC_CALL usage consistent; otherwise consider limiting it to boundary APIs.
*/

// used for determining the scope of relative names in XML_Dump
RTC_CALL extern const PersistentObject* s_RelativeScope;


//using PersistentSharedObj = SharedObjWrap< PersistentObject>;


#endif // __PTR_PERSISTENTSHAREDOBJ_H`
