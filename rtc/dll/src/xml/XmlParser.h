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

#ifndef __RTC_XML_XMLPARSER_H
#define __RTC_XML_XMLPARSER_H


#include "geo/BaseBounds.h"
#include "set/QuickContainers.h"
#include "set/Token.h"

#include <vector>

enum class XmlElementType { Paired, UnPaired, Header, ClosingTag };

struct XmlElement {
	typedef std::vector<char> TextType;
	typedef std::map<TokenID, SharedStr> AttrValuesType;
	typedef AttrValuesType::const_iterator AttrValuesConstIterator;

	RTC_CALL XmlElement(XmlElement* parent = nullptr) noexcept;
	RTC_CALL XmlElement(XmlElement&& rhs) noexcept;
	RTC_CALL ~XmlElement() noexcept;

	RTC_CALL CharPtr GetAttrValue(TokenID attrNameID) const;
	RTC_CALL SharedStr& GetAttrValueRef(TokenID attrNameID);
	RTC_CALL std::size_t GetNrAttrValues() const;
	RTC_CALL static void Inc(AttrValuesConstIterator& iter);

	RTC_CALL AttrValuesConstIterator GetAttrValuesBegin() const;
	RTC_CALL AttrValuesConstIterator GetAttrValuesEnd() const;

	TokenID                m_NameID;
	XmlElement*            m_Parent;
	TextType               m_EnclText;
	TextType               m_TailText;
	std::vector<XmlElement> m_SubElements;
	XmlElementType         m_ElementType;
	void*                  m_ClientData;
private: // Let op: een std::map heeft een static _NIL dit per .DLL instantiering wordt aangemaakt
	// Dus NIET van buitenaf deze map benaderen, gebruik de gelinkte interface funcs.
	AttrValuesType         m_AttrValues;
	XmlElement(const XmlElement&) = delete;
};

#include "ser/FormattedStream.h"
#include "ser/StringStream.h"

class XmlParser : protected FormattedInpStream
{
public:
	RTC_CALL  XmlParser(InpStreamBuff* inpBuff);
	RTC_CALL ~XmlParser();

	RTC_CALL void XmlRead();

protected:
	virtual void ReadAttrCallback(XmlElement& element); // called when all attributes of elem has been read
	virtual bool ReadElemCallback(XmlElement& element); // called when all sub-element have been read
	                                                    // return true if element must be kept for the parent.

private:
	XmlParser(const XmlParser&); // forbidden to use
	XmlParser();                 // forbidden to use

	bool ReadElem(XmlElement& element); // returns true if element must be kept for the parent.
	void ReadAttr(XmlElement& element);
	void ReadEncl(XmlElement& element);
	void ReadText(XmlElement::TextType& elementText);
	void TransformChar(char& nextChar);

	XmlElement m_XmlVersionSpec;
};

#endif // __RTC_XML_XMLPARSER_H
