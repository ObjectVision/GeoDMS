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

#if !defined(__RTC_DBG_TIMER_H)
#define __RTC_DBG_TIMER_H

#include <time.h>
#include <atomic>

#include "geo/MinMax.h"

struct Timer
{
	bool PassedSecs(time_t nrSecs)
	{
		time_t curr_time = time(nullptr);
//		time_t delta_time = curr_time - begin_time;
//		MakeMax(nrSecs, delta_time / 5);
		MakeMin<time_t>(nrSecs, 600);

		if (curr_time < last_time + nrSecs)
			return false;

		// avoid double ticketing
		time_t old_time = last_time.exchange(curr_time);

		return curr_time >= old_time + nrSecs;
	}


//	time_t begin_time;
	std::atomic<time_t> last_time = time(nullptr);
};

#endif // __RTC_DBG_TIMER_H
