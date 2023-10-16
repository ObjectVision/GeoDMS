// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef __XML_XMLTREEPARSER_H
#define __XML_XMLTREEPARSER_H

#include "xml/XmlParser.h"

class XmlTreeParser : public XmlParser
{
public:
	TIC_CALL XmlTreeParser(InpStreamBuff* inpBuff);
	TIC_CALL ~XmlTreeParser();

	TIC_CALL TreeItem* ReadTree(TreeItem* context);

protected: // override XmlParser
	virtual void ReadAttrCallback(XmlElement& element);
	virtual bool ReadElemCallback(XmlElement& element);

private:
	TreeItem* m_CurrItem = nullptr;

	UInt32 m_CurrItemLevel;
	UInt32 m_CurrElemLevel;

	friend struct XmlContextHandle; // must get access to protected inherited FormmatedInpStream
};

#endif // __XML_XMLTREEPARSER_H
