// Copyright (C) 1998-2025 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// High-level overview:
// - This header defines a meta-programming based visitor over a typelist of ValueClasses,
//   and the ValueClass interface itself.
// - The visitor is constructed with tl::fold_t to produce a single vptr interface with
//   one Visit overload per Host type in ValueClassList, minimizing RTTI/virtual table bloat.
// - ValueClass exposes properties about data (size, bit size, dims, numeric/integral/signed,
//   composition, undefined/min/max values, related scalar/range/sequence classes) and
//   provides creation and invitation of a visitor (processor).
//

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_MCI_VALUECLASS_H)
#define __RTC_MCI_VALUECLASS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "geo/BitValue.h"     // Bit-sized value helpers (sub-byte storage).
#include "mci/AbstrValue.h"   // Abstract value interface created by ValueClass::CreateValue.
#include "mci/Class.h"        // Base Class type providing core meta information.

// Forward declarations for friend ties and related metadata classes.
// Ownership note: All pointers in this file appear to be non-owning observers.
class UnitClass;
class DataItemClass;

//----------------------------------------------------------------------

#include "RtcTypeLists.h"     // Provides typelists::vc_types and typelists::range_objects.
#include "cpc/transform.h"    // 'tl::' meta-programming utilities, e.g., fold_t, jv2_t, empty_base.
#include "utl/TypeListOper.h" // 'tl_oper::' meta-programming utilities, e.g., inst_tuple_templ and TypeListClassReg
//----------------------------------------------------------------------

// Compose the full set of value "host" types used to instantiate visitor overloads.
// tl::jv2_t joins two type lists (value classes and their range-object counterparts).
using ValueClassList = tl::jv2_t<typelists::vc_types, typelists::range_objects>;

// Base for the visitor interface. For each Host in ValueClassList a Visit overload will be provided.
// Using CRTP-ish pattern with fold to produce a flat interface (single vptr).
template <typename Host, typename Base>
struct ValueClassVisitorBase : Base {
	using Base::Visit;  // keep earlier overloads visible; essential for overload merging via inheritance

	// Default Visit throws, ensuring unhandled Host types are surfaced at runtime.
	// Note that specific Hosts might not override all ValueClassList elements.
	virtual void Visit(const Host* inviter) const {
		throwIllegalAbstract(MG_POS, "ValueClassVisitor");
	}
};

// Build the visitor interface with a single vptr referring to a virtual Visit method
// for each ValueClass in ValueClassList. tl::fold_t inherits ValueClassVisitorBase<Host, ...>
// across the list to accumulate overloads.
using ValueClassVisitor = tl::fold_t<ValueClassList, tl::empty_base, ValueClassVisitorBase>;

// Helper base that adapts a generic auto-lambda into a ValueClassVisitor.
// The lambda must accept a pointer to the concrete Host: lambda(const Host*).
// Suggestion: Consider SFINAE/concepts to static-assert lambda signature compatibility for clearer diagnostics.
template<typename ValueClassAutoLambda>
struct ValueClassAutoLambdaCallerBase : ValueClassVisitor
{
	// Perfect-forward the lambda. Lifetime: stored by value.
	// Suggestion: Consider making this constructor explicit to avoid accidental conversions.
	ValueClassAutoLambdaCallerBase(ValueClassAutoLambda&& al) : m_AutoLambda(std::forward<ValueClassAutoLambda>(al)) {};
	ValueClassAutoLambda m_AutoLambda;

	// Called by concrete Visit overrides to target the correct Host type.
	template <typename Host>
	void VisitImpl(const Host* inviter) const
	{
		// Suggestion: Consider noexcept if lambda is declared noexcept.
		m_AutoLambda(inviter);
	}
};

// Inject a Visit(const Host*) override for each Host type into the visitor adapter,
// delegating to the type-erased VisitImpl above.
// Note: Using inheritance of constructors to reuse parent's ctors.
template <typename Host, typename Base>
struct ValueClassProcessorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Host* inviter) const override
	{
		this->VisitImpl(inviter);
	}
};

// Fold to build the chain of ValueClassProcessorImpl specializations for all TL types.
// This yields a single type that overrides Visit for every Host in TL.
template<typename TypeList, typename AutoLambda>
using ValueClassLambdaCaller = tl::fold_t<TypeList, ValueClassAutoLambdaCallerBase<AutoLambda>, ValueClassProcessorImpl>;

//----------------------------------------------------------------------
// class  : ValueClass
//----------------------------------------------------------------------
//
// Represents a metadata descriptor for a value type, including:
// - Type identity (ValueClassID), dimensionality, numeric/integral/signed flags.
// - Size information (bytes and optional sub-byte bit-size).
// - Undefined/min/max values for numeric/bits, stored as float64 for uniformity.
// - Composition info (scalar/range/sequence), and related class pointers.
//
// Thread-safety: Some related class pointers are mutable caches (lazy init).
// Suggestion: If accessed from multiple threads, guard lazy initialization with
//             std::once_flag or atomics to avoid races.
//
class ValueClass  : public Class
{
	using base_type = Class;

public:
	// The "inviter" is a function that Knowing the concrete ValueClass specialization,
	// calls the corresponding Visit method on the provided visitor.
	// Signature accepts a const ValueClassVisitor*.
	// Suggestion: For const-correctness symmetry, consider taking const ValueClassVisitor* in InviteProcessor too.
	using InviterFunc = void(*)(const ValueClassVisitor*);

