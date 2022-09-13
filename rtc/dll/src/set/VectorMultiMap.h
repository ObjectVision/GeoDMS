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
#if !defined(__SET_VECTORMULTIMAP_H)
#define __SET_VECTORMULTIMAP_H

#include "set/CompareFirst.h"
#include "geo/Pair.h"

#include <vector>
#include <algorithm>

/*
	vector_multimap<Key, Data, Compare>
		uses a vector<pair<Key, Data> > to implement (parts of) the contract of 
		map<Key, Data, Compare>

	complexity            multimap       vector_multimap
	iterator++/--         constant time  constant time, but much faster
	find                  log N          log N but much faster
	insert                log N          N
	operator [] 
		on existing key   log N          log N but much faster
		on new key        log N          N

    i.e.: vector_multimap is good for static (non changing ) data

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

template <class Key, class Data, class Compare = std::less<Key> >
class vector_multimap
{
  public:
	typedef Key                             key_type;
	typedef Data                            data_type;
	typedef Pair<Key, Data>                 value_type;
	typedef std::vector<value_type>         ContainerType;
	typedef ContainerType::iterator         iterator;
	typedef ContainerType::const_iterator   const_iterator;
	typedef std::allocator<Data>::reference reference;
	typedef Compare                         key_compare;
	typedef Point<iterator>                 iterator_pair;
	typedef Point<const_iterator>           const_iterator_pair;
//	typedef std::pair<iterator, bool>       insert_pair;

	typedef comp_first<key_type, value_type, key_compare> value_compare;

	vector_multimap(const key_compare& compare = key_compare())
		: m_Compare(compare) {}

	vector_multimap(iterator first, iterator last, key_compare compare = key_compare())
		: m_Data(first, last),
		  m_Compare(compare)
	{
		std::sort(begin(), end(), value_compare(m_Compare) );
		dms_assert(adjacent_find(begin(), end(), value_compare(m_Compare) ) == end());
	}

	iterator       begin()       { return m_Data.begin(); }
	iterator       end()         { return m_Data.end();   }
	const_iterator begin() const { return m_Data.begin(); }
	const_iterator end()   const { return m_Data.end();   } 
	UInt32         size()  const { return m_Data.size();  }
	bool           empty() const { return begin() == end(); }

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
	iterator_pair equal_range(const Key& key)
	{
		return iterator_pair(
			lower_bound(key), 
			upper_bound(key)
		);
	}
	const_iterator_pair equal_range(const Key& key) const
	{
		return iterator_pair(
			lower_bound(key), 
			upper_bound(key)
		);
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
		return const_cast<vector_multimap*>(this)->find(key);
	}

  	iterator insert(const value_type& keyData)
	{
		return insert(
			upper_bound(keyData.first), 
			keyData
		);
	}

  	iterator insert(iterator hint, const value_type& keyData)
	{
		dms_assert(hint == m_Data.end()   || !m_Compare(hint[0].first, keyData.first));
		dms_assert(hint == m_Data.begin() || !m_Compare(keyData.first, hint[-1].first));
		return m_Data.insert(hint, keyData);
	}

	reference operator [](const Key& key) { return insert(value_type(key, Data())).first->second; }

  	void erase(iterator first, iterator last)
	{
		m_Data.erase(first, last);
	}
	void swap(vector_multimap& rhs)
	{
		m_Data.swap(rhs.m_Data);
		omni::swap(m_Compare, rhs.m_Compare);
	}

protected:
	ContainerType m_Data;
	key_compare   m_Compare;
friend bool operator ==(const vector_multimap&, const vector_multimap&);
friend void swap(vector_multimap& a, vector_multimap& b) { a.swap(b); }
};

template <class Key, class Data, class Compare >
bool operator ==(const vector_multimap<Key, Data, Compare>& lhs, const vector_multimap<Key, Data, Compare>& rhs)
{
	return lhs.m_Data == rhs.m_Data;
}

#endif // __SET_VECTORMULTIMAP_H
