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

#ifndef __GEO_SPATIALINDEX_H
#define __GEO_SPATIALINDEX_H

#include "geo/Pair.h"
#include "utl/IncrementalLock.h"

template <typename SpatialIndexType> struct neighbour_iter;

namespace SpatialIndexImpl {

template <typename T>
T _FirstElem(const Range<T>& r) 
{
	return r.first;
}

template <typename T>
Point<T> _FirstElem(const Point<T>& p) 
{
	return p;
}

template<typename PointType>
Range<PointType>
RangeFromPtr(typename sequence_traits<PointType>::cseq_t::const_iterator point)
{
	return Range<PointType>(*point, *point);
}

template<typename PointType>
Range<PointType>
RangeFromPtr(typename sequence_traits<std::vector<PointType> >::seq_t::iterator polygon)
{
	return RangeFromSequence(polygon->begin(), polygon->end());
}

template<typename PointType>
Range<PointType>
RangeFromPtr(typename sequence_traits<std::vector<PointType> >::cseq_t::const_iterator polygon)
{
	return RangeFromSequence(polygon->begin(), polygon->end());
}

template<typename PointType>
Range<PointType>
RangeFromPtr(typename sequence_array_index<PointType> polygon)
{
	return RangeFromSequence((*polygon).begin(), (*polygon).end());
}

// *****************************************************************************
//  GetQuadrantOffset
// *****************************************************************************

template <typename PointType>
UInt32 GetQuadrantOffset(UInt32 offset, const PointType& p, const PointType& mid)
{
	//bool firstHigh
	if (mid.first <= p.first)
		offset+= 2;
	
	//bool secondHigh 
	if (mid.second <= p.second)
		offset+= 1;

	return offset;
}

template <typename PointType>
UInt32 GetQuadrantOffset(UInt32 offset, const Range<PointType>& r, const PointType& mid)
{
	//bool firstHigh
	dms_assert(r.first.first <= r.second.first);
	if (mid.first <= r.first.first)
		offset += 2;
	else if (mid.first <= r.second.first)
		return 0;
	
	//bool secondHigh 
	if (mid.second <= r.first.second)
		offset += 1;
	else if (mid.second <= r.second.second)
		return 0;

	return offset;
}

template <typename PointType>
bool InOneQuadrant(const PointType& p, const PointType& mid)
{
	return true;
}

template <typename PointType>
bool InOneQuadrant(const Range<PointType>& r, const PointType& mid)
{
	return GetQuadrantOffset<PointType>(1, r, mid);
}


// *****************************************************************************
//									SpatialIndexImpl::Leaf
// *****************************************************************************

template <typename PointType, typename ObjectPtr, typename LeafType>
struct LeafBase
{
	ObjectPtr get_ptr()  const { return m_ObjectPtr; }
	LeafType* GetNext() const { return m_NextLeaf; }
	void SetNext(LeafType* lf) { m_NextLeaf = lf; }

	LeafBase(ObjectPtr ptr) : m_ObjectPtr(ptr), m_NextLeaf(nullptr) {}

private:
	ObjectPtr m_ObjectPtr;
	LeafType* m_NextLeaf;
};

template <typename PointType>
struct PointLeaf: LeafBase<PointType, const PointType*, PointLeaf<PointType> >
{
	typedef PointType        extents_type;

	PointLeaf(const PointType* ptr) : LeafBase<PointType, const PointType*, PointLeaf<PointType> >(ptr) { dms_assert(ptr); }
	const extents_type& GetExtents() const { return *this->get_ptr(); }
	bool IsDefined() const { return ::IsDefined(GetExtents()); }
};

template <typename PointType, typename ObjectPtr>
struct PolyLeaf: LeafBase<PointType, ObjectPtr, PolyLeaf<PointType, ObjectPtr>>
{
	typedef Range<PointType> extents_type;

	PolyLeaf(ObjectPtr ptr) : LeafBase<PointType, ObjectPtr, PolyLeaf<PointType, ObjectPtr>>(ptr), m_Bounds(RangeFromPtr<PointType>(ptr)) { }

