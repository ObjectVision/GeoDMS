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

#ifndef __XML_PROPWRITERE_H
#define __XML_PROPWRITERE_H

#include "ser/FileStreamBuff.h"
#include "utl/Environment.h"
#include "xml/XmlOut.h"


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
		m_CurrSection.assign(new XML_OutElement(m_Xml, name));
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
	OwningPtr<XML_OutElement> m_CurrSection;
};

struct XmlPropWriter : FileOutStreamBuff, XmlPropWriterBase
{
	XmlPropWriter(WeakStr fileName)
		: FileOutStreamBuff(fileName, nullptr, true)
		, XmlPropWriterBase(this)
	{}
};


#endif // __XML_PROPWRITERE_H