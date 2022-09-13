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

#ifndef __RTC_GEO_NEIGHBOURITER_H
#define __RTC_GEO_NEIGHBOURITER_H

#include "geo/HeapElem.h"
#include "geo/SpatialIndex.h"

/******************************************************************************/

template <typename T>
inline typename sqr_acc_type<T>::type
SqrMinDistTo(T c, T a, T b)
{
	dms_assert(a <= b);
	if (c<a)
		return Sqr(sqr_acc_type<T>::type(a-c));
	if (c>b)
		return Sqr(sqr_acc_type<T>::type(c-b));
	return 0;
}

template <typename T>
inline typename sqr_acc_type<T>::type
SqrMaxDistTo(T c, T a, T b)
{
	dms_assert(a <= b);
	return Max(
		(a<c) ? Sqr(sqr_acc_type<T>::type(c-a)) : 0
	,	(b>c) ? Sqr(sqr_acc_type<T>::type(c-b)) : 0
	);
}

template <typename T>
inline typename sqr_acc_type<T>::type
SqrDistTo(T c, T a)
{
	return SqrMinDistTo(c, a, a);
}

template <typename T>
inline typename sqr_acc_type<T>::type
MinDist(const Point<T>& center, const Point<T>& point)
{
	return 
		SqrDistTo(center.first , point.first ) 
	+	SqrDistTo(center.second, point.second);
}

template <typename T>
inline typename sqr_acc_type<T>::type
MinDist(const Point<T>& center, const Range<Point<T> >& range)
{
	return 
		SqrMinDistTo(center.first , range.first.first , range.second.first) 
	+	SqrMinDistTo(center.second, range.first.second, range.second.second);
}

template <typename T>
inline typename sqr_acc_type<T>::type
MaxDist(const Point<T>& center, const Range<Point<T>>& range)
{
	return 
		SqrMaxDistTo(center.first , range.first.first , range.second.first) 
	+	SqrMaxDistTo(center.second, range.first.second, range.second.second);
}

/******************************************************************************/

template <typename SpatialIndexType>
struct neighbour_iter 
{
	typedef typename SpatialIndexType::PointType     PointType;
	typedef typename SpatialIndexType::DistType      DistType;
	typedef typename SpatialIndexType::ObjectPtrType ObjectPtr;
	typedef typename SpatialIndexType::LeafType*     LeafPtr;
	typedef typename sqr_acc_type<DistType>::type    SqrDistType;

	typedef heapElemType<SqrDistType, SizeT>     NodeRec;
	typedef heapElemType<SqrDistType, ObjectPtr> LeafRec;

	neighbour_iter(SpatialIndexType* spi)
		:	m_SPI(spi)
	{
		dms_assert(m_SPI);
		dms_assert(m_SPI->m_Nodes.size());
	}

	void Reset(PointType center)
	{
		m_NodeHeap.clear();
		m_LeafHeap.clear();
		m_Center = center;
		AddNode(0);
		ReFit();
	}

	explicit operator bool () const { return !AtEnd(); }

	void operator ++()
	{
		PopLeaf();
	}

	ObjectPtr operator *()
	{
		return CurrLeaf().Value();
	}

	const LeafRec& CurrLeaf() const
	{
		dms_assert(!AtEnd());
		return m_LeafHeap[0];
	}

private:
	void PopLeaf()
	{
		dms_assert(IsNormal());
		std::pop_heap(m_LeafHeap.begin(), m_LeafHeap.end());
		m_LeafHeap.pop_back();
		ReFit();
	}

	bool AtEnd() const
	{
		return m_NodeHeap.empty() && m_LeafHeap.empty();
	}

	bool IsNormal()
	{
		if (m_LeafHeap.empty())
			return false;
		if (m_NodeHeap.empty())
			return true;
		return m_LeafHeap[0].Imp() <= m_NodeHeap[0].Imp();
	}
	void ReFit()
	{
		while (!AtEnd() && !IsNormal())
			PopNode();
	}
	void AddNode(SizeT nodeID)
	{
		const SpatialIndexType::Node& node = m_SPI->m_Nodes[nodeID];
		if (!node.IsNonEmpty())
			return;
		SqrDistType sqrDist = MinDist(m_Center, node.m_BoundingBox);
		m_NodeHeap.push_back(NodeRec(nodeID, sqrDist));
		std::push_heap(m_NodeHeap.begin(), m_NodeHeap.end());
	}
	void AddLeaf(LeafPtr leafPtr)
	{
		SqrDistType sqrDist = MinDist(m_Center, leafPtr->GetExtents());
		m_LeafHeap.push_back(LeafRec(leafPtr->get_ptr(), sqrDist));
		std::push_heap(m_LeafHeap.begin(), m_LeafHeap.end());
	}
	void PopNode()
	{
		dms_assert(m_NodeHeap.size());
		SizeT nodeID = m_NodeHeap.front().Value();
		std::pop_heap(m_NodeHeap.begin(), m_NodeHeap.end());
		m_NodeHeap.pop_back();

		const SpatialIndexType::Node& node = m_SPI->m_Nodes[nodeID];
		SizeT offset = node.m_OffsetToFirstQuadrant;
		if (offset)
		{
			AddNode(nodeID + offset++);
			AddNode(nodeID + offset++);
			AddNode(nodeID + offset++);
			AddNode(nodeID + offset);
		}
		LeafPtr leafPtr = node.m_FirstLeaf;
		while (leafPtr)
		{
			AddLeaf(leafPtr);
			leafPtr = leafPtr->GetNext();
		}
	}

	WeakPtr<SpatialIndexType> m_SPI;
	PointType                 m_Center;
	std::vector<NodeRec>      m_NodeHeap;
	std::vector<LeafRec>       m_LeafHeap;
};

#endif // __RTC_GEO_NEIGHBOURITER_H
