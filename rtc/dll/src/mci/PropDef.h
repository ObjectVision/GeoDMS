// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Property definitions: provides streaming, rtti and serialisation.
 */

#if !defined(__RTC_MCI_PROPDEF_H)
#define __RTC_MCI_PROPDEF_H


#include "dbg/DebugCast.h"
#include "mci/Object.h"
#include "mci/PropdefEnums.h"
#include "ptr/SharedStr.h"
#include "mci/AbstrValue.h"
#include "LockLevels.h" // DMS_ENTERS

//*****************************************************************
//**********         PropDef Interface                   **********
//*****************************************************************

class AbstrPropDef : public Object
{
	typedef Object base_type;
protected:
	AbstrPropDef(CharPtr propName, 
		const Class* pc, const ValueClass* vt, 
		set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, chg_mode chgMode, bool isStored, bool evaluateIndirectExpr, bool addImplicitSuppl);

public:
    ~AbstrPropDef();
	// get set
	virtual void GetAbstrValue(const Object* self, AbstrValue* value) const=0;
	virtual void SetAbstrValue(Object* self, const AbstrValue* value) =0;
	// Contract: DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v), same as Object::GetFullName.
	// The error-reporting path reads properties to say what it was doing -- see AbstrMsgGenerator in
	// dbg/DebugContext.h -- so asking whether a property is set, and rendering the value it stores,
	// must stay inside that ceiling: read the registry, take nothing outer to it. In particular an
	// override may not EVALUATE (TreeItemPropertyValue does, and evaluating parses, and parsing
	// registers tokens); that is why the cooked GetValueAsSharedStr carries no such contract (#1227).
	virtual bool HasNonDefaultValue(const Object* self) const;

	virtual SharedStr GetValueAsSharedStr   (const Object* self) const=0;
	virtual SharedStr GetRawValueAsSharedStr(const Object* self) const=0; // same contract as HasNonDefaultValue above
	virtual void   SetValueAsCharArray(Object* self, CharPtr value) =0;
	virtual void SetValueAsCharRange(Object* self, CharPtr begin, CharPtr end) =0;

	virtual void   CopyValue(const Object* src, Object* dst) =0;

// non virtual helper functions

	const Class*      GetItemClass()     const { return m_Class; }
	const ValueClass* GetValueClass()    const { return m_ValueClass; }
	set_mode          GetSetMode()       const { return m_SetMode; }
	xml_mode          GetXmlMode()       const { return m_XmlMode; }
	cpy_mode          GetCpyMode()       const { return m_CpyMode; }
	chg_mode          GetChgMode()       const { return m_ChgMode; }
	bool              CanBeIndirect()    const { return m_CanBeIndirect; }
	bool              AddImplicitSuppl() const { return m_AddImplicitSuppl; }
	// An alias PropDef is a second name for a value that another PropDef already defines, kept so
	// that existing configurations keep parsing. Only the primary name is rendered, or the detail
	// page would list the very same value twice under two names. Expr is the alias of CalcRule.
	bool              IsAlias()          const { return m_IsAlias; }

	void SetIsAlias() { m_IsAlias = true;  }

	auto CreateValue() const ->std::unique_ptr<AbstrValue>;

	TokenID GetNameID() const override;

	AbstrPropDef* GetPrevPropDef        () const { return m_PrevPD; }
	AbstrPropDef* GetPrevCopyablePropDef() const { return m_PrevCopyablePD; }

	virtual void RemoveValue(Object* item);

private:
	TokenID           m_PropNameID;
	const ValueClass* m_ValueClass;
	const Class*      m_Class;

	set_mode          m_SetMode : (2+1);
	xml_mode          m_XmlMode : (3+1);
	cpy_mode          m_CpyMode : (2+1);
	chg_mode          m_ChgMode : (2+1);
	bool              m_AddImplicitSuppl : 1 = false;
	bool              m_CanBeIndirect: 1 = false;
	bool              m_IsAlias : 1 = false;

	AbstrPropDef*     m_PrevPD = nullptr;
	AbstrPropDef*     m_PrevCopyablePD = nullptr;

#if defined(MG_DEBUGDATA)
	SharedStr         md_Name;
#endif

	DECL_RTTI(, Class)
};

#include "mci/ValueWrap.h"
#include "ser/AsString.h"
#include "ser/StringStream.h"
#include "ser/PairStream.h"
#include "vt/ElemTraits.h"

template <class ItemType, class PropType>
class PropDef : public AbstrPropDef
{
public:
	using ApiType = typename api_type<PropType>::type;
	using ParamType = typename param_type<PropType>::type;

