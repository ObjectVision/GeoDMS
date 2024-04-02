// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "UnitCreators.h"

#include "mci/ValueClass.h"
#include "mci/ValueClassID.h"
#include "utl/mySPrintF.h"

#include "Metric.h"
#include "Projection.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
// shared OperGroups
// *****************************************************************************

CommonOperGroup
	cog_mul("mul"), 
	cog_div("div"),
	cog_add("add"), // used for addition and string concatenation
	cog_sub("sub"),
	cog_bitand("bitand"),
	cog_bitor ("bitor"),
	cog_bitxor("bitxor"),
	cog_pow("pow"),
	cog_eq("eq"),
	cog_ne("ne"),
	cog_substr("substr");

// *****************************************************************************

ConstUnitRef CastUnit(const UnitClass* uc, ConstUnitRef v)
{
	if (v->IsKindOf(uc))
		return v;

	ConstUnitRef u = uc->CreateDefault();

	if (!v->UnifyValues(u, "", "", UM_AllowTypeDiff))
	{
		AbstrUnit* newArg2 = uc->CreateTmpUnit(nullptr);
		u = newArg2;

		newArg2->DuplFrom(v);
	}
	dms_assert(u);
	return u;
}

ConstUnitRef CastUnit(const UnitClass* uc, const ArgSeqType& args)
{
	dms_assert(args.size() >= 1);
	return CastUnit(uc, AsDataItem(args[0])->GetAbstrValuesUnit());
}


// *****************************************************************************


ConstUnitRef count_unit_creator(const AbstrDataItem* adi)
{
	const AbstrUnit* adu = adi->GetAbstrDomainUnit(); // Partition Domain
	assert(adu);

	const ValueClass* vc = adu->GetValueType(); assert(vc);
	const ValueClass* vcCrd = vc->GetCrdClass(); assert(vcCrd);
	auto uc = UnitClass::Find(vcCrd); assert(uc);
	return uc->CreateDefault();
}

ConstUnitRef unique_count_unit_creator(const AbstrDataItem* adi, const AbstrDataItem* groupBy_rel)
{
	const AbstrUnit* adu = adi->GetAbstrDomainUnit(); // Partition Domain
	assert(adu);

	const ValueClass* dvc = adu->GetValueType(); assert(dvc);
	const ValueClass* vcCrd = dvc->GetCrdClass(); assert(vcCrd);

	if (adi->GetValueComposition() == ValueComposition::Single)
	{
		auto vvc = adi->GetAbstrValuesUnit()->GetValueType();
		if (vvc->GetValueClassID() != ValueClassID::VT_SharedStr)
		{
			const ValueClass* vvcCrd = vvc->GetCrdClass(); assert(vvcCrd);
			if (vvcCrd->GetBitSize() < vcCrd->GetBitSize())
				vcCrd = vvcCrd;

		}
	}
	if (groupBy_rel->GetValueComposition() == ValueComposition::Single)
	{
		auto vvc = groupBy_rel->GetAbstrValuesUnit()->GetValueType();
		if (vvc->GetValueClassID() != ValueClassID::VT_SharedStr)
		{
			const ValueClass* vvcCrd = vvc->GetCrdClass(); assert(vvcCrd);
			if (vvcCrd->GetBitSize() < vcCrd->GetBitSize())
				vcCrd = vvcCrd;

		}
	}
	auto uc = UnitClass::Find(vcCrd); assert(uc);
	return uc->CreateDefault();
}

// *****************************************************************************

ConstUnitRef inv_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size() >= 1);
	ArgSeqType tmpArgs; tmpArgs.reserve(2);
	tmpArgs.push_back(Unit<Float32>::GetStaticClass()->CreateDefault());
	tmpArgs.push_back(args[0]);
	return operated_unit_creator(&cog_div, tmpArgs);
}

// *****************************************************************************

[[noreturn]] void throwCompatibleError(
	const AbstrOperGroup* gr, 
	UInt32 nrSkippedArgs, 
	UInt32 i,
	CharPtr aspectStr,
	CharPtr val1,
	CharPtr val2
)
{
	gr->throwOperErrorF("Arguments must have compatible units, but arg%d has %s %s and arg%d has %s %s"
	,	nrSkippedArgs+1, aspectStr, val1
	,	i+1,             aspectStr, val2
	);
}

