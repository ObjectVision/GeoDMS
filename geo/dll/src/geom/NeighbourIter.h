// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  Neighbourhood iteration support: square-distance primitives and
 *  neighbour_iter over spatial-index buckets.
 */

#if !defined(__GEO_NEIGHBOURITER_H)
#define __GEO_NEIGHBOURITER_H

#include "vt/HeapElem.h"
#include "geom/SpatialIndex.h"

/******************************************************************************/

template <typename T>
inline typename sqr_acc_type<T>::type
SqrMinDistTo(T c, T a, T b)
{
	dms_assert(a <= b);
	if (c<a)
		return Sqr(typename sqr_acc_type<T>::type(a-c));
	if (c>b)
		return Sqr(typename sqr_acc_type<T>::type(c-b));
	return 0;
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
	// The leaves come out by distance, and of equal distances by object, which is the order of the objects' index in their
	// array: the spatial index holds them in an order that follows from how it was built (#1289), and heapElemType compares
	// the distance only, so equally near leaves came out in the order in which they were pushed.
	static bool LeafLess(const LeafRec& a, const LeafRec& b) // a comes out after b
	{
		if (a.Imp() != b.Imp())
			return a.Imp() > b.Imp();
		return b.Value() < a.Value();
	}

	void PopLeaf()
	{
		dms_assert(IsNormal());
		std::pop_heap(m_LeafHeap.begin(), m_LeafHeap.end(), LeafLess);
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
		// strictly nearer than every node not yet opened: a node at the same distance can hold a leaf at that distance
		// with a lower index, which LeafLess must see before the leaf on top comes out
		return m_LeafHeap[0].Imp() < m_NodeHeap[0].Imp();
	}
	void ReFit()
	{
		while (!AtEnd() && !IsNormal())
			PopNode();
	}
	void AddNode(SizeT nodeID)
	{
		const typename SpatialIndexType::Node& node = m_SPI->m_Nodes[nodeID];
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
		std::push_heap(m_LeafHeap.begin(), m_LeafHeap.end(), LeafLess);
	}
	void PopNode()
	{
		dms_assert(m_NodeHeap.size());
		SizeT nodeID = m_NodeHeap.front().Value();
		std::pop_heap(m_NodeHeap.begin(), m_NodeHeap.end());
		m_NodeHeap.pop_back();

		const typename SpatialIndexType::Node& node = m_SPI->m_Nodes[nodeID];
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

#endif // __GEO_NEIGHBOURITER_H
