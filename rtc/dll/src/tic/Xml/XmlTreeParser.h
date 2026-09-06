// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __XML_XMLTREEPARSER_H
#define __XML_XMLTREEPARSER_H

#include <vector>

#include "xml/XmlParser.h"
#include "TicBase.h" // for SharedMutableTreeItem

class XmlTreeParser : public XmlParser
{
public:
	TIC_CALL XmlTreeParser(InpStreamBuff* inpBuff);
	TIC_CALL ~XmlTreeParser();

	TIC_CALL SharedMutableTreeItem ReadTree(TreeItem* context, bool rootIsFirstItem);

protected: // override XmlParser
	virtual void ReadAttrCallback(XmlElement& element);
	virtual bool ReadElemCallback(XmlElement& element);

private:
	TreeItem*             m_CurrItem = nullptr;

	// The item that was current when each still-open item-creating element opened, so that
	// ReadElemCallback can restore it. The bottom entry is therefore what ReadTree was handed:
	// the #include context, or null when this parse creates the root. The XML element tree
	// cannot answer this: the outermost element has no parent element at all, and an element
	// that maps to no MetaClass creates no item to point back to (#1254).
	std::vector<TreeItem*> m_EnclosingItems;

	SharedMutableTreeItem m_RootHolder; // owns a brand-new (parentless) root for the parse lifetime
	bool                  m_RootIsFirstItem = false;

	UInt32 m_CurrItemLevel;
	UInt32 m_CurrElemLevel;

	friend struct XmlContextHandle; // must get access to protected inherited FormmatedInpStream
};

#endif // __XML_XMLTREEPARSER_H
