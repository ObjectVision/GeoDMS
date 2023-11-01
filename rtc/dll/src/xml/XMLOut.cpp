// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "dbg/DmsCatch.h"
#include "xml/XMLOut.h"
#include "mci/Class.h"
#include "mci/PropDef.h"
#include "mci/PropdefEnums.h"
#include "xml/XmlConst.h"
#include "utl/mySPrintF.h"
#include "utl/Quotes.h"

//----------------------------------------------------------------------
// OutStreamBase
//----------------------------------------------------------------------

OutStreamBase::OutStreamBase(OutStreamBuff* out, bool needsIndent, const AbstrPropDef* primaryPropDef, FormattingFlags flgs)
	: m_OutStream(out, flgs)
	, m_NeedsIndent(needsIndent) 
	, m_LastNewLinePos(out->CurrPos())
	, m_PrimaryPropDef(primaryPropDef)
{}

void OutStreamBase::BeginSubItems()
{}

void OutStreamBase::ItemEnd()
{}

void OutStreamBase::EndSubItems()
{}

void OutStreamBase::WriteValueWithConfigSourceDecorations(CharPtr data)
{
	WriteValue(data);
}

void OutStreamBase::DumpPropList(const Object* self, const Class* cls)
{
	// process propDefs of all baseClasses first
	const Class* base= cls->GetBaseClass();
	if (base)
		DumpPropList(self, base);

	// dump props of cls
	AbstrPropDef* propDef = cls->GetLastPropDef();
	while (propDef) 
	{
		if (propDef->GetSetMode() > set_mode::none) // set mode obligated or optional, or construction
		if (	(propDef->GetXmlMode()  == xml_mode::attribute 
				|| (propDef->GetXmlMode() == xml_mode::signature && GetSyntaxType() != OutStreamBase::ST_DMS))
			&& propDef->HasNonDefaultValue(self) 
			)
		{
			SharedStr propValue = propDef->GetRawValueAsSharedStr(self);
			WriteAttr(propDef->GetName().c_str(), propValue.c_str());
		}
		propDef = propDef->GetPrevPropDef();
	}
}

void OutStreamBase::DumpPropList(const Object* self)
{
	DumpPropList(self, self->GetDynamicClass());
}

void OutStreamBase::DumpSubTags(const Object* self, const Class* cls)
{
	// process propDefs of all baseClasses first
	const Class* base= cls->GetBaseClass();
	if (base)
		DumpSubTags(self, base);

	// dump props of cls
	AbstrPropDef* propDef = cls->GetLastPropDef();
	while (propDef) 
	{
		if (  propDef->GetSetMode() >  set_mode::none // set mode construction, obligated or optional,
			&& propDef != m_PrimaryPropDef
			&& propDef->GetXmlMode() == xml_mode::element 
			&& propDef->HasNonDefaultValue(self)
			)
		{
			SharedStr propValue = propDef->GetRawValueAsSharedStr(self);
			DumpSubTag(propDef, propValue.c_str(), false);
		}
		propDef = propDef->GetPrevPropDef();
	}
}

void OutStreamBase::DumpSubTags(const Object* self)
{
	if (m_PrimaryPropDef && m_PrimaryPropDef->HasNonDefaultValue(self))
		DumpSubTag(m_PrimaryPropDef, m_PrimaryPropDef->GetRawValueAsSharedStr(self).c_str(), true);
	DumpSubTags(self, self->GetDynamicClass());
}

//=============== private members

void OutStreamBase::NewLine() 
{
	if (m_OutStream.Buffer().CurrPos() == m_LastNewLinePos)
		return;

	m_OutStream << "\n";
	m_LastNewLinePos = m_OutStream.Buffer().CurrPos();

	if (m_NeedsIndent) 
		for (UInt32 i=0; i<m_Level; i++) 
			m_OutStream << "\t";
}

void OutStreamBase::DumpSubTag(const AbstrPropDef* propDef, CharPtr tagValue, bool isPrimaryTag)
{
	DumpSubTag(propDef->GetName().c_str(), tagValue, isPrimaryTag);
}

//----------------------------------------------------------------------
// OutStream_XmlBase
//----------------------------------------------------------------------