	// construction

	PropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode)
		:	AbstrPropDef(
				propName, 
				ItemType::GetStaticClass(), 
				ValueWrap<PropType>::GetStaticClass(), 
				setMode, xmlMode, cpyMode, chg_mode::none, false, false, false
			)
	{}

	PropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, chg_mode chgMode, bool isStored, bool canBeIndirect, bool addImplicitSupplier)
		:	AbstrPropDef(
				propName, 
				ItemType::GetStaticClass(), 
				ValueWrap<PropType>::GetStaticClass(), 
				setMode, xmlMode, cpyMode, chgMode, isStored, canBeIndirect, addImplicitSupplier
			)
	{}

	// extent PropDef interface
	virtual ApiType GetValue   (ItemType const * item) const=0;
	// Same contract as AbstrPropDef::HasNonDefaultValue (see above): the raw accessor is what the
	// error-reporting path may use, so an override must stay registry-shared -- read a member or a
	// stored map, never evaluate. NOTE this default forwards to GetValue, which carries no such
	// contract: a PropDef whose GetValue computes must override GetRawValue to stay within it.
	virtual ApiType GetRawValue(ItemType const * item) const DMS_CALLEE_ENTERS(ord_level_type::IndexedString, dms_shared_v)
	{
		return GetValue(item);
	}
	virtual void SetValue(ItemType* /*item*/, ParamType /*value*/)
	{
		throwIllegalAbstract(MG_POS, "AbstrPropDef::SetValue");
	}

	// override AbstrPropDef virtuals
	void GetAbstrValue(const Object* self, AbstrValue* value) const override
	{
		const ItemType* item = debug_cast<const ItemType*>(self);
		assert(item);
		ValueWrap<PropType>* propValue = debug_cast<ValueWrap<PropType>*>(value);
		assert(propValue);
		propValue->SetValue( GetValue(item) );
	}
	void SetAbstrValue(Object* self, const AbstrValue* value) override
	{
		ItemType* item = debug_cast<ItemType*>(self);
		dms_assert(item);
		const ValueWrap<PropType>* propValue = debug_cast<const ValueWrap<PropType>*>(value);
		dms_assert(propValue);
		SetValue(item, propValue->Get());
	}

	void CopyValue(const Object* src, Object* dst) override
	{
		PropType v = GetRawValue(debug_cast<const ItemType*>(src));
		SetValue(debug_cast<ItemType*>(dst), v);
	}

	bool HasNonDefaultValue(const Object* self) const override
	{
		DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v); // see AbstrPropDef::HasNonDefaultValue
		const ItemType* item = debug_cast<const ItemType*>(self);
		dms_assert(item);
		return !(GetRawValue(item) == ApiType());
	}

	SharedStr GetValueAsSharedStr(const Object* self) const override
	{
		const ItemType* item = debug_cast<const ItemType*>(self);
		dms_assert(item);
		typename ValueWrap<PropType>::value_type propValue = GetValue(item);
		return ::AsString( propValue );
	}
	SharedStr GetRawValueAsSharedStr(const Object* self) const override
	{
		DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v); // see AbstrPropDef::HasNonDefaultValue
		const ItemType* item = debug_cast<const ItemType*>(self);
		dms_assert(item);
		typename ValueWrap<PropType>::value_type propValue = GetRawValue(item);
		return ::AsString( propValue );
	}

	void SetValueAsCharArray(Object* self, CharPtr value) override
	{
		ItemType* item = debug_cast<ItemType*>(self);
		dms_assert(item);
		typename ValueWrap<PropType>::value_type propValue;
		::AssignValueFromCharPtr(propValue, value);
		SetValue(item, propValue);
	}
	void SetValueAsCharRange(Object* self, CharPtr begin, CharPtr end) override
	{
		ItemType* item = debug_cast<ItemType*>(self);
		dms_assert(item);
		typename ValueWrap<PropType>::value_type propValue;
		::AssignValueFromCharPtrs_Checked(propValue, begin, end);
		SetValue(item, propValue);
	}
};

template <class ItemType, class PropType>
struct ReadOnlyPropDef : PropDef<ItemType, PropType>
{
	using base_type = PropDef<ItemType, PropType>;

	ReadOnlyPropDef(CharPtr propName, set_mode setMode = set_mode::none, xml_mode xmlMode = xml_mode::none)
		: base_type(propName, setMode, xmlMode, cpy_mode::none, chg_mode::none, false, false, false)
	{}
};



#endif // __RTC_MCI_PROPDEF_H
