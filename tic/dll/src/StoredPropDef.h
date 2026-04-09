// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

#ifndef __TIC_STOREDPROPDEF_H
#define __TIC_STOREDPROPDEF_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include <set>

#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"

#include "StateChangeNotification.h"

//----------------------------------------------------------------------
template <typename T> struct can_be_indirect;
template <> struct can_be_indirect<Bool>      : std::false_type {};
template <> struct can_be_indirect<SharedStr> : std::true_type  {};
template <> struct can_be_indirect<TokenID  > : std::false_type {};

extern std::mutex g_RemoveRequestMutex;

template <class ItemType, class PropType>
class StoredPropDef: public PropDef<ItemType, PropType>
{
public:
	using base_type = PropDef<ItemType, PropType>;
	using typename base_type::ApiType;
	using typename base_type::ParamType;
private:
	using DataType = std::map<const ItemType*, PropType>;

public:
	// construction
	StoredPropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, bool addImplicitSuppl, chg_mode chgMode = chg_mode::none)
		:	base_type(propName, setMode, xmlMode, cpyMode, chgMode, true, can_be_indirect<PropType>::value, addImplicitSuppl)
	{}
	~StoredPropDef()
	{
		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);
		auto lock = std::scoped_lock(m_Mutex);
		for (const auto& keyValue: m_Data)
			keyValue.first->SubPropAssoc(this);
	}
	// implement PropDef get/set virtuals
	ApiType GetValue(const ItemType* item) const override
	{ 
		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(item);
		if (i == m_Data.end())
			return PropType();
		return i->second; 
	}

	bool SetValueImpl(ItemType* item, ParamType value)
	{ 
		assert(IsMetaThread());

		assert(item);
		item->AssertPropChangeRights( this->GetName().c_str() );

		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);

		auto lock = std::scoped_lock(m_Mutex);

		auto i = m_Data.lower_bound(item);

		if (i == m_Data.end() || i->first != item)
		{
			if (value == PropType())
				return false;

			// insert
			m_Data.insert(i, typename DataType::value_type(item, value));
			item->AddPropAssoc(this);
			return true;
		}
		if (value != PropType())
		{
			if (value == i->second)
				return false;
			// replace
			i->second = value;
			return true;
		}
		// remove
		m_Data.erase(i);
		item->SubPropAssoc(this);
		return true;
	}

	void SetValue(ItemType* item, ParamType value) override
	{
		if (!SetValueImpl(item, value))
			return;

		if (this->GetChgMode() != chg_mode::none)
		{
			if (this->GetChgMode() == chg_mode::invalidate)
				item->Invalidate();
			else
			{
				assert(this->GetChgMode() == chg_mode::eval);
				item->TriggerEvaluation();
			}
		}
		NotifyStateChange(item, NC_PropValueChanged);
	}

	// override AbstrPropDef for more selective prop persistency
	bool HasNonDefaultValue(const Object* item) const override
	{
		if (!debug_cast<const ItemType*>(item)->GetTSF(TSF_HasStoredProps))
			return false;

		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(static_cast<const ItemType*>(item));
		assert(i == m_Data.end() || i->second != PropType());
		return i != m_Data.end();
	}

protected:
	void RemoveValue(Object* item) override
	{
		auto lock = std::scoped_lock(m_Mutex);
		assert(m_Data.find(static_cast<const ItemType*>(item)) != m_Data.end());
		m_Data.erase(static_cast<const ItemType*>(item));
	}

private:
	DataType m_Data;
	mutable std::mutex m_Mutex;
};

//----------------------------------------------------------------------
// specialization for PropTyp == PropBool
//----------------------------------------------------------------------

template <class ItemType>
class StoredPropDef<ItemType, PropBool> : public PropDef<ItemType, PropBool>
{
	using base_type = PropDef<ItemType, PropBool>;
	using typename base_type::ApiType;
	using typename base_type::ParamType;

	using DataType = std::set<const ItemType*>;

public:
	// construction
	StoredPropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, bool addImplicitSuppl, chg_mode chgMode = chg_mode::none)
		: base_type(propName, setMode, xmlMode, cpyMode, chgMode, true, can_be_indirect<PropBool>::value, addImplicitSuppl)
	{
	}
	~StoredPropDef()
	{
		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);
		auto lock = std::scoped_lock(m_Mutex);
		for (const auto& key: m_Data)
			key->SubPropAssoc(this);
	}
	// implement PropDef get/set virtuals
	ApiType GetValue(const ItemType* item) const override
	{
		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(item);
		return i != m_Data.end();
	}

	bool SetValueImpl(ItemType* item, ParamType value)
	{
		assert(IsMetaThread());

		assert(item);
		item->AssertPropChangeRights(this->GetName().c_str());

		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);
		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.lower_bound(item);

		if (i == m_Data.end())
		{
			if (!value)
				return false;

			// insert
			m_Data.insert(i, item);
			item->AddPropAssoc(this);
			return true;
		}
		if (value)
			return false;

		// remove
		m_Data.erase(i);
		item->SubPropAssoc(this);
		return true;
	}

	void SetValue(ItemType* item, ParamType value) override
	{
		if (!SetValueImpl(item, value))
			return;

		if (this->GetChgMode() != chg_mode::none)
		{
			if (this->GetChgMode() == chg_mode::invalidate)
				item->Invalidate();
			else
			{
				assert(this->GetChgMode() == chg_mode::eval);
				item->TriggerEvaluation();
			}
		}
		NotifyStateChange(item, NC_PropValueChanged);
	}

	// override AbstrPropDef for more selective prop persistency
	bool HasNonDefaultValue(const Object* item) const override
	{
		if (!debug_cast<const ItemType*>(item)->GetTSF(TSF_HasStoredProps))
			return false;

		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(static_cast<const ItemType*>(item));
		return i != m_Data.end();
	}