	// Fully-specified constructor wiring all metadata and class relations.
	// Ownership: All pointer parameters are non-owning observers (must outlive ValueClass).
	// Precondition notes:
	// - size >= 0; bitSize in [0, 8) for sub-byte elements; nrDims >= 0.
	// - If isNumeric is true, undefined/min/max Float64 fields must be valid.
	// - fieldClass/scalarClass/cardinalityClass must be coherent with composition.
	// Suggestion: Consider runtime assertions or contract checks to validate invariants here.
	ValueClass(
		Constructor           cFunc,           // factory for AbstrValue instances
		InviterFunc           iFunc,           // visitor invitation bridge
		TokenID               scriptID,        // script-visible name/id
		ValueClassID          vt,              // type identity tag
		Int32                 size,            // size in bytes (0 for aggregated?)
		Int32                 bitSize,         // sub-byte element size (0 or 1..7), see IsSubByteElem
		DimType               nrDims,          // number of dimensions (sequence/shape semantics)
		bool                  isNumeric,       // numeric or logical (sub-byte)
		bool                  isIntegral,      // integral numeric domain
		bool                  isSigned,        // signedness for numeric domain
		ValueComposition      vc,              // scalar/range/sequence composition
		const Byte*           undefinedValuePtr, // pointer to a canonical "undefined" representation
		Float64               undefAsFloat64,  // undefined value coerced to Float64 (for numeric)
		Float64               maxAsFloat64,    // max value coerced to Float64 (for numeric)
		Float64               minAsFloat64,    // min value coerced to Float64 (for numeric)
		const ValueClass*     fieldClass,      // nested/field type in composites (if any)
		const ValueClass*     scalarClass,     // scalar counterpart (for range/sequence)
		const ValueClass*     cardinalityClass // countable counterpart (for integral or scalar integral)
	);
	~ValueClass(); // Suggestion: If no custom cleanup, declare = default and consider noexcept.

	// Factory to create an AbstrValue instance of this class.
	// Ownership: Returns a raw pointer; caller likely assumes ownership.
	// Suggestion: Consider returning std::unique_ptr<AbstrValue> if ABI allows.
	RTC_CALL auto CreateValue() const->std::unique_ptr<AbstrValue>;

	// Invite a visitor to process this value class via the InviterFunc indirection.
	// Requires m_iFunc to be non-null (asserted).
	// Suggestion: Align parameter type to const ValueClassVisitor* to match InviterFunc.
	void InviteProcessor(ValueClassVisitor* vcp) const { dms_assert(m_iFunc); m_iFunc(vcp); }

	// Lightweight inspectors
	// Suggestion: Consider [[nodiscard]] and noexcept on getters.
	UInt32  GetSize   () const { return m_Size;      }
	UInt32  GetBitSize() const { return m_BitSize;   }
	DimType GetNrDims () const { return m_NrDims;    }
	bool    IsNumeric () const { return m_IsNumeric; }

	// Sub-byte elements (bit-packed values).
	// bitSize in range (0, 8) indicates sub-byte element; equal to 0 or >=8 means regular byte-sized.
	bool    IsSubByteElem() const { return GetBitSize() > 0 && GetBitSize() < 8; }

	// Numeric or boolean-like (sub-byte) value domains.
	bool    IsNumericOrBool() const { return m_IsNumeric || IsSubByteElem(); }

	// Integral domain indicator.
	bool    IsIntegral()  const { return m_IsIntegral;}

	// Countable either if this or its scalar counterpart is integral.
	// Suggestion: Document if scalarClass may be null and how that impacts use sites.
	bool    IsCountable() const { return m_IsIntegral || (m_ScalarClass && m_ScalarClass->m_IsIntegral); }

	bool    IsSigned  () const { return m_IsSigned;  }

	// Shapes: 2D sequences (domain knowledge).
	bool    IsShape   () const { return GetNrDims() == 2 && IsSequence(); }

	// Undefined values are not supported for sub-byte elements due to bit-packing constraints.
	bool    HasUndefined() const { return !IsSubByteElem(); }

	// Sub-byte elements have a fixed set of values determined by bit width.
	bool    HasFixedValues() const { return IsSubByteElem(); }

	// hide ValueComposition from header bloat
	// Suggestion: Mark noexcept if these are pure queries without side effects.
	RTC_CALL bool    IsRange   () const;
	RTC_CALL bool    IsSequence() const;

	// Identity and numeric bounds
	ValueClassID     GetValueClassID           () const { return m_ValueClassID; }
	const Byte*      GetUndefinedValuePtr      () const { return m_UndefinedValuePtr; }