OutStream_XmlBase::OutStream_XmlBase(OutStreamBuff* out, CharPtr header, CharPtr mainTag, CharPtr mainTagName, bool needsIndent, const AbstrPropDef* primaryPropDef, FormattingFlags flgs)
:	OutStreamBase(out, needsIndent, primaryPropDef, flgs)
,	m_RootElem(0)
{
	dms_assert(header);
	dms_assert(mainTag);
	dms_assert(mainTagName);

	if (*header)
		m_OutStream << header;

	if (*mainTag)
	{  
		m_RootElem = new XML_OutElement(*this, mainTag, mainTagName, ClosePolicy::pairedOnNewline);
		CloseAttrList();
		--m_Level; // keep level at zero for various purposes.
	}
}

OutStream_XmlBase::~OutStream_XmlBase()
{
	if (m_RootElem)
		delete m_RootElem;
}


//=============== private pseudo members

bool OutStream_XmlBase_IsSpecialChar(char ch)
{
	return (!ch) || CharGetSymbol(ch) || (ch == '\n');
}

inline void OutStream_XmlBase_WriteChar(OutStream_XmlBase* self, char ch)
{
	CharPtr symb = CharGetSymbol(ch);
	if (symb)
		self->FormattingStream() << '&' << symb << ';';
	else if (ch == '\n')
	{
		self->FormattingStream() << "<BR/>";
		self->NewLine();
	}
	else
		self->FormattingStream() << ch;
}

inline void OutStream_XmlBase_WriteAttrChar(OutStream_XmlBase* self, char ch)
{
	CharPtr symb = CharGetSymbol(ch);
	if (symb)
		self->FormattingStream() << '&' << symb << ';';
	else 
		self->FormattingStream() << ch;
}

//=============== public members

void OutStream_XmlBase::WriteName(XML_OutElement& elem, CharPtr itemName)
{
	WriteAttr("name", itemName);
	m_NrSubTags = 0;
}

void OutStream_XmlBase::DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag)
{
	dms_assert(tagName);
	dms_assert(tagValue);
	if (!tagValue || !*tagValue) 
		return;

	DumpSubTagDelim();
	XML_OutElement xmlElem(*this, tagName, "");
	*this << tagValue;
}

void OutStream_XmlBase::DumpSubTagDelim()
{
	m_NrSubTags++;
}


void OutStream_XmlBase_WriteEncoded(OutStream_XmlBase* self, CharPtr data)
{
	auto& buffer = self->FormattingStream().Buffer();
	while (true)
	{
		SizeT i = 0;
		while (!OutStream_XmlBase_IsSpecialChar(data[i]))
			++i;
		buffer.WriteBytes(data, i);
		data += i;
		if (!*data)
			return;
		OutStream_XmlBase_WriteChar(self, *data++);
	}
}

void OutStream_XmlBase_WriteEncoded(OutStream_XmlBase* self, CharPtr data, CharPtr dataEnd)
{
	auto& buffer = self->FormattingStream().Buffer();
	while (true)
	{
		SizeT i = 0;
		while ((data+i) != dataEnd && !OutStream_XmlBase_IsSpecialChar(data[i]))
			++i;
		buffer.WriteBytes(data, i);
		data += i;
		if (data == dataEnd || !*data)
			return;
		OutStream_XmlBase_WriteChar(self, *data++);
	}
}

void OutStream_XmlBase::WriteValue(CharPtr data)
{
	CloseAttrList();
	OutStream_XmlBase_WriteEncoded(this, data);
}


void OutStream_XmlBase::WriteValueWithConfigSourceDecorations(CharPtr data)
{
	CloseAttrList();

	std::size_t dataSize = StrLen(data);
	std::size_t currPos = 0, currLineNumber = 0;

	while (currPos < dataSize)
	{
		auto currLineEnd = std::find(data+currPos, data+dataSize, '\n')-data;

		auto lineView = std::string_view(data + currPos, data+currLineEnd);
		auto round_bracked_open_pos = lineView.find_first_of('(');
		auto comma_pos = lineView.find_first_of(',');
		auto round_bracked_close_pos = lineView.find_first_of(')');
		auto illegal_anglebracketopen_pos = lineView.find_first_of('<');
		auto illegal_anglebracketclose_pos = lineView.find_first_of('>');
		auto illegal_placeholder_symbol_pos = lineView.find_first_of('%');
		auto illegal_symbol_pos = Min(illegal_anglebracketopen_pos, Min(illegal_anglebracketclose_pos, illegal_placeholder_symbol_pos));
		static_assert(std::string::npos > 0);

		if (round_bracked_open_pos < comma_pos 
			&& comma_pos < round_bracked_close_pos 
			&& round_bracked_close_pos < illegal_symbol_pos
			)
		{
//			auto filename = lineView.substr(0, round_bracked_open_pos);
//			auto line_number = lineView.substr(round_bracked_open_pos + 1, comma_pos - (round_bracked_open_pos + 1));
//			auto col_number = lineView.substr(comma_pos + 1, round_bracked_close_pos - (comma_pos + 1));

			auto currEnd = currPos + round_bracked_close_pos + 1;
			auto ecfRef = CharPtrRange(data + currPos, data + currEnd);
			auto ecsURL = mySSPrintF("editConfigSource:%s", ecfRef);

			XML_hRef hRef(*this, ecsURL.AsRange());
			CloseAttrList();

			OutStream_XmlBase_WriteEncoded(this, ecfRef.first, ecfRef.second);
			currPos = currEnd;
		}

		OutStream_XmlBase_WriteEncoded(this, data+currPos, data + currLineEnd + 1);
		currPos = currLineEnd + 1;

		currLineNumber++;
	}
}

