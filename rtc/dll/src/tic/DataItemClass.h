// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
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

	bool IsDataObjType() const override;
	// Constructs a new DataItem of the type indicated by this DataItemClass
	SharedMutableDataItem CreateDataItem(
			TreeItem*         parent,
			TokenID           nameID,
			const AbstrUnit*  domainUnit, // Default unit will be selected when 0
			const AbstrUnit*  valuesUnit,
			ValueComposition  vc) const;

	SharedMutableDataItem CreateDataItemFromPath(
		TreeItem*         parent,
		CharPtr           path,
		const AbstrUnit*  domainUnit, // Default unit will be selected when 0
		const AbstrUnit*  valuesUnit,
		ValueComposition  vc) const;

	const ValueClass* GetValuesType() const { return m_ValuesType; }

	TIC_CALL static const DataItemClass* Find(const ValueClass* valueType);
	static const DataItemClass* FindCertain(
		const ValueClass* valuesType,
		const TreeItem* context);

	static std::shared_ptr<Actor> CreateFromXml(Object* context, struct XmlElement& elem);

  private:
	const ValueClass*   m_ValuesType;

	DECL_RTTI(, MetaClass)
};

TIC_CALL SharedMutableDataItem CreateAbstrDataItem(
	TreeItem*        context,
	TokenID          nameID,
	TokenID          tDomainUnit,
	TokenID          tValuesUnit,
	ValueComposition vc = ValueComposition::Single
);

TIC_CALL SharedMutableDataItem CreateDataItem(
	TreeItem*        context,
	TokenID          nameID,
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc = ValueComposition::Single
);

TIC_CALL SharedMutableDataItem CreateDataItemFromPath(
	TreeItem*        context,
	CharPtr          path,
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc);

TIC_CALL SharedMutableDataItem CreateCacheDataItem(
	const AbstrUnit* domainUnit,
	const AbstrUnit* valuesUnit,
	ValueComposition vc = ValueComposition::Single
);

//----------------------------------------------------------------------
// string representation utility functions
//----------------------------------------------------------------------


#endif // __TIC_DATAITEMCLASS_H
