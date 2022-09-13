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

#if !defined(__RTC_GEO_BI_GRAPH_H)
#define __RTC_GEO_BI_GRAPH_H

#include <functional>

// *****************************************************************************
//									directions
// *****************************************************************************

struct dir_forward_tag {};
struct dir_backward_tag {};

inline dir_forward_tag  dir_reverse(dir_backward_tag) { return dir_forward_tag();  }
inline dir_backward_tag dir_reverse(dir_forward_tag)  { return dir_backward_tag(); }

//#define GRAPH_FORWARD  (dir_forward_tag ())
//#define GRAPH_BACKWARD (dir_backward_tag())

// *****************************************************************************
//									struct bi_graph
// *****************************************************************************

struct bi_graph_link
{
	bi_graph_link(UInt32 src, UInt32 dst, UInt32 nextLnkSrc, UInt32 nextLnkDst)
		:	m_Src(src), m_Dst(dst), m_NextLnkSrc(nextLnkSrc), m_NextLnkDst(nextLnkDst)
	{}

	UInt32 m_Src, m_Dst, m_NextLnkSrc, m_NextLnkDst;
};

struct bi_graph
{
	bi_graph(UInt32 nrSrc, UInt32 nrDst, UInt32 nrLnk=0)
		:	m_FirstLnkSrc(nrSrc, UNDEFINED_VALUE(UInt32))
		,	m_FirstLnkDst(nrDst, UNDEFINED_VALUE(UInt32))
		
	{
		m_Links.reserve(nrLnk);
	}

	void AddLink(UInt32 src, UInt32 dst)
	{
		dms_assert(src < m_FirstLnkSrc.size());
		dms_assert(dst < m_FirstLnkDst.size());
		SizeT lnk = m_Links.size();
		m_Links.push_back(bi_graph_link(src, dst, m_FirstLnkSrc[src], m_FirstLnkDst[dst]));

		m_FirstLnkSrc[src] = lnk;
		m_FirstLnkDst[dst] = lnk;
	}

	UInt32 GetFirstLink(UInt32 src, dir_forward_tag) const 
	{ 
		dms_assert(src < m_FirstLnkSrc.size());
		return m_FirstLnkSrc[src];
	}

	UInt32 GetNextLink(UInt32 lnk, dir_forward_tag) const
	{ 
		dms_assert(lnk < m_Links.size());
		return m_Links[lnk].m_NextLnkSrc;
	}

	UInt32 GetFirstLink(UInt32 dst, dir_backward_tag) const
	{ 
		dms_assert(dst < m_FirstLnkDst.size());
		return m_FirstLnkDst[dst];
	}
	UInt32 GetNextLink(UInt32 lnk, dir_backward_tag) const
	{ 
		dms_assert(lnk < m_Links.size());
		return m_Links[lnk].m_NextLnkDst;
	}
	UInt32 GetSrcNode(UInt32 lnk, dir_forward_tag) const
	{ 
		dms_assert(lnk < m_Links.size());
		return m_Links[lnk].m_Src;
	}
	UInt32 GetDstNode(UInt32 lnk, dir_forward_tag) const
	{ 
		dms_assert(lnk < m_Links.size());
		return m_Links[lnk].m_Dst;
	}
	UInt32 GetSrcNode(UInt32 lnk, dir_backward_tag) const { return GetDstNode(lnk, dir_forward_tag()); }
	UInt32 GetDstNode(UInt32 lnk, dir_backward_tag) const { return GetSrcNode(lnk, dir_forward_tag()); }

	UInt32 GetNrSrcNodes(dir_forward_tag) const { return m_FirstLnkSrc.size(); }
	UInt32 GetNrDstNodes(dir_forward_tag) const { return m_FirstLnkDst.size(); }
	UInt32 GetNrSrcNodes(dir_backward_tag) const { return m_FirstLnkDst.size(); }
	UInt32 GetNrDstNodes(dir_backward_tag) const { return m_FirstLnkSrc.size(); }

	UInt32 GetNrLinks   () const { return m_Links.size(); }

private:
	std::vector<UInt32>        m_FirstLnkSrc, m_FirstLnkDst;
	std::vector<bi_graph_link> m_Links;
};

// *****************************************************************************
//									bi_graph_dijkstra
// *****************************************************************************

