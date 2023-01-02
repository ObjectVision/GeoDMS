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

#ifndef __TIC_STOREDPROPDEF_H
#define __TIC_STOREDPROPDEF_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "mci/PropDef.h"
#include "mci/PropDefEnums.h"
#include "ser/MapStream.h"

#include "StateChangeNotification.h"

//----------------------------------------------------------------------
template <typename T> struct can_be_indirect;
template <> struct can_be_indirect<Bool>      : boost::mpl::false_ {};
template <> struct can_be_indirect<SharedStr> : boost::mpl::true_  {};
template <> struct can_be_indirect<TokenID  > : boost::mpl::false_ {};

template <class ItemType, class PropType>
class StoredPropDef: public PropDef<ItemType, PropType>
{
	using base_type = PropDef<ItemType, PropType>;
	using typename base_type::ApiType;
	using typename base_type::ParamType;

	typedef std::map<const ItemType*, PropType> DataType;

public:
	// construction
	StoredPropDef(CharPtr propName, set_mode setMode, xml_mode xmlMode, cpy_mode cpyMode, bool addImplicitSuppl, chg_mode chgMode = chg_mode::none)
		:	base_type(propName, setMode, xmlMode, cpyMode, chgMode, true, can_be_indirect<PropType>::value, addImplicitSuppl)
	{}
	~StoredPropDef()
	{
		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.begin(), e=m_Data.end();
		while (i!=e)
			(*i++).first->SubPropAssoc(this);
	}
	// implement PropDef get/set virtuals
	ApiType GetValue(const ItemType* item) const override
	{ 
		dms_assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(item);
		if (i == m_Data.end())
			return PropType();
		return i->second; 
	}

	bool  SetValueImpl(ItemType* item, ParamType value)
	{ 
		dms_assert(IsMetaThread());

		dms_assert(item);
		item->AssertPropChangeRights( this->GetName().c_str() );

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.lower_bound(item);

		if (i == m_Data.end() || i->first != item)
		{
			if (value == PropType())
				return false;

			// insert
			m_Data.insert(i, DataType::value_type(item, value));
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
				dms_assert(this->GetChgMode() == chg_mode::eval);
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

		dms_assert(IsMetaThread());

		auto lock = std::scoped_lock(m_Mutex);
		auto i = m_Data.find(static_cast<const ItemType*>(item));
		dms_assert(i == m_Data.end() || i->second != PropType());
		return i != m_Data.end();
	}

protected:
	void RemoveValue(Object* item) override
	{
		auto lock = std::scoped_lock(m_Mutex);
		dms_assert(m_Data.find(static_cast<const ItemType*>(item)) != m_Data.end());
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
	using typename MF_RoPropDef::ApiType;
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
