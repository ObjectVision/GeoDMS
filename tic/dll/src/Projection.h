// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
	const AbstrUnit* GetBaseUnit() const { assert(m_BaseUnit); return m_BaseUnit; }

	TIC_CALL bool operator ==(const UnitProjection& rhs) const;
	bool operator !=(const UnitProjection& rhs) const { return !operator ==(rhs); }

	static TIC_CALL CrdTransformation GetCompositeTransform(const UnitProjection* curr);
	TIC_CALL const AbstrUnit* GetCompositeBase() const;
	
private:
	SharedPtr<const AbstrUnit> m_BaseUnit;
};

FormattedOutStream& operator <<(FormattedOutStream& str, const UnitProjection& repr);


#endif // __TIC_PROJECTION_H