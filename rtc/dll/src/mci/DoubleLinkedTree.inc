// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_MCI_DOUBLELINKEDTREE_INC
#define __RTC_MCI_DOUBLELINKEDTREE_INC

#include "mci/DoubleLinkedTree.h"
#include "dbg/Check.h"

/***********    composites                           **********/

template <typename BT> void
double_linked_tree<BT>::AddSub(this_type_ptr subItem)
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
double_linked_tree<BT>::DelSub(this_type_ptr subItem)
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
double_linked_tree<BT>::Reorder(this_type_ptr* first, this_type_ptr* last)
{
	m_FirstSub = nullptr;
	for (; first != last; ++first)
		AddSub(*first);
}

#endif // __RTC_MCI_DOUBLELINKEDTREE_INC
