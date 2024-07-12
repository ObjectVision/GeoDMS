// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

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
