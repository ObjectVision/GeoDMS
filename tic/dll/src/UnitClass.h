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

#if !defined(__TIC_UNITCLASS_H)
#define __TIC_UNITCLASS_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/Class.h"
#include "mci/ValueComposition.h"
#include "ptr/SharedPtr.h"

class AbstrUnit;
class Entity;
class ValueClass;

struct UnitClassRegComponentLock
#if defined(MG_DEBUG_DATA)
	: TreeItemAdmLock
#endif
{
	UnitClassRegComponentLock();
	~UnitClassRegComponentLock();
};
	//----------------------------------------------------------------------
// class  : UnitClass
//----------------------------------------------------------------------

class UnitClass : public Class, private UnitClassRegComponentLock
{
	typedef Class base_type;
public:
	UnitClass(Constructor cFunc, TokenID typeID, const ValueClass* valueClass);
	~UnitClass();

    // Constructs a new Unit of the type indicated by this UnitClass in context with given name
	TIC_CALL AbstrUnit* CreateUnit      (TreeItem* context, TokenID typeID) const;
	TIC_CALL AbstrUnit* CreateUnitFromPath(TreeItem* context, CharPtr path) const;
	TIC_CALL AbstrUnit* CreateResultUnit(TreeItem* context) const;
	TIC_CALL AbstrUnit* CreateTmpUnit   (TreeItem* context) const;

	TIC_CALL const AbstrUnit* CreateDefault() const;
	TIC_CALL void             DropDefault  () const;
	TIC_CALL const ValueClass* GetValueType(ValueComposition vc = ValueComposition::Single) const;
	TIC_CALL static const AbstrUnit* GetUnitOrDefault(const TreeItem* context, TokenID id, ValueComposition* vcPtr);

	TIC_CALL static const UnitClass* Find(const ValueClass*);
	TIC_CALL static Object* CreateFromXml(Object* context, struct XmlElement& elem);

private:
	const ValueClass*            m_ValueType;
	mutable SharedPtr<AbstrUnit> m_DefaultUnit;

	DECL_RTTI(TIC_CALL, MetaClass)
};


#endif // __TIC_UNITCLASS_H
