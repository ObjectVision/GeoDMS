// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#pragma once

//	OutStream
//	Typical usage:
//		ofstream os("file.xml");
//		OutStream_??? xmlOut(os);
//		XML_OutElement elem(xmlOut, "DOCTYPE", "");
//		{
//			xmlOut.WriteAttr(elem, "name", "value);
//		}
//		{
//			xmlOut << data;
//			XML_OutElement subElem(xmlOut, "ELEMENTTYPE", "name);
//			...
//		}	

#ifndef __XML_XMLOUT_H
#define __XML_XMLOUT_H

#include "dbg/check.h"
#include "ser/FormattedStream.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedStr.h"

struct XML_OutElement;
class ImpStreamBuff;
class OutStreamBuff;

enum class ClosePolicy {
	nonPairedElement,
	pairedButWithoutSeparator,
	pairedWithTabbedSeparator,
	pairedOnNewline
};

//----------------------------------------------------------------------
// OutStreamBase
//----------------------------------------------------------------------

const UInt32 MAX_TEXTOUT_SIZE = 400;

struct OutStreamBase {

	enum SyntaxType { ST_XML, ST_DMS, ST_HTM, ST_MD, ST_Count, ST_Unknown = -1 };

	RTC_CALL OutStreamBase(OutStreamBuff* out, bool needsIndent, const AbstrPropDef* primaryPropDef, FormattingFlags flgs);
	virtual ~OutStreamBase() {}


	virtual void WriteName(XML_OutElement& elem, CharPtr itemName) = 0;

	RTC_CALL virtual void BeginSubItems();
	RTC_CALL virtual void ItemEnd();
	RTC_CALL virtual void EndSubItems();

	virtual void DumpSubTagDelim() =0;

	OutStreamBase& operator << (CharPtr data) { WriteValue(data); return *this; }

	virtual void WriteValue (CharPtr data) = 0;
	RTC_CALL virtual void WriteValueWithConfigSourceDecorations(CharPtr data);
	virtual void WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr) = 0;
	void WriteTrimmed(CharPtr data) { WriteValueN(data, MAX_TEXTOUT_SIZE - 3, "..."); }
	void WriteRange(CharPtr first, CharPtr last) { WriteValueN(first, last - first, ""); }

	virtual void WriteAttr(CharPtr name, CharPtr value) =0;
	virtual void WriteAttr(CharPtr name, bool value) =0;
	virtual void WriteAttr(CharPtr name, UInt32 value) =0;

	virtual void WriteInclude(CharPtr includeHref) = 0;

	virtual SyntaxType GetSyntaxType() = 0;

	RTC_CALL void DumpPropList(const Object* self, const Class* cls);
	RTC_CALL void DumpPropList(const Object* self);

	RTC_CALL void DumpSubTags(const Object* self, const Class* cls);
	RTC_CALL void DumpSubTags(const Object* self);

	UInt32     GetLevel()      { return m_Level;  }
	WeakStr    GetFileName()   { return  m_OutStream.Buffer().FileName(); }
	bool       HasFileName()   { return !m_OutStream.Buffer().FileName().empty(); }

	auto&           FormattingStream() { return m_OutStream; }
	FormattingFlags GetFormattingFlags() const { return m_OutStream.GetFormattingFlags();  }

	void NewLine();

private:
	friend struct XML_OutElement;

	virtual void DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag) =0;

	void DumpSubTag(const AbstrPropDef* propDef, CharPtr tagValue, bool isPrimaryTag);

	virtual void OpenTag (CharPtr tagName, ClosePolicy cp) =0;
	virtual void CloseTag(CharPtr tagName, ClosePolicy cp) =0;

	virtual void AttrDelim    () =0;
	virtual void CloseAttrList() =0;


protected:
	FormattedOutStream  m_OutStream;
	UInt32              m_Level = 0;
	UInt32              m_NrSubTags = 0;
	XML_OutElement*     m_CurrElem = nullptr;
	const AbstrPropDef* m_PrimaryPropDef = nullptr;

private:
	bool                m_NeedsIndent;
	streamsize_t        m_LastNewLinePos;
};


//----------------------------------------------------------------------
// OutStream_XML
//----------------------------------------------------------------------

struct OutStream_XmlBase : OutStreamBase
{
protected:
	RTC_CALL OutStream_XmlBase(OutStreamBuff* out, CharPtr header, CharPtr mainTag, CharPtr mainTagName, bool needsIndent, const AbstrPropDef* primaryPropDef, FormattingFlags flgs = FormattingFlags::None);
	RTC_CALL ~OutStream_XmlBase();

public:
	RTC_CALL void WriteName(XML_OutElement& elem, CharPtr itemName) override;

	RTC_CALL void DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag) override;
	RTC_CALL void DumpSubTagDelim() override;

	RTC_CALL void WriteValue (CharPtr data) override;
	RTC_CALL void WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr) override;
	RTC_CALL void WriteValueWithConfigSourceDecorations(CharPtr data) override;

	RTC_CALL void WriteAttr(CharPtr name, CharPtr value) override;
	RTC_CALL void WriteAttr(CharPtr name, bool value) override;
	RTC_CALL void WriteAttr(CharPtr name, UInt32 value) override;

	RTC_CALL void WriteInclude(CharPtr includeHref) override;

private:
	RTC_CALL void OpenTag (CharPtr tagName, ClosePolicy cp) override;
	RTC_CALL void CloseTag(CharPtr tagName, ClosePolicy cp) override;

	RTC_CALL void AttrDelim    () override;
	RTC_CALL void CloseAttrList() override;

