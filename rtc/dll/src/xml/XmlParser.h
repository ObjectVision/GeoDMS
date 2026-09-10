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
	// A raw pointer into the parent's m_SubElements, which reallocates as siblings are added: only
	// valid while the parent is still open on the parser's stack (XmlTreeParser::ReadEncl), which is
	// the only time it is used. Not a general back link.
	XmlElement*            m_Parent;
	TextType               m_EnclText;
	TextType               m_TailText;
	std::vector<XmlElement> m_SubElements;
	XmlElementType         m_ElementType;
	void*                  m_ClientData;
private: // NB: a std::map has a static _NIL that is instantiated per DLL, so this map must NOT be
	// accessed from outside; use the linked interface functions.
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

	// Malformed input is user input: report it as an error that names the position in the file, the
	// way XmlTreeParser already does. An MG_CHECK here would route through throwCheckFailed, which
	// asserts before it throws, so a headless Debug run died on the assertion instead of reporting
	// the error (#1261).
	[[noreturn]] void ThrowXmlErr(CharPtr msg);

private:
	XmlParser(const XmlParser&); // forbidden to use
	XmlParser();                 // forbidden to use

	bool ReadElem(XmlElement& element); // returns true if element must be kept for the parent.
	void ReadAttr(XmlElement& element);
	void ReadEncl(XmlElement& element);
	void ReadText(XmlElement::TextType& elementText);
	void TransformChar(char& nextChar);

	// Tag scanning, character by character. FormattedInpStream's word reader splits on white space
	// and on its own field separators only, which is why every '<', '=', '?' and '/' used to need a
	// space around it; see ReadAttr.
	void      SkipSpace();
	SharedStr ReadName();
	SharedStr ReadAttrValue(TokenID tagNameID, WeakStr attrName);

	// '<' and '!' have been consumed: skip an XML comment '<!-- ... -->' or any other markup
	// declaration '<!...>'. ReadText does the skipping, because it is what runs in front of every
	// tag, so a comment is accepted wherever text may stand as well as wherever a tag may start.
	void SkipMarkupDeclaration();

	// ReadText consumes the '<' that ends it before it can tell a tag from a comment; this says so
	// to the ReadAttr that follows, which then starts at the tag name. The stream reads forward
	// only, which is why this is a flag rather than a look-ahead.
	bool m_TagOpenConsumed = false;

	XmlElement m_XmlVersionSpec;
};

#endif // __RTC_XML_XMLPARSER_H
