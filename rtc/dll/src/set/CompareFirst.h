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
#if !defined(__SET_COMPAREFIRST_H)
#define __SET_COMPAREFIRST_H

#include <functional>
#include "set/DataCompare.h"

template <typename Key, typename KeyValuePair, typename Comp = DataCompare<Key> >
struct comp_first
{
	typedef Comp         key_compare;
	typedef KeyValuePair value_type;

	comp_first(const key_compare& keyCompare = key_compare()) : m_KeyCompare(keyCompare) {}

	bool operator()(const value_type& a, const value_type& b) { return m_KeyCompare(a.first, b.first); }

	bool operator()(const value_type& a, const Key& b) { return m_KeyCompare(a.first, b); }

	bool operator()(const Key& a, const value_type& b) { return m_KeyCompare(a, b.first); }

	key_compare m_KeyCompare;
};


#endif // __SET_COMPAREFIRST_H
