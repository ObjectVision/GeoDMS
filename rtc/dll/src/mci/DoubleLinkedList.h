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

#ifndef __RTC_MCI_DOUBLELINKEDLIST_H
#define __RTC_MCI_DOUBLELINKEDLIST_H

/***********    composition                       **********/

template <typename CompleteType>
struct double_linked_list
{
protected:
	typedef CompleteType*       this_type_ptr;
	typedef const CompleteType* this_type_cptr;

	double_linked_list();

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
	this_type_ptr m_FirstSub;
	this_type_ptr m_Next, m_Prev;
};

#endif // __RTC_MCI_DOUBLELINKEDLIST_H
