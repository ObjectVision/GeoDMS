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
#pragma hdrstop

#include "dbg/DmsCatch.h"
#include "xml/XMLOut.h"
#include "mci/Class.h"
#include "mci/PropDef.h"
#include "mci/PropDefEnums.h"
#include "xml/XmlConst.h"
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
		m_RootElem = new XML_OutElement(*this, mainTag, mainTagName, true);
		CloseAttrList();
		--m_Level; // keep level at zero for various purposes.
	}
}

OutStream_XmlBase::~OutStream_XmlBase()
{
	if (m_RootElem)
		delete m_RootElem;
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


void OutStream_XmlBase::WriteValue(CharPtr data)
{
	CloseAttrList();
	while (true)
	{
		SizeT i = 0;
		while (!IsSpecialChar(data[i]))
			++i;
		m_OutStream.Buffer().WriteBytes(data, i);
		data += i;
		if (!*data)
			return;
		WriteChar(*data++);
	}
}

void OutStream_XmlBase::WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr)
{
	CloseAttrList();
	while (true)
	{
		SizeT i = 0; 
		while (maxSize && !IsSpecialChar(data[i]))
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
		WriteChar(*data++);
	}
}

void OutStream_XmlBase::WriteAttr(CharPtr name, CharPtr value)
{
	if (!value || !*value)
		return;

	AttrDelim();

	m_OutStream << " " << name << "=\"";
	while (*value != 0)
		WriteChar(*value++);
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
	XML_OutElement elem(*this, "xinclude:include", "", false); // tagName, but no objName and not paired
	WriteAttr("href", includeHref);
}

//=============== private members

void OutStream_XmlBase::OpenTag(CharPtr tagName)
{
	NewLine();

	m_OutStream << "<";

	m_OutStream << tagName;
}

void OutStream_XmlBase::CloseTag(CharPtr tagName)
{
	NewLine();
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

	m_CurrElem = 0;
}

inline bool OutStream_XmlBase::IsSpecialChar(char ch) 
{
	return (!ch) || CharGetSymbol(ch) || (ch == '\n');
}

inline void OutStream_XmlBase::WriteChar(char ch) 
{
	CharPtr symb = CharGetSymbol(ch);
	if (symb) 
		m_OutStream << '&' << symb << ';';
	else if (ch == '\n')
	{
		m_OutStream << "<BR/>";
		NewLine();
	}
	else
		m_OutStream << ch;
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

void OutStream_DMS::OpenTag(CharPtr tagName)
{
	NewLine();
	m_OutStream << tagName;
}

void OutStream_DMS::CloseTag(CharPtr tagName)
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
// OutStream_MD
//----------------------------------------------------------------------

OutStream_MD::OutStream_MD(OutStreamBuff* out, const AbstrPropDef* primaryPropDef)
	: OutStreamBase(out, false /*needsIndent*/, primaryPropDef, FormattingFlags::None)
{
}

void OutStream_MD::WriteInclude(CharPtr includeHref)
{
	NewLine();
	m_OutStream << "#include <" << includeHref << ">";
}

//=============== public members

void OutStream_MD::WriteName(XML_OutElement& elem, CharPtr itemName)
{
	m_OutStream << "" << itemName;
	m_NrSubTags = 0;
}

void OutStream_MD::BeginSubItems()
{
	--m_Level;
	NewLine();
	*this << "{";
	++m_Level;
}

void OutStream_MD::ItemEnd()
{
	//m_OutStream << ";";
	//NewLine();
}

void OutStream_MD::EndSubItems()
{
	--m_Level;
	NewLine();
	*this << "}";
	++m_Level;
}

void OutStream_MD::DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag)
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

void OutStream_MD::DumpSubTagDelim()
{
	*this << ((!m_NrSubTags) ? ":" : ",");
	NewLine();
	m_NrSubTags++;
}

void OutStream_MD::WriteValue(CharPtr data)
{
	CloseAttrList();
	if (m_in_href)
		m_href.first = data;
	else
		m_cell_data += data;//m_OutStream << data;
}

void OutStream_MD::WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr)
{
	dms_assert(maxSize);
	CloseAttrList();
	UInt32 strLen = StrLen(data);
	if (strLen <= maxSize)
	{
		if (m_in_href)
			m_href.first = data;
		else
			m_cell_data += data;
		//m_OutStream.Buffer().WriteBytes(data, strLen);
	}
	else
	{
		if (m_in_href)
			m_href.first = data + std::string("...");
		else
			m_cell_data += data + std::string("...");
		//m_OutStream.Buffer().WriteBytes(data, maxSize);
		//m_OutStream << moreIndicationStr;
	}
}

void OutStream_MD::WriteAttr(CharPtr name, CharPtr value)
{
	if (!value || !*value)
		return;

	auto name_str = std::string(name);
	if (name_str.compare("bgcolor") == 0)
		return;

	else if (name_str.compare("href") == 0)
		m_href.second = value;
	else 
	{
		AttrDelim();
		m_OutStream << value;
	}
}

void OutStream_MD::WriteAttr(CharPtr name, bool value)
{
	AttrDelim();
	m_OutStream
		<< "" << name << "="
		<< (value ? "\"TRUE\"" : "\"FALSE\"");
}

void OutStream_MD::WriteAttr(CharPtr name, UInt32 value)
{
	if (std::string(name).compare("COLSPAN") == 0)
		m_cell_data = "";
	else 
		m_cell_data = " ";
	//AttrDelim();
	//m_OutStream << "" << name << "=\"" << value << "\"";
}

//=============== private members


