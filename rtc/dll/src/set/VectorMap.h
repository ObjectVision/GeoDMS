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
#if !defined(__RTC_SET_VECTORMAP_H)
#define __RTC_SET_VECTORMAP_H

#include "geo/Pair.h"
#include "geo/seqvector.h"
#include "set/CompareFirst.h"

#include <vector>
#include <algorithm>

/*
	vector_map<Key, Data, Compare>
		uses a vector<pair<Key, Data> > to implement (parts of) the contract of 
		map<Key, Data, Compare>

	complexity            map            vector_map
	iterator++/--         constant time  constant time, but much faster
	find                  log N          log N but much faster
	insert                log N          N
	operator [] 
		on existing key   log N          log N but much faster
		on new key        log N          N

    i.e.: vector_map is good for (almost) static (non changing) orderable collections

	types:
		value_type:     pair<const Key, data>
		iterator:       vector<value_type>::iterator
		const_iterator: vector<value_type>::const_iterator

	operations:
		construction(iterator* first, iterator* last)

		iterator begin()
		iterator end()
		const_iterator begin() const
		const_iterator end() const
		Data& operator [](const Key&)
		iterator find(const Key&);
		const_iterator find(const Key&) const

  		pair<iterator, bool> insert(const value_type& _X)
  		pair<iterator, bool> erase(iterator first, iterator last)

#include <map>


*/

//#if defined(MG_DEBUG)

template<typename ForwarIter, typename OrderFunc> inline
bool check_order(ForwarIter curr, ForwarIter last, OrderFunc orderFunc)
{	// find breach in required order with successor
	if (curr != last)
	{
		ForwarIter oldcurr;
		while ((oldcurr = curr), ++curr != last)
			if (!orderFunc(*oldcurr, *curr))
				return false;
	}
	return true;
}

template <class Key, class Data, class Compare = DataCompare<Key> >
class vector_map
{
  public:
	using key_type       = Key;
	using data_type      = Data;
	using value_type     = Pair<Key, Data>;
	using ContainerType  = std::vector<value_type>;
	using size_type      = typename ContainerType::size_type;
	using iterator       = typename ContainerType::iterator;
	using const_iterator = typename ContainerType::const_iterator;
	using reference      = Data&;
	using key_compare    = Compare;
	using insert_pair    = std::pair<iterator, bool>;

	typedef comp_first<key_type, value_type, key_compare> value_compare;

	vector_map(const key_compare& compare = key_compare())
		:	m_Compare(compare) 
	{}

	template <typename Iter>
	vector_map(Iter first, Iter last, key_compare compare = key_compare())
		:	m_Data(first, last)
		,	m_Compare(compare)
	{
		std::sort(begin(), end(), value_compare(m_Compare) );
		dms_assert(check_order(begin(), end(), value_compare(m_Compare) ) );
	}

	iterator       begin()       { return m_Data.begin(); }
	iterator       end()         { return m_Data.end();   }
	const_iterator begin() const { return m_Data.begin(); }
	const_iterator end()   const { return m_Data.end();   } 
	size_type      size()  const { return m_Data.size();  }
	bool           empty() const { return begin() == end(); }

	const value_type& back() const { dms_assert(!empty()); return end()[-1]; }
	      value_type& back()       { dms_assert(!empty()); return end()[-1]; }

	iterator lower_bound(const Key& key)
	{
		return std::lower_bound(m_Data.begin(), m_Data.end(), key, value_compare(m_Compare));
	}

	iterator upper_bound(const Key& key)
	{
		return std::upper_bound(m_Data.begin(), m_Data.end(), key, value_compare(m_Compare));
	}

	const_iterator lower_bound(const Key& key) const
	{
		return std::lower_bound(m_Data.begin(), m_Data.end(), key, value_compare(m_Compare));
	}

	const_iterator upper_bound(const Key& key) const
	{
		return std::upper_bound(m_Data.begin(), m_Data.end(), key, value_compare(m_Compare));
	}

	iterator find(const Key& key)
	{
		iterator e = end();
		iterator f = lower_bound(key);
		return (f == e || m_Compare(key, (*f).first))
			? e
			: f;
	}

	const_iterator find(const Key& key) const
	{
		return const_cast<vector_map*>(this)->find(key);
	}

  	insert_pair insert(const value_type& keyData)
	{
		iterator e = m_Data.end();
		iterator f = lower_bound(keyData.first);
		bool isNew = (f == e || m_Compare(keyData.first, (*f).first));
		if (isNew) 
			f = insert(f, keyData);
		return insert_pair(f, isNew);
	}

  	iterator insert(iterator hint, const value_type& keyData)
	{
		dms_assert(hint == m_Data.end()   || m_Compare(keyData.first, (*hint).first));
		dms_assert(hint == m_Data.begin() || m_Compare((*(hint-1)).first, keyData.first));
		// if not: return insert(keyData)
		return m_Data.insert(hint, keyData);
	}

	void push_back(const value_type& keyData)
	{
		insert(m_Data.end(), keyData);
	}

	reference operator [](const Key& key) { return insert(value_type(key, Data())).first->second; }

  	void erase(iterator first, iterator last)
	{
		m_Data.erase(first, last);
	}
	void swap(vector_map& rhs)
	{
		m_Data.swap(rhs.m_Data);
		omni::swap(m_Compare, rhs.m_Compare);
	}
	void reserve(size_type c) { m_Data.reserve(c); }
	size_type capacity() const { return m_Data.capacity(); }

protected:
	ContainerType m_Data;
	key_compare   m_Compare;
	friend void swap(vector_map& a, vector_map& b) { a.swap(b); }
};

template <class Key, class Data, class Compare >
bool operator ==(const vector_map<Key, Data, Compare>& lhs, const vector_map<Key, Data, Compare>& rhs)
{
	return lhs.size() == rhs.size()
		&& std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

#endif // __RTC_SET_VECTORMAP_H
