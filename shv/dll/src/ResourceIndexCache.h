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

#ifndef __SHV_INDEXCACHE_H
#define __SHV_INDEXCACHE_H

#include "geo/BaseBounds.h"
#include "ptr/WeakPtr.h"

#include <vector>

#include "ShvBase.h"

class Theme;
class AbstrUnit;
class AbstrThemeValueGetter;

//----------------------------------------------------------------------
// struct  : ResourceIndexCache
//----------------------------------------------------------------------

struct ResourceIndexCache
{
	resource_index_t GetKeyIndex(entity_id entityId) const;

protected: 
	ResourceIndexCache(
		const Theme* pixelSizeTheme   // Character Height in #Points (1 Point = (1/72) inch = 0.3527777 mm)
	,	const Theme* worldSizeTheme  // Additional zoom-level dependent factor of Character Height in World Coord Units
	,	Float64 defPixelSize
	,	Float64 defWorldSize
	,	const AbstrUnit* entityDomain);

	bool CompareValueGetter(const AbstrThemeValueGetter* additionalTheme) const;
	bool IsDifferent(Float64 nrPixelsPerWorldUnit, Float64 subPixelFactor) const;

	entity_id m_EntityDomainCount;
	Float64   m_DefaultPixelWidth, m_DefaultWorldWidth;

	WeakPtr<const AbstrThemeValueGetter> m_PixelWidthValueGetter;
	WeakPtr<const AbstrThemeValueGetter> m_WorldWidthValueGetter;

	mutable WeakPtr<const AbstrThemeValueGetter> m_CompatibleTheme;

	mutable	Float64                 m_LastNrPixelsPerWorldUnit;
	mutable	Float64                 m_LastSubPixelFactor;

	mutable std::vector<resource_index_t> m_KeyIndices;
};


#endif // __SHV_INDEXCACHE_H

