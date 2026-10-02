// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  SpatialIndex<ScalarType, ObjectPtr>: a quad-tree-like spatial index over
 *  point, box and sequence (arc/polygon) collections, with range queries
 *  and neighbour iteration support (see geo/NeighbourIter.h).
 */

#ifndef __GEO_SPATIALINDEX_H
#define __GEO_SPATIALINDEX_H

#include "vt/Pair.h"
#include "vt/SequenceArray.h"
#include "mem/MyContainers.h"
#include "set/VectorFunc.h" // RangeFromSequence_SkipUndefined
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
RangeFromPtr(const Range<PointType>* rect)
{
	return *rect;
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
	return RangeFromSequence_SkipUndefined(polygon->begin(), polygon->end());
}

template<typename PointType>
Range<PointType>
RangeFromPtr(typename sequence_traits<std::vector<PointType> >::cseq_t::const_iterator polygon)
{
	return RangeFromSequence_SkipUndefined(polygon->begin(), polygon->end());
}

template<typename PointType>
Range<PointType>
RangeFromPtr(sequence_array_index<PointType> polygon)
{
	return RangeFromSequence_SkipUndefined((*polygon).begin(), (*polygon).end());
}

// *****************************************************************************
//  GetQuadrantOffset
// *****************************************************************************

template <typename PointType>
UInt32 GetQuadrantOffset(UInt32 offset, const PointType& p, const PointType& mid)
{
	//bool y_High
	if (mid.Y() <= p.Y())
		offset+= 2;
	
	//bool x_High 
	if (mid.X() <= p.X())
		offset+= 1;

	return offset;
}

template <typename PointType>
UInt32 GetQuadrantOffset(UInt32 offset, const Range<PointType>& r, const PointType& mid)
{
	//bool y_High
	assert(r.first.first <= r.second.first);
	if (mid.Y() <= r.first.Y())
		offset += 2;
	else if (mid.Y() <= r.second.Y())
		return 0;
	
	//bool x_High 
	if (mid.X() <= r.first.X())
		offset += 1;
	else if (mid.X() <= r.second.X())
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
	using extents_type = PointType;

	PointLeaf(const PointType* ptr) : LeafBase<PointType, const PointType*, PointLeaf<PointType> >(ptr) { dms_assert(ptr); }
	const extents_type& GetExtents() const { return *this->get_ptr(); }
	bool IsDefined() const { return ::IsDefined(GetExtents()); }
};

template <typename PointType, typename ObjectPtr>
struct PolyLeaf: LeafBase<PointType, ObjectPtr, PolyLeaf<PointType, ObjectPtr>>
{
	using extents_type = Range<PointType>;

	PolyLeaf(ObjectPtr ptr) : LeafBase<PointType, ObjectPtr, PolyLeaf<PointType, ObjectPtr>>(ptr), m_Bounds(RangeFromPtr<PointType>(ptr)) { }

	const extents_type& GetExtents() const { return m_Bounds; }
	bool IsDefined() const { return ::IsDefined(m_Bounds) && !m_Bounds.inverted(); }

private:
	extents_type m_Bounds;
};

template <typename PointType, typename ObjectPtr> struct LeafTypeGetter
{
	using type = PolyLeaf<PointType, ObjectPtr>;
};

template <typename PointType> struct LeafTypeGetter<PointType, const PointType*>
{ 
	using type = PointLeaf<PointType>;
};


/*
template <typename PointType> struct LeafTypeGetter<PointType, const Range<PointType>*>      { typedef PolyLeaf<PointType, const Range<PointType>*> type; };
template <typename PointType> struct LeafTypeGetter<PointType, const std::vector<PointType>*> { typedef PolyLeaf <PointType, const std::vector<PointType>*> type; };
template <typename PointType> struct LeafTypeGetter<PointType, SA_ConstIterator<PointType> > { typedef PolyLeaf <PointType, SA_ConstIterator<PointType> > type; };
template <typename PointType> struct LeafTypeGetter<PointType, sequence_array_index<PointType> > { typedef PolyLeaf <PointType, sequence_array_index<PointType> > type; };
*/

