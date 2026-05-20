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
#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <ctype.h>

#include "xml/XmlParser.h"
#include "xml/XmlConst.h"
#include "dbg/debug.h"

// *****************************************************************************

XmlElement::XmlElement(XmlElement* parent) noexcept
	:	m_NameID(TokenID::GetEmptyID())
	,	m_Parent(parent)
	,	m_ElementType(XmlElementType::Paired)
	,	m_ClientData(0) 
{}

XmlElement::XmlElement(XmlElement&& src) noexcept
	:	m_NameID(src.m_NameID)
	,	m_Parent(src.m_Parent)
	,	m_EnclText(std::move(src.m_EnclText))
	,	m_TailText(std::move(src.m_TailText))
	,	m_SubElements(std::move(src.m_SubElements))
	,	m_ElementType(src.m_ElementType)
	,	m_ClientData(src.m_ClientData)
	,	m_AttrValues(src.m_AttrValues)
{}

XmlElement::~XmlElement()
{}

CharPtr XmlElement::GetAttrValue(TokenID attrNameID) const
{
	AttrValuesConstIterator r = m_AttrValues.find(attrNameID);
	if (r != m_AttrValues.end())
		return (*r).second.c_str();
	return "";
}

SharedStr& XmlElement::GetAttrValueRef(TokenID attrNameID)
{
	return m_AttrValues[attrNameID];
}

std::size_t XmlElement::GetNrAttrValues() const
{
	return m_AttrValues.size();
}

XmlElement::AttrValuesConstIterator XmlElement::GetAttrValuesBegin() const
{
	return m_AttrValues.begin();
}

XmlElement::AttrValuesConstIterator XmlElement::GetAttrValuesEnd() const
{
	return m_AttrValues.end();
}

void XmlElement::Inc(AttrValuesConstIterator& iter)
{
	++iter;
}

// *****************************************************************************

static TokenID t_xml = GetTokenID_st("xml");

XmlParser::XmlParser(InpStreamBuff* inpBuff)
	: FormattedInpStream(inpBuff) 
{
	// read tagName + attr values
	ReadElem(m_XmlVersionSpec);
	MG_CHECK(m_XmlVersionSpec.m_NameID == t_xml);
	MG_CHECK(m_XmlVersionSpec.m_ElementType == XmlElementType::Header);
}

XmlParser::~XmlParser()
{}

void XmlParser::XmlRead() 
{
	XmlElement element;
	ReadElem(element);
}

// called when all attributes of elem has been read
void XmlParser::ReadAttrCallback(XmlElement& ) 
{}

// called when all sub-element have been read
bool XmlParser::ReadElemCallback(XmlElement& )
{
	return true; // keep element for the parent.
}

void XmlParser::TransformChar(char& nextChar)
{
	if (nextChar == '&')
	{
		char nextToken[MAX_TOKEN_LEN+1], *nextTokenPtr = nextToken;
		ReadChar();
		nextChar = NextChar();
		while (nextChar != ';' && nextChar !=EOF)
		{
			*nextTokenPtr++ = nextChar;
			ReadChar();
			nextChar = NextChar();
		}
		ReadChar(); // nextChar = one after ';'
		dms_assert(nextTokenPtr - nextToken <= MAX_TOKEN_LEN);
		*nextTokenPtr = 0;
		nextChar = SymbolGetChar(nextToken);
	}
}

void XmlParser::ReadText(XmlElement::TextType& elementText)
{
	bool newToken = true; 
	bool writeSpace = false;
	char nextChar = NextChar();
	while (nextChar !='<' && nextChar != EOF)
	{
		if (isspace(UChar(nextChar)))
		{
			if (!newToken)
			{
				writeSpace = true;
				newToken = true;
			}
		}
		else
		{
			if (writeSpace)
			{
				elementText.push_back(' ');
				writeSpace = false;
			}
			TransformChar(nextChar);
			elementText.push_back(nextChar);
			newToken = false;
		}
		nextChar = ReadChar();
	}
	elementText.push_back(0);
}

