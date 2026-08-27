// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#if !defined(__TIC_TILEACCESS_H)
#define __TIC_TILEACCESS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DataArray.h"
#include "DataLocks.h"
#include "TileLock.h"
#include "ser/FormattedStream.h"

struct AbstrReadableTileData {
	virtual void WriteFormattedValue(FormattedOutStream& out, SizeT index) const = 0;
	virtual Float64 GetAsFloat64(SizeT index) const = 0;
};

template<typename V>
struct ReadableTileData :AbstrReadableTileData
{
	using cseq_t = typename DataArrayBase<V>::locked_cseq_t;
	using value_type = typename DataArrayBase<V>::value_type;

	cseq_t m_CSeq;

	ReadableTileData(cseq_t cseq)
		:	m_CSeq(std::move(cseq))
	{}

	value_type GetValue(SizeT index) const {
		dms_assert(index < m_CSeq.size());
		return Convert<value_type>(m_CSeq[index]);
	}

	void WriteFormattedValue(FormattedOutStream& out, SizeT index) const override
	{
		dms_assert(index < m_CSeq.size());
		::WriteDataString(out, m_CSeq[index]);
	}
	Float64 GetAsFloat64(SizeT index) const override 
	{
		if constexpr (is_numeric_v<V> || is_bitvalue_v<V>)
			return Convert<Float64>(m_CSeq[index]);
		else
			throwIllegalAbstract(MG_POS, "ReadableTileData::GetAsFloat64");
	}
};

#endif //!defined(__TIC_TILEACCESS_H)