void OutStream_XmlBase::WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr)
{
	CloseAttrList();
	while (true)
	{
		SizeT i = 0; 
		while (maxSize && !OutStream_XmlBase_IsSpecialChar(data[i]))
			++i, --maxSize;
		m_OutStream.Buffer().WriteBytes(data, i);
		data += i;
		if (!*data)
			return;
		if (!maxSize--)
		{
			(*this) << moreIndicationStr;
			break;
		}
		OutStream_XmlBase_WriteChar(this, *data++);
	}
}

void OutStream_XmlBase::WriteAttr(CharPtr name, CharPtr value)
{
	if (!value || !*value)
		return;

	AttrDelim();

	m_OutStream << " " << name << "=\"";
	while (*value != 0)
		OutStream_XmlBase_WriteAttrChar(this, *value++);
	m_OutStream << "\"";
}

void OutStream_XmlBase::WriteAttr(CharPtr name, bool value)
{
	AttrDelim();

	m_OutStream 
		<<	" " << name << "=" 
		<<	(value ? "TRUE" : "FALSE");
}

void OutStream_XmlBase::WriteAttr(CharPtr name, UInt32 value)
{
	AttrDelim();

	m_OutStream << " " << name << "=" << value;
}

void OutStream_XmlBase::WriteInclude(CharPtr includeHref)
{
	NewLine();
	// <xinclude:include href="common.xml#xptr(a/b)"/>
	XML_OutElement elem(*this, "xinclude:include", "", ClosePolicy::nonPairedElement); // tagName, but no objName and not paired
	WriteAttr("href", includeHref);
}

//=============== private members

void OutStream_XmlBase::OpenTag(CharPtr tagName, ClosePolicy cp)
{
	if (cp == ClosePolicy::nonPairedElement || cp == ClosePolicy::pairedOnNewline)
		NewLine();

	m_OutStream << "<";

	m_OutStream << tagName;
}

void OutStream_XmlBase::CloseTag(CharPtr tagName, ClosePolicy cp)
{
	switch (cp)
	{
	case ClosePolicy::pairedWithTabbedSeparator:
		m_OutStream << "\t"; break;
	case ClosePolicy::pairedOnNewline:
		NewLine(); break;
	}
	m_OutStream << "</" << tagName << ">";
}

void OutStream_XmlBase::AttrDelim()
{
	dms_assert(m_CurrElem);
	if (m_CurrElem->IncAttrCount() )
		NewLine();
}

void OutStream_XmlBase::CloseAttrList()
{
	if (!m_CurrElem) 
		return; // already Closed

	if (!m_CurrElem->IsPaired()) 
		m_OutStream << "/";
	m_OutStream << ">";

	m_CurrElem = nullptr;
}

//----------------------------------------------------------------------
// OutStream_XML
//----------------------------------------------------------------------

OutStream_XML::OutStream_XML(OutStreamBuff* out, CharPtr docType, const AbstrPropDef* primaryPropDef)
:	OutStream_XmlBase(out,
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	,	(*docType) ? "DocType" : "", docType
	,	true /*needsIndent*/
	,	primaryPropDef
	,	FormattingFlags::None
	)
{
}

//----------------------------------------------------------------------
// OutStream_HTM
//----------------------------------------------------------------------