	// The following getters assert numeric-or-bool domain.
	// Suggestion: Consider avoiding assert in headers (ODR bloat) or use constexpr checks where possible.
	Float64          GetUndefinedValueAsFloat64() const { dms_assert(IsNumericOrBool()); return m_UndefinedValueAsFloat64; }
	Float64          GetMaxValueAsFloat64      () const { dms_assert(IsNumericOrBool()); return m_MaxValueAsFloat64; }
	Float64          GetMinValueAsFloat64      () const { dms_assert(IsNumericOrBool()); return m_MinValueAsFloat64; }
	ValueComposition GetValueComposition       () const { return m_ValueComposition; }

	// Related classes (observers). Range/Sequence may be lazily resolved (mutable members).
          const ValueClass* GetScalarClass  () const { return m_ScalarClass; }
          const ValueClass* GetRangeClass   () const { return m_RangeClass; }
          const ValueClass* GetSequenceClass() const { return m_SequenceClass; }

	// Signedness / unsignedness conversion counterparts and "countable" (ordinal) class retrieval.
	// Suggestion: Document if these may return 'this' when already matching.
	RTC_CALL const ValueClass* GetUnsignedClass() const;
	RTC_CALL const ValueClass* GetSignedClass  () const;
	RTC_CALL const ValueClass* GetCrdClass     () const; // ord version for Countable

	// Lookup utilities by id or script name.
	// Suggestion: Document lifetime/ownership of returned pointers. Likely global singletons.
	RTC_CALL static const ValueClass* FindByValueClassID(ValueClassID vt);
	RTC_CALL static       ValueClass* FindByScriptName(TokenID valueTypeID);

private:
	// Bridge used by InviteProcessor to dispatch to the correct Visit overload.
	// Suggestion: Consider marking pointer as not_null (GSL) for clarity.
	InviterFunc      m_iFunc;

	// Identity and composition metadata.
	ValueClassID     m_ValueClassID;
	ValueComposition m_ValueComposition;

	// Size metadata: m_Size in bytes, m_BitSize for sub-byte elems.
	// Suggestion: Consider class invariants (e.g., m_BitSize == 0 or in [1..7]).
	UInt32           m_Size,
                  m_BitSize;

	// Dimensionality and domain flags.
	DimType          m_NrDims;
	bool             m_IsNumeric,
                  m_IsIntegral,
                  m_IsSigned;

	// Undefined/min/max handling. undefinedValuePtr points to a canonical representation
	// (format depends on the actual value type).
	const Byte*      m_UndefinedValuePtr;
	Float64          m_UndefinedValueAsFloat64;
	Float64          m_MaxValueAsFloat64;
	Float64          m_MinValueAsFloat64;

	// Related classes (non-owning). Field for composite, scalar counterpart, and cardinality.
	// Suggestion: Consider clarifying lifetime and whether these can be null.
			const ValueClass* m_FieldClass;
         const ValueClass* m_ScalarClass;
			const ValueClass* m_CardinalityClass;

	// Lazily resolved composition counterparts (non-owning).
	// Suggestion: Guard with thread-safe lazy init if accessed concurrently.
	mutable const ValueClass* m_RangeClass;
	mutable const ValueClass* m_SequenceClass;

	// Related meta classes (friends). Non-owning, possibly lazily assigned.
	mutable UnitClass*        m_UnitClass;     friend UnitClass;
	mutable DataItemClass*    m_DataItemClass; friend DataItemClass;

	// RTTI macro for the meta class system.
	// Suggestion: Ensure no ODR issues from macro expansion; prefer out-of-line definitions where possible.
	DECL_RTTI(RTC_CALL, MetaClass)
};

// Utility: Determine if two value classes overlap (same pointer or same integral size).
// Note: Equal size for integrals does not guarantee identical domain (signedness differs).
// Suggestion: Consider extending with signedness/range checks if stricter overlap is needed.
// Suggestion: Make constexpr/noexcept if feasible.
inline Bool OverlappingTypes(const ValueClass *vc1, const ValueClass *vc2)
{
	return vc1 == vc2 || (vc1->IsIntegral() && vc2->IsIntegral() && vc1->GetSize() == vc2->GetSize());
}

//----------------------------------------------------------------------
// utility operations
//----------------------------------------------------------------------

// visit helper: Instantiate a lambda caller matching the provided TypeList,
// then invite the inviter to dispatch to the correct Visit overload.
// Example:
//   visit<ValueClassList>(vc, [](auto* host){ /* host is deduced */ });
//
// Suggestion: Add requires clause to constrain ValueClassAutoLambda compatibility.
template<typename TypeList, typename ValueClassAutoLambda>
void visit(const ValueClass* inviter, ValueClassAutoLambda&& al)
{
	ValueClassLambdaCaller<TypeList, ValueClassAutoLambda> alc(std::forward<ValueClassAutoLambda>(al));
	inviter->InviteProcessor(&alc);
}

// Equality: pointer identity. Two different instances representing the same logical
// ValueClassID will compare false.
// Suggestion: If logical identity is desired, compare GetValueClassID() instead.
inline bool operator ==(const ValueClass& a, const ValueClass& b) { return &a == &b; }

#endif // __RTC_MCI_VALUECLASS_H