	const extents_type& GetExtents() const { return m_Bounds; }
	bool IsDefined() const { return ::IsDefined(m_Bounds) && !m_Bounds.inverted(); }

private:
	extents_type m_Bounds;
};

template <typename PointType, typename ObjectPtr> struct LeafTypeGetter;

template <typename PointType> struct LeafTypeGetter<PointType, const PointType*>             { typedef PointLeaf<PointType> type; };
template <typename PointType> struct LeafTypeGetter<PointType, const std::vector<PointType>*> { typedef PolyLeaf <PointType, const std::vector<PointType>*> type; };
template <typename PointType> struct LeafTypeGetter<PointType, SA_ConstIterator<PointType> > { typedef PolyLeaf <PointType, SA_ConstIterator<PointType> > type; };
template <typename PointType> struct LeafTypeGetter<PointType, sequence_array_index<PointType> > { typedef PolyLeaf <PointType, sequence_array_index<PointType> > type; };
 

template <typename PointType, typename LeafType>
Bool AnyOffCross(PointType center, LeafType lf)
{
	while (lf)
	{
		if (GetQuadrantOffset(1, lf->GetExtents(), center))
			return true;
		lf = lf->GetNext();
	}
	return false;
}

// Leaf must split if the extent of the set of objects that falls into specific quadrants is non-zero 
// thus any of the dimensions is non-zero, thus furhter splitting will eventually separate
template <typename LeafType, typename PointType>
Bool MustSplit(const LeafType* lf, PointType center)
{
	dms_assert(lf); // follows from !IsSplit and NrObjects() > 3

	do {
		dms_assert(lf->IsDefined());
		auto extents = lf->GetExtents();
		if (InOneQuadrant(extents, center)) // is lf in a specific quadrant? Always true for Points
		{
			while (lf = lf->GetNext())
			{
				dms_assert(lf->IsDefined());
				auto nextExtents = lf->GetExtents();
				if (InOneQuadrant(nextExtents, center)) // is next lf in a specific quadrant? Always true for Points
					if (extents != nextExtents)
						return true;
			}
			return false;
		}
		lf = lf->GetNext();
	} while (lf);
	return false;
}

} // namespace SpatialIndexImpl

// *****************************************************************************
//									SpatialIndex
// *****************************************************************************

template <typename T, typename ObjectPtr>
struct SpatialIndex
{
//	using namespace SpatialIndexImpl;
	typedef T                          DistType;
	typedef ObjectPtr                  ObjectPtrType;
	typedef Point<T>                   PointType;
	typedef Range<PointType>           RangeType;

	typedef typename SpatialIndexImpl::LeafTypeGetter<PointType, ObjectPtrType>::type LeafType;
	typedef std::vector<LeafType>       LeafContainer;

	struct Node
	{
		Node(const RangeType& bb, SizeT offsetFromParent) : m_BoundingBox(bb), m_OffsetFromParent(offsetFromParent), 
			m_OffsetToFirstQuadrant(0), m_FirstLeaf(0), m_NrObjects(0) {}

		bool IsSplit()    const { return m_OffsetToFirstQuadrant; }
		bool IsNonEmpty() const { return IsSplit() || m_FirstLeaf; }
		UInt32 NrObjects()const { return m_NrObjects; }
		bool MustSplit() const
		{
			if (IsSplit() || NrObjects() <= 3)
				return false;

			// any gain from splitting?
			PointType center = Center(m_BoundingBox);
			return SpatialIndexImpl::MustSplit(m_FirstLeaf, center);
		}

		void AddLeaf(LeafType* lf) 
		{ 
			++m_NrObjects;

			lf->SetNext( m_FirstLeaf );
			m_FirstLeaf = lf;
		}
		const Node* GetNextSibbling() const
		{
			if (!m_OffsetFromParent)
				return nullptr;
			const Node* parent = (this - m_OffsetFromParent);
			dms_assert(parent->IsSplit());

			UInt32 i = m_OffsetFromParent - parent->m_OffsetToFirstQuadrant;
			dms_assert(i < 4);
			if (i == 3)
				return nullptr;
			return this + 1;
		}

		SizeT GetQuadrantOffset(const PointType& p) const // actually, result is part of this
		{
			dms_assert(IsSplit());
			return SpatialIndexImpl::GetQuadrantOffset(m_OffsetToFirstQuadrant, p, Center(m_BoundingBox));
		}
		SizeT GetQuadrantOffset(const RangeType& r) const // actually, result is part of this
		{
			dms_assert(IsSplit()); 
			return SpatialIndexImpl::GetQuadrantOffset(m_OffsetToFirstQuadrant, r, Center(m_BoundingBox));
		}

