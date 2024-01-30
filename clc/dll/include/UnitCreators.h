// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__CLC_UNITCREATORS_H)
#define __CLC_UNITCREATORS_H

#include "RtcGeneratedVersion.h"
#include "utl/mySPrintF.h"
#include "ClcBase.h"

#include "AbstrDataItem.h"
#include "Unit.h"
#include "UnitClass.h"

// *****************************************************************************
//									UNIT CREATORS
// *****************************************************************************

using UnitCreatorPtr = ConstUnitRef(*)(const AbstrOperGroup* gr, const ArgSeqType& args);

// *****************************************************************************
//									UNIT CREATORS
// *****************************************************************************

inline ConstUnitRef operated_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return gr->CreateValuesUnit(args);
}

template<typename T>
inline ConstUnitRef default_unit_creator()
{
	return Unit<field_of_t<T>>::GetStaticClass()->CreateDefault();  // metric = unitary.
}

template<typename T>
inline ConstUnitRef default_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return default_unit_creator<T>();
}

template<typename T>
inline ConstUnitRef default_unit_creator_and_check_input(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	// check input
	for (const auto& arg : args)
	{
		if (IsDataItem(arg))
		{
			auto metric = AsDataItem(arg)->GetAbstrValuesUnit()->GetMetric();
			if (metric && !metric->Empty())
			{
				auto diagnostic = mySSPrintF("value(s) of operator %s have metric %s but are expected to be without metric"
					, gr->GetName()
					, metric->AsString(FormattingFlags::ThousandSeparator)
				);
				if constexpr (DMS_VERSION_MAJOR >= 15)
					throwDmsErrD(diagnostic);
				else
					reportD(SeverityTypeID::ST_Warning, diagnostic.c_str());
			}
		}
	}

	return default_unit_creator<T>();
}

inline ConstUnitRef mul2_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size()== 2);
	return operated_unit_creator(&cog_mul, args);
}

inline ConstUnitRef mulx_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size() > 2);
	ArgSeqType tmpArgs; tmpArgs.reserve(2);
	tmpArgs.push_back(args[0]);
	tmpArgs.push_back(args[1]);
	return operated_unit_creator(&cog_mul, tmpArgs);
}

inline ConstUnitRef div_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size()== 2);
	return operated_unit_creator(&cog_div, args);
}

CLC_CALL ConstUnitRef inv_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args);


inline ConstUnitRef square_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size() >= 1); // ook gebruikt voor var_partial
	ArgSeqType tmpArgs; tmpArgs.reserve(2);
	tmpArgs.push_back(args[0]);
	tmpArgs.push_back(args[0]);

	return mul2_unit_creator(gr, tmpArgs);
}


inline ConstUnitRef arg1_values_unit(const ArgSeqType& args)
{
	dms_assert(args.size() >=1 && IsDataItem(args[0])); // PRECONDITION
	return AsDataItem(args[0])->GetAbstrValuesUnit();
}

inline ConstUnitRef arg1_values_unit(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return arg1_values_unit(args);
}

CLC_CALL ConstUnitRef compatible_values_unit_creator_func(UInt32 nrSkippedArgs, const AbstrOperGroup* gr, const ArgSeqType& args, bool mustCheckCategories);

inline ConstUnitRef compatible_values_or_categories_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return compatible_values_unit_creator_func(0, gr, args, true);
}

inline ConstUnitRef compatible_simple_values_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return compatible_values_unit_creator_func(0, gr, args, false);
}

inline ConstUnitRef boolean_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return default_unit_creator<Bool>(gr, args);
};

inline ConstUnitRef compare_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args, bool mustCheckCategories)
{
	compatible_values_unit_creator_func(0, gr, args, mustCheckCategories);
	return boolean_unit_creator(gr, args);
};

inline ConstUnitRef domain_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	assert(args.size() >= 1 && IsDataItem(args[0])); // PRECONDITION
	return AsDataItem(args[0])->GetAbstrDomainUnit();
}

CLC_CALL ConstUnitRef count_unit_creator(const AbstrDataItem* adi);

inline ConstUnitRef count_unit_creator(const ArgSeqType& args)
{
	assert(args.size() >= 1 && IsDataItem(args[0])); // PRECONDITION
	return count_unit_creator(AsDataItem(args[0]));
}


CLC_CALL ConstUnitRef CastUnit(const UnitClass* uc, ConstUnitRef v);
CLC_CALL ConstUnitRef CastUnit(const UnitClass* uc, const ArgSeqType& args);

template <typename Field>
inline ConstUnitRef cast_unit_creator(const ArgSeqType& args)
{
	return CastUnit(Unit<Field>::GetStaticClass(), args);
}

template<typename R>
ConstUnitRef CastUnit(ConstUnitRef v)
{
	return CastUnit(Unit<field_of_t<R>>::GetStaticClass(), v);
}


#endif // !defined(__CLC_UNITCREATORS_H)