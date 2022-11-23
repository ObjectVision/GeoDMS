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

#if !defined(__RTC_XCT_ERRMSG_H)
#define __RTC_XCT_ERRMSG_H

#include "ptr/SharedStr.h"
#include "ptr/SharedPtr.h"
#include "ptr/WeakPtr.h"
#include "ptr/PersistentSharedObj.h"
#include "utl/SourceLocation.h"

struct FormattedOutStream;
struct OutStreamBase;
struct Actor;
class Class;

struct ErrMsg {

	ErrMsg() {}
	RTC_CALL explicit ErrMsg(WeakStr msg, const PersistentSharedObj* ptr = nullptr);
	explicit ErrMsg(CharPtr msg, const PersistentSharedObj* ptr = nullptr) : ErrMsg(SharedStr(msg), ptr) {}

	RTC_CALL void TellWhere(const PersistentSharedObj* ptr);

	void TellExtra(CharPtr msg) { TellExtra(CharPtrRange(msg)); }
	RTC_CALL void TellExtra(CharPtrRange msg);

	template <class ...Args>
	void TellExtraF(CharPtr fmt, Args&& ...args)
	{
		TellExtra(mgFormat2string(fmt, std::forward<Args>(args)...).c_str());
	}
	bool MustReport() const
	{
		if (m_HasBeenReported)
			return false;
		m_HasBeenReported = true;
		return true;
	}

	SharedStr Why() const { return m_Why;  }
	RTC_CALL SharedStr GetAsText() const;
	SharedStr FullName() const { return m_FullName;  }

	SharedStr m_Why, m_FullName, m_Context;
//	WeakPtr<const Class> m_Class;

private:
	mutable bool m_HasBeenReported = false;

	friend RTC_CALL FormattedOutStream& operator <<(FormattedOutStream& str, const ErrMsg& value);
	friend RTC_CALL OutStreamBase&      operator <<(OutStreamBase&      str, const ErrMsg& value);
	friend bool IsDefaultValue(const ErrMsg& msg) { return msg.m_Why.empty(); }
};

extern leveled_critical_section sc_FailSection;

using ErrMsgPtr = std::shared_ptr<ErrMsg>;


#endif // __RTC_XCT_ERRMSG_H

