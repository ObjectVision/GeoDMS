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

#if !defined(__TIC_UNITCLASSREG_H)
#define __TIC_UNITCLASSREG_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "mci/PropDef.h"
#include "mci/PropDefEnums.h"
#include "Unit.h"

//----------------------------------------------------------------------
// class  : RangeProp
//----------------------------------------------------------------------

template <class T>
struct RangeProp : PropDef<Unit<T>, typename Unit<T>::range_t >
{
	using typename RangeProp::ApiType;
	using typename RangeProp::ParamType;

	typedef          Unit<T>         unit_t;
	typedef typename unit_t::range_t range_t;

	RangeProp(bool isCategorical)
		: PropDef<unit_t, range_t>(isCategorical ? "cat_range" : "range"
			, set_mode::optional, xml_mode::element, cpy_mode::none, chg_mode::invalidate
			, false, false, false)
		,	m_IsCategorical(isCategorical)
	{}

	// override base class
	ApiType GetValue(const unit_t* u) const override
	{
		SharedUnitInterestPtr holder(u);
		if (u->GetTSF(TSF_Categorical) == m_IsCategorical)
			if (IsDataReady(u->GetCurrRangeItem()))
				return u->GetRange();
		return ApiType{};
	}
	void SetValue(unit_t* u, ParamType val) override
	{
		u->SetRange(val);
		u->SetTSF(USF_HasConfigRange);
	}
	bool HasNonDefaultValue(const Object* self) const
	{
		const unit_t* u = debug_cast<const unit_t*>(self);
		if constexpr (has_var_range_field_v<T>)
		{
			if (u->GetTSF(USF_HasConfigRange))
				return u->GetTSF(TSF_Categorical) == m_IsCategorical;
		}
		return false;
	}

	bool m_IsCategorical;
};

#endif // __TIC_UNITCLASSREG_H
