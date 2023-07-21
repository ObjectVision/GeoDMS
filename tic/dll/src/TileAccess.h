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
	typedef typename DataArrayBase<V>::locked_cseq_t cseq_t;
	typedef typename DataArrayBase<V>::value_type value_type;

	cseq_t m_CSeq;

	ReadableTileData(cseq_t cseq)
		:	m_CSeq(std::move(cseq))
	{}

	value_type GetValue(SizeT index) const {
		dms_assert(index < m_CSeq.size());
		return Convert<value_type>(m_CSeq[index]);
//		return m_CSeq[index];
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