protected:
	void RemoveValue(Object* item) override
	{
		auto lock = std::scoped_lock(m_Mutex);
		assert(m_Data.find(static_cast<const ItemType*>(item)) != m_Data.end());
		m_Data.erase(static_cast<const ItemType*>(item));
	}

private:
	DataType m_Data;
	mutable std::mutex m_Mutex;
};

template <class ItemType>
class NonDefaultBoolPropDef : public PropDef<ItemType, PropBool>
{
	using base_type = PropDef<ItemType, PropBool>;
	using typename base_type::ApiType;
	using typename base_type::ParamType;

	using DataType = std::map<const ItemType*, PropBool>;

public:
	// construction
	NonDefaultBoolPropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, bool addImplicitSuppl, chg_mode chgMode = chg_mode::none)
		: base_type(propName, setMode, xmlMode, cpyMode, chgMode, true, can_be_indirect<PropBool>::value, addImplicitSuppl)
	{}

	~NonDefaultBoolPropDef()
	{
		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);
		auto lock = std::scoped_lock(m_Mutex);
		for (const auto& keyValue : m_Data)
			keyValue.first->SubPropAssoc(this);
	}
	// implement PropDef get/set virtuals
	ApiType GetValue(const ItemType* item) const override
	{
		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(item);
		if (i == m_Data.end())
			return PropBool();
		return i->second;
	}

	bool SetValueImpl(ItemType* item, ParamType value)
	{
		assert(IsMetaThread());

		assert(item);
		item->AssertPropChangeRights(this->GetName().c_str());

		auto lockRemoveRequest = std::scoped_lock(g_RemoveRequestMutex);
		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.lower_bound(item);

		if (i == m_Data.end() || i->first != item)
		{
			// insert
			m_Data.insert(i, typename DataType::value_type(item, value));
			item->AddPropAssoc(this);
			return true;
		}

		if (value == i->second)
			return false;

		// replace
		i->second = value;
		return true;
	}

	void SetValue(ItemType* item, ParamType value) override
	{
		if (!SetValueImpl(item, value))
			return;

		if (this->GetChgMode() != chg_mode::none)
		{
			if (this->GetChgMode() == chg_mode::invalidate)
				item->Invalidate();
			else
			{
				assert(this->GetChgMode() == chg_mode::eval);
				item->TriggerEvaluation();
			}
		}
		NotifyStateChange(item, NC_PropValueChanged);
	}

	// override AbstrPropDef for more selective prop persistency
	bool HasNonDefaultValue(const Object* item) const override
	{
		if (!debug_cast<const ItemType*>(item)->GetTSF(TSF_HasStoredProps))
			return false;

		assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(static_cast<const ItemType*>(item));
		return i != m_Data.end();
	}

protected:
	void RemoveValue(Object* item) override
	{
		auto lock = std::scoped_lock(m_Mutex);
		assert(m_Data.find(static_cast<const ItemType*>(item)) != m_Data.end());
		m_Data.erase(static_cast<const ItemType*>(item));
	}

private:
	DataType m_Data;
	mutable std::mutex m_Mutex;
};

//*****************************************************************
// Property definition connected to a memberfunc Get & Set function
//*****************************************************************

template <class ItemType, class PropType>
struct MF_RoPropDef : ReadOnlyPropDef<ItemType, PropType>
{
	using base_type = ReadOnlyPropDef<ItemType, PropType>;
	using typename base_type::ApiType;
//	using MF_RoPropDef::ParamType;

	using GetFunc = ApiType(ItemType::*)() const;
	using SetFunc = void   (ItemType::*)(const ApiType&);

	// construction
	MF_RoPropDef(CharPtr propName, GetFunc getFuncPtr)
		:	ReadOnlyPropDef<ItemType, PropType>(propName)
		,	m_GetFuncPtr(getFuncPtr)
	{}

	// implement PropDef get/set virtuals
	ApiType GetValue(const ItemType* item) const override
	{
		return (item->*m_GetFuncPtr)();
	}

private:
	GetFunc m_GetFuncPtr;
};

#endif // __TIC_STOREDPROPDEF_H
