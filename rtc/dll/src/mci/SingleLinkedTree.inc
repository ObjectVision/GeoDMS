// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_MCI_SINGLELINKEDTREE_INC
#define __RTC_MCI_SINGLELINKEDTREE_INC

#include "mci/SingleLinkedTree.h"
#include "dbg/Check.h"

/***********    composites                           **********/

template <typename BT> void
single_linked_tree<BT>::AddSub(this_type_ptr subItem)
{
	dms_assert(subItem);
	this_type_ptr* subPtrPtr = &m_FirstSub;
	while (*subPtrPtr) subPtrPtr = &((*subPtrPtr)->m_Next);
	*subPtrPtr = subItem;
	subItem->m_Next = nullptr; // re-install end-of-list indicator
}

template <typename BT> void
single_linked_tree<BT>::DelSub(this_type_ptr subItem)
{
	dms_assert(m_FirstSub); // subItems was once added, so this must have a firstSub
	this_type_ptr* subPtrPtr = &m_FirstSub;
	while (*subPtrPtr != subItem) subPtrPtr = &((*subPtrPtr)->m_Next);
	*subPtrPtr = subItem->m_Next;
}

template <typename BT> void
single_linked_tree<BT>::Reorder(this_type_ptr* first, this_type_ptr* last)
{
	m_FirstSub = nullptr;
	for (; first != last; ++first)
		AddSub(*first);
}

#endif // __RTC_MCI_SINGLELINKEDTREE_INC