void XmlParser::ReadEncl(XmlElement& rootEnclElement)
{
	// Iterative form of the original ReadElem <-> ReadEncl mutual recursion.
	// openStack tracks the chain of Paired elements whose subElement loop is
	// in progress; descend by emplacing into m_SubElements and pushing, ascend
	// when a matching ClosingTag is encountered. Keeps stack usage O(1)
	// regardless of XML nesting depth (attacker-controlled input).
	//
	// The caller (ReadElem) has consumed rootEnclElement's open tag and will
	// consume its tail text + ReadElemCallback after we return; we only
	// finalize *descendants* of rootEnclElement here.
	ReadText(rootEnclElement.m_EnclText);

	std::vector<XmlElement*> openStack;
	openStack.push_back(&rootEnclElement);

	while (!openStack.empty())
	{
		XmlElement& parent = *openStack.back();

		dms_assert(NextChar() == '<');
		parent.m_SubElements.emplace_back(&parent);
		XmlElement& subElement = parent.m_SubElements.back();

		// Inline of ReadElem(subElement), with the Paired branch redirected
		// back through the outer loop instead of recursing.
		ReadAttr(subElement);
		if (subElement.m_ElementType != XmlElementType::ClosingTag)
			ReadAttrCallback(subElement);

		if (subElement.m_ElementType == XmlElementType::Paired)
		{
			ReadText(subElement.m_EnclText);
			openStack.push_back(&subElement);
			continue;
		}

		// Non-Paired (UnPaired, Header, or ClosingTag): finalize this slot.
		bool isClosingTag = (subElement.m_ElementType == XmlElementType::ClosingTag);
		bool keepElem = false;
		if (isClosingTag)
		{
			MG_CHECK(subElement.m_NameID == parent.m_NameID);
			MG_CHECK(subElement.GetNrAttrValues() == 0);
		}
		else
		{
			ReadText(subElement.m_TailText);
			keepElem = ReadElemCallback(subElement);
		}

		if (isClosingTag || !keepElem)
			parent.m_SubElements.pop_back();

		if (!isClosingTag)
			continue;

		// Closing tag for parent: pop parent off the stack and finalize it on
		// behalf of its grandparent (the next stack entry, if any).
		openStack.pop_back();
		if (openStack.empty())
			break;

		XmlElement* grandparent = openStack.back();
		ReadText(parent.m_TailText);
		bool parentKeep = ReadElemCallback(parent);
		if (!parentKeep)
			grandparent->m_SubElements.pop_back();
	}
}

bool XmlParser::ReadElem(XmlElement& element)
{
	ReadAttr(element);
	if (element.m_ElementType != XmlElementType::ClosingTag) // fake element, defer text to parent
		ReadAttrCallback(element);
	if (element.m_ElementType == XmlElementType::Paired)
		ReadEncl(element);
	if (element.m_ElementType != XmlElementType::ClosingTag) // fake element, defer text to parent
	{
		ReadText(element.m_TailText);
		return ReadElemCallback(element);
	}
	return false;
}

void HtmlDecode(SharedStr& token)
{
	SharedCharArray* sca = token.GetAsMutableCharArray();
	if (!sca)
		return;
	SharedCharArray::iterator nextPos = 0;
	SharedCharArray::iterator end;
	while (end = sca->end(), (nextPos = std::find(sca->begin(), end, '&')) != end)
	{
		SharedCharArray::iterator semicolPos = std::find(++nextPos, end, ';');
		if (semicolPos == end)
			return;
		char ch = SymbolGetChar(nextPos); //, semicolPos);
		if (ch) 
		{
			nextPos[-1] = ch;
			sca->erase(nextPos, semicolPos - nextPos + 1);
		}
	}
}

void XmlParser::ReadAttr(XmlElement& element)
{
	SharedStr name;
	// read tagName + attr values
	(*this) >> "<" >> name;
	if (name == "?")
	{
		(*this) >> name;		
		element.m_ElementType = XmlElementType::Header;
	}
	if (name == "/")
	{
		(*this) >> name;
		element.m_ElementType = XmlElementType::ClosingTag;
	}
	element.m_NameID = GetTokenID_mt(name.c_str());

	SharedStr nextToken;
	while (true)
	{
		(*this) >> nextToken;
		if (nextToken == "/")
		{
			element.m_ElementType = XmlElementType::UnPaired;
			(*this) >> nextToken;
			MG_CHECK(nextToken == ">");
		}
		else if ((element.m_ElementType == XmlElementType::Header) && (nextToken == "?"))
		{
			(*this) >> nextToken;
			MG_CHECK(nextToken == ">");
		}

		if (nextToken == ">")
			return;
		// Transform nextToken

		HtmlDecode(nextToken);

		TokenID nextTokenID = GetTokenID_mt(nextToken.c_str());
		(*this) >> "=" >> element.GetAttrValueRef(nextTokenID);
	}
}

// *****************************************************************************
