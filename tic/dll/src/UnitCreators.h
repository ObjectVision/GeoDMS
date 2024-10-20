// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_UNITCREATORS_H)
#define __TIC_UNITCREATORS_H

#include "TicBase.h"

#include "RtcGeneratedVersion.h"
#include "utl/mySPrintF.h"

#include "AbstrDataItem.h"
#include "OperGroups.h"
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
				if constexpr (DMS_VERSION_MAJOR >= 16)
					throwDmsErrD(diagnostic.c_str());
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

TIC_CALL ConstUnitRef inv_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args);


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
	assert(args.size() >=1 && IsDataItem(args[0])); // PRECONDITION
	return AsDataItem(args[0])->GetAbstrValuesUnit();
}

inline ConstUnitRef arg1_values_unit(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return arg1_values_unit(args);
}

TIC_CALL ConstUnitRef compatible_values_unit_creator_func(UInt32 nrSkippedArgs, const AbstrOperGroup* gr, const ArgSeqType& args, bool mustCheckCategories);

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

TIC_CALL ConstUnitRef count_unit_creator(const AbstrDataItem* adi);
TIC_CALL ConstUnitRef unique_count_unit_creator(const AbstrDataItem* adi, const AbstrDataItem* groupBy_rel);

inline ConstUnitRef count_unit_creator(const ArgSeqType& args)
{
	assert(args.size() >= 1 && IsDataItem(args[0])); // PRECONDITION
	return count_unit_creator(AsDataItem(args[0]));
}

inline ConstUnitRef count_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return count_unit_creator(args);
}


TIC_CALL ConstUnitRef CastUnit(const UnitClass* uc, ConstUnitRef v);
TIC_CALL ConstUnitRef CastUnit(const UnitClass* uc, const ArgSeqType& args);

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


#endif // !defined(__TIC_UNITCREATORS_H)