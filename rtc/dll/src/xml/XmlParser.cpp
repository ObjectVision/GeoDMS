// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

// (XmlConst merged in, 2026-08)


#include <ctype.h>

#include "xml/XmlParser.h"
#include "xml/XmlConst.h"
#include "dbg/debug.h"
#include "utl/StrFormat.h" // mgFormat2SharedStr, for the error messages of this reader

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
	,	m_AttrValues(std::move(src.m_AttrValues))
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

static StaticTokenID t_xml("xml");

XmlParser::XmlParser(InpStreamBuff* inpBuff)
	: FormattedInpStream(inpBuff) 
{
	// Only the header's own tag, not ReadElem: a document whose first element is an ordinary one is
	// Paired, so ReadElem would read its whole subtree before the missing header could be reported.
	// Nothing is lost by not calling the element callbacks here, since the derived parser does not
	// exist yet during a base-class constructor and they resolve to the no-ops of this class.
	ReadAttr(m_XmlVersionSpec);
	if (m_XmlVersionSpec.m_ElementType != XmlElementType::Header)
		ThrowXmlErr(mgFormat2SharedStr("the document must open with an '<?xml ... ?>' header, but it opens with the element '<{}>'"
			, m_XmlVersionSpec.m_NameID).c_str());
	if (m_XmlVersionSpec.m_NameID != t_xml)
		ThrowXmlErr(mgFormat2SharedStr("the document header must be named 'xml', but it is named '{}'"
			, m_XmlVersionSpec.m_NameID).c_str());
	ReadText(m_XmlVersionSpec.m_TailText);
}

void XmlParser::ThrowXmlErr(CharPtr msg)
{
	throwErrorF("XML", "{}({}, {}): {}", Buffer().FileName(), GetLineNr(), GetColNr(), msg);
}

// What the reader ran into, for an error message. NextChar() answers 0 at the end of the input, so
// the end of the file is a case of its own rather than a quoted NUL.
static auto AsFoundText(char ch) -> SharedStr
{
	if (!ch)
		return SharedStr("the end of the file");
	return mgFormat2SharedStr("'{}'", ch);
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
		// An entity reference: the name between '&' and ';' goes into a stack buffer. The name comes
		// from the file, so its length is checked before every store. The loop stops at the end of
		// the input as AtEnd() reports it; NextChar() is then 0, never EOF, so the earlier test for
		// EOF ran past the end of any buffer without a trailing sentinel byte.
		char nextToken[MAX_TOKEN_LEN+1], *nextTokenPtr = nextToken;
		ReadChar();
		nextChar = NextChar();
		while (!AtEnd() && nextChar != ';')
		{
			if (nextTokenPtr - nextToken >= MAX_TOKEN_LEN)
				throwDmsErrF("XML entity reference '&{}...' is longer than the {} characters supported", SharedStr(CharPtrRange(nextToken, nextTokenPtr)), MAX_TOKEN_LEN);
			*nextTokenPtr++ = nextChar;
			ReadChar();
			nextChar = NextChar();
		}
		// Leave the stream ON the ';', the last character this entity consumed. ReadText advances
		// once per iteration at the bottom of its loop, so consuming the ';' here as well ate the
		// character that followed the entity: 'a &amp; b' decoded to 'a &b' (#1260).
		*nextTokenPtr = 0;
		nextChar = SymbolGetChar(nextToken);
	}
}

