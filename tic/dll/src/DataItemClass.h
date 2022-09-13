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

#if !defined(__TIC_DATAITEMCLASS_H)
#define __TIC_DATAITEMCLASS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Class.h"
#include "mci/ValueComposition.h"

class AbstrDataItem;
class AbstrDataObject;
class AbstrUnit;
class ValueClass;

//----------------------------------------------------------------------
// class  : DataItemClass
//----------------------------------------------------------------------

class DataItemClass : public Class
{
	typedef Class base_type;
public:
    DataItemClass(Constructor cFunc, TokenID typeID, const ValueClass* valuesType);
   ~DataItemClass ();

	TIC_CALL bool IsDataObjType() const override;
	// Constructs a new DataItem of the type indicated by this DataItemClass
	TIC_CALL AbstrDataItem* CreateDataItem(
			TreeItem*         parent, 
			TokenID           nameID, 
			const AbstrUnit*  domainUnit, // Default unit will be selected when 0
			const AbstrUnit*  valuesUnit,
			ValueComposition  vc) const;

	TIC_CALL AbstrDataItem* CreateDataItemFromPath(
		TreeItem*         parent,
		CharPtr           path,
		const AbstrUnit*  domainUnit, // Default unit will be selected when 0
		const AbstrUnit*  valuesUnit,
		ValueComposition  vc) const;

	const ValueClass* GetValuesType() const { return m_ValuesType; }

	TIC_CALL static const DataItemClass* Find(const ValueClass* valueType);
	TIC_CALL static const DataItemClass* FindCertain(
		const ValueClass* valuesType,
		const TreeItem* context);

	static Object* CreateFromXml(Object* context, struct XmlElement& elem);

  private:
	const ValueClass*   m_ValuesType;

	DECL_RTTI(TIC_CALL, MetaClass)
};

TIC_CALL AbstrDataItem* CreateAbstrDataItem(
	TreeItem*        context, 
	TokenID          nameID, 
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc = ValueComposition::Single
);

TIC_CALL AbstrDataItem* CreateDataItem(
	TreeItem*        context, 
	TokenID          nameID, 
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc = ValueComposition::Single
);

TIC_CALL AbstrDataItem* CreateDataItemFromPath(
	TreeItem*        context,
	CharPtr          path,
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc);

TIC_CALL AbstrDataItem* CreateCacheDataItem(
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc = ValueComposition::Single
);

//----------------------------------------------------------------------
// string representation utility functions
//----------------------------------------------------------------------


#endif // __TIC_DATAITEMCLASS_H
