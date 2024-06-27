// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

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
template <typename T> struct assign_null_value : nullary_assign<T>
{
	void operator()(typename nullary_assign<T>::assignee_ref res) const
	{
		MakeUndefined(res);
	}
};

template <typename T> struct assign_null_or_zero: nullary_assign_from_func< null_or_zero_value<T> > {};
template <typename T> struct assign_min_value : nullary_assign_from_func< min_value <T> > {};
template <typename T> struct assign_max_value : nullary_assign_from_func< max_value <T> > {};

template <typename TNullaryAssign>
struct initializer
{
	initializer(const TNullaryAssign& aFunc = TNullaryAssign()) : m_Assign(aFunc) {}

	auto InitialValue() const
	{
		typename TNullaryAssign::assignee_type accumulator;
		m_Assign(accumulator);
		return accumulator;
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

	void AssignOutput(typename TUnaryAssign::assignee_ref res, auto&& accumulator) const
	{
		m_Assign(res, accumulator);
	}
private:
	TUnaryAssign m_Assign;
};

template <typename T>
struct assign_output_direct : assign_output<ident_assignment<T> >
{
	using dms_result_type = T;
};

template <typename T, typename S>
struct assign_output_convert
{
	using dms_result_type = T;

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
		assert(res.size() == outputs.size());
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

	void operator()(typename unary_assign_total_accumulation::assignee_ref accumulator, typename unary_assign_total_accumulation::value_cseq1 input) const
	{ 
		aggr1_total<TUniAssign>(accumulator, input.begin(), input.end(), m_AssignFunc);
	}

	void CombineValues(typename TUniAssign::assignee_type& a, const typename TUniAssign::assignee_type& rhs) const
	{
		m_AssignFunc.CombineValues(a, rhs);
	}
	void CombineRefs(typename TUniAssign::assignee_ref a, cref_t<typename TUniAssign::assignee_type> rhs) const
	{
		m_AssignFunc.CombineRefs(a, rhs);
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
		aggr_fw_partial<TUniAssign>(outputs.begin(), input.begin(), input.end(), indices, m_AssignFunc);
	}

	void CombineRefs(typename TUniAssign::assignee_ref accumulator, cref_t<typename TUniAssign::assignee_type> rhs) const
	{
		m_AssignFunc.CombineRefs(accumulator, rhs);
	}

	TUniAssign     m_AssignFunc;
	TNullaryAssign m_Initializer;
};

#endif