		RangeType m_BoundingBox; 
//	private:
		SizeT     m_OffsetFromParent;     // used to be: SpatialIndex<T>*
		SizeT     m_OffsetToFirstQuadrant;  // index of first quadrant node (always allocated in groups of 4).
		LeafType* m_FirstLeaf;  // index of first leaf; leafs form a singly-linked list
		SizeT     m_NrObjects;
	};
	typedef std::vector<Node> NodeContainer;

	template <typename SelType>
	struct iterator
	{
		iterator() : m_NodePtr(nullptr), m_LeafPtr(nullptr) {}
		explicit operator bool () const { return m_NodePtr; }
		void operator ++()
		{
			dms_assert(!AtEnd() && m_LeafPtr);
			m_LeafPtr = m_LeafPtr->GetNext();
			ReFit();
		}
		const LeafType* operator *()
		{
			dms_assert(!AtEnd() && m_LeafPtr);
			return m_LeafPtr;
		}

		iterator(SelType searchObj, const Node* firstNode)
			: m_SearchObj(std::move(searchObj))
			, m_NodePtr(GoDeep(firstNode))
			, m_LeafPtr(m_NodePtr->m_FirstLeaf)
		{
			ReFit();
		}
		void RefineSearch(SelType newSearchObj)
		{
			dms_assert(IsIncluding(m_SearchObj, newSearchObj));
			m_SearchObj = newSearchObj;
		}

	private:
		bool AtEnd() const { return !m_NodePtr; }
		bool DoesFit() { return IsIntersecting(m_SearchObj, (**this)->GetExtents()); }
		void ReFit()
		{
			while (!AtEnd() && (!m_LeafPtr || !DoesFit()))
			{
				if (m_LeafPtr)
					m_LeafPtr = m_LeafPtr->GetNext();
				else
					NextCollection();
			}
		}
		void NextCollection()
		{
			dms_assert(m_NodePtr);
			const Node* nextSibbling = m_NodePtr;
			do {
				nextSibbling = nextSibbling->GetNextSibbling();
			} while (nextSibbling && !IsTouching(nextSibbling->m_BoundingBox, m_SearchObj));
			if (nextSibbling)
				m_NodePtr = GoDeep(nextSibbling);
			else
			{
				if (m_NodePtr->m_OffsetFromParent)
					m_NodePtr -= m_NodePtr->m_OffsetFromParent;
				else
				{
					m_LeafPtr = 0;
					m_NodePtr = 0;
					return;
				}
			}
			m_LeafPtr = m_NodePtr->m_FirstLeaf;
		}
		// uses local nodePtr and m_SearchObj
		const Node* GoDeep(const Node* nodePtr)
		{
			//go as deep as possible as far as it fits
			while (true)
			{
//				dms_assert(IsTouching(nodePtr->m_BoundingBox, m_SearchObj));
				if (!nodePtr->IsSplit())
					return nodePtr;
				nodePtr += nodePtr->GetQuadrantOffset(SpatialIndexImpl::_FirstElem( m_SearchObj ) );
			}
		}
		SelType         m_SearchObj;
		const Node*     m_NodePtr;
		const LeafType* m_LeafPtr;
	};

	friend struct neighbour_iter<SpatialIndex>;
	friend struct iterator<PointType>;
	friend struct iterator<RangeType>;

	SpatialIndex(ObjectPtr first, ObjectPtr last, SizeT maxNrFutureInserts)
	{
		MG_CHECK(first != last || !maxNrFutureInserts); // future inserts must be within the current determinable boundingbox
		m_Leafs.reserve((last-first) + maxNrFutureInserts);
		RangeType boundingBox;

		for (; first != last; ++first)
		{
			m_Leafs.push_back(first);
			if (m_Leafs.back().IsDefined())
				boundingBox |= m_Leafs.back().GetExtents();
		}
		m_Nodes.push_back(Node(boundingBox, 0));

		typename LeafContainer::iterator 
			i = m_Leafs.begin(),
			e = m_Leafs.end();
		for (; i != e; ++i)
			if (IsTouching(boundingBox, i->GetExtents())) // beware of undefined or empty 
			{
				dms_assert(i->IsDefined());
				_Add(&*i);
			}
			else
			{
				dms_assert(!i->IsDefined());
			}
	}
	SpatialIndex(SpatialIndex&& rhs) = default;

	~SpatialIndex() {}

	RangeType GetBoundingBox() const { return m_Nodes.front().m_BoundingBox; }

	void Add(ObjectPtr obj)
	{
		dms_assert(m_Leafs.size() < m_Leafs.capacity());
		m_Leafs.push_back(obj);
		LeafType& lf = m_Leafs.back();
		_Add(&lf);
	}

