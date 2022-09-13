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

#ifndef __TIC_PROJECTION_H
#define __TIC_PROJECTION_H

#include "ptr/PersistentSharedObj.h"
#include "geo/Transform.h"

// *****************************************************************************
// UnitProjection, used to express coordinate units as (x,y) scale + offset 
// in a base coordinate system, thus m_Factor*p + m_Offset transforms grid to world coords
// *****************************************************************************

struct UnitProjection : PersistentSharedObj, CrdTransformation
{
private:
	typedef PersistentSharedObj base_type;
	UnitProjection();

public:
	TIC_CALL UnitProjection(const AbstrUnit* unit);
	TIC_CALL UnitProjection(const AbstrUnit* unit, DPoint offset, DPoint factor);
	TIC_CALL UnitProjection(const UnitProjection& src);
	TIC_CALL UnitProjection(const AbstrUnit* unit, const CrdTransformation& tr);
	TIC_CALL ~UnitProjection();

	TIC_CALL SharedStr AsString(FormattingFlags ff) const;
	const AbstrUnit* GetBaseUnit() const { dms_assert(m_BaseUnit); return m_BaseUnit; }

	TIC_CALL bool operator ==(const UnitProjection& rhs) const;
	bool operator !=(const UnitProjection& rhs) const { return !operator ==(rhs); }

	static TIC_CALL CrdTransformation GetCompositeTransform(const UnitProjection* curr);
	TIC_CALL const AbstrUnit* GetCompositeBase() const;
	
private:
	SharedPtr<const AbstrUnit> m_BaseUnit;
};

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitProjection& repr);


#endif // __TIC_PROJECTION_H