private:
	XML_OutElement* m_RootElem;
};

struct OutStream_XML : OutStream_XmlBase
{
	RTC_CALL OutStream_XML(OutStreamBuff* out, CharPtr docType, const AbstrPropDef* primaryPropDef);

	SyntaxType GetSyntaxType() override { return ST_XML; }
};

struct OutStream_HTM :OutStream_XmlBase
{
	RTC_CALL OutStream_HTM(OutStreamBuff* out, CharPtr docType, const AbstrPropDef* primaryPropDef);

	SyntaxType GetSyntaxType() override { return ST_HTM; }
};


struct OutStream_DMS :OutStreamBase
{
	RTC_CALL OutStream_DMS(OutStreamBuff* out, const AbstrPropDef* primaryPropDef);

	RTC_CALL void WriteName(XML_OutElement& elem, CharPtr itemName) override;

	RTC_CALL void BeginSubItems() override;
	RTC_CALL void ItemEnd() override;
	RTC_CALL void EndSubItems() override;

	RTC_CALL void DumpSubTag(CharPtr tagName, CharPtr tagValue, bool isPrimaryTag) override;
	RTC_CALL void DumpSubTagDelim() override;

	RTC_CALL void WriteValue (CharPtr data) override;
	RTC_CALL void WriteValueN(CharPtr data, UInt32 maxSize, CharPtr moreIndicationStr) override;

	RTC_CALL void WriteAttr(CharPtr name, CharPtr value) override;
	RTC_CALL void WriteAttr(CharPtr name, bool value) override;
	RTC_CALL void WriteAttr(CharPtr name, UInt32 value) override;

	RTC_CALL void WriteInclude(CharPtr includeHref) override;

	SyntaxType GetSyntaxType() override { return ST_DMS; }

private:
	RTC_CALL void OpenTag (CharPtr tagName, ClosePolicy cp) override;
	RTC_CALL void CloseTag(CharPtr tagName, ClosePolicy cp) override;

	RTC_CALL void AttrDelim    () override;
	RTC_CALL void CloseAttrList() override;

};

//----------------------------------------------------------------------
// XML_OutElement
//----------------------------------------------------------------------

struct XML_OutElement
{
	RTC_CALL XML_OutElement(OutStreamBase& xmlStream, CharPtr tagName, CharPtr objName = "", ClosePolicy cp = ClosePolicy::pairedOnNewline);
	RTC_CALL void SetHasSubItems();
	RTC_CALL ~XML_OutElement();

	inline bool IncAttrCount()    { return m_AttrCount++; } 
	inline bool AttrCount() const { return m_AttrCount;   } 
	inline bool IsPaired()  const { return m_ClosePolicy != ClosePolicy::nonPairedElement;    }
	inline OutStreamBase& OutStream() { return m_XmlStream; }
	inline const OutStreamBase& OutStream() const { return m_XmlStream; }

	FormattingFlags GetFormattingFlags() const { return OutStream().GetFormattingFlags(); }

  private: 
	OutStreamBase& m_XmlStream;
	CharPtr        m_TagName;
	ClosePolicy    m_ClosePolicy;
	bool           m_HasSubItems;
	UInt32         m_AttrCount;
};

struct XML_h1 : XML_OutElement
{
	XML_h1(OutStreamBase& xmlStream, CharPtr caption) : XML_OutElement(xmlStream, "h1")
	{
		xmlStream << caption;
	}
};

struct XML_h2 : XML_OutElement
{
	XML_h2(OutStreamBase& xmlStream, CharPtr caption) : XML_OutElement(xmlStream, "h2")
	{
		xmlStream << caption;
	}
};

struct XML_h3 : XML_OutElement
{
	XML_h3(OutStreamBase& xmlStream, CharPtr caption) : XML_OutElement(xmlStream, "h3")
	{
		xmlStream << caption;
	}
};

struct XML_hRef : XML_OutElement
{
	RTC_CALL XML_hRef(OutStreamBase& xmlStream, CharPtr url);
	RTC_CALL XML_hRef(OutStreamBase& xmlStream, CharPtrRange url);
};

inline void hRefWithText(OutStreamBase& xmlStream, CharPtr value, CharPtr hRef)
{
	XML_hRef xmlElemA(xmlStream, hRef);
	xmlStream.WriteTrimmed(value);

}

struct XML_DataBracket
{
	RTC_CALL  XML_DataBracket(OutStreamBase& xmlStream);
	RTC_CALL ~XML_DataBracket();

private:
	OwningPtr<XML_OutElement> m_DataElement;
	OutStreamBase&            m_Stream;
};

class AbstrPropDef;
class Class;

//----------------------------------------------------------------------
// C-Style interface 
//----------------------------------------------------------------------

extern "C" {

RTC_CALL struct OutStreamBase*
DMS_CONV XML_OutStream_Create(OutStreamBuff* streamBuff, OutStreamBase::SyntaxType st, CharPtr docType, const AbstrPropDef* primaryPropDef);

RTC_CALL void 
DMS_CONV XML_OutStream_Destroy(OutStreamBase*);

RTC_CALL void 
DMS_CONV XML_OutStream_WriteText(OutStreamBase* xmlStr, CharPtr txt);

RTC_CALL void 
DMS_CONV XML_ReportPropDef(OutStreamBase* xmlStr, AbstrPropDef* pd);

RTC_CALL void 
DMS_CONV XML_ReportSchema(
	OutStreamBase* xmlStr, 
	const Class* cls, 
	bool withSubclasses );

} // extern "C"

#endif // __XML_XMLOUT_H