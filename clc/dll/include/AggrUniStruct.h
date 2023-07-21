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

#if !defined(__CLC_AGGRUNISTRUCT_H)
#define __CLC_AGGRUNISTRUCT_H

#include "Param.h"
#include "ser/VectorStream.h"
#include "geo/BaseBounds.h"

#include "Prototypes.h"

// *****************************************************************************
//						initializers
// *****************************************************************************

template <typename T>
struct default_value : public nullary_func<T>
{
	T operator()() const { return T(); }
};

template <typename T>
struct min_value : public nullary_func<T>
{
	T operator()() const { return MIN_VALUE(T); }
};

template <typename T>
struct max_value : public nullary_func<T>
{
	T operator()() const { return MAX_VALUE(T); }
};

template <typename T>
struct null_value : public nullary_func<T>
{
	T operator()() const { return UNDEFINED_VALUE(T); }
};

template <typename T>
struct null_or_zero_value : public nullary_func<T>
{
	T operator()() const { return UNDEFINED_OR_ZERO(T); }
};

template <typename T> struct assign_default   : nullary_assign_from_func< default_value<T> > {};
template <typename T> struct assign_null_value: nullary_assign_from_func< null_value<T> > {};
template <typename T> struct assign_null_or_zero: nullary_assign_from_func< null_or_zero_value<T> > {};
template <typename T> struct assign_min_value : nullary_assign_from_func< min_value <T> > {};
template <typename T> struct assign_max_value : nullary_assign_from_func< max_value <T> > {};

template <typename TNullaryAssign>
struct initializer
{
	initializer(const TNullaryAssign& aFunc = TNullaryAssign()) : m_Assign(aFunc) {}

	void Init(typename TNullaryAssign::assignee_ref accumulator) const
	{
		m_Assign(accumulator);
	}
private:
	TNullaryAssign m_Assign;
};

// *****************************************************************************
//						AssignOutput
// *****************************************************************************

template <typename TUnaryAssign>
struct assign_output
{
	assign_output(const TUnaryAssign& aFunc = TUnaryAssign()) : m_Assign(aFunc) {}

	void AssignOutput(typename TUnaryAssign::assignee_ref res, typename TUnaryAssign::arg1_cref accumulator) const
	{
		m_Assign(res, accumulator);
	}
private:
	TUnaryAssign m_Assign;
};

template <typename T>
struct assign_output_direct : assign_output<ident_assignment<T> >
{
	typedef T dms_result_type;
};

template <typename T, typename S>
struct assign_output_convert
{
	typedef T dms_result_type;

	void AssignOutput(typename sequence_traits<T>::reference res, typename sequence_traits<S>::const_reference buf) const
	{
		res = Convert<T>(buf);
	}
};


template <typename ElemAssigner>
struct assign_partial_output_from_buffer
{
	typedef typename ElemAssigner::dms_result_type           dms_result_type;
	typedef typename sequence_traits<dms_result_type>::seq_t dms_seq;

	template<typename Container>
	void AssignOutput(dms_seq res, const Container& outputs) const
	{
		dms_assert(res.size() == outputs.size());
		auto ri = res.begin();
		auto
			oi = outputs.begin(),
			oe = outputs.end();
		for (; oi != oe; ++oi, ++ri)
			m_Assign.AssignOutput(*ri, make_result( *oi ));
	}

	ElemAssigner m_Assign;
};

// *****************************************************************************
//						AGGREGATION OPERATORS FROM ASSIGNORS
// *****************************************************************************

template <
	typename TUniAssign, 
	typename TNullaryOper = assign_default<typename TUniAssign::assignee_type>
> 
struct unary_assign_total_accumulation
:	unary_total_accumulation<
		typename TUniAssign::assignee_type, 
		typename TUniAssign::arg1_type
	>
,	initializer<TNullaryOper>
{
	template <typename R>
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return cast_unit_creator<R>(args); }

	unary_assign_total_accumulation(TUniAssign f = TUniAssign(), TNullaryOper init = TNullaryOper())
		:	m_AssignFunc(f)
		,	m_Initializer(init) 
	{}

	void operator()(typename unary_assign_total_accumulation::assignee_ref output, typename unary_assign_total_accumulation::value_cseq1 input) const
	{ 
		aggr1_total_best<TUniAssign>(output, input.begin(), input.end(), m_AssignFunc);
	}

private:
	TUniAssign   m_AssignFunc;
	TNullaryOper m_Initializer;
};

// unary_assign_partial_accumulation(res, data, indices): init res with TNullaryFunc; for all v in vector: TUniAssign(res, v)
template <
	typename TUniAssign, 
	typename TNullaryAssign = assign_default<typename TUniAssign::assignee_type>
> 
struct unary_assign_partial_accumulation 
:	unary_partial_accumulation<
		typename TUniAssign::assignee_type
	,	typename TUniAssign::arg1_type
	>
{
	using result_value_type = typename unary_assign_partial_accumulation::accumulation_seq::value_type;

	template <typename R> static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TUniAssign::template unit_creator<R>(gr, args);  }

	unary_assign_partial_accumulation(
		TUniAssign assignFunc = TUniAssign(), 
		TNullaryAssign n = TNullaryAssign()
	)
	:	m_AssignFunc(assignFunc), 
		m_Initializer(n)  
	{}

	void Init(typename unary_assign_partial_accumulation::accumulation_seq outputs) const
	{
		transform_assign(outputs.begin(), outputs.end(), m_Initializer);
	}

	void operator()(typename unary_assign_partial_accumulation::accumulation_seq outputs, typename unary_assign_partial_accumulation::value_cseq1 input, const IndexGetter* indices) const
	{ 
		aggr_fw_best_partial<TUniAssign>(outputs.begin(), input.begin(), input.end(), indices, m_AssignFunc);
	}

	TUniAssign     m_AssignFunc;
	TNullaryAssign m_Initializer;
};

#endif

