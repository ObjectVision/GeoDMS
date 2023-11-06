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
#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mth/Mathlib.h"

#include "dbg/Check.h"
#include "geo/BaseBounds.h"

#include <math.h>

/******************************************************************************/
//                         Analytical Functions
/******************************************************************************/

/* NYI: 


	LCM should be better protected against numeric overflow
	all three should be better protected for negative arguments become more generic and specialized for unsigned arguments

RTC_CALL long Modulo(const long  a, const long b)
{
	return ((a % b) + b) % b;
}


RTC_CALL unsigned long GCD(const long a, const long b) // Greatest Common Divisor
{
   if (a == 0 || b == 0)
      return 0;

   unsigned long  high  = max(Abs(a), Abs(b)),
						low   = min(Abs(a), Abs(b)),
						mod;

	while ((mod = Modulo(high, low)) != 0)
	{
		high	= low;
		low	= mod;
	}

	return low;
} // GCD

RTC_CALL unsigned long  LCM(const long a, const long b) // Least Common Multiple
{
	unsigned long	gcd = GCD(a, b);
	return gcd == 0 ? 0 : (a * b) / gcd;
} // LCM

NYI */

