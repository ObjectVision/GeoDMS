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

template <typename Host>
struct UnitVisitor {
	virtual void Visit(const Unit<Host>* inviter) const  { throwIllegalAbstract(MG_POS, inviter, "UnitVisitor"); }

};
using UnitVisitorsList = tl::transform_t<typelists::all_unit_types, tl::bind_placeholders<UnitVisitor, ph::_1>>;

struct UnitProcessor: inherit_all<UnitVisitorsList> {};

template <typename Host, typename Base>
struct UnitVisitorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Unit<Host>* inviter) const override 
	{ 
		this->VisitImpl(inviter); 
	}
};


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

template<typename TypeList, typename AutoLambda>
struct UnitLambdaCaller : fold_t<TypeList, AutoLambdaCallerBase<AutoLambda>, UnitVisitorImpl>
{
	using base_type = fold_t<TypeList, AutoLambdaCallerBase<AutoLambda>, UnitVisitorImpl>;
	using base_type::base_type; // inherit ctors
};

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