OutStream_HTM::OutStream_HTM(OutStreamBuff* out, CharPtr docType, const AbstrPropDef* primaryPropDef)
:	OutStream_XmlBase(out, "", docType, "", false, primaryPropDef, FormattingFlags::None)
{
}

//----------------------------------------------------------------------
// OutStream_DMS
//----------------------------------------------------------------------

OutStream_DMS::OutStream_DMS(OutStreamBuff* out, const AbstrPropDef* primaryPropDef)
	:	OutStreamBase(out, true /*needsIndent*/, primaryPropDef, FormattingFlags::None)
{
}

void OutStream_DMS::WriteInclude(CharPtr includeHref)
{
	NewLine();
	m_OutStream << "#include <" << includeHref << ">";
}

//=============== public members

void OutStream_DMS::WriteName(XML_OutElement& elem, CharPtr itemName)
{
	m_OutStream << " " << itemName;
	m_NrSubTags = 0;
}

void OutStream_DMS::BeginSubItems()
{
	--m_Level;
	NewLine();
	*this << "{";
	++m_Level;
}

void OutStream_DMS::ItemEnd()
{
	m_OutStream << ";";
	NewLine();
}

void OutStream_DMS::EndSubItems()
{
	--m_Level;
	NewLine();
	*this <<  "}";
	++m_Level;
}

void OutStream_DMS::DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag)
{
	dms_assert(tagName);
	dms_assert(tagValue);
	if (!tagValue || !*tagValue) 
		return;

	DumpSubTagDelim();
	if (isPrimaryTag)
		*this << " = " << tagValue;
	else
		*this << tagName << " = " << DoubleQuote(tagValue).c_str();
}

void OutStream_DMS::DumpSubTagDelim()
{
	*this << ((!m_NrSubTags) ? ":" : ",");
	NewLine();
	m_NrSubTags++;
}

void OutStream_DMS::WriteValue(CharPtr data)
{
	CloseAttrList();
	m_OutStream << data;
}

void OutStream_DMS::WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr)
{
	dms_assert(maxSize);
	CloseAttrList();
	UInt32 strLen = StrLen(data);
	if (strLen <= maxSize)
		m_OutStream.Buffer().WriteBytes(data, strLen);
	else
	{
		m_OutStream.Buffer().WriteBytes(data, maxSize);
		m_OutStream << moreIndicationStr;
	}
}


void OutStream_DMS::WriteAttr(CharPtr name, CharPtr value)
{
	if (!value || !*value)
		return;

	AttrDelim();
	m_OutStream << value;
}

void OutStream_DMS::WriteAttr(CharPtr name, bool value)
{
	AttrDelim();
	m_OutStream 
		<<	" " << name << "=" 
		<<	(value ? "\"TRUE\"" : "\"FALSE\"");
}

void OutStream_DMS::WriteAttr(CharPtr name, UInt32 value)
{
	AttrDelim();
	m_OutStream << " " << name << "=\"" << value << "\"";
}

//=============== private members

void OutStream_DMS::OpenTag(CharPtr tagName, ClosePolicy cp)
{
	if (cp == ClosePolicy::nonPairedElement || cp == ClosePolicy::pairedOnNewline)
		NewLine();
	m_OutStream << tagName;
}

void OutStream_DMS::CloseTag(CharPtr tagName, ClosePolicy cp)
{
}

void OutStream_DMS::AttrDelim()
{
	dms_assert(m_CurrElem);
	if (!m_CurrElem->IncAttrCount())
		m_OutStream << "(";
	else
		m_OutStream << ",";
}

void OutStream_DMS::CloseAttrList()
{
	if (!m_CurrElem) 
		return; // already Closed or never opened.

	if (m_CurrElem->AttrCount())
		m_OutStream << ")";

	m_CurrElem = nullptr;
}

//----------------------------------------------------------------------
// XML_OutElement
//----------------------------------------------------------------------

XML_OutElement::XML_OutElement(OutStreamBase& xmlStream, CharPtr tagName, CharPtr objName, ClosePolicy cp)
	: m_XmlStream(xmlStream)
	, m_ClosePolicy(cp)
	, m_TagName(tagName)
	, m_AttrCount(0)
	, m_HasSubItems(false)
{
	xmlStream.CloseAttrList();
	m_XmlStream.m_CurrElem = this;

	m_XmlStream.OpenTag(tagName, cp);
	m_XmlStream.WriteName(*this, objName);

	if (IsPaired())
		++m_XmlStream.m_Level;
}

