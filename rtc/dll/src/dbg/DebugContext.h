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

#if !defined(__DBG_DEBUGCONTEXT_H)
#define __DBG_DEBUGCONTEXT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DbgInterface.h"

#include "dbg/CheckPtr.h"
#include "ptr/PersistentSharedObj.h"
#include "ptr/SharedStr.h"
#include "set/Token.h"
struct FormattedOutStream;

/********** AbstrContextHandle **********/

struct AbstrContextHandle :noncopyable
{
	RTC_CALL AbstrContextHandle() noexcept;
	RTC_CALL ~AbstrContextHandle()  noexcept;

	RTC_CALL virtual bool Describe(FormattedOutStream& fos); // default: calls GetDescription
	
	RTC_CALL virtual CharPtr GetDescription();

	RTC_CALL AbstrContextHandle* GetPrev() const;
	static RTC_CALL AbstrContextHandle* GetLast();
	RTC_CALL UInt32 GetContextLevel() const;
	RTC_CALL virtual bool IsFinalContext() const { return false; }

protected:
	RTC_CALL void PushNotification();
	RTC_CALL void PopNotification();

private:
	AbstrContextHandle *m_Prev;
	
};

/********** FixedContextHandle **********/

struct FixedContextHandle : AbstrContextHandle
{
	FixedContextHandle(CharPtr msg) : m_Msg(msg) 
	{}

	RTC_CALL CharPtr GetDescription() override;

private:
	CharPtr m_Msg;
};

/********** SharedStrContextHandle **********/

struct SharedStrContextHandle : AbstrContextHandle
{
	SharedStrContextHandle(WeakStr msgStr) 
		: m_MsgStr(msgStr) 
	{}

	RTC_CALL CharPtr GetDescription() override;
private:
	SharedStr m_MsgStr;
};

/********** ContextHandle **********/

struct ContextHandle : AbstrContextHandle 
{
	RTC_CALL CharPtr GetDescription() override;

protected:
	RTC_CALL virtual void GenerateDescription() = 0;
	RTC_CALL         void SetText(WeakStr context);

private:
	SharedStr m_Context;
};

/********** ObjectContextHandle **********/

struct ObjectContextHandleBase : ContextHandle 
{
	ObjectContextHandleBase(const Object* obj, CharPtr role = nullptr) : m_Obj(obj), m_Role(role) {}

	RTC_CALL void GenerateDescription() override;
	RTC_CALL bool IsFinalContext() const override final;

protected:
	CharPtr m_Role;
	const Object* m_Obj;
};

struct ObjectContextHandle : ObjectContextHandleBase 
{
	ObjectContextHandle(const Object* obj, CharPtr role = nullptr)
		: ObjectContextHandleBase(obj, role) 
	{}

	ObjectContextHandle(const Object* obj, const Class* cls, CharPtr role = nullptr)
		: ObjectContextHandleBase(obj, role)
	{
		CheckPtr(obj, cls, role);
	}
};

struct ObjectIdContextHandle : ObjectContextHandle
{
	ObjectIdContextHandle(const Object* context, TokenID id, CharPtr role)
		: ObjectContextHandle(context, role), m_ID(id) 
	{}

protected:
	RTC_CALL void GenerateDescription() override;

private:
	TokenID m_ID;
};


/********** LambdaContextHandle **********/

template <typename GenerateFunc>
struct LambdaContextHandle: ContextHandle 
{
	LambdaContextHandle(GenerateFunc&& func = GenerateFunc())
		:	m_Func(func)
	{}

protected:
	void GenerateDescription() override
	{
		SetText(m_Func());
	}

protected:
	GenerateFunc m_Func;
};

template <typename Func>
LambdaContextHandle<Func> MakeLCH(Func&& func)
{
	return LambdaContextHandle<Func>(std::forward<Func>(func));
}

/********** CDebugCOutHandle  **********/

struct CDebugCOutHandle
{
	RTC_CALL CDebugCOutHandle();
	RTC_CALL ~CDebugCOutHandle();
};

/********** CDebugContextHandle  **********/

struct CDebugContextHandle : ContextHandle 
{
	RTC_CALL CDebugContextHandle(CharPtr className, CharPtr funcName, bool active);
	RTC_CALL ~CDebugContextHandle();

	RTC_CALL void LogTime(CharPtr action);

	bool m_Active;

protected:
	RTC_CALL void GenerateDescription() override;

	double RunningTime() const;

private:
	CharPtr    m_ClassName;
	CharPtr    m_FuncName;
	ClockTicks m_StartTime;
};

/********** helper funcs  **********/

RTC_CALL UInt32 GetCallCount();

#endif // __DBG_DEBUGCONTEXT_H