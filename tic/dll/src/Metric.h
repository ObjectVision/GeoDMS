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

#ifndef __TIC_METRIC_H
#define __TIC_METRIC_H

#include "set/VectorMap.h"
enum class FormattingFlags;

// *****************************************************************************
// UnitMetric, used to express a unit as term of base unit power factors and one numeric factor
// *****************************************************************************

struct UnitMetric : SharedBase
{
	using BaseUnitsMapType = vector_map<SharedStr, Int32>;
	using BaseUnitsIterator = BaseUnitsMapType::iterator;


	void Release() const { if (!DecRef()) delete this;	}

	TIC_CALL UnitMetric() : m_Factor(1.0) {}

	TIC_CALL SharedStr AsString(FormattingFlags ff) const;

	TIC_CALL void SetProduct (const UnitMetric* arg1SI, const UnitMetric* arg2SI);
	TIC_CALL void SetQuotient(const UnitMetric* arg1SI, const UnitMetric* arg2SI);
	TIC_CALL bool IsNumeric() const;

	TIC_CALL bool operator ==(const UnitMetric& rhs) const;
	bool operator !=(const UnitMetric& rhs) const { return !(*this == rhs); }

	bool Empty() const 
	{
		return m_Factor == 1.0 && m_BaseUnits.empty();
	}

	Float64          m_Factor;
	BaseUnitsMapType m_BaseUnits;

	friend bool IsEmpty(const UnitMetric* self) { return !self || self->Empty(); }
};

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitMetric& repr);

#endif // __TIC_METRIC_H