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

void XmlParser::ReadEncl(XmlElement& element)
{
	ReadText(element.m_EnclText);

	// read sub element zooi
	while (true)
	{
		dms_assert(NextChar() == '<');
		element.m_SubElements.emplace_back(&element); //in-place creation of an XmlElement with element as parent.
		XmlElement& subElement = element.m_SubElements.back();
		bool keepElem = ReadElem(subElement);
		bool isClosingTag = subElement.m_ElementType == XmlElementType::ClosingTag; 
		if (isClosingTag)
		{
			MG_CHECK(subElement.m_NameID == element.m_NameID);
			MG_CHECK(subElement.GetNrAttrValues() == 0);
		}

		if (isClosingTag || !keepElem)
			element.m_SubElements.pop_back(); // destroy subElement
		if (isClosingTag)
			return;
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
