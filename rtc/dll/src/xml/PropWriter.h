// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#pragma once

#ifndef __XML_PROPWRITERE_H
#define __XML_PROPWRITERE_H

#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "xml/XMLOut.h"


struct AbstrPropWriter
{
	virtual void OpenSection(CharPtr name) = 0;
	virtual void CloseSection() {}
	virtual void WriteKey(CharPtr name, CharPtr value) = 0;
	virtual ~AbstrPropWriter() {};

	void WriteKey(CharPtr name, WeakStr value) { WriteKey(name, value.c_str()); }

};

struct IniPropWriter : AbstrPropWriter
{
	IniPropWriter(WeakStr fileName)
		: m_FileName(fileName)
	{}

	void OpenSection(CharPtr name) override
	{
		m_SectionName = name;
	}
	void WriteKey(CharPtr name, CharPtr value) override
	{
		SetConfigKeyString(m_FileName, m_SectionName.c_str(), name, value);
	}
	SharedStr m_FileName, m_SectionName;
};

struct XmlPropWriterBase : AbstrPropWriter
{
	XmlPropWriterBase(OutStreamBuff* osb)
		: m_Xml(osb, "GeoDmsMetaInfo", nullptr)
	{}

	void OpenSection(CharPtr name) override
	{
		m_CurrSection.reset(new XML_OutElement(m_Xml, name));
	}
	void CloseSection() override
	{
		m_CurrSection.reset();
	}
	void WriteKey(CharPtr name, CharPtr value) override
	{
		XML_OutElement keyAttr(m_Xml, name);
		m_Xml.WriteValue(value);
	}

	OutStream_XML m_Xml;
	std::unique_ptr<XML_OutElement> m_CurrSection;
};

struct XmlPropWriter : FileOutStreamBuff, XmlPropWriterBase
{
	XmlPropWriter(WeakStr fileName)
		: FileOutStreamBuff(fileName, true)
		, XmlPropWriterBase(this)
	{}
};


#endif // __XML_PROPWRITERE_H