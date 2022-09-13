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

#ifndef __RTC_GEO_RNDPOINT_H
#define __RTC_GEO_RNDPOINT_H

#include "geo/SelectPoint.h"

//----------------------------------------------------------------------
// Section      : wrapper around boost::random
//----------------------------------------------------------------------

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>

typedef boost::mt19937               rnd_engine_type;
typedef rnd_engine_type::result_type rnd_seed_type;

//----------------------------------------------------------------------
// Section      : use PointSelector at random
//----------------------------------------------------------------------

template<typename PointType>
struct RndPointGenerator
{
	RndPointGenerator(SA_ConstReference<PointType> poly, rnd_seed_type seed)
		:	m_Polygon(poly)
		,	m_RndEngine(seed)
	{}

	PointType operator () const
	{
		return AreaPercentile<ScalarType, m_Polygon.begin(), m_Polygon.end(), m_RndEngine(), m_RndEngine());
	}
private:
	SA_ConstReference<PointType> m_Polygon;
	mutable rnd_engine_type      m_RndEngine;
};

#endif // __RTC_GEO_RNDPOINT_H


