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

#include "ClcPCH.h"
#pragma hdrstop

#include "UnitCreators.h"

#include "mci/ValueClass.h"
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
	cog_add("add"), // used for additioon and string concatenation
	cog_sub("sub"),
	cog_bitand("bitand"),
	cog_bitor ("bitor"),
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
	dms_assert(adu);

	const ValueClass* vc = adu->GetValueType(); dms_assert(vc);
	const ValueClass* vcCrd = vc->GetCrdClass();
	if (vc == vcCrd)
		return adu;
	return UnitClass::Find(vcCrd)->CreateDefault();
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
	assert(args.size() - 2 >= nrSkippedArgs); // PRECONDITION
	const AbstrUnit* arg1 = AsDataItem(args[nrSkippedArgs])->GetAbstrValuesUnit(); // the first considered argument
	dms_assert(arg1);
	const UnitMetric* a1MetricPtr = arg1->GetMetric();
	const UnitProjection* a1ProjectionPtr = arg1->GetProjection();
	dms_assert(IsEmpty(a1MetricPtr) || !a1ProjectionPtr); // this code assumes units never have both a metric and a projection

	const AbstrUnit* catUnit = nullptr;
	arg_index cat_unit_index = nrSkippedArgs;
	if (mustCheckCategories)
	{
		for (; cat_unit_index != args.size(); ++cat_unit_index)
			if (AsDataItem(args[cat_unit_index])->GetTSF(DSF_Categorical))
			{
				catUnit = AsDataItem(args[cat_unit_index])->GetAbstrValuesUnit();
				break;
			}
	}

	for (arg_index i = nrSkippedArgs + 1; i != args.size(); ++i)
	{
		// al other considered arguments
		const AbstrUnit*arg2 = AsDataItem(args[i])->GetAbstrValuesUnit();
		dms_assert(arg2);

		if (arg1->GetValueType() != arg2->GetValueType())
			throwCompatibleError(gr, nrSkippedArgs, i, "ValueType", arg1->GetValueType()->GetName().c_str(), arg2->GetValueType()->GetName().c_str());

		if (catUnit && !catUnit->UnifyDomain(arg2, "", "", UnifyMode(UM_AllowDefaultRight)))
		{
			auto leftRole = mySSPrintF("Values of argument %d", cat_unit_index + 1);
			auto rightRole = mySSPrintF("Values of argument %d", i + 1);
			catUnit->UnifyDomain(arg2, leftRole.c_str(), rightRole.c_str(), UnifyMode(UM_AllowDefaultRight | UM_Throw));
		}

		const UnitMetric* a2MetricPtr = arg2->GetMetric();
		if (!IsEmpty(a1MetricPtr))
		{
			if ((!IsEmpty(a2MetricPtr)) && !(*a1MetricPtr == *a2MetricPtr))
				throwCompatibleError(gr, nrSkippedArgs, i, "Metric", arg1->GetMetricStr(FormattingFlags::ThousandSeparator).c_str(), arg2->GetMetricStr(FormattingFlags::ThousandSeparator).c_str());
		}
		else if (!IsEmpty(a2MetricPtr))
		{
			// empty metrics are overruled
			a1MetricPtr = a2MetricPtr;
			arg1        = arg2;
		}

		const UnitProjection* a2ProjectionPtr = arg2->GetProjection();
		if (a1ProjectionPtr)
		{
			if (a2ProjectionPtr && !(*a1ProjectionPtr == *a2ProjectionPtr))
				throwCompatibleError(gr, nrSkippedArgs, i, "Projection", arg1->GetProjectionStr(FormattingFlags::ThousandSeparator).c_str(), arg2->GetProjectionStr(FormattingFlags::ThousandSeparator).c_str());
		}
		else if (a2ProjectionPtr)
		{
			// empty projections are overruled
			a1ProjectionPtr = a2ProjectionPtr;
			arg1            = arg2;
		}
		dms_assert(IsEmpty(a2MetricPtr) || !a2ProjectionPtr); // this code assumes units never have both a metric and a projection
	}
	return arg1;
}
