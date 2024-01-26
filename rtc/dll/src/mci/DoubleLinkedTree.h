// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_MCI_DOUBLELINKEDTREE_H
#define __RTC_MCI_DOUBLELINKEDTREE_H

/***********    composition                       **********/

template <typename CompleteType>
struct double_linked_tree
{
protected:
	using this_type_ptr = CompleteType*;
	using this_type_cptr = const CompleteType* ;

public:
	this_type_ptr  _GetFirstSubItem()   { return m_FirstSub; }
	this_type_ptr  GetNextItem()       { return m_Next; }
	this_type_ptr  GetPrevItem()       { return m_Prev; }

	this_type_cptr _GetFirstSubItem() const { return m_FirstSub; }
	this_type_cptr _GetLastSubItem () const { return m_FirstSub ? m_FirstSub->m_Prev : nullptr; }

	this_type_cptr GetNextItem() const { return m_Next; }
	this_type_cptr GetPrevItem() const { return m_Prev; }

	void Reorder(this_type_ptr* first, this_type_ptr* last); // client is responsible that 

protected: // implemented in mci/DoubleLinkedList.inc; include from instantiating code-unit.
	void AddSub(this_type_ptr subItem);
	void DelSub(this_type_ptr subItem);

private:
	this_type_ptr m_FirstSub = nullptr;
	this_type_ptr m_Next = nullptr, m_Prev = nullptr;
};

#endif // __RTC_MCI_DOUBLELINKEDTREE_H
