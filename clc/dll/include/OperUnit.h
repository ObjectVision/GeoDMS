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

#if !defined(__OPERUNIT_H)
#define __OPERUNIT_H

#include "dbg/DmsCatch.h"

#include "Unit.h"
#include "UnitGroup.h"
#include "UnitClass.h"
#include "Metric.h"
#include "DataArray.h"
#include "CheckedDomain.h"
#include "AttrUniStructNum.h"

// *****************************************************************************
// DefaultUnitOperator
// *****************************************************************************

class DefaultUnitOperator : public UnaryOperator
{
	typedef AbstrUnit            ResultType;
	typedef DataArray<SharedStr> Arg1Type;

public:
	DefaultUnitOperator(AbstrOperGroup* gr);

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override;
};

// *****************************************************************************
// DefaultUnit0Operator 
// n.b. met typenaam als string-arg in OperUnit.cpp
// *****************************************************************************

template<typename T>
class Default0UnitOperator : public NullaryOperator
{
	typedef Unit<T>           ResultType;

public:
	Default0UnitOperator()
		:	NullaryOperator(GetUnitGroup<T>(), ResultType::GetStaticClass())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		if (!resultHolder)
			resultHolder = Unit<T>::GetStaticClass()->CreateDefault();
		return true;
	}
};

// *****************************************************************************
// CastUnitOperator
// *****************************************************************************

class CastUnitOperator : public UnaryOperator
{
	typedef AbstrUnit Arg1Type;
	const UnitClass* m_ResultUnitClass;
public:
	CastUnitOperator(AbstrOperGroup* gr, const UnitClass* resultUnitClass);

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override;
};

// *****************************************************************************
// UnitOperator
// *****************************************************************************

class AbstrBinUnitOperator : public BinaryOperator
{
public:
	AbstrBinUnitOperator(AbstrOperGroup* gr, const UnitClass* resType, const UnitClass* arg1Type, const UnitClass* arg2Type)
		:	BinaryOperator(gr, resType, arg1Type, arg2Type)
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrUnit* arg1A = AsUnit(args[0]);
		const AbstrUnit* arg2A = AsUnit(args[1]);

		dms_assert(arg1A);
		dms_assert(arg2A);

		AbstrUnit* res = debug_cast<const UnitClass*>(GetResultClass())->CreateTmpUnit(resultHolder);
		resultHolder = res;

		dms_assert(res);

		res->SetMetric(MetricOperation(arg1A->GetCurrMetric(), arg2A->GetCurrMetric()) );

		return true;
	}
	virtual SharedPtr<const UnitMetric> MetricOperation(const UnitMetric* arg1, const UnitMetric* arg2) const= 0;
};

template <typename MetricFunctor>
class BinUnitOperator : public AbstrBinUnitOperator
{
public:
	BinUnitOperator(AbstrOperGroup* gr, const UnitClass* resType, const UnitClass* arg1Type, const UnitClass* arg2Type, const MetricFunctor& metricFunctor = MetricFunctor())
		:	AbstrBinUnitOperator(gr
			,	resType
			,	arg1Type
			,	arg2Type
			)
		,	m_MetricFunctor(metricFunctor)
	{}

	// Override Operator
	SharedPtr<const UnitMetric> MetricOperation(const UnitMetric* arg1, const UnitMetric* arg2) const override
	{
		return m_MetricFunctor(arg1, arg2);
	}
	MetricFunctor m_MetricFunctor;
};

// *****************************************************************************
// ParamUnitOperator
// *****************************************************************************

#include "TreeItemClass.h"

template <typename MetricFunctor>
class ParamUnitOperator : public BinaryOperator
{
	typedef AbstrUnit     ResultType;
	typedef AbstrDataItem Arg1Type;
	typedef AbstrUnit     Arg2Type;

public:
	ParamUnitOperator(AbstrOperGroup* gr, const UnitClass* uc, const MetricFunctor& metricFunctor = MetricFunctor())
		:	BinaryOperator(gr, uc, Arg1Type::GetStaticClass(), uc)
		,	m_MetricFunctor(metricFunctor)
	{}

	// Override Operator
	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		dms_assert(firstArgValue == nullptr || *firstArgValue == char(0));
		if (argNr==0)
			return oper_arg_policy::calc_always;
		return BinaryOperator::GetArgPolicy(argNr, firstArgValue);
	}

	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrDataItem* adi = AsDataItem(args[0]);
		checked_domain<Void>(args[0], "a1");

		const AbstrUnit* arg2A = AsUnit(args[1]);
		dms_assert(arg2A);
		dms_assert(arg2A->GetUnitClass() == GetArgClass(1));
		dms_assert(arg2A->GetUnitClass() == GetResultClass());

		AbstrUnit* result = arg2A->GetUnitClass()->CreateTmpUnit(resultHolder);
		dms_assert(result);
		resultHolder = result;

		InterestPtr<const TreeItem*> hackToFixFuncDcMakeResultDueToUnderspecifiedOperatorgroup(adi); // REMOVE, FIX
		DataReadLock lck(adi);

		result->SetMetric(
			m_MetricFunctor(
				adi->GetAbstrValuesUnit()->GetMetric()
			,	arg2A->GetMetric()
			,	adi->GetCurrRefObj()->GetValueAsFloat64(0)
			)
		);
		return true;
	}
	MetricFunctor m_MetricFunctor;
};