void XmlParser::ReadText(XmlElement::TextType& elementText)
{
	// Element text is kept as it stands, and only the white space before the first and after the
	// last non-space character is dropped. Runs of white space in between used to be collapsed to a
	// single space, which is invisible in a Descr but flattened every multi-line expression and
	// every aligned value array the config dump writes: a configuration written as XML and read
	// back differed from its source on every one of them (#1261). Trimming the two ends is what
	// makes the spaced-out markup of the older fixtures, '< Descr > a &amp; < / Descr >', still
	// yield 'a &' rather than ' a & '.
	bool seenNonSpace = false;
	char nextChar = NextChar();
	while (!AtEnd() && nextChar != '<') // not EOF: ReadChar answers 0 at the end, see TransformChar
	{
		if (!isspace(UChar(nextChar)))
		{
			TransformChar(nextChar);
			elementText.push_back(nextChar);
			seenNonSpace = true;
		}
		else if (seenNonSpace)
			elementText.push_back(nextChar); // interior white space, verbatim
		nextChar = ReadChar();
	}

	// XML line-end normalization (XML 1.0 section 2.11): a CRLF and a lone CR each count as one LF.
	// The config dump is written through a text-mode stream, so on Windows its line ends arrive here
	// as CRLF; without this an expression read back from an .xml would carry a CR that the .dms
	// source it was written from never had, and every further round trip would add one more (#1251
	// is the same defect on the writing side).
	auto out = elementText.begin();
	for (auto in = elementText.begin(), e = elementText.end(); in != e; ++in)
	{
		if (*in == '\r')
		{
			*out++ = '\n';
			if (in + 1 != e && in[1] == '\n')
				++in;
		}
		else
			*out++ = *in;
	}
	elementText.erase(out, elementText.end());

	while (!elementText.empty() && isspace(UChar(elementText.back())))
		elementText.pop_back();
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

		if (NextChar() != '<') // external input, so a reported error rather than an assert
			ThrowXmlErr(mgFormat2SharedStr("'<' expected at the start of a tag inside the element '<{}>', but {} was found"
				, parent.m_NameID, AsFoundText(NextChar())).c_str());
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
			if (subElement.m_NameID != parent.m_NameID)
				ThrowXmlErr(mgFormat2SharedStr("the closing tag '</{}>' does not match the open tag '<{}>'"
					, subElement.m_NameID, parent.m_NameID).c_str());
			if (subElement.GetNrAttrValues() != 0)
				ThrowXmlErr(mgFormat2SharedStr("the closing tag '</{}>' carries an attribute; only an open tag may"
					, subElement.m_NameID).c_str());
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

// In place, over the five entities of XmlConstMap only; unrelated to the exported HtmlDecode(WeakStr)
// of utl/Encodes.h, which has its own table.
static void HtmlDecodeInPlace(SharedStr& token)
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

// The characters that end a name inside a tag. Anything else that is not white space belongs to
// the name.
static bool IsXmlTagPunct(char ch)
{
	return ch == '<' || ch == '>' || ch == '/' || ch == '?' || ch == '=';
}

void XmlParser::SkipSpace()
{
	while (!AtEnd() && isspace(UChar(NextChar())))
		ReadChar();
}

SharedStr XmlParser::ReadName()
{
	std::vector<char> name;
	while (!AtEnd())
	{
		char ch = NextChar();
		if (isspace(UChar(ch)) || IsXmlTagPunct(ch) || ch == '"' || ch == '\'')
			break;
		name.push_back(ch);
		ReadChar();
	}
	if (name.empty())
		return SharedStr();
	return SharedStr(CharPtrRange(name.data(), name.data() + name.size()));
}

SharedStr XmlParser::ReadAttrValue(TokenID tagNameID, WeakStr attrName)
{
	std::vector<char> value;
	char quote = NextChar();
	if (quote == '"' || quote == '\'')
	{
		ReadChar(); // past the opening quote
		while (true)
		{
			if (AtEnd())
				ThrowXmlErr(mgFormat2SharedStr("the value of attribute '{}' of the tag '<{}' is not closed before the end of the file"
					, attrName, tagNameID).c_str());
			char ch = NextChar();
			ReadChar();
			if (ch == quote)
				break;
			value.push_back(ch);
		}
	}
	else
	{
		// Unquoted, which is not XML but is what the bool and UInt32 overloads of
		// OutStream_XmlBase::WriteAttr emit ("name=TRUE"), so this reader accepts it.
		while (!AtEnd() && !isspace(UChar(NextChar())) && !IsXmlTagPunct(NextChar()))
		{
			value.push_back(NextChar());
			ReadChar();
		}
		if (value.empty())
			ThrowXmlErr(mgFormat2SharedStr("a value expected after '{}=' in the tag '<{}', but {} was found"
				, attrName, tagNameID, AsFoundText(NextChar())).c_str());
	}
	SharedStr result = value.empty()
		? SharedStr()
		: SharedStr(CharPtrRange(value.data(), value.data() + value.size()));
	// An attribute value carries entity references, not backslash escapes. Reading it through the
	// word reader ran it past ReadDQuote, which swallows a backslash and rewrites the character
	// after it, so a StorageName spelled as an attribute lost its path separators; and it left the
	// entities encoded, unlike element text, which TransformChar decodes.
	HtmlDecodeInPlace(result);
	return result;
}

// Reads one tag, from its '<' up to and including the '>' that ends it, character by character.
//
// It used to read through FormattedInpStream's word reader, which splits on white space and on its
// own field separators (';', ',', tab, newline) and on nothing else. So '<', '=', '?' and '/' each
// had to be surrounded by spaces, the way the fixtures in testcases\data still write them: '<?xml'
// came back as one word named "?xml", and 'version="1.0"' as one word, after which the reader
// demanded the '=' it had already swallowed. That is why this reader could not read back what the
// XML writer of XMLOut.cpp emits -- ItemSave, so @dumpconfig with an .xml name -- and so why the
// round trip that the testcases battery gets in DMS syntax could not be run in XML at all (#1261).
// Both spellings are accepted now.
void XmlParser::ReadAttr(XmlElement& element)
{
	SkipSpace();
	if (NextChar() != '<')
		ThrowXmlErr(mgFormat2SharedStr("'<' expected at the start of a tag, but {} was found", AsFoundText(NextChar())).c_str());
	ReadChar();
	SkipSpace();
	if (NextChar() == '?')
	{
		element.m_ElementType = XmlElementType::Header;
		ReadChar();
		SkipSpace();
	}
	else if (NextChar() == '/')
	{
		element.m_ElementType = XmlElementType::ClosingTag;
		ReadChar();
		SkipSpace();
	}

	SharedStr name = ReadName();
	if (name.empty())
		ThrowXmlErr(mgFormat2SharedStr("a tag name expected, but {} was found", AsFoundText(NextChar())).c_str());
	element.m_NameID = GetTokenID_mt(name.c_str());

	while (true)
	{
		SkipSpace();
		char ch = NextChar();
		if (ch == '>')
		{
			ReadChar();
			return;
		}
		if (ch == '/' && element.m_ElementType == XmlElementType::Paired)
		{
			element.m_ElementType = XmlElementType::UnPaired;
			ReadChar();
			SkipSpace();
			if (NextChar() != '>')
				ThrowXmlErr(mgFormat2SharedStr("'>' expected after the '/' that ends the unpaired element '<{}/>', but {} was found"
					, element.m_NameID, AsFoundText(NextChar())).c_str());
			ReadChar();
			return;
		}
		if (ch == '?' && element.m_ElementType == XmlElementType::Header)
		{
			ReadChar();
			SkipSpace();
			if (NextChar() != '>')
				ThrowXmlErr(mgFormat2SharedStr("'>' expected after the '?' that ends the header '<?{} ... ?>', but {} was found"
					, element.m_NameID, AsFoundText(NextChar())).c_str());
			ReadChar();
			return;
		}

		SharedStr attrName = ReadName();
		if (attrName.empty())
			ThrowXmlErr(mgFormat2SharedStr("an attribute name or the end of the tag '<{}' expected, but {} was found"
				, element.m_NameID, AsFoundText(ch)).c_str());
		SkipSpace();
		if (NextChar() != '=')
			ThrowXmlErr(mgFormat2SharedStr("'=' expected after the attribute name '{}' in the tag '<{}', but {} was found"
				, attrName, element.m_NameID, AsFoundText(NextChar())).c_str());
		ReadChar();
		SkipSpace();
		element.GetAttrValueRef(GetTokenID_mt(attrName.c_str())) = ReadAttrValue(element.m_NameID, attrName);
	}
}

// *****************************************************************************


// ==== XmlConst ====

#include "xml/XmlConst.h"

CharPtr XmlConstTable[256];
static std::map<CharPtr, Char, CompCharPtr> XmlConstMap; // filled by RegisterConst at static initialisation, read-only afterwards


char SymbolGetChar(CharPtr symbol)
{
	// A lookup, never an insertion: operator[] on a miss stored the caller's pointer as a key --
	// the stack buffer of TransformChar, or a slice of a string that HtmlDecode erases right
	// after -- and every later lookup compared against that dangling key. An unknown entity
	// decodes to 0, as before.
	auto i = XmlConstMap.find(symbol);
	return i == XmlConstMap.end() ? 0 : i->second;
}

struct RegisterConst {
	RegisterConst(char ch, CharPtr token)
	{
		XmlConstTable[static_cast<unsigned char>(ch)] = token;
		XmlConstMap[token] = ch;
	}
};
namespace {

RegisterConst lt('<',  "lt" );
RegisterConst gt('>',  "gt" );
RegisterConst amp('&', "amp");
RegisterConst apos('\'', "apos");
RegisterConst quot('"',"quot");

// Any further entity must fit MAX_TOKEN_LEN (xml/XmlConst.h) and must not contain ';': CompCharPtr
// treats ';' as the end of a key.

} // namespace
