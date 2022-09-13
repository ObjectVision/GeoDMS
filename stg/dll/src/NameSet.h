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

#if !defined(__STG_NAMESET_H)
#define __STG_NAMESET_H

#include "RtcBase.h"
#include <map>

#include "set/IndexedStrings.h"

#include "AbstrUnit.h"

// ------------------------------------------------------------------------
//
// TNameSet for mapping to/from limited namespace
//
// ------------------------------------------------------------------------

struct TNameSet : SharedBase
{
	STGDLL_CALL TNameSet(UInt32 len);

	void Release() const { if (!DecRef()) delete this;	}

	void InsertItem(const AbstrDataItem* ti);
	STGDLL_CALL SharedStr InsertFieldName(CharPtr fieldName);
	void InsertIfColumn(const TreeItem* ti, const AbstrUnit* tableDomain);

	STGDLL_CALL SharedStr FieldNameToMappedName(CharPtr fieldName) const;
	STGDLL_CALL SharedStr FieldNameToItemName(CharPtr fieldName) const;
	SharedStr ItemNameToFieldName(CharPtr itemName) const;
	STGDLL_CALL SharedStr ItemNameToMappedName(CharPtr itemName) const;

	CharPtr GetItemName(CharPtr fieldName) const;

	UInt32 GetMappedNameBufferLength() const { return m_Len+1; }

	STGDLL_CALL static bool EqualName(CharPtr n1, CharPtr n2);

private:
	bool HasMappedName(CharPtr name);

	UInt32 m_Len;

	std::map < SharedStr, Couple<SharedStr>, SharedPtrInsensitiveCompare> m_Mappings;
};


#endif __STG_NAMESET_H