struct MulMetricFunctor {
	SharedPtr<const UnitMetric> operator()(const UnitMetric* arg1, const UnitMetric* arg2, Float64 factor = 1.0) const
	{
		if (factor == 1.0)
		{
			if (IsEmpty(arg2))
				return IsEmpty(arg1)
					? 0
					: arg1;

			if (IsEmpty(arg1))
				return arg2;
		}
		SharedPtr<UnitMetric> m = new UnitMetric;
		m->m_Factor = factor;
		m->SetProduct(arg1, arg2);
		return m.get_ptr();
	}
};

struct DivMetricFunctor {
	SharedPtr<const UnitMetric> operator()(const UnitMetric* arg1, const UnitMetric* arg2, Float64 factor = 1.0) const
	{
		if (factor == 1.0 && IsEmpty(arg2))
			return IsEmpty(arg1)
				? 0
				: arg1;

		SharedPtr<UnitMetric> m = new UnitMetric();
		m->m_Factor = factor;
		m->SetQuotient(arg1, arg2);
		return m.get_ptr();
	}
};

typedef ParamUnitOperator<MulMetricFunctor> ParamMulUnitOperator;
typedef ParamUnitOperator<DivMetricFunctor> ParamDivUnitOperator;

// *****************************************************************************
// UnitPowerOperator
// *****************************************************************************

class UnitPowerOperator : public BinaryOperator
{
	typedef AbstrUnit     ResultType;
	typedef AbstrUnit     Arg1Type;
	typedef AbstrDataItem Arg2Type;

public:
	UnitPowerOperator(AbstrOperGroup* gr, const UnitClass* uc)
		:	BinaryOperator(gr, uc, uc, Arg2Type::GetStaticClass())
	{}

	// Override Operator
	oper_arg_policy GetArgPolicy(arg_index argNr, CharPtr firstArgValue) const override
	{
		if (argNr==1)
			return oper_arg_policy::calc_always;
		return BinaryOperator::GetArgPolicy(argNr, firstArgValue);
	}
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() == 2);

		const AbstrUnit* arg1A = AsUnit(args[0]);
		dms_assert(arg1A);
		dms_assert(arg1A->GetUnitClass() == GetArgClass(0));
		dms_assert(arg1A->GetUnitClass() == GetResultClass());

		checked_domain<Void>(args[1], "a2");

		AbstrUnit* result = arg1A->GetUnitClass()->CreateTmpUnit(resultHolder);
		resultHolder = result;

		const UnitMetric* arg1SI = arg1A->GetMetric();
		if (IsEmpty(arg1SI))
			result->SetMetric(nullptr);
		else
		{
			auto metric = std::make_unique<UnitMetric>(*arg1SI);

			const AbstrDataItem* adi = AsDataItem(args[2]);

			DataReadLock lck(adi);
			Float64 power = adi->GetCurrRefObj()->GetValueAsFloat64(0);
			metric->m_Factor = exp(log(metric->m_Factor) * power);

			UnitMetric::BaseUnitsIterator
				b1 = metric->m_BaseUnits.begin(),
				e1 = metric->m_BaseUnits.end();
			for (; b1 != e1; ++b1)
				(*b1).second = Int32((*b1).second * power);
			result->SetMetric(metric.release());
		}
		return true;
	}
};


// *****************************************************************************
// UnitSqrtOperator
// *****************************************************************************

template <class T>
class UnitSqrtOperator : public UnaryOperator
{
	typedef Unit<typename sqrt_func_checked<T>::res_type> ResultType;
	typedef Unit<T>                                       Arg1Type;

public:
	UnitSqrtOperator(AbstrOperGroup* gr)
		:	UnaryOperator(gr,
				ResultType::GetStaticClass(),
				Arg1Type  ::GetStaticClass())
	{}

	// Override Operator
	bool CreateResult(TreeItemDualRef& resultHolder, const ArgSeqType& args, bool mustCalc) const override
	{
		dms_assert(args.size() >= 1);

		const Arg1Type *arg1 = debug_cast<const Arg1Type*>(args[0]);

		dms_assert(arg1);

		AbstrUnit* result = ResultType::GetStaticClass()->CreateTmpUnit(resultHolder.GetNew());
		resultHolder = result;

		const UnitMetric* arg1SI = arg1->GetMetric();
		if (IsEmpty(arg1SI))
			result->SetMetric(nullptr);
		else
		{
			auto metric = std::make_unique<UnitMetric>(*arg1SI);

			metric->m_Factor = sqrt(metric->m_Factor);

			UnitMetric::BaseUnitsIterator 
				b1 = metric->m_BaseUnits.begin(), 
				e1 = metric->m_BaseUnits.end();
			while (b1 != e1)
			{
				if ((*b1).second % 2 != 0)
					GetGroup()->throwOperErrorF("argument %s has a non-square metric", arg1->GetFullName().c_str());
				(*b1++).second /= 2;
			}
			result->SetMetric(metric.release());
		}
		return true;
	}
};

#endif //!defined(__OPERUNIT_H)
