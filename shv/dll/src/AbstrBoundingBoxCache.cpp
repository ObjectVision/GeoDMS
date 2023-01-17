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
#include "ShvDllPch.h"

#include "BoundingBoxCache.h"

#include "set/QuickContainers.h"
#include "LockLevels.h"

//----------------------------------------------------------------------
// class  : AbstrBoundingBoxCache
//----------------------------------------------------------------------

std::map<const AbstrDataObject*, const AbstrBoundingBoxCache*> g_BB_Register;
leveled_critical_section cs_BB(item_level_type(0), ord_level_type::BoundingBoxCache1, "BoundingBoxCache");

AbstrBoundingBoxCache::AbstrBoundingBoxCache(const AbstrDataObject* featureData)
	:	m_FeatureData(featureData)
{}

AbstrBoundingBoxCache::~AbstrBoundingBoxCache()
{
	if (m_HasBeenRegistered)
	{
		leveled_critical_section::scoped_lock lockBB_register(cs_BB);
		g_BB_Register.erase(m_FeatureData);
	}
}

void AbstrBoundingBoxCache::GlobalRegister(const AbstrDataObject* featureData)
{
	assert(cs_BB.isLocked()); // logic requires that the cs has been locked by the caller as part of the construction process.
	const AbstrBoundingBoxCache*& bbPtr = g_BB_Register[featureData];
	assert(bbPtr == nullptr);
	bbPtr = this;
	m_HasBeenRegistered = true;
}


DRect AbstrBoundingBoxCache::GetBounds(SizeT featureID) const
{
	throwIllegalAbstract(MG_POS, "AbstrBoundingBoxCache::GetBounds");
}

