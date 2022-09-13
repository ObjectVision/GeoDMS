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

#if !defined(__CLC_MAKE_CULULATIVE_H)
#define __CLC_MAKE_CULULATIVE_H

// C_i := C_(i-1) + V_i = SUM(j=0..i, V_j)

template <typename IterAddable>
void make_cumulative(IterAddable b, IterAddable e)
{
	typedef typename std::iterator_traits<IterAddable>::value_type value_type;
	if (b == e)
		return;
	value_type accumulator = *b;
	while (++b != e)
	{
		accumulator += *b;
		*b = accumulator;
	}
}

// C_i := C_(i-1) + V_(i-1) = SUM(j=0..i-1, V_j)

template <typename IterAddable>
void make_cumulative_base(IterAddable b, IterAddable e)
{
	typedef typename std::iterator_traits<IterAddable>::value_type value_type;
	if (b == e)
		return;
	value_type accumulation_so_far = value_type();
	while (b != e)
	{
		value_type accumulator = accumulation_so_far + *b;
		*b++ = accumulation_so_far;
		accumulation_so_far = accumulator;
	}
}


#endif //!defined(__CLC_MAKE_CULULATIVE_H)
