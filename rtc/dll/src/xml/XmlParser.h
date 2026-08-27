// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_XML_XMLPARSER_H
#define __RTC_XML_XMLPARSER_H


#include "vt/BaseBounds.h"
#include "sym/Token.h"

#include <vector>

enum class XmlElementType { Paired, UnPaired, Header, ClosingTag };

struct XmlElement {
	typedef std::vector<char> TextType;
	typedef std::map<TokenID, SharedStr> AttrValuesType;
	typedef AttrValuesType::const_iterator AttrValuesConstIterator;

	XmlElement(XmlElement* parent = nullptr) noexcept;
	XmlElement(XmlElement&& rhs) noexcept;
	~XmlElement() noexcept;

	CharPtr GetAttrValue(TokenID attrNameID) const;
	SharedStr& GetAttrValueRef(TokenID attrNameID);
	std::size_t GetNrAttrValues() const;
	static void Inc(AttrValuesConstIterator& iter);

	AttrValuesConstIterator GetAttrValuesBegin() const;
	AttrValuesConstIterator GetAttrValuesEnd() const;

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
	 XmlParser(InpStreamBuff* inpBuff);
	~XmlParser();

	void XmlRead();

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
