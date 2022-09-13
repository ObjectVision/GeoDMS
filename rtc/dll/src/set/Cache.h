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

#if !defined(__SET_CACHE_H)
#define __SET_CACHE_H

#include "set/QuickContainers.h"
#include "utl/Environment.h"

#include <functional>
#include <atomic>

/****************** struct Cache                  *******************/
template <typename Arg, typename Res>
struct duplicate_arg {
	const Arg& operator()(const Arg& arg, const Res&) const { return arg; }
};

template<
	typename Func, 
	typename ArgOrder = std::less<typename Func::argument_type>, 
	typename DuplFunc = duplicate_arg<typename Func::argument_type, typename Func::result_type> 
>
struct Cache
{
	typedef Func function;
	typedef typename function::argument_type         argument_type;
	typedef typename param_type<argument_type>::type argparam_type;
	typedef typename function::result_type           result_type;

	typedef std::map<argument_type, result_type, ArgOrder> map_type;

	Cache() { MG_DEBUGCODE( md_NrCalls = md_NrMisses = 0; ) }

	const result_type& apply(argparam_type arg)
	{
		MG_DEBUGCODE(md_NrCalls++; )
		dms_check_not_debugonly; 
		auto i = m_Map.lower_bound(arg);
		if (i == m_Map.end() || m_Comp(arg, i->first))
		{
			result_type res = m_Func(arg);
			i = m_Map.insert(i, typename map_type::value_type(m_Dupl(arg, res), res));
			MG_DEBUGCODE(md_NrMisses++; )
		}
		return i->second;
	}
	const result_type& operator()(argparam_type arg)
	{
		return apply(arg);
	}
	UInt32 size () const { return m_Map.size (); }
	bool   empty() const { return m_Map.empty(); }

	void remove(const argument_type& arg)
	{
		m_Map.erase(arg);
	}
	void clear() { m_Map.clear(); }

private:
	MG_DEBUGCODE(UInt32 md_NrCalls; )
	MG_DEBUGCODE(UInt32 md_NrMisses; )

	Func      m_Func;
	ArgOrder  m_Comp;
	DuplFunc  m_Dupl;
	map_type  m_Map;
};


/****************** struct StaticCache                  *******************/


template<
	typename Func, 
	typename ArgOrder = std::less<Func::argument_type>,
	typename DuplFunc = duplicate_arg<Func::argument_type, Func::result_type> 
>
struct StaticCache
{
	typedef          Cache<Func, ArgOrder, DuplFunc> cache_type;
	typedef typename cache_type::argument_type       argument_type;
	typedef typename cache_type::result_type         result_type;

	StaticCache()
		: m_PtrSection(item_level_type(0), ord_level_type::LispObjCache, "LispObjCache")
	{}

	const result_type& operator()(const argument_type& arg)
	{
		leveled_std_section::scoped_lock lock(m_PtrSection);

		if( m_Cache.is_null())
			m_Cache.assign(new cache_type);
		return m_Cache->apply(arg);
	}

	SizeT size() const { return m_Cache->size(); } // only defined if !IsNull()

	void remove(const argument_type& arg)           // only defined if !IsNull()
	{
		leveled_std_section::scoped_lock lock(m_PtrSection);

		m_Cache->remove(arg);
		if (m_Cache->empty())
			m_Cache.reset();
	}
	void clear() { m_Cache.reset(); }

private:
	static_ptr<cache_type> m_Cache;
	leveled_std_section m_PtrSection;
};

#endif // !defined(__SET_CACHE_H)
