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

#if !defined(__CLC_UNITCREATORS_H)
#define __CLC_UNITCREATORS_H

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

CLC1_CALL ConstUnitRef inv_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args);


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

CLC1_CALL ConstUnitRef compatible_values_unit_creator_func(UInt32 nrSkippedArgs, const AbstrOperGroup* gr, const ArgSeqType& args);

inline ConstUnitRef compatible_values_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return compatible_values_unit_creator_func(0, gr, args);
}

inline ConstUnitRef boolean_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	return default_unit_creator<Bool>(gr, args);
};

inline ConstUnitRef compare_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	compatible_values_unit_creator_func(0, gr, args);
	return boolean_unit_creator(gr, args);
};

inline ConstUnitRef domain_unit_creator(const AbstrOperGroup* gr, const ArgSeqType& args)
{
	dms_assert(args.size() >= 1 && IsDataItem(args[0])); // PRECONDITION
	return AsDataItem(args[0])->GetAbstrDomainUnit();
}

CLC1_CALL ConstUnitRef count_unit_creator(const AbstrDataItem* adi);

inline ConstUnitRef count_unit_creator(const ArgSeqType& args)
{
	dms_assert(args.size() >= 1 && IsDataItem(args[0])); // PRECONDITION
	return count_unit_creator(AsDataItem(args[0]));
}


CLC1_CALL ConstUnitRef CastUnit(const UnitClass* uc, ConstUnitRef v);
CLC1_CALL ConstUnitRef CastUnit(const UnitClass* uc, const ArgSeqType& args);

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