ConstUnitRef compatible_values_unit_creator_func(arg_index nrSkippedArgs, const AbstrOperGroup* gr, const ArgSeqType& args, bool mustCheckCategories)
{
	arg_index nrArgs = args.size();
	assert(nrSkippedArgs < nrArgs); // PRECONDITION

	const AbstrUnit* arg1_ValuesUnit = AsDataItem(args[nrSkippedArgs])->GetAbstrValuesUnit(); // the first considered argument
	assert(arg1_ValuesUnit);
	for (arg_index i = nrSkippedArgs + 1; i < nrArgs; ++i)
	{
		if (!arg1_ValuesUnit->IsDefaultUnit())
			break;
		arg1_ValuesUnit = AsDataItem(args[i])->GetAbstrValuesUnit(); // the first considered argument
	}

	const UnitMetric* a1MetricPtr = arg1_ValuesUnit->GetMetric();
	const UnitProjection* a1ProjectionPtr = arg1_ValuesUnit->GetProjection();
	assert(IsEmpty(a1MetricPtr) || !a1ProjectionPtr); // this code assumes units never have both a metric and a projection

	const AbstrUnit* catUnit = nullptr;
	arg_index cat_unit_index = nrSkippedArgs;
	if (mustCheckCategories)
	{
		for (; cat_unit_index != nrArgs; ++cat_unit_index)
		{
			auto adi = AsDataItem(args[cat_unit_index]);
			auto avu = AbstrValuesUnit(adi);
			if (avu && adi->GetTSF(TSF_Categorical))
			{
				catUnit = avu;
				break;
			}
		}
	}

	for (arg_index i = nrSkippedArgs; i != nrArgs; ++i)
	{
		// al other considered arguments
		auto adi = AsDataItem(args[i]);
		const AbstrUnit*currArg_ValuesUnit = adi->GetAbstrValuesUnit();
		assert(currArg_ValuesUnit);

		if (currArg_ValuesUnit != arg1_ValuesUnit && arg1_ValuesUnit->GetValueType() != currArg_ValuesUnit->GetValueType())
			throwCompatibleError(gr, nrSkippedArgs, i, "ValueType", arg1_ValuesUnit->GetValueType()->GetName().c_str(), currArg_ValuesUnit->GetValueType()->GetName().c_str());

		if (catUnit && currArg_ValuesUnit != catUnit && !catUnit->UnifyDomain(currArg_ValuesUnit, "", "", UM_AllowDefaultRight))
		{
			auto leftRole = mySSPrintF("Values of argument %d", cat_unit_index + 1);
			auto rightRole = mySSPrintF("Values of argument %d", i + 1);
			catUnit->UnifyDomain(currArg_ValuesUnit, leftRole.c_str(), rightRole.c_str(), UnifyMode(UM_AllowDefaultRight|UM_Throw));
		}

		const UnitMetric* currArg_MetricPtr = currArg_ValuesUnit->GetMetric();
		if (!IsEmpty(a1MetricPtr))
		{
			if ((!IsEmpty(currArg_MetricPtr)) && !(*a1MetricPtr == *currArg_MetricPtr))
				throwCompatibleError(gr, nrSkippedArgs, i, "Metric", arg1_ValuesUnit->GetMetricStr(FormattingFlags::ThousandSeparator).c_str(), currArg_ValuesUnit->GetMetricStr(FormattingFlags::ThousandSeparator).c_str());
		}
		else if (!IsEmpty(currArg_MetricPtr))
		{
			// empty metrics are overruled
			a1MetricPtr     = currArg_MetricPtr;
			arg1_ValuesUnit = currArg_ValuesUnit;
		}

		const UnitProjection* currArg_ProjectionPtr = currArg_ValuesUnit->GetProjection();
		if (a1ProjectionPtr)
		{
			if (currArg_ProjectionPtr && !(*a1ProjectionPtr == *currArg_ProjectionPtr))
				throwCompatibleError(gr, nrSkippedArgs, i, "Projection", arg1_ValuesUnit->GetProjectionStr(FormattingFlags::ThousandSeparator).c_str(), currArg_ValuesUnit->GetProjectionStr(FormattingFlags::ThousandSeparator).c_str());
		}
		else if (currArg_ProjectionPtr)
		{
			// empty projections are overruled
			a1ProjectionPtr = currArg_ProjectionPtr;
			arg1_ValuesUnit            = currArg_ValuesUnit;
		}
		dms_assert(IsEmpty(currArg_MetricPtr) || !currArg_ProjectionPtr); // this code assumes units never have both a metric and a projection
	}
	MG_CHECK(!catUnit || arg1_ValuesUnit && catUnit->UnifyDomain(arg1_ValuesUnit, "", "", UM_AllowDefaultRight));

	return arg1_ValuesUnit;
}
