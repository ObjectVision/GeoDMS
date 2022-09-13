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

#if !defined(__RTC_SET_STATICQUICKASSOCS_H)
#define __RTC_SET_STATICQUICKASSOCS_H

#include <map>

template<typename V> bool IsDefaultValue(const V& v) { return v == V(); }

//----------------------------------------------------------------------
// static_quick_assoc
//----------------------------------------------------------------------

template <typename K, typename V, typename Pred = std::less<K> >
struct static_quick_assoc : private static_ptr<std::map<K, V, Pred> >
{
	typedef typename param_type<K>::type key_param_type;
	typedef typename param_type<V>::type val_param_type;
	typedef V&                           val_ref_type;

	bool assoc(key_param_type key, val_param_type value)
	{
		if (IsDefaultValue(value))
		{
			erase(key);
			return false;
		}

		GetAssoc()[key] = value;
		return true;
	}
	void eraseExisting(key_param_type key)
	{
		dms_assert(this->has_ptr());
		auto mapPtr = this->get_ptr();
		mapPtr->erase(key);
		if (!mapPtr->size())
			this->reset();
	}
	void erase(key_param_type key)
	{
		if (this->has_ptr())
			eraseExisting(key);
	}
	const V* get_value_ptr(key_param_type key) const
	{
		if (this->has_ptr())
		{
			auto mapPtr = this->get_ptr();
			auto i = mapPtr->find(key);
			if (i != mapPtr->end())
				return &(i->second);
		}
		return nullptr;
	}
	val_ref_type operator [](key_param_type key)
	{
		return GetAssoc()[key];
	}
	const V& GetExisting(key_param_type key) const
	{
		dms_assert(this->has_ptr());
		auto mapPtr = this->get_ptr();
		auto i = mapPtr->find(key);
		dms_assert(i != mapPtr->end());
		dms_assert(i->first == key);
		return i->second;
	}

	const V& GetExistingOrDefault(key_param_type key, const V& defaultValue) const
	{
		dms_assert(this->has_ptr());
		auto mapPtr = this->get_ptr();
		auto i = mapPtr->find(key);
		if (i == mapPtr->end() || i->first != key)
			return defaultValue;
		return i->second;
	}

private:
	std::map<K, V, Pred>& GetAssoc()
	{
		if (this->is_null())
			this->assign(new std::map<K, V>);
		return *(this->get_ptr());
	}
};


#endif // __RTC_SET_STATICQUICKASSOCS_H