XML_OutElement::~XML_OutElement() 
{
	m_XmlStream.CloseAttrList();
	if (!m_HasSubItems)
		m_XmlStream.ItemEnd();
	if (IsPaired())
	{
		if (m_XmlStream.m_Level) // prevent underflow due to non-levelled docType tag
			--m_XmlStream.m_Level;
		m_XmlStream.CloseTag(m_TagName, m_ClosePolicy);
	}
}
void XML_OutElement::SetHasSubItems()
{
	m_HasSubItems = true;
}

//----------------------------------------------------------------------
// XML_hRef
//----------------------------------------------------------------------

XML_hRef::XML_hRef(OutStreamBase& xmlStream, CharPtr url)
:	XML_OutElement(xmlStream, "A")
{
	xmlStream.WriteAttr("href", url);
}

XML_hRef::XML_hRef(OutStreamBase& xmlStream, CharPtrRange url)
	: XML_OutElement(xmlStream, "A")
{
	SharedStr urlStr(url); // TODO: make progres with CharPtrRange as primary string-in type.
	xmlStream.WriteAttr("href", urlStr.c_str());
}

//----------------------------------------------------------------------
// XML_DataBracket
//----------------------------------------------------------------------

XML_DataBracket::XML_DataBracket(OutStreamBase& xmlStream) : m_Stream(xmlStream)
{
	if (xmlStream.GetSyntaxType() != OutStreamBase::ST_DMS)
		m_DataElement.assign(new XML_OutElement(xmlStream, "DATA", ""));
	else
		m_Stream << "[";
}

XML_DataBracket::~XML_DataBracket()
{
	if (m_Stream.GetSyntaxType() == OutStreamBase::ST_DMS)
		m_Stream << "]";
}


//----------------------------------------------------------------------
// C-Style interface 
//----------------------------------------------------------------------

RTC_CALL struct OutStreamBase*
DMS_CONV XML_OutStream_Create(OutStreamBuff* out, OutStreamBase::SyntaxType st, CharPtr docType, const AbstrPropDef* primaryPropDef)
{
	DMS_CALL_BEGIN
		switch (st)
		{
			case OutStreamBase::ST_XML: return new OutStream_XML(out, docType, primaryPropDef);
			case OutStreamBase::ST_HTM: return new OutStream_HTM(out, docType, primaryPropDef);
			case OutStreamBase::ST_DMS: return new OutStream_DMS(out, primaryPropDef);
		}
	DMS_CALL_END
	return nullptr;
}

RTC_CALL void
DMS_CONV XML_OutStream_Destroy(OutStreamBase* xml)
{
	delete xml;
}

RTC_CALL void 
DMS_CONV XML_OutStream_WriteText(OutStreamBase* xmlStr, CharPtr txt)
{
	dms_assert(xmlStr);
	*xmlStr << txt;
}


/********** PropDef Reporting Interface **********/

#include "dbg/debug.h"
#include "mci/PropDef.h"
#include "mci/ValueClass.h"

RTC_CALL void DMS_CONV XML_ReportPropDef(OutStreamBase* xmlStr, AbstrPropDef* pd)
{
	XML_OutElement xml_pd(*xmlStr,"PropDef", pd->GetName().c_str(), ClosePolicy::nonPairedElement);
		xmlStr->WriteAttr("ClassName", pd->GetItemClass()->GetName().c_str());
		xmlStr->WriteAttr("ValueClass", pd->GetValueClass()->GetName().c_str());
}

RTC_CALL void DMS_CONV XML_ReportSchema(
	OutStreamBase* xmlStr, 
	const Class* cls, 
	bool withSubclasses )
{
	XML_OutElement xml_cls(*xmlStr,"ClassDef", cls->GetName().c_str());

	// Report associated property defs
	AbstrPropDef* pd = cls->GetLastPropDef();
	while (pd) {
		XML_ReportPropDef(xmlStr, pd);
		pd = pd->GetPrevPropDef();
	}

	if (!withSubclasses)
		return;

	// Report Inherited base classes
	const Class* base= cls->GetBaseClass();
	if (base)
		XML_OutElement xml_baseClass(*xmlStr, "BaseClass", base->GetName().c_str(), ClosePolicy::nonPairedElement);

	// Report subClasses
	cls = cls->GetLastSubClass();
	while (cls)
	{
		XML_ReportSchema(xmlStr, cls, true);
		cls = cls->GetPrevClass();
	}
}

