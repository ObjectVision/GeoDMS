// Copyright (C) 1998-2024 Object Vision b.v.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__UNITPROCESSOR_H)
#define __UNITPROCESSOR_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include<utility>

#include "Unit.h"

// MSVC warning C654: overloaded virtual function "UnitProcessor::Visit" is only partially overridden.
// This pattern is intentional: we generate many Visit overloads across a folded hierarchy.
// Disabling locally to keep build output clean.

 #pragma warning(push)
 #pragma warning(disable : 654)

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"


// UnitVisitorBase
// - Host: a specific unit host/type in the typelist.
// - Base: the previously folded base in the inheritance chain.
// Purpose:
//   - Bring prior Visit overloads into scope (using Base::Visit).
//   - Declare a virtual Visit for Unit<Host> that throws by default (abstract-like behavior).
// Notes:
//   - The throwing Visit ensures that if a specific Visit is not overridden downstream,
//     a runtime error occurs (safer than UB).
//   - Relying on EBO for Base to keep the final UnitProcessor size minimal.

template <typename Host, typename Base>
struct UnitVisitorBase : Base
{
	using Base::Visit;  // keep earlier overloads visible; avoids hiding in derived classes

	virtual void Visit(const Unit<Host>* inviter) const  { throwIllegalAbstract(MG_POS, inviter, "UnitVisitor"); }
};

// UnitProcessor
// Purpose:
//   - Fold over typelists::all_unit_types to create a visitor interface that has one virtual Visit per unit type.
//   - Produces a single concrete type with all Visit overloads available.
// Implementation detail:
//   - tl::fold_t<TL, Initial, Template> builds a linear inheritance chain Template<T, Prev>...
//   - The result should typically have a single vptr.

struct UnitProcessor : tl::fold_t<typelists::all_unit_types, tl::empty_base, UnitVisitorBase>
{};

// AutoLambdaCallerBase
// - UnitPtrAutoLambda: the lambda/functor type to be stored and invoked.
// Purpose:
//   - Provide VisitImpl<E>(Unit<E>*) that just invokes m_AutoLambda on the unit.
// Notes:
//   - Currently stores m_AutoLambda as UnitPtrAutoLambda (which may be a reference type).
//   - The constructor copies parameter 'al' into m_AutoLambda without forwarding.
// Risks:
//   - If UnitPtrAutoLambda is deduced as T&& (forwarding ref) or T&, member may become a reference that dangles.
// Suggestions:
//   - Store as std::decay_t<UnitPtrAutoLambda> and take constructor parameter as universal reference.
//   - Example (suggestion only, do not change now):
//       template<typename AL>
//       struct AutoLambdaCallerBase : UnitProcessor {
//         using AutoLambdaT = std::decay_t<AL>;
//         explicit AutoLambdaCallerBase(AL&& al) : m_AutoLambda(std::forward<AL>(al)) {}
//         AutoLambdaT m_AutoLambda;
//         template<typename E> void VisitImpl(const Unit<E>* inviter) const { m_AutoLambda(inviter); }
//       };
//   - This prevents dangling references when passing temporaries.
// TODO: Consider noexcept on VisitImpl if m_AutoLambda is noexcept-invocable.
template<typename UnitPtrAutoLambda>
struct AutoLambdaCallerBase : UnitProcessor
{
	AutoLambdaCallerBase(UnitPtrAutoLambda al) : m_AutoLambda(al) {};
	UnitPtrAutoLambda m_AutoLambda;

	template <typename E>
	void VisitImpl(const Unit<E>* inviter) const
	{
		m_AutoLambda(inviter);
	}
};

// UnitVisitorImpl
// - Host: a specific unit host/type in the typelist.
// - Base: previous fold base in the chain.
// Purpose:
//   - Override Visit for Unit<Host> and forward to VisitImpl supplied by a base (e.g., AutoLambdaCallerBase).
// Notes:
//   - using Base::Base inherits constructors (important for folding).
template <typename Host, typename Base>
struct UnitVisitorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Unit<Host>* inviter) const override
	{
		this->VisitImpl(inviter);
	}
};

// UnitLambdaCallerBase_t
// - TL: a custom typelist to fold over.
// - AL: the lambda type.
// Purpose:
//   - Build a UnitProcessor that overrides all Visit overloads in TL, forwarding to VisitImpl (which calls the lambda).
template<class TL, class AL>
using UnitLambdaCallerBase_t = tl::fold_t<TL, AutoLambdaCallerBase<AL>, UnitVisitorImpl>;

// UnitLambdaCaller
// - TypeList: typelist to build visitor for.
// - AutoLambda: lambda/functor type.
// Purpose:
//   - Thin named wrapper around UnitLambdaCallerBase_t to expose Base and inherit constructors.
// Notes:
//   - This type models UnitProcessor and can be passed to inviter->InviteUnitProcessor.
template<typename TypeList, typename AutoLambda>
struct UnitLambdaCaller : UnitLambdaCallerBase_t<TypeList, AutoLambda>
{
	using Base = UnitLambdaCallerBase_t<TypeList, AutoLambda>;
	using Base::Base; // inherit constructors
};

//----------------------------------------------------------------------

// visit
// - TypeList: typelist containing unit Host types to support.
// - UnitPtrAutoLambda: generic lambda/functor to be invoked for the matched unit pointer.
// Purpose:
//   - Create a UnitLambdaCaller and pass it to the inviter for dynamic dispatch.

template<typename TypeList, typename UnitPtrAutoLambda>
void visit(const AbstrUnit* inviter, UnitPtrAutoLambda&& al)
{
	UnitLambdaCaller<TypeList, UnitPtrAutoLambda> alc(std::forward<UnitPtrAutoLambda>(al));
	inviter->InviteUnitProcessor(alc);
}

// visit_and_return_result
// - TypeList: typelist containing unit Host types to support.
// - ResultType: the return type to produce from the lambda.
// - UnitPtrAutoLambda: lambda taking auto unit ptr and returning ResultType.
// Purpose:
//   - Wrap visit to capture the lambda's result.
// Implementation:
//   - Default-constructs ResultType, then assigns inside the visit callback.
// Notes:
//   - Requires ResultType to be default-constructible and assignable from lambda result.
// Suggestions:
//   - Alternative to support non-default-constructible ResultType:
//       return visit<TypeList, ResultType>(inviter, [&](auto unit){ return al(unit); }); // with a variant of visit returning value
//   - Or use std::optional<ResultType> and emplace within the callback.
//   - Consider [[nodiscard]] to flag uninspected results.
template<typename TypeList, typename ResultType, typename UnitPtrAutoLambda>
auto visit_and_return_result(const AbstrUnit* inviter, UnitPtrAutoLambda&& al) -> ResultType
{
	ResultType result;
	visit<TypeList>(inviter, [&result, al = std::forward<UnitPtrAutoLambda>(al)](auto unit) { result = al(unit); });
	return result;
}


#pragma warning(pop)

#endif // __UNITPROCESSOR_H
