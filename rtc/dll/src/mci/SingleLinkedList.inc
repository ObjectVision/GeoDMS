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

#ifndef __RTC_MCI_SINGLELINKEDLIST_INC
#define __RTC_MCI_SINGLELINKEDLIST_INC

#include "mci/DoubleLinkedList.h"
#include "dbg/Check.h"

/***********    composites                           **********/

template <typename BT> void
single_linked_list<BT>::AddSub(this_type_ptr subItem)
{
	dms_assert(subItem);
	this_type_ptr* subPtrPtr = &m_FirstSub;
	while (*subPtrPtr) subPtrPtr = &((*subPtrPtr)->m_Next);
	*subPtrPtr = subItem;
	subItem->m_Next = nullptr; // re-install end-of-list indicator
}

template <typename BT> void
single_linked_list<BT>::DelSub(this_type_ptr subItem)
{
	dms_assert(m_FirstSub); // subItems was once added, so this must have a firstSub
	this_type_ptr* subPtrPtr = &m_FirstSub;
	while (*subPtrPtr != subItem) subPtrPtr = &((*subPtrPtr)->m_Next);
	*subPtrPtr = subItem->m_Next;
}

template <typename BT> void
single_linked_list<BT>::Reorder(this_type_ptr* first, this_type_ptr* last)
{
	m_FirstSub = nullptr;
	for (; first != last; ++first)
		AddSub(*first);
}

#endif // __RTC_MCI_SINGLELINKEDLIST_INC