	template <typename SqrDistType>
	SqrDistType GetSqrProximityUpperBound(const PointType& p, UInt32& maxDepth, const SqrDistType* sqrDist) const
	{
		dms_assert(m_Nodes.size());
		dms_assert(maxDepth);
		const Node* nodePtr = &*m_Nodes.begin();

		UInt32 depth = 0;
		while (true)
		{
			dms_assert(nodePtr->IsNonEmpty());
			SizeT q;
			if (++depth >= maxDepth
				|| !(nodePtr->IsSplit())
				|| !(q = nodePtr->GetQuadrantOffset(p))
				|| !(nodePtr[q].IsNonEmpty())
			)
			{
				maxDepth = depth - 1;
				SqrDistType result = Norm<SqrDistType>(
					PointType(
						Max<DistType>(p.first  - nodePtr->m_BoundingBox.first.first , nodePtr->m_BoundingBox.second.first  - p.first),
						Max<DistType>(p.second - nodePtr->m_BoundingBox.first.second, nodePtr->m_BoundingBox.second.second - p.second)
					)
				);
				if (sqrDist)
					MakeMin(result, *sqrDist);
				return result;
			}
			nodePtr += q;
		}
	}		
			
	iterator<RangeType> begin(const RangeType& searchBox) const { return iterator<RangeType>(searchBox, &*m_Nodes.begin()); }
	iterator<PointType> begin(const PointType& searchPnt) const { return iterator<PointType>(searchPnt, &*m_Nodes.begin()); }

//REMOVE?	template <typename SelType> iterator<SelType> end() const { return {}; }

	ObjectPtr first_leaf() const { dms_assert(m_Leafs.size()); return m_Leafs.begin()->get_ptr(); }
private:
	SpatialIndex(const SpatialIndex&) {}
	SpatialIndex() {}

	void _Add(LeafType* obj)
	{
		const auto& objExtents = obj->GetExtents();
		Node*  nodePtr = &*m_Nodes.begin();
		UInt32 nodeIdx = 0;
		while (true)
		{
			dms_assert( IsTouching(nodePtr->m_BoundingBox, objExtents ) );
			if (nodePtr->MustSplit())
				nodePtr = DoSplit(nodeIdx);
			dms_assert(nodePtr == &*m_Nodes.begin() + nodeIdx); // nodePtr survived possible growth of m_Nodes

			if (!nodePtr->IsSplit())
				break;
			auto q = nodePtr->GetQuadrantOffset(objExtents);
			if (!q)
				break;


			nodePtr += q;
			nodeIdx += q;
		}
		nodePtr->AddLeaf(obj);
	}

	Node* DoSplit(UInt32 nodeIdx) // must be member of SpatialIndex to grow m_Nodes.
	{
		Node* nodePtr = &*m_Nodes.begin() + nodeIdx;
		dms_assert(nodePtr->m_OffsetToFirstQuadrant == 0);

		RangeType box = nodePtr->m_BoundingBox;
		PointType mid = Center(box);

		UInt32 offset = m_Nodes.size() - nodeIdx;
		nodePtr->m_OffsetToFirstQuadrant = offset;
		m_Nodes.push_back(Node(RangeType(box.first, mid), offset++));
		m_Nodes.push_back(Node(RangeType(PointType(_Top(box), mid.Col()), PointType(mid.Row(), _Right(box) )), offset++));
		m_Nodes.push_back(Node(RangeType(PointType(mid.Row(), _Left(box)), PointType(_Bottom(box), mid.Col() )), offset++));
		m_Nodes.push_back(Node(RangeType(mid, box.second), offset++));

		nodePtr = &*m_Nodes.begin() + nodeIdx; // m_Nodes could have been grown

		LeafType* lf = nodePtr->m_FirstLeaf; nodePtr->m_FirstLeaf = nullptr;
		nodePtr->m_NrObjects = 0; // reset for recount
		while (lf)
		{
			LeafType* lf2 = lf->GetNext();
			UInt32 q = nodePtr->GetQuadrantOffset(lf->GetExtents());
			nodePtr[q].AddLeaf(lf); // could be *nodePtr itself when q==0
			lf = lf2;
		}
		dms_assert(nodePtr == &*m_Nodes.begin() + nodeIdx); // m_Nodes shouldn't have grown anymore
		return nodePtr;
	}

	LeafContainer m_Leafs;
	NodeContainer m_Nodes;
};

#endif // __GEO_SPATIALINDEX_H