template <typename PointType, typename ObjectPtr> using LeafTypeGetter_t = typename LeafTypeGetter<PointType, ObjectPtr>::type;


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
		const auto& extents = lf->GetExtents();
		if (InOneQuadrant(extents, center)) // is lf in a specific quadrant? Always true for Points
		{
			while ((lf = lf->GetNext()))
			{
				dms_assert(lf->IsDefined());
				const auto& nextExtents = lf->GetExtents();
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
	using ObjectPtrType = ObjectPtr;
	using DistType	    = T;

	using PointType     = Point<T>;
	using RangeType     = Range<PointType>;


	using LeafType = SpatialIndexImpl::LeafTypeGetter_t<PointType, ObjectPtrType>;
	// One leaf per indexed object and quadtree nodes in groups of 4: the whole index is
	// operator working set, so it lives in the allocation stocks where the census sees it.
	using LeafContainer = my_vec_t<LeafType>;

	struct Node
	{
		Node(const RangeType& bb, UInt32 offsetFromParent) : m_BoundingBox(bb), m_OffsetFromParent(offsetFromParent), 
			m_OffsetToFirstQuadrant(0), m_FirstLeaf(0), m_NrObjects(0) {}

		bool IsSplit()    const { return m_OffsetToFirstQuadrant; }
		bool IsNonEmpty() const { return IsSplit() || m_FirstLeaf; }
		UInt32 NrObjects()const { return m_NrObjects; }
		bool MustSplit() const
		{
			if (IsSplit() || NrObjects() <= 3)
				return false;

			// any gain from splitting? Only if the leaves that lie in one quadrant have more than one extent between them
			assert(m_QuadrantLeafExtentsDiffer == SpatialIndexImpl::MustSplit(m_FirstLeaf, Center(m_BoundingBox)));
			return m_QuadrantLeafExtentsDiffer;
		}

		void AddLeaf(LeafType* lf)
		{
			++m_NrObjects;

			// What MustSplit asks, kept up to date here: it rescanned the whole leaf list on every insert, k * k / 2
			// comparisons for k coincident objects, which never split (GEO-A54). A leaf added to a node that has split
			// (one that lies in no quadrant) stays with it and needs no record. The record points at the first such leaf
			// instead of copying its extents: the copy took a node of a dpoint index from 64 to 104 bytes, and every query
			// walks the nodes (GEO-A54 follow-up). Once two extents differed, nothing more needs recording.
			if (!IsSplit() && !m_QuadrantLeafExtentsDiffer && SpatialIndexImpl::InOneQuadrant(lf->GetExtents(), Center(m_BoundingBox)))
			{
				if (!m_QuadrantLeaf)
					m_QuadrantLeaf = lf;
				else if (m_QuadrantLeaf->GetExtents() != lf->GetExtents())
					m_QuadrantLeafExtentsDiffer = true;
			}

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
		// UInt32, as DoSplit and _Add compute them; Rebuild checks the number of objects and DoSplit the number of nodes
		UInt32    m_OffsetFromParent;     // offset back to the parent node, 0 for the root
		UInt32    m_OffsetToFirstQuadrant;  // index of first quadrant node (always allocated in groups of 4).
		LeafType* m_FirstLeaf;  // index of first leaf; leafs form a singly-linked list

		// the first leaf added that lies in one quadrant, and whether a later one had other extents; see AddLeaf.
		// It points into m_Leafs, which does not reallocate after Rebuild, as m_FirstLeaf does.
		const LeafType* m_QuadrantLeaf = nullptr;
		UInt32    m_NrObjects;
		bool      m_QuadrantLeafExtentsDiffer = false;
	};
	static_assert(sizeof(RangeType) != 32 || sizeof(Node) == 64, "a node of a dpoint index keeps the 64 bytes it had before GEO-A54: every query walks the nodes");
	typedef my_vec_t<Node> NodeContainer;

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
			assert(IsIncluding(m_SearchObj, newSearchObj));
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

	SpatialIndex(ObjectPtr first, ObjectPtr last, SizeT maxNrFutureInserts = 0)
	{
		Rebuild(first, last, maxNrFutureInserts);
	}
	SpatialIndex(SpatialIndex&& rhs) = default;

	// An empty index, to Rebuild before use: an index kept as a member and rebuilt for set after
	// set of objects keeps the capacity of its leaf and node containers.
	SpatialIndex() {}

	// The index of [first, last), as the constructor makes it, in place of the current one.
	void Rebuild(ObjectPtr first, ObjectPtr last, SizeT maxNrFutureInserts = 0)
	{
		MG_CHECK(first != last || !maxNrFutureInserts); // future inserts must be within the current determinable boundingbox
		MG_USERCHECK2(SizeT(last - first) + maxNrFutureInserts <= MAX_VALUE(UInt32), "SpatialIndex: cannot index more than 4294967295 objects"); // Node::m_NrObjects
		m_Leafs.clear();
		m_Nodes.clear();
		m_Leafs.reserve((last-first) + maxNrFutureInserts); // the nodes point into m_Leafs: it must not grow after this
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
			if (i->IsDefined() && IsTouching(boundingBox, i->GetExtents())) // beware of undefined or empty 
			{
				_Add(&*i);
			}
	}

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
				SqrDistType result;
				if constexpr (std::is_integral_v<DistType>)
				{
					// the farthest side of the box per axis, in Float64: in the coordinate type the side p lies beyond
					// gave a negative difference, which wrapped for unsigned coordinates and won the Max (GEO-A34)
					const auto& bb = nodePtr->m_BoundingBox;
					Float64 dx = Max<Float64>(Float64(p.first ) - Float64(bb.first.first ), Float64(bb.second.first ) - Float64(p.first ));
					Float64 dy = Max<Float64>(Float64(p.second) - Float64(bb.first.second), Float64(bb.second.second) - Float64(p.second));
					result = SqrDistType(dx * dx + dy * dy);
				}
				else
					result = Norm<SqrDistType>(
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

	ObjectPtr first_leaf() const { dms_assert(m_Leafs.size()); return m_Leafs.begin()->get_ptr(); }
private:
	SpatialIndex(const SpatialIndex&) {}

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

		// the node offsets, nodeIdx and offset are UInt32. The number of nodes depends on the depth that separates the
		// objects, not only on their number, so it is checked here and not in Rebuild.
		MG_USERCHECK2(m_Nodes.size() <= MAX_VALUE(UInt32) - 4, "SpatialIndex: more than 4294967295 quadtree nodes");
		UInt32 offset = m_Nodes.size() - nodeIdx;
		nodePtr->m_OffsetToFirstQuadrant = offset;
		m_Nodes.push_back(Node(RangeType(box.first, mid), offset++));
		m_Nodes.push_back(Node(RangeType(rowcol2dms_order( Top(box), mid.Col()), rowcol2dms_order(  mid.Row(), Right(box) )), offset++));
		m_Nodes.push_back(Node(RangeType(rowcol2dms_order(mid.Row(), Left(box)), rowcol2dms_order(Bottom(box), mid.Col()  )), offset++));
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