struct bi_graph_dijkstra
{
	typedef UInt32 ChronoID;
	typedef std::pair<ChronoID, UInt32> flag_elem;

	bi_graph_dijkstra(const bi_graph& gr )
		:	m_Graph(gr)
		,	m_CurrFlag(0)
		,	m_srcTraceBack(gr.GetNrSrcNodes(dir_forward_tag()), flag_elem(0, 0))
		,	m_dstTraceBack(gr.GetNrDstNodes(dir_forward_tag()), flag_elem(0, 0))
	{}


	bool is_flagged_dst(UInt32 node, dir_forward_tag)
	{
		dms_assert(node < m_dstTraceBack.size());
		return m_dstTraceBack[node].first == m_CurrFlag;
	}

	bool is_flagged_dst(UInt32 node, dir_backward_tag)
	{
		dms_assert(node < m_srcTraceBack.size());
		return m_srcTraceBack[node].first == m_CurrFlag;
	}

	UInt32 get_traceback_dst(UInt32 node, dir_forward_tag)
	{
		dms_assert(node < m_dstTraceBack.size());
		dms_assert(is_flagged_dst(node, dir_forward_tag()));
		return m_dstTraceBack[node].second;
	}

	UInt32 get_traceback_dst(UInt32 node, dir_backward_tag)
	{
		dms_assert(node < m_srcTraceBack.size());
		dms_assert(is_flagged_dst(node, dir_backward_tag()));
		return m_srcTraceBack[node].second;
	}

	void set_traceback_dst(UInt32 node, const flag_elem& fe, dir_backward_tag)
	{
		dms_assert(node < m_srcTraceBack.size());
		m_srcTraceBack[node] = fe;
	}

	void set_traceback_dst(UInt32 node, const flag_elem& fe, dir_forward_tag)
	{
		dms_assert(node < m_dstTraceBack.size());
		m_dstTraceBack[node] = fe;
	}

	template <typename GRAPH_DIR>
	void push_node_pos(
		UInt32 srcNode,
		UInt32 link,
		UInt32 maxFlow,
		const UInt32* dstMaxCapacity, 
		UInt32* lnkAllocated,
		GRAPH_DIR graphDir
	);

	template<typename GRAPH_DIR>
	void push_node_neg(
		UInt32 dstNode,
		UInt32 link,
		UInt32 maxFlow,
		const UInt32* lnkAllocated, 
		GRAPH_DIR graphDir = GRAPH_DIR()
	);

	template<typename GRAPH_DIR>
	void augment(
		const UInt32* dstMaxRestriction, 
		UInt32* lnkAllocated, 
		UInt32* dstAllocated,
		GRAPH_DIR graphDir = GRAPH_DIR()
	);

	template<typename GRAPH_DIR>
	UInt32 allocate(
		UInt32 srcNode, 
		UInt32 nrAlloc, 
		const UInt32* dstMaxCapacity, 
		UInt32* lnkAllocated, 
		UInt32* dstAllocated,
		GRAPH_DIR graphDir = GRAPH_DIR()
	);

protected:
	struct heap_elem
	{
		heap_elem(UInt32 maxAugmentation, UInt32 link, bool isPositiveAugmentation)
			: m_MaxAugmentation(maxAugmentation), m_SignedLink(isPositiveAugmentation ? link : ~link)
		{
			dms_assert(IsDefined(link));
		}

		bool   IsPositive() const { return m_SignedLink >= 0; }
		UInt32 Link() const { return IsPositive() ? m_SignedLink : ~m_SignedLink; }

		bool operator <(const heap_elem& rhs) const { return m_MaxAugmentation < rhs.m_MaxAugmentation; }

		UInt32 m_MaxAugmentation;
		Int32  m_SignedLink;      // sign indicated direction of augmentation; sign * context determines directionNOT is used in order to 
	};

private:
	const bi_graph&        m_Graph;
	std::vector<heap_elem> m_Heap;
	std::vector<flag_elem> m_srcTraceBack, m_dstTraceBack;
	ChronoID               m_CurrFlag;

	UInt32 m_maxFoundAugmentation, m_maxFoundLink;
};


