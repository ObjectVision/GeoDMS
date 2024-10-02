// Copyright (C) 1998-2024 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


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

