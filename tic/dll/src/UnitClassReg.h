// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__TIC_UNITCLASSREG_H)
#define __TIC_UNITCLASSREG_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/ValueWrap.h"
#include "mci/ValueClass.h"
#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"
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
			if (IsDataReady(u->GetCurrRangeItem()) || u->GetTSF(USF_HasConfigRange))
			{
				SuspendTrigger::FencedBlocker blockThis("RangeProp::GetValue");
				u->PrepareDataUsage(DrlType::Suspendible);
				return u->GetRange();
			}
		return ApiType{};
	}
	void SetValue(unit_t* u, ParamType val) override
	{
		u->SetRange(val);
		u->SetTSF(USF_HasConfigRange);
		u->SetTSF(TSF_Categorical, m_IsCategorical);
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