template <typename GRAPH_DIR>
void bi_graph_dijkstra::push_node_pos(
	UInt32 srcNode,
	UInt32 link,
	UInt32 maxFlow,
	const UInt32* dstMaxCapacity, 
	UInt32* lnkAllocated,
	GRAPH_DIR graphDir
)
{
	dms_assert(!is_flagged_dst(srcNode, dir_reverse(graphDir)));
	dms_assert(maxFlow >  m_maxFoundAugmentation);

	set_traceback_dst( srcNode, flag_elem(m_CurrFlag, link), dir_reverse(graphDir) );

	link = m_Graph.GetFirstLink(srcNode, graphDir);
	while (IsDefined(link))
	{
		UInt32 dstNode = m_Graph.GetDstNode(link, graphDir);
		if (!is_flagged_dst(dstNode, graphDir))
		{
			UInt32 maxIncrease = dstMaxCapacity[dstNode] - lnkAllocated[link];
			if (maxIncrease > m_maxFoundAugmentation)
			{
				MakeMin(maxIncrease, maxFlow);
				m_Heap.push_back(
					heap_elem(
						maxIncrease,
						link,
						true
					)
				);

				std::push_heap(m_Heap.begin(), m_Heap.end());
			}
		}
		link = m_Graph.GetNextLink(link, graphDir);
	}
}

template<typename GRAPH_DIR>
void bi_graph_dijkstra::push_node_neg(
	UInt32 dstNode,
	UInt32 link,
	UInt32 maxFlow,
	const UInt32* lnkAllocated, 
	GRAPH_DIR graphDir
)
{
	dms_assert(!is_flagged_dst(dstNode, graphDir));
	dms_assert(maxFlow >  m_maxFoundAugmentation);

	set_traceback_dst( dstNode, flag_elem(m_CurrFlag, link), graphDir);

	link = m_Graph.GetFirstLink(dstNode, dir_reverse(graphDir));
	while (IsDefined(link))
	{
		UInt32 srcNode = m_Graph.GetDstNode(link, dir_reverse(graphDir));
		if (!is_flagged_dst(srcNode, dir_reverse(graphDir)))
		{
			UInt32 maxDecrease = lnkAllocated[link];
			MakeMin(maxDecrease, maxFlow);
			if (maxDecrease  > m_maxFoundAugmentation)
			{
				m_Heap.push_back(
					heap_elem(
						maxDecrease,
						link,
						false
					)
				);

				std::push_heap(m_Heap.begin(), m_Heap.end());
			}
		}
		link = m_Graph.GetNextLink(link, dir_reverse(graphDir));
	}

}

// *****************************************************************************
//									augmentation
// *****************************************************************************

template<typename GRAPH_DIR>
void bi_graph_dijkstra::augment(
	const UInt32* dstMaxRestriction, 
	UInt32* lnkAllocated, 
	UInt32* dstAllocated,
	GRAPH_DIR graphDir
)
{
	dms_assert(IsDefined(m_maxFoundLink));

	UInt32 dstNode = m_Graph.GetDstNode(m_maxFoundLink, graphDir);
	dstAllocated[dstNode] += m_maxFoundAugmentation;
	dms_assert( dstAllocated[dstNode] <= dstMaxRestriction[dstNode] );

	while (true)
	{
		dms_assert(IsDefined(m_maxFoundLink));

		lnkAllocated[m_maxFoundLink] += m_maxFoundAugmentation;

		UInt32 srcNode = m_Graph.GetSrcNode(m_maxFoundLink, graphDir);
		m_maxFoundLink = get_traceback_dst(srcNode, dir_reverse(graphDir));
		if (!IsDefined(m_maxFoundLink))
			break;

		lnkAllocated[m_maxFoundLink] -= m_maxFoundAugmentation;
		dstNode = m_Graph.GetDstNode(m_maxFoundLink, graphDir);
		m_maxFoundLink = get_traceback_dst(dstNode, graphDir);
	}
}

// *****************************************************************************
//									Allocate flow funcs
// *****************************************************************************

