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

#include "Unit.h"
#	pragma warning( disable : 654) // overloaded virtual function "UnitProcessor::Visit" is only partially overridden

#include "RtcTypeLists.h"
#include "utl/TypeListOper.h"

template <typename Host, typename Base>
struct UnitVisitorBase : Base 
{
	using Base::Visit;  // <- keep earlier overloads visible

	virtual void Visit(const Unit<Host>* inviter) const  { throwIllegalAbstract(MG_POS, inviter, "UnitVisitor"); }
};

// Build the visitor interface with a single vptr referring to a virtual Visit method for each unit_types
struct UnitProcessor : tl::fold_t<typelists::all_unit_types, tl::empty_base, UnitVisitorBase>
{};

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

template <typename Host, typename Base>
struct UnitVisitorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Unit<Host>* inviter) const override
	{
		this->VisitImpl(inviter);
	}
};

template<class TL, class AL>
using UnitLambdaCallerBase_t = tl::fold_t<TL, AutoLambdaCallerBase<AL>, UnitVisitorImpl>;

template<typename TypeList, typename AutoLambda>
struct UnitLambdaCaller : UnitLambdaCallerBase_t<TypeList, AutoLambda>
{
	using Base = UnitLambdaCallerBase_t<TypeList, AutoLambda>;
	using Base::Base; // inherit constructors
};

//----------------------------------------------------------------------

template<typename TypeList, typename UnitPtrAutoLambda>
void visit(const AbstrUnit* inviter, UnitPtrAutoLambda&& al)
{
	UnitLambdaCaller<TypeList, UnitPtrAutoLambda> alc(std::forward<UnitPtrAutoLambda>(al));
	inviter->InviteUnitProcessor(alc);
}

template<typename TypeList, typename ResultType, typename UnitPtrAutoLambda>
auto visit_and_return_result(const AbstrUnit* inviter, UnitPtrAutoLambda&& al) -> ResultType
{
	ResultType result;
	visit<TypeList>(inviter, [&result, al = std::forward<UnitPtrAutoLambda>(al)](auto unit) { result = al(unit); });
	return result;
}


#endif // __UNITPROCESSOR_H
