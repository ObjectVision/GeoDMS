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

#if !defined(__TIC_TILEDUNIT_H)
#define __TIC_TILEDUNIT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------
#include "Unit.h"

// *****************************************************************************
// MakeTiledUnitFromSize
// *****************************************************************************

const int X_GRANULARITY = 1 << 8; //256
const int Y_GRANULARITY = 1 << 8; //256
const int D32_GRANULARITY = 1 << 14; //16384
const int D64_GRANULARITY = 1 << 14; //16384
const int MAX_NR_TILES    = 1 << 16; //65536

template <typename T>
inline Point<T> GetGranularity(const Point<T>*) { return shp2dms_order<T>(X_GRANULARITY, Y_GRANULARITY); }
inline UInt32 GetGranularity(const Int32* ) { return D32_GRANULARITY; }
inline UInt32 GetGranularity(const UInt32*) { return D32_GRANULARITY; }
inline UInt64 GetGranularity(const UInt64*) { return D64_GRANULARITY; }


TIC_CALL void CheckNrTiles(SizeT nrTiles);

template <typename D>
void Inc(D& p, const Range<D>& r) 
{ 
	++p; 
}

template <typename F>
void Inc(Point<F>& p, const Range< Point<F> >& r)
{
	if (++p.Col() == r.second.Col())
	{
		++p.Row();
		p.Col() = r.first.Col();
	}
}

template <typename D>
tile_id CeilDivide(D numer, tile_offset denom)
{
	dms_assert(numer > 0);
	dms_assert(denom > 0);

	D quot = numer / denom;
	if (quot * denom < numer)
		++quot;
	return quot;
}

template <typename D>
UInt16 CeilDivide(D numer, UInt16 denom)
{
	dms_assert(numer > 0);
	dms_assert(denom > 0);

	D quot = numer / denom;
	if (quot * denom < numer)
		++quot;
	return quot;
}

template <typename D>
WPoint CeilDivide(Point<D> numer, WPoint denom)
{
	return Point<D>(
		CeilDivide(numer.first, denom.first), 
		CeilDivide(numer.second, denom.second)
	);
}


#endif // __TIC_TILEDUNIT_H