template<typename GRAPH_DIR>
UInt32 bi_graph_dijkstra::allocate(
	UInt32 srcNode, 
	UInt32 nrAlloc, 
	const UInt32* dstMaxCapacity, 
	UInt32* lnkAllocated, 
	UInt32* dstAllocated,
	GRAPH_DIR graphDir
)
{
	while (nrAlloc)
	{
		m_Heap.clear();
		++m_CurrFlag;
		m_maxFoundAugmentation = 0;

		push_node_pos<GRAPH_DIR>(srcNode, UNDEFINED_VALUE(UInt32), nrAlloc, dstMaxCapacity, lnkAllocated, GRAPH_DIR() );

		while (true)
		{
			if (m_Heap.empty())
				break;

			std::pop_heap(m_Heap.begin(), m_Heap.end());
			const heap_elem& currElem = m_Heap.back();
			UInt32 currLink = currElem.Link();
			UInt32 maxAug   = currElem.m_MaxAugmentation;
			bool   isPos    = currElem.IsPositive();
			m_Heap.pop_back();

			if (maxAug <= m_maxFoundAugmentation) 
				break;

			if (isPos)
			{
				UInt32 dstNode = m_Graph.GetDstNode(currLink, graphDir);
				if (is_flagged_dst(dstNode, graphDir))
					continue;


				dms_assert(maxAug <= dstMaxCapacity[dstNode] );
				dms_assert(maxAug > 0);

				UInt32 thisCap = dstMaxCapacity[dstNode] - dstAllocated[dstNode];
				UInt32 thisAug = Min<UInt32>(maxAug, thisCap);

				if (thisAug > m_maxFoundAugmentation)
				{
					m_maxFoundAugmentation = thisAug;
					m_maxFoundLink         = currLink;

					if	(	thisAug >= thisCap  // we like to fill rows or columns
						||	thisAug >= nrAlloc)
						break; // no point of looking further; poppedCap is maximum agmentation
				}

				if (maxAug > thisAug)  // does it make sense to continue searching for augmenting path?
					push_node_neg<GRAPH_DIR>(dstNode, currLink, maxAug, lnkAllocated, graphDir);
			}
			else
			{
				UInt32 newSrcNode = m_Graph.GetSrcNode(currLink, graphDir);
				if (is_flagged_dst(newSrcNode, dir_reverse(graphDir)))
					continue;

				dms_assert(maxAug <= lnkAllocated[currLink]);

				push_node_pos<GRAPH_DIR>(newSrcNode, currLink, maxAug, dstMaxCapacity, lnkAllocated, graphDir);
			}
		}
		dms_assert(m_maxFoundAugmentation <= nrAlloc);
		if (m_maxFoundAugmentation == 0)
			return nrAlloc;

		augment<GRAPH_DIR>(dstMaxCapacity, lnkAllocated, dstAllocated);
		nrAlloc -= m_maxFoundAugmentation;
	}
	return 0;
}


// *****************************************************************************
//									directed_dijkstra
// *****************************************************************************

template <typename cost_type>
struct directed_heap_elem
{
	directed_heap_elem(cost_type cost, UInt32 link)
		:	m_Cost(cost), m_Link(link)
	{
		dms_assert(cost >= cost_type() );
	}

	UInt32    Link() const { return m_Link; }
	cost_type Cost() const { return m_Cost; }

	// the smaller, the more priority; true == rhs more priority
	bool operator <(const directed_heap_elem<cost_type>& rhs) const 
	{ 
		return rhs.m_Cost < m_Cost; // let rhs gain priority only if stricty cheaper
	} 

	cost_type  m_Cost;
	Int32      m_Link;
};

template <typename DIRECTED_GRAPH>
struct directed_dijkstra
{

	typedef DIRECTED_GRAPH                 graph_type;
	typedef typename graph_type::cost_type cost_type;
	typedef directed_heap_elem<cost_type>  heap_elem;

	typedef UInt32 ChronoID;
	typedef std::pair<ChronoID, heap_elem> flag_elem;

	directed_dijkstra(const DIRECTED_GRAPH& gr )
		:	m_Graph(gr)
		,	m_CurrFlag(0)
		,	m_MaxLinkCost(MAX_VALUE(cost_type))
	{}


	bool is_flagged(UInt32 node) const
	{
		dms_assert(node < m_NodeTraceBack.size());
		return m_NodeTraceBack[node].first == m_CurrFlag;
	}

	const heap_elem& get_traceback(UInt32 node) const
	{
		dms_assert(is_flagged(node));
		return m_NodeTraceBack[node].second;
	}


	void set_traceback(UInt32 node, const heap_elem& he)
	{
		dms_assert(! is_flagged(node));
		m_NodeTraceBack[node] = flag_elem(m_CurrFlag, he);
	}

