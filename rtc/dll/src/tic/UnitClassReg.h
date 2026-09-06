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
	using base_type = PropDef<Unit<T>, typename Unit<T>::range_t>;
	using typename base_type::ApiType;
	using typename base_type::ParamType;

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
			if (IsDataReady(u->GetCurrRangeItem().get()) || u->GetTSF(USF_HasConfigRange))
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
		u->AssignTSF(TSF_Categorical, m_IsCategorical);
	}
	// #1256: a configuration dump writes a property through GetRawValueAsSharedStr, and that
	// dump is read back, so the range must carry no thousand separators. AsString asks for them
	// by default and FormattedOutStream keeps them when the display option is on, which turned a
	// range up to 300000 into "[xy(0; 300,000), xy(280,000; 625,000))" and made the reader stop
	// at the first comma ("PointStream Error: expected ')' but got ','"). Grouping is display
	// formatting: it belongs to GetValueAsSharedStr and to the detail pages (whose Range row has
	// its own formatter, GetStrRange), not to the raw value a configuration is written from.
	SharedStr GetRawValueAsSharedStr(const Object* self) const override
	{
		DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v); // as in the base implementation
		typename ValueWrap<range_t>::value_type propValue = this->GetRawValue(debug_cast<const unit_t*>(self));
		SharedStr result = ::AsString(propValue, FormattingFlags::None);

		// the shared Range formatter ends its rendering with a space (RangeStream.h); a
		// configuration keeps the value as it was written, so drop it here rather than in that
		// operator, which error messages, .mmd dictionaries and detail pages also go through
		CharPtr b = result.begin(), e = result.send();
		while (e != b && e[-1] == ' ')
			--e;
		return (e == result.send()) ? result : SharedStr(CharPtrRange(b, e));
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
