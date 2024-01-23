// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __RTC_MCI_SINGLELINKEDTREE_H
#define __RTC_MCI_SINGLELINKEDTREE_H

/***********    composition                       **********/

template <typename CompleteType>
struct single_linked_tree
{
protected:
	using this_type_ptr = CompleteType*;
	using this_type_cptr = const CompleteType*;

public:
	this_type_ptr  _GetFirstSubItem()   { return m_FirstSub; }
	this_type_ptr  GetNextItem()       { return m_Next; }

	this_type_cptr _GetFirstSubItem() const { return m_FirstSub; }

	this_type_cptr GetNextItem() const { return m_Next; }

	void Reorder(this_type_ptr* first, this_type_ptr* last); // client is responsible that *.inc is instantiated

protected: // implemented in mci/SingleLinkedList.inc; include from instantiating code-unit.
	void AddSub(this_type_ptr subItem);
	void DelSub(this_type_ptr subItem);

private:
	this_type_ptr m_FirstSub = nullptr;
	this_type_ptr m_Next     = nullptr;
};

#endif // __RTC_MCI_SINGLELINKEDTREE_H
