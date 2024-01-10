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
#include "TicPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "TreeItemContexthandle.h"

#include "dbg/DmsCatch.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h" 

//----------------------------------------------------------------------
// class  : TreeItemContextHandle
//----------------------------------------------------------------------

TreeItemContextHandle::TreeItemContextHandle(const TreeItem* obj, CharPtr role)
	:	m_Obj(obj)
	,	m_Role(role) 
{
	dms_assert(GetPrev() != this);
}

TreeItemContextHandle::TreeItemContextHandle(const TreeItem* obj, const Class* cls, CharPtr role)
	:	m_Obj(0)
	,	m_Role(role) 
{
	CheckPtr(obj, cls, role);
	m_Obj = obj;
	dms_assert(m_Obj);

	dms_assert(GetPrev() != this);
}

TreeItemContextHandle::~TreeItemContextHandle()
{}

auto TreeItemContextHandle::ItemAsStr() const -> SharedStr
{
	auto cci = GetItem();
	if (!cci)
		return {};
	return cci->GetSourceName();
}

void TreeItemContextHandle::GenerateDescription()
{
	CharPtr role = m_Role ? m_Role : "TreeItem";

	if (m_Obj)
	{
		SharedStr objNameStr = m_Obj->GetFullName();
		SetText(
			mgFormat2SharedStr("while in %1%( %2%: %3% )"
				,	role
				,	objNameStr.empty() ? m_Obj->GetName().c_str() : objNameStr.c_str()
				,	m_Obj->GetClsName()
			)
		);
	}
	else
		SetText(SharedStr());
}

//----------------------------------------------------------------------
// SystemContext for providing system info in error messages
//----------------------------------------------------------------------

#include "xml/PropWriter.h"
#include "RtcInterface.h"
#include "SessionData.h"

void GenerateSystemInfo(AbstrPropWriter& apw, const TreeItem* curr)
{
	// Default handling when no configured MetaInfo
	dms_assert(curr);

	apw.OpenSection("DefaultInfo");

	if (curr)
		apw.WriteKey("FullName", curr->GetFullName().c_str());

	apw.WriteKey("GeoDmsVersion", DMS_GetVersion());
	apw.WriteKey("SessionStartTime", GetSessionStartTimeStr());
	apw.WriteKey("CurrentTime", GetCurrentTimeStr());
	apw.WriteKey("StatusFlags",
		mySSPrintF("0x%x = %s%s%s%s%s%s%s%s%s%s"
			, GetRegStatusFlags()
			, (GetRegStatusFlags() & RSF_AdminMode) ? "AdminMode " : ""
			, (GetRegStatusFlags() & RSF_SuspendForGUI) ? "SuspendForGUI " : ""
			, (GetRegStatusFlags() & RSF_ShowStateColors) ? "ShowStateColors " : ""
			, (GetRegStatusFlags() & RSF_TraceLogFile) ? "TraceLogFile " : ""
			, (GetRegStatusFlags() & RSF_TreeViewVisible) ? "TreeViewVisible " : ""
			, (GetRegStatusFlags() & RSF_DetailsVisible) ? "DetailsVisible " : ""
			, (GetRegStatusFlags() & RSF_EventLogVisible) ? "EventLogVisible " : ""
			, (GetRegStatusFlags() & RSF_ToolBarVisible) ? "ToolBarVisible " : ""
			, (GetRegStatusFlags() & RSF_MultiThreading1) ? "MultiThreading1 " : ""
			, (GetRegStatusFlags() & RSF_MultiThreading2) ? "MultiThreading2 " : ""
			)
		);
	apw.CloseSection();
}

void WriteContextImpl(const TreeItem* curr, OutStreamBuff* osb)
{
	dms_assert(osb);

	XmlPropWriterBase writer(osb);
	GenerateSystemInfo(writer, curr);
}

//void AbstrStorageManager::ExportMetaInfo(const TreeItem* storageHolder, const TreeItem* curr)
struct SystemContextHandle : AbstrContextHandle
{
	bool Describe(FormattedOutStream& fos) override
	{
		const TreeItem* curr = nullptr;
		AbstrContextHandle* ach = AbstrContextHandle::GetLast();
		while (ach)
		{
			if (dynamic_cast<TreeItemContextHandle*>(ach))
				curr = static_cast<TreeItemContextHandle*>(ach)->GetItem();
			ach = ach->GetPrev();
		}
		if (!curr && SessionData::Curr())
			curr = SessionData::Curr()->GetConfigRoot();
		if (curr)
		{
			try {
				WriteContextImpl(curr, &(fos.Buffer()));
			}
			catch (...)
			{
				auto err = catchException(true);
				if (err)
					fos << "MetaInfo generation caused " << err->GetAsText();
			}
		}
		return true;
	}
};


// SystemContextHandle systemContext;

