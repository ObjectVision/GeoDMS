// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_GEO_ASSOCTOWER_H
#define __RTC_GEO_ASSOCTOWER_H

#include <utility>
#include <vector>

template<typename SemiGroup, typename AssociativeReducer>
struct assoc_tower
{
	using semi_group = SemiGroup;
	using associative_reducer = AssociativeReducer;
	using data_tower = std::vector<SemiGroup>;

	assoc_tower(associative_reducer&& ar = associative_reducer())
		: m_AssociativeReducer(std::move(ar))
	{}

	void add(SemiGroup&& sg)
	{
		assert(std::popcount(m_Index) == m_DataTower.size()); // pre-condition
		m_DataTower.emplace_back(std::move(sg));
		auto currIndex = ++m_Index;
		MG_CHECK(currIndex); // no overflow on uint64_t
		while ((currIndex & 0x01)==0)
		{
			assert(m_DataTower.size() > 1);
			ReduceTop();
			currIndex >>= 1;
		}
		assert(std::popcount(currIndex) == m_DataTower.size()); // post-condition
	}

	void assemble()
	{
		while (m_DataTower.size() > 1)
			ReduceTop();

		m_Index = 1;
	}

	bool empty() { return m_DataTower.empty(); }

	semi_group& front()
	{
		assemble();
		return m_DataTower.front();
	}

	semi_group get_result()
	{
		if (m_DataTower.empty())
			return SemiGroup();
		while (m_DataTower.size() > 1)
			ReduceTop();

		SemiGroup sg = std::move(m_DataTower.front());
		m_DataTower.pop_back();
		assert(m_DataTower.empty());
		m_Index = 0;
		return sg;
	}

private:
	void ReduceTop()
	{
		m_AssociativeReducer(m_DataTower.end()[-2], std::move(m_DataTower.end()[-1]));
		m_DataTower.pop_back();
	}

	associative_reducer m_AssociativeReducer;
	data_tower m_DataTower;
	SizeT      m_Index = 0;
};


#endif // __RTC_GEO_ASSOCTOWER_H
