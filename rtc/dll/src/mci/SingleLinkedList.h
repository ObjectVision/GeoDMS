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

#ifndef __RTC_MCI_SINGLELINKEDLIST_H
#define __RTC_MCI_SINGLELINKEDLIST_H

/***********    composition                       **********/

template <typename CompleteType>
struct single_linked_list
{
protected:
	typedef CompleteType*       this_type_ptr;
	typedef const CompleteType* this_type_cptr;

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

#endif // __RTC_MCI_SINGLELINKEDLIST_H