	void add_node(
		UInt32    srcNode,
		UInt32    link,       // this link indicated how to traceback from srcNode, but is reused as outgoing link
		cost_type travelCost  // must be at least as big as what was popped for link in order to maintain consistency
	)
	{
		dms_assert(!is_flagged(srcNode));
		dms_assert(! (travelCost < cost_type()) ); // travelCost may not have higher priority than ZERO

		set_traceback( srcNode, heap_elem(travelCost, link));
	}

	template <typename DIR_TAG>
	void fix_link(
		UInt32    traceBackLink,
		cost_type travelCost,
		DIR_TAG   dirTag
	);

	template <typename DIR_TAG>
	void fix_node(
		UInt32    srcNode,
		UInt32    traceBackLink,
		cost_type travelCost,
		DIR_TAG   dirTag
	);

	template <typename DIR_TAG>
	void init_tree(UInt32 rootNode, DIR_TAG dirTag)
	{
		m_CurrFlag++;
		m_Heap.clear();
		m_NodeTraceBack.resize(m_Graph.GetNrNodes(), flag_elem(0, heap_elem(cost_type(), UInt32())));

		fix_node(rootNode, UNDEFINED_VALUE(UInt32), cost_type(), dirTag);
	}

	template <typename DIR_TAG>
	bool get_next(DIR_TAG dirTag)
	{
		while (!empty())
		{
			if (!is_flagged(m_Graph.GetDstNode(m_Heap.front().Link(), dirTag)))
				return true;
			pop_node();
		}
		return false;
	}

	void pop_node()
	{
		std::pop_heap(m_Heap.begin(), m_Heap.end() );
		m_Heap.pop_back();
	}

	void run_tree()
	{
		while (!m_Heap.empty())
		{
			const heap_elem& currElem = m_Heap.front();

			UInt32    currLink = currElem.Link();
			cost_type minCost  = currElem.Cost();

			pop_node();

			fix_link(
				currLink,
				minCost
			);
		}
	}

	bool              empty() const { return m_Heap.empty(); }
	const heap_elem&  top()   const { return m_Heap.front(); }


private:
	const DIRECTED_GRAPH&  m_Graph;
	std::vector<heap_elem>  m_Heap;
	std::vector<flag_elem>  m_NodeTraceBack;
	ChronoID               m_CurrFlag;
	cost_type              m_MaxLinkCost;
};


template <typename DIRECTED_GRAPH> template <typename DIR_TAG>
void directed_dijkstra<DIRECTED_GRAPH>::fix_link(
	UInt32    link,
	cost_type travelCost,
	DIR_TAG   dirTag
)
{
	dms_assert( m_Graph.CheckLink(link) );

//	UInt32 srcNode = m_Graph.GetDstNode(link);
//	dms_assert(! is_flagged(srcNode) )
	fix_node(m_Graph.GetDstNode(link, dirTag), link, travelCost, dirTag);
}

template <typename DIRECTED_GRAPH> template <typename DIR_TAG>
void directed_dijkstra<DIRECTED_GRAPH>::fix_node(
	UInt32    srcNode,
	UInt32    link,        // this link indicated how to traceback from srcNode, but is reused as outgoing link
	cost_type travelCost,  // must be at least as big as what was popped for link in order to maintain consistency
	DIR_TAG   dirTag
)
{
	add_node(srcNode, link, travelCost);

	link = m_Graph.GetFirstLink(srcNode, dirTag);
	while (IsDefined(link))
	{
		UInt32 dstNode = m_Graph.GetDstNode(link, dirTag);
		if (!is_flagged(dstNode))
		{
			cost_type linkCost = m_Graph.GetLinkCost(link);
			if (linkCost < m_MaxLinkCost)
			{
				dms_assert( m_Graph.CheckLink(link) );

				dms_assert(!(linkCost < cost_type())); // linkCost may not have higher priority than ZERO
				m_Heap.push_back(
					heap_elem(
						travelCost + linkCost,
						link
					)
				);

				std::push_heap(m_Heap.begin(), m_Heap.end() );  // low cost == high priority
			}
		}
		link = m_Graph.GetNextLink(link, dirTag);
	}
}

// *****************************************************************************
//									make Tree func
// *****************************************************************************


#endif //!defined(__RTC_GEO_BI_GRAPH_H)
