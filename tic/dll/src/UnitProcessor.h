//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#pragma once

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
	virtual void Visit(const Host* inviter) const  { throwIllegalAbstract(MG_POS, inviter, "UnitVisitor"); }
};

struct UnitProcessor
	:	boost::mpl::fold<
			typelists::all_unit_types
		,	tl_oper::impl::empty_base<>
		,	tl_oper::impl::scattered_hierarchy<UnitVisitor<Unit<boost::mpl::_2> >, boost::mpl::_1>
	>	::	type
{};

template <typename Host, typename Base>
struct VisitorImpl : Base
{
	using Base::Base; // inherit ctors

	void Visit(const Host* inviter) const override 
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
struct UnitLambdaCaller : boost::mpl::fold<TypeList, AutoLambdaCallerBase<AutoLambda>, VisitorImpl<Unit<_2>, _1> >::type
{
	using base_type = boost::mpl::fold<TypeList, AutoLambdaCallerBase<AutoLambda>, VisitorImpl<Unit<_2>, _1> >::type;
	using base_type::base_type; // inherit ctors
};

template<typename TypeList, typename UnitPtrAutoLambda>
void visit(const AbstrUnit* inviter, UnitPtrAutoLambda&& al)
{
	UnitLambdaCaller<TypeList, UnitPtrAutoLambda> alc(std::forward<UnitPtrAutoLambda>(al));
	inviter->InviteUnitProcessor(alc);
}


template<typename ValuePtrAutoLambda>
auto value_ptr_caller(ValuePtrAutoLambda&& autoLambda)
{
	return [autoLambda](auto valuesUnit) {
		using values_unit = std::remove_reference_t<decltype(*valuesUnit)>;
		using value_type = typename values_unit::value_t;
		value_type* v = nullptr;
		autoLambda(v);
	};
}

template<typename TypeList, typename ValuePtrAutoLambda>
void visit_value_ptr(const AbstrUnit* inviter, ValuePtrAutoLambda&& autoLambda)
{
	visit<TypeList>(value_ptr_caller(std::forward<ValuePtrAutoLambda>(autoLambda)));
}




#endif // __UNITPROCESSOR_H
