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
#if !defined(__SER_MAPSTREAM_H)
#define __SER_MAPSTREAM_H

#include <map>

template <class S, class T1, class T2, class T3>
S& operator >> (S& ar, std::map<T1, T2, T3>& m)
{
	typedef  std::map<T1, T2, T3>::value_type    value_type;
	typename std::map<T1, T2, T3>::key_type      k;
	typename std::map<T1, T2, T3>::referent_type r;

	typename std::map<T1, T2, T3>::size_type len;
	ar >> len;
	m.clear();
	for(int i=0; i<len; i++)
	{
		ar >> k >> r;
		m.insert(value_type(k, r));
	}
	return ar;
}

template <class S, class T1, class T2, class T3>
S& operator << (S& ar, const std::map<T1, T2, T3>& m)
{
	typename std::map<T1, T2, T3>::const_iterator 
		first = m.begin(),
		last  = m.end();
	ar << m.size();
	while (first != last)
	{
		ar << (*first).first << (*first).second;
		++first;
	}
	return ar;
}


#endif // __SER_MAPSTREAM_H
