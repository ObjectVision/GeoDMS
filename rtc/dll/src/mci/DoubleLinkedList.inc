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

#ifndef __RTC_MCI_DOUBLELINKEDLIST_INC
#define __RTC_MCI_DOUBLELINKEDLIST_INC

#include "mci/DoubleLinkedList.h"
#include "dbg/Check.h"

/***********    composites                           **********/

template <typename BT> 
double_linked_list<BT>::double_linked_list()
	:	m_FirstSub(0)
	,	m_Next(0)
	,	m_Prev(0)
{}

template <typename BT> void
double_linked_list<BT>::AddSub(this_type_ptr subItem)
{
	dms_assert(subItem);

	if (m_FirstSub)
	{
		this_type_ptr oldLast = m_FirstSub->m_Prev;

		oldLast->m_Next    = subItem;
		m_FirstSub->m_Prev = subItem;

		subItem->m_Prev = oldLast; 
	}
	else
	{
		m_FirstSub = subItem;

		subItem->m_Prev = subItem; // last := first
	}
	subItem->m_Next = nullptr; // end-of-list indicator
}

template <typename BT> void
double_linked_list<BT>::DelSub(this_type_ptr subItem)
{
	dms_assert(subItem && subItem->m_Prev);
	dms_assert(m_FirstSub); // subItems was once added, so this must have a firstSub

	if (subItem == m_FirstSub)
	{
		dms_assert(!subItem->m_Prev->m_Next);
		m_FirstSub = subItem->m_Next; 

		if (m_FirstSub) // subItem is not last: let next point to subItem->m_Prev, which is the last item.
			m_FirstSub->m_Prev = subItem->m_Prev;
	}
	else
	{
		dms_assert(subItem->m_Prev->m_Next == subItem); // subItem is not first: let next of previous point to subItem
		subItem->m_Prev->m_Next = subItem->m_Next;

		if (subItem->m_Next)       // subItem is not last: let next point to subItem->m_Prev
			subItem->m_Next->m_Prev = subItem->m_Prev;
		else                       // subItem is last, make previous of first point to previous of subItem
			m_FirstSub->m_Prev = subItem->m_Prev;
	}
}

template <typename BT> void
double_linked_list<BT>::Reorder(this_type_ptr* first, this_type_ptr* last)
{
	m_FirstSub = nullptr;
	for (; first != last; ++first)
		AddSub(*first);
}

#endif // __RTC_MCI_DOUBLELINKEDLIST_INC
