// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#if !defined(__CLC_AGGRBINSTRUCTNUM_H)
#define __CLC_AGGRBINSTRUCTNUM_H

#include "dbg/debug.h"

#include "AggrUniStructNum.h"
#include "DataCheckMode.h"

template <
	typename TBinAssign, 
	typename TNullaryOper = assign_default<typename TBinAssign::assignee_type>
> 
struct binary_assign_total_accumulation
:	binary_total_accumulation<
		typename TBinAssign::assignee_type, 
		typename TBinAssign::arg1_type,
		typename TBinAssign::arg2_type
	>
,	initializer<TNullaryOper>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TBinAssign::unit_creator(gr, args); }

	binary_assign_total_accumulation(TBinAssign f = TBinAssign(), TNullaryOper init = TNullaryOper())
		:	m_AssignFunc(f)
		,	m_Initializer(init) 
	{}

	void operator()(typename binary_assign_total_accumulation::assignee_ref output, typename binary_assign_total_accumulation::value_cseq1 input1, typename binary_assign_total_accumulation::value_cseq2 input2) const
	{ 
		aggr2_total<TBinAssign>(output, input1.begin(), input1.end(), input2.begin(), m_AssignFunc);
	}

private:
	TBinAssign   m_AssignFunc;
	TNullaryOper m_Initializer;
};

template <
	typename TBinAssign, 
	typename TNullaryAssign = assign_default<typename TBinAssign::assignee_type>
> 
struct binary_assign_partial_accumulation 
:	binary_partial_accumulation<
		typename TBinAssign::assignee_type
	,	typename TBinAssign::arg1_type
	,	typename TBinAssign::arg2_type
	>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return TBinAssign::unit_creator(gr, args); }

	binary_assign_partial_accumulation(
		TBinAssign assignFunc = TBinAssign(), 
		TNullaryAssign n = TNullaryAssign()
	)
	:	m_AssignFunc(assignFunc), 
		m_Initializer(n)  
	{}

	void Init(typename binary_assign_partial_accumulation::accumulation_seq outputs) const
	{
		transform_assign(outputs.begin(), outputs.end(), m_Initializer);
	}

	void operator()(typename binary_assign_partial_accumulation::accumulation_seq outputs, typename binary_assign_partial_accumulation::value_cseq1 input1, typename binary_assign_partial_accumulation::value_cseq2 input2, const IndexGetter* indices) const
	{ 
		assert(input1.size() == input2.size());

		aggr2_fw_partial<TBinAssign>(outputs.begin(), input1.begin(), input1.end(), input2.begin(), indices, m_AssignFunc);
	}

	TBinAssign     m_AssignFunc;
	TNullaryAssign m_Initializer;
};

/*****************************************************************************/
//			COVARIANCE
/*****************************************************************************/

template <typename T> 
struct cov_accumulation_type
{
	using count_type = SizeT;
	using sum_type= acc_type<T>::type;
	using cov_type = aggr_type<T>::type;

	operator cov_type() const
	{
		if (!n)
			return UNDEFINED_VALUE(cov_type);

		// form E(X), E(Y), E(x * y)
		cov_type exp_x  = x;  exp_x  /= n;
		cov_type exp_y  = y;  exp_y  /= n;
		cov_type exp_xy = xy; exp_xy /= n;

		// form E(X * Y) - E(X) * E(Y)
		return exp_xy - exp_x * exp_y;
	}

	friend cov_type make_result(const cov_accumulation_type& output) { return output.operator cov_type(); } // move casting stuff here

	count_type n = 0;
	sum_type   x = sum_type(), y = sum_type(), xy = sum_type();
};

template <typename T>
struct binary_assign_cov: binary_assign<cov_accumulation_type<T>, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mul2_unit_creator(gr, args); }

	void operator () (typename binary_assign_cov::assignee_ref a, typename binary_assign_cov::arg1_cref x, typename binary_assign_cov::arg2_cref y) const
	{
		if constexpr (has_undefines_v<T>)
		{
			if (!IsDefined(x) || !IsDefined(y))
				return;
		}
		++ a.n;
		MG_CHECK(a.n);
		a.x = m_SafeAdder(a.x, x);
		a.y = m_SafeAdder(a.y, y);
		a.xy = m_SafeAdder(a.xy,  m_MulFunc(x, y));
	}
	using sum_type = typename cov_accumulation_type<T>::sum_type;
	safe_plus<sum_type> m_SafeAdder;
	mulx_func<T> m_MulFunc;
};

template <class T> 
struct covariance_total
	:	binary_assign_total_accumulation< binary_assign_cov<T> >
	,	assign_output_convert<T, typename aggr_type<T>::type>
{};

template <typename T> 
struct covariance_partial_best
	:	binary_assign_partial_accumulation<binary_assign_cov<T> >
	,	assign_partial_output_from_buffer< assign_output_convert<T, typename cov_accumulation_type<T>::cov_type> >
{
//	typedef T dms_result_type;
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return mulx_unit_creator(gr, args); }
};


/*****************************************************************************/
//			CORRELATION
/*****************************************************************************/

template <typename T> 
struct corr_accumulation_type : cov_accumulation_type<T>
{
	using corr_type = typename cov_accumulation_type<T>::cov_type;
	using sum_type = typename acc_type<T>::type;
	using agregator = typename aggr_type<T>::type;

	corr_accumulation_type(): xx(), yy() {}

	operator corr_type() const
	{
		typename aggr_type<T>::type cov = cov_accumulation_type<T>::operator cov_type();
		var_accumulation_type<T> sx(this->n, this->x, xx);
		var_accumulation_type<T> sy(this->n, this->y, yy);

		div_func<corr_type>     div;
		sqrt_func_checked<corr_type> sqrt;
		return div(cov, sqrt(sx*sy));
	}
	friend corr_type make_result(const corr_accumulation_type& output) { return output.operator corr_type(); } // move casting stuff here

	sum_type xx, yy;
};

template <typename T>
struct binary_assign_corr: binary_assign<corr_accumulation_type<T>, T, T>
{
	static ConstUnitRef unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args) { return default_unit_creator<T>(); }

	using sum_type = typename acc_type<T>::type;
	using agregator = typename aggr_type<T>::type;

	void operator () (typename binary_assign_corr::assignee_ref a, cref_t<T> x, cref_t<T> y) const
	{
		m_CovAssign(a, x, y);
		a.xx = m_SafePlus(a.xx, m_MulX(x, x));
		a.yy = m_SafePlus(a.yy, m_MulX(y, y));
	}

private:
	binary_assign_cov<T>               m_CovAssign;
	mulx_func<T> m_MulX;
	safe_plus<sum_type> m_SafePlus;
};

template <class T> 
struct correlation_total
	:	binary_assign_total_accumulation<binary_assign_corr<T> >
	,	assign_output_convert<T, typename aggr_type<T>::type>
{};


template <typename T> 
struct correlation_partial_best
	:	binary_assign_partial_accumulation<binary_assign_corr<T> >
	,	assign_partial_output_from_buffer< assign_output_convert<T, typename corr_accumulation_type<T>::cov_type> >
{};


#endif
