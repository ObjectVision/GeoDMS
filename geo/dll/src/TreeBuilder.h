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

// *****************************************************************************
//									TreeBuilder
// *****************************************************************************

#pragma once

#if !defined(__GEO_TREEBUILDER_H)
#define __GEO_TREEBUILDER_H

typedef UInt32 NodeType;
typedef UInt32 LinkType;

struct TreeNode;
typedef TreeNode* treenode_pointer;

struct TreeNode
{
	TreeNode()
		:	m_ParentNode    (treenode_pointer())
		,	m_FirstChildNode(treenode_pointer())
		,	m_SibblingNode  (treenode_pointer())
	{}

	void AddChild(treenode_pointer childPtr)
	{
		dms_assert(childPtr->m_SibblingNode == treenode_pointer()); // guaranteed to be processed once
		dms_assert(childPtr->m_ParentNode   == treenode_pointer()); // guaranteed to be processed once

		if (m_FirstChildNode != treenode_pointer())
			childPtr->m_SibblingNode = m_FirstChildNode;
		m_FirstChildNode = childPtr;
		childPtr->m_ParentNode = this;
	}

	treenode_pointer m_ParentNode;
	treenode_pointer m_FirstChildNode;
	treenode_pointer m_SibblingNode;
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
		while (tn->m_FirstChildNode)
			tn = tn->m_FirstChildNode;
		return tn;
	}

	treenode_pointer NextRoot(TreeNode* tn)
	{
		treenode_pointer lastNode = end_ptr( m_TreeNodes );
		while (tn != lastNode)
		{
			if (!tn->m_ParentNode)
				return tn;
			++tn;
		}
		return treenode_pointer();
	}
	treenode_pointer NextRoot_Bottom(TreeNode* tn)
	{
		treenode_pointer lastNode = end_ptr( m_TreeNodes );
		while (tn != lastNode)
		{
			if (!tn->m_ParentNode)
				return MostDown(tn);
			++tn;
		}
		return nullptr;
	}

	treenode_pointer WalkDepthFirst_BottomUp(treenode_pointer tn)
	{
		dms_assert(tn);

		if (tn->m_SibblingNode)
		{
			return MostDown( tn->m_SibblingNode );
		}
		return tn->m_ParentNode;
	}

	treenode_pointer WalkDepthFirst_BottomUp_all(treenode_pointer tn)
	{
		if (!tn)
			return NextRoot_Bottom(begin_ptr( m_TreeNodes ));
		if (tn->m_SibblingNode)
			return MostDown( tn->m_SibblingNode );
		if (tn->m_ParentNode)
			return tn->m_ParentNode;
		return NextRoot_Bottom(++tn);
	}

	treenode_pointer WalkDepthFirst_TopDown(treenode_pointer tn)
	{
		dms_assert(tn);

		if (tn->m_FirstChildNode)
			return tn->m_FirstChildNode;
		
		while (!tn->m_SibblingNode) 
		{
			tn = tn->m_ParentNode;
			if (!tn)
				return nullptr;
		};
		return tn->m_SibblingNode;
	}

	treenode_pointer WalkDepthFirst_TopDown_all(treenode_pointer tn)
	{
		if (!tn)
			return NextRoot(begin_ptr( m_TreeNodes ));

		if (tn->m_FirstChildNode)
			return tn->m_FirstChildNode;
		
		findSibbling:
			if (tn->m_SibblingNode)
				return tn->m_SibblingNode;
			if (tn->m_ParentNode)
			{
				tn = tn->m_ParentNode;
				goto findSibbling;
			}
		
		return NextRoot(++tn);
	}

	UInt32 NrOfNode(TreeNode* tn)
	{
		if (!tn)
			return UNDEFINED_VALUE(UInt32);
		return tn - &*m_TreeNodes.begin();
	}

	treenode_container m_TreeNodes;
};

#endif // !defined(__GEO_TREEBUILDER_H)
