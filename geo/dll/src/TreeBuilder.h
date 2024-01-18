// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// *****************************************************************************
//									TreeBuilder
// *****************************************************************************

#pragma once

#if !defined(__GEO_TREEBUILDER_H)
#define __GEO_TREEBUILDER_H

using NodeType = UInt32;
using LinkType = UInt32;

struct TreeNode;
using treenode_pointer = TreeNode*;
using const_treenode_pointer = const TreeNode*;

struct TreeNode
{
	TreeNode() = default;

	void AddChild(treenode_pointer childPtr)
	{
		assert(childPtr->m_SibblingNode == treenode_pointer()); // guaranteed to be processed once
		assert(childPtr->m_ParentNode   == treenode_pointer()); // guaranteed to be processed once

		if (m_FirstChildNode != treenode_pointer())
			childPtr->m_SibblingNode = m_FirstChildNode;
		m_FirstChildNode = childPtr;
		childPtr->m_ParentNode = this;
	}

	const_treenode_pointer GetParent() const { return m_ParentNode; }
	treenode_pointer GetParent() { return m_ParentNode; }
	const_treenode_pointer GetFirstChild() const { return m_FirstChildNode; }
	treenode_pointer GetFirstChild() { return m_FirstChildNode; }
	const_treenode_pointer GetSibbling() const { return m_SibblingNode; }
	treenode_pointer GetSibbling() { return m_SibblingNode; }

private:
	treenode_pointer m_ParentNode = {};
	treenode_pointer m_FirstChildNode = {};
	treenode_pointer m_SibblingNode = {};
};

struct TreeRelations
{
	typedef std::vector<TreeNode> treenode_container; // TODO, replace by uninitialized variant

	TreeRelations(NodeType nrNodes = 0): m_TreeNodes(nrNodes) // initialize later.
	{}

	void InitNodes(NodeType nrNodes)
	{
		vector_resize(m_TreeNodes, nrNodes);
	}
	bool IsUsed() const { return !m_TreeNodes.empty(); }

	TreeRelations(
		const LinkType* firstTB, 
		const NodeType* f1,
		const NodeType* f2,
		NodeType          nrNodes,
		LinkType          nrEdges
	)	:	m_TreeNodes(nrNodes)
	{
		treenode_pointer tnPtr = begin_ptr( m_TreeNodes );
		
		for (NodeType v = 0; v != nrNodes; ++firstTB, ++tnPtr, ++v)
		{
			LinkType tbEdge = *firstTB;
			if (tbEdge >= nrEdges) continue;
			if (f2[tbEdge] == v && f1[tbEdge] < nrNodes)
				m_TreeNodes[ f1[tbEdge] ] .AddChild(tnPtr);
			if (f1[tbEdge] == v && f2[tbEdge] < nrNodes)
				m_TreeNodes[ f2[tbEdge] ] .AddChild(tnPtr);
		}
		dms_assert(tnPtr == end_ptr( m_TreeNodes ) );
	}

	void InitRootNode(NodeType node)
	{
		dms_assert(node < m_TreeNodes.size());
		m_TreeNodes[node] = TreeNode();
	}
	void InitChildNode(NodeType node, NodeType parent)
	{
		InitRootNode(node);
		dms_assert(parent < m_TreeNodes.size());
		m_TreeNodes[parent].AddChild(&m_TreeNodes[node]);
	}

	static treenode_pointer MostDown(TreeNode* tn)
	{
		while (auto cn = tn->GetFirstChild())
			tn = cn;
		return tn;
	}

	static const_treenode_pointer MostDown(const TreeNode* tn)
	{
		while (auto cn = tn->GetFirstChild())
			tn = cn;
		return tn;
	}

	const_treenode_pointer NextRoot(const TreeNode* tn) const
	{
		auto lastNode = end_ptr( m_TreeNodes );
		while (tn != lastNode)
		{
			if (!tn->GetParent())
				return tn;
			++tn;
		}
		return {};
	}

	treenode_pointer NextRoot(TreeNode* tn)
	{
		auto lastNode = end_ptr(m_TreeNodes);
		while (tn != lastNode)
		{
			if (!tn->GetParent())
				return tn;
			++tn;
		}
		return {};
	}

	const_treenode_pointer NextRoot_Bottom(const TreeNode* tn) const
	{
		auto lastNode = end_ptr( m_TreeNodes );
		while (tn != lastNode)
		{
			if (!tn->GetParent())
				return MostDown(tn);
			++tn;
		}
		return nullptr;
	}

	treenode_pointer NextRoot_Bottom(TreeNode* tn)
	{
		auto lastNode = end_ptr( m_TreeNodes );
		while (tn != lastNode)
		{
			if (!tn->GetParent())
				return MostDown(tn);
			++tn;
		}
		return nullptr;
	}

	treenode_pointer WalkDepthFirst_BottomUp(treenode_pointer tn)
	{
		assert(tn);

		if (auto sn = tn->GetSibbling())
		{
			return MostDown( sn );
		}
		return tn->GetParent();
	}

	treenode_pointer WalkDepthFirst_BottomUp_all(treenode_pointer tn)
	{
		if (!tn)
			return NextRoot_Bottom(begin_ptr( m_TreeNodes ));

		if (auto sn = tn->GetSibbling())
			return MostDown( sn );

		if (auto pn = tn->GetParent())
			return pn;

		return NextRoot_Bottom(++tn);
	}

	const_treenode_pointer WalkDepthFirst_TopDown(const_treenode_pointer tn) const
	{
		assert(tn);

		if (auto cn = tn->GetFirstChild())
			return cn;
		
		while (true) 
		{
			if (auto sn = tn->GetSibbling())
				return sn;
			tn = tn->GetParent();
			if (!tn)
				return nullptr;
		};
	}

	treenode_pointer WalkDepthFirst_TopDown(treenode_pointer tn)
	{
		assert(tn);

		if (auto cn = tn->GetFirstChild())
			return cn;

		while (true)
		{
			if (auto sn = tn->GetSibbling())
				return sn;
			tn = tn->GetParent();
			if (!tn)
				return nullptr;
		};
	}

	treenode_pointer WalkDepthFirst_TopDown_all(treenode_pointer tn)
	{
		if (!tn)
			return NextRoot(begin_ptr( m_TreeNodes ));

		if (auto cn = tn->GetFirstChild())
			return cn;
		
		findSibbling:
			if (auto sn = tn->GetSibbling())
				return sn;
			if (auto pn = tn->GetParent())
			{
				tn = pn;
				goto findSibbling;
			}
		
		return NextRoot(++tn);
	}

	UInt32 NrOfNode(this auto& self, const TreeNode* tn)
	{
		if (!tn)
			return UNDEFINED_VALUE(UInt32);
		return tn - &*self.m_TreeNodes.begin();
	}

	/*
	UInt32 NrOfNode(const TreeNode* tn) const
	{
		if (!tn)
			return UNDEFINED_VALUE(UInt32);
		return tn - &*m_TreeNodes.begin();
	}

	UInt32 NrOfNode(TreeNode* tn)
	{
		if (!tn)
			return UNDEFINED_VALUE(UInt32);
		return tn - &*m_TreeNodes.begin();
	}
	*/

	treenode_container m_TreeNodes;
};

#endif // !defined(__GEO_TREEBUILDER_H)
