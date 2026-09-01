// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#if !defined(__DBG_DEBUGCONTEXT_H)
#define __DBG_DEBUGCONTEXT_H

//----------------------------------------------------------------------
// used modules and forward class references
//----------------------------------------------------------------------

#include "DbgInterface.h"

#include "dbg/CheckPtr.h"
#include "ptr/PersistentObject.h"
#include "ptr/SharedStr.h"
#include "sym/Token.h"
#include "LockLevels.h" // DMS_ENTERS / DMS_CALLEE_ENTERS
struct FormattedOutStream;

/********** AbstrMsgGenerator **********/

// The lock-level contract of the whole reporting path (#1227).
//
// Describe / GetDescription / ItemAsStr are called from GenerateContext while an error is being
// built, and an error can be raised anywhere -- including from inside the token registry itself.
// So an override may read names and write to the debug stream, and nothing else:
//
//     DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v)
//
// permits IndexedString SHARED (TokenID::GetStrLock, Object::GetName/GetFullName/GetClsName,
// LispObj::Print) and everything inner to it -- LispObjCache, ObjectRegister, DebugOutStream,
// OperationQueue. It forbids the two things that turn a report into a hang or a recursion:
// REGISTERING a token (IndexedString exclusive, i.e. GetTokenID_mt -> GetOrCreateID_mt), and any
// lock outer to the registry -- storage, tiles, the operation machinery, the item register.
//
// Practically: format from what you already have. Do not evaluate a property (that parses, and
// parsing registers tokens) and do not prepare data. Two overrides used to do exactly that and
// were changed for #1227 -- DataView::GenerateDescription (which called GetCaption(), and so
// PrepareDataOrUpdateViewLater + GetDataCount) and OdbcTableContextHandle::GenerateDescription
// (which called TreeItemPropertyValue).
//
// The ceiling is declared once, in MsgGeneratorPolicy::GetDescription below and in
// AbstrMsgGenerator::Describe, so it covers every override through the virtual dispatch.
struct AbstrMsgGenerator :noncopyable
{
	RTC_CALL AbstrMsgGenerator() noexcept;
	RTC_CALL virtual ~AbstrMsgGenerator()  noexcept;

	RTC_CALL virtual bool Describe(FormattedOutStream& fos); // default: calls GetDescription

	RTC_CALL virtual CharPtr GetDescription();

	RTC_CALL virtual bool IsFinalContext() const { return false; }
	RTC_CALL virtual bool HasItemContext() const { return false; }
	RTC_CALL virtual auto ItemAsStr() const->SharedStr;
};

/********** AbstrContextHandle **********/

template<typename Base>
struct StackHandle : Base
{
	RTC_CALL StackHandle() noexcept;
	RTC_CALL ~StackHandle()  noexcept;

	StackHandle* GetPrev() const;
	static StackHandle* GetLast();
	UInt32 GetContextLevel() const;

private:
	StackHandle* m_Prev;
	THREAD_LOCAL static StackHandle* s_Last;
};

using AbstrContextHandle = StackHandle<AbstrMsgGenerator>;

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

template <typename Base>
struct MsgGeneratorPolicy : Base 
{
	CharPtr GetDescription() override
	{
		// Every concrete context handle reaches its GenerateDescription() through here, so this one
		// declaration is what checks all of them against the reporting-path contract above.
		DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
		try {

			if(m_Context.empty())	
				GenerateDescription();
			return m_Context.c_str();

		}
		catch (...) {}
		return "<MsgGeneratorPolicy::GenerateDescription() threw an exception>";
	}

protected:
	RTC_CALL virtual void GenerateDescription() = 0;

	void SetText(WeakStr context)
	{
		m_Context = context;
	}

private:
	SharedStr m_Context;
};

using MsgGenerator = MsgGeneratorPolicy< AbstrMsgGenerator >;
using ContextHandle = MsgGeneratorPolicy< AbstrContextHandle >;


/********** ObjectContextHandle **********/

template <typename Base>
struct ObjectContextPolicy : Base
{
	ObjectContextPolicy(const Object* obj, CharPtr role = nullptr) : m_Obj(obj), m_Role(role) {}

	RTC_CALL void GenerateDescription() override;
	RTC_CALL bool IsFinalContext() const override final;

protected:
	const Object* m_Obj;
	CharPtr m_Role;
};

using ObjectContextHandleBase = ObjectContextPolicy<ContextHandle>;
using ObjectMsgGenerator = ObjectContextPolicy<MsgGenerator>;

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

// The one context handle whose ceiling belongs to the CALLEE rather than to the class: what
// m_Func may take is a property of the lambda each MakeLCH call site passes, not of this template.
// Every such lambda runs inside GetDescription's DMS_ENTERS above and must stay within it.
template <typename GenerateFunc>
struct LambdaContextHandle: ContextHandle 
{
	LambdaContextHandle(GenerateFunc&& DMS_CALLEE_ENTERS(ord_level_type::IndexedString) func = GenerateFunc())
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

/********** CDebugContextHandle  **********/

struct CDebugContextHandle : ContextHandle 
{
	RTC_CALL CDebugContextHandle(CharPtr className, CharPtr funcName, bool active);
	RTC_CALL ~CDebugContextHandle();

	void LogTime(CharPtr action);

	bool m_Active;

protected:
	void GenerateDescription() override;

	double RunningTime() const;

private:
	CharPtr    m_ClassName;
	CharPtr    m_FuncName;
	ClockTicks m_StartTime;
};

/********** helper funcs  **********/

UInt32 GetCallCount();

#endif // __DBG_DEBUGCONTEXT_H