void WriteTableToStream(FormattedOutStream& out, table_data& table)
{
	// write empty table header rows
	for (int j = 0; j < table[0].size(); j++)
	{
		out << "|   ";
	}
	out << "|\n";
	for (int j = 0; j < table[0].size(); j++)
	{
		out << "|---";
	}
	out << "|\n";

	for (auto& row : table)
	{
		if (row.empty())
			continue;

		for (auto& element : row)
		{
			out << "|" << element.c_str();
		}
		out << "|\n";
	}
}

void WriteDropdownToStream()
{
	/*<details>

		<summary>Tips for collapsed sections< / summary>

		### You can add a header

		You can add text within a collapsed section.

		You can add an image or a code block, too.

		```ruby
		puts "Hello World"
		```

	< / details>*/


}

void OutStream_MD::OpenTag(CharPtr tagName)
{
	//NewLine();
	auto tag = std::string(tagName);
	if (tag.compare("H2") == 0)
		m_OutStream << "# ";
	else if(tag.compare("A") == 0)
	{
		m_in_href = true;
	}
	else if (tag.compare("HR") == 0)
	{
		//m_OutStream << "\n"
		m_OutStream << "\n";
		WriteTableToStream(m_OutStream, m_table);
		m_table.clear();
		m_OutStream << "---\n";
	}
	else if (tag.compare("TD") == 0)
	{
		m_in_row_data = true;
		//m_OutStream << "|";
	}
	else if (tag.compare("TR") == 0)
	{
		m_in_row = true;
	}
	else if (tag.compare("TABLE") == 0)
		return;
	else if (tag.compare("BODY") == 0)
		return;
	else 
		m_OutStream << tagName;
}

void OutStream_MD::CloseTag(CharPtr tagName)
{
	auto tag = std::string(tagName);
	if (tag.compare("TABLE") == 0)
	{
		WriteTableToStream(m_OutStream, m_table);
		m_table.clear();
		m_in_table = false;
		return;
	}
	else if (tag.compare("TD")==0)
	{
		if (!m_cell_data.empty())
			m_table_row.push_back(m_cell_data);

		m_cell_data.clear();
		m_in_row_data = false;
	}
	else if (tag.compare("TR") == 0)
	{
		m_in_row = false;
		if (!m_table_row.empty() && m_table_row.size()>1)
			m_table.push_back(m_table_row);
		m_table_row.clear();
		//m_OutStream << "|";
	}
	else if (tag.compare("A") == 0)
	{
		m_in_href = false;
		std::string markdown_href = "[" + m_href.first + "]" + "(" + m_href.second + ")";
		m_href.first.clear();
		m_href.second.clear();

		if (m_in_row_data)
			m_cell_data += markdown_href;
		else
			m_OutStream << markdown_href.c_str();
		return;
	}
	else
	{
		int i = 0;
	}
}

void OutStream_MD::AttrDelim()
{
	dms_assert(m_CurrElem);
	if (!m_CurrElem->IncAttrCount())
		m_OutStream << "";
	else
		m_OutStream << ",";
}

void OutStream_MD::CloseAttrList()
{
	if (!m_CurrElem)
		return; // already Closed or never opened.

	if (m_CurrElem->AttrCount())
		m_OutStream << ""; //)

	m_CurrElem = nullptr;
}

//----------------------------------------------------------------------
// XML_OutElement
//----------------------------------------------------------------------

XML_OutElement::XML_OutElement(OutStreamBase& xmlStream, 
							   CharPtr tagName, CharPtr objName, bool isPaired)
	: m_XmlStream(xmlStream)
	, m_IsPaired(isPaired)
	, m_TagName(tagName)
	, m_AttrCount(0)
	, m_HasSubItems(false)
{
	xmlStream.CloseAttrList();
	m_XmlStream.m_CurrElem = this;

	m_XmlStream.OpenTag(tagName);
	m_XmlStream.WriteName(*this, objName);

	if (m_IsPaired) 
		++m_XmlStream.m_Level;
}

XML_OutElement::~XML_OutElement() 
{
	m_XmlStream.CloseAttrList();
	if (!m_HasSubItems)
		m_XmlStream.ItemEnd();
	if (m_IsPaired)
	{
		if (m_XmlStream.m_Level) // prevent underflow due to non-levelled docType tag
			--m_XmlStream.m_Level;
		m_XmlStream.CloseTag(m_TagName);
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

//----------------------------------------------------------------------
// XML_DataBracket
//----------------------------------------------------------------------

XML_DataBracket::XML_DataBracket(OutStreamBase& xmlStream) : m_Stream(xmlStream)
{
	if (xmlStream.GetSyntaxType() != OutStreamBase::ST_DMS)
		m_DataElement.assign(new XML_OutElement(xmlStream, "DATA", "",true));
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
			case OutStreamBase::ST_MD:  return new OutStream_MD(out, primaryPropDef);
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
	XML_OutElement xml_pd(*xmlStr,"PropDef", pd->GetName().c_str(), false);
		xmlStr->WriteAttr("ClassName", pd->GetItemClass()->GetName().c_str());
		xmlStr->WriteAttr("ValueClass", pd->GetValueClass()->GetName().c_str());
}

RTC_CALL void DMS_CONV XML_ReportSchema(
	OutStreamBase* xmlStr, 
	const Class* cls, 
	bool withSubclasses )
{
	XML_OutElement xml_cls(*xmlStr,"ClassDef", cls->GetName().c_str(), true);

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
		XML_OutElement xml_baseClass(*xmlStr, "BaseClass", base->GetName().c_str(), false);

	// Report subClasses
	cls = cls->GetLastSubClass();
	while (cls)
	{
		XML_ReportSchema(xmlStr, cls, true);
		cls = cls->GetPrevClass();
	}
}

