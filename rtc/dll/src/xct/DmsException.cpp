// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3

#include "RtcPCH.h"
#pragma hdrstop

#include "RtcInterface.h"

#include "xct/DmsException.h"

#include "act/TriggerOperator.h"
#include "dbg/Debug.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "geo/IterRangeFuncs.h"
#include "mci/Object.h"
#include "ptr/PersistentSharedObj.h"
#include "ser/DebugOutStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Environment.h"
#include "utl/IncrementalLock.h"
#include "utl/MySprintF.h"
#include "xml/XMLOut.h"
#include "Parallel.h"

#include <stdarg.h>
#include <time.h>

THREAD_LOCAL UInt32    g_DumpContextCount = 0;

THREAD_LOCAL ErrMsgPtr g_TopmostUnrollingErrMsgPtr;
ErrMsgPtr g_LastHandledErrMsgPtr;

ErrMsgPtr SetUnrollingErrMsgPtr(ErrMsgPtr msg)
{
	std::swap( g_TopmostUnrollingErrMsgPtr, msg );
	return msg;
}

void SetLastHandledErrMsg(ErrMsgPtr msg)
{
	if (IsMainThread())
		g_LastHandledErrMsgPtr = msg;
}

const UInt32 g_MaxNrContexts = 10;

SharedStr GenerateContext()
{
	AbstrContextHandle* ach = AbstrContextHandle::GetLast();

	VectorOutStreamBuff osb;
	FormattedOutStream fos(&osb, FormattingFlags::None);

	DynamicIncrementalLock<> lock2(g_DumpContextCount);


	if (g_DumpContextCount > 1)
		fos << "while generating context.";
	else
	{
		fos << "\nContext:";

		if (!IsMainThread())
			fos << "\nThread " << GetThreadID();

		UInt32 nrContexts = 0;
		bool last5 = false;
		while (ach)
		{
			if (last5 || nrContexts < g_MaxNrContexts)
			{
				try {
					fos << "\n" << (nrContexts + 1) << ". ";
					if (ach->Describe(fos))
					{
						++nrContexts;
						if (ach->IsFinalContext())
							break; // give it a break, TODO: clean-up too much code
					}
				}
				catch (...) {}
			}
			else
			{
				UInt32 nrRemaining = ach->GetContextLevel();
				if (nrRemaining > 5)
					fos << "... skipped " << nrRemaining - 5 << " contexts ...";
				while (ach && nrRemaining-- > 5)
					ach = ach->GetPrev();
				last5 = true;
			}

			ach = ach->GetPrev();
		}
	}
	return SharedStr(osb.GetData(), osb.GetDataEnd());
}

CharPtr FailTypeStr(FailType ft)
{
	switch (ft) {
	case FR_Determine: return "Determining State";
	case FR_MetaInfo: return "Deriving item properties and subitems";
	case FR_Data: return "Deriving primary data";
	case FR_Validate: return "Validation";
	case FR_Committed: return "Committing data to storage";
	}
	return "<Unexpected FailType>";
}

#include "utl/SourceLocation.h"

ErrMsg::ErrMsg(WeakStr msg, const PersistentSharedObj* ptr)
	:	m_Why(msg)
{
	TellWhere(ptr);

	AbstrContextHandle* ach = AbstrContextHandle::GetLast();
	if (ach)
		m_Context = GenerateContext();
}

void ErrMsg::TellExtra(CharPtrRange msg)
{
	if (msg.empty())
		return;
	if (m_Context.empty())
		m_Context = SharedStr(msg);
	else
		m_Context += mySSPrintF("%s\n%s", m_Context, msg);
}

void ErrMsg::TellWhere(const PersistentSharedObj* ptr)
{
	if (!ptr)
		return;

	if (m_FullName.empty())
	{
		m_FullName = ptr->GetFullName();
		m_HasBeenReported = false;
	}
}

SharedStr ErrMsg::GetAsText() const
{
	return mgFormat2SharedStr("%s\n%s\n%s"
		, m_Why
		, m_FullName
		, m_Context
	);
}

OutStreamBase& operator << (OutStreamBase& osb, const ErrMsg& obj)
{
	osb.WriteValueWithConfigSourceDecorations(obj.m_Why.c_str());
	osb.WriteValue("\n");
	if (!obj.m_FullName.empty())
	{
		XML_hRef hRef(osb, (CharPtrRange("dms:dp.general:") + obj.m_FullName).c_str());
		osb.WriteValue(obj.m_FullName.c_str());
	}
	if (!obj.m_Context.empty())
	{
		osb.WriteValue("\n\n");
		osb.WriteValueWithConfigSourceDecorations(obj.m_Context.c_str());
	}
	return osb;
}


DmsException::DmsException(ErrMsgPtr msg)
	:	ErrMsgPtr(msg)
	,	m_PrevUnrollingErrMsgPtr(SetUnrollingErrMsgPtr(msg))
{
	dms_check_not_debugonly; 
}

DmsException::~DmsException()
{
	SetLastHandledErrMsg(*this);
	SetUnrollingErrMsgPtr(m_PrevUnrollingErrMsgPtr);
}

#if defined(MG_DEBUG)
#define MG_COUNT_EXCEPTIONS
#endif

//#define MG_COUNT_EXCEPTIONS

#if defined(MG_COUNT_EXCEPTIONS)
static int sd_ThrowItemErrorCount = 0;
#endif

RTC_CALL const char* DmsException::what() const noexcept
{
	return get()->m_Why.c_str();
}

[[noreturn]] RTC_CALL void DmsException::throwMsg(ErrMsgPtr msg)
{
#if defined(MG_COUNT_EXCEPTIONS)
	sd_ThrowItemErrorCount++;
//	reportF(ST_MajorTrace, "Logging throwItemError %d: %s", sd_ThrowItemErrorCount, msg.Why());
#endif

	throw DmsException(msg);
}

[[noreturn]] RTC_CALL void DmsException::throwMsgD(WeakStr str)
{
	throwMsg(std::make_shared<ErrMsg>(str ));
}

[[noreturn]] RTC_CALL void DmsException::throwMsgD(CharPtr msg)
{
	throwMsgD(SharedStr(msg));
}

namespace {
	SharedStr memoryAllocFailureMsg("memory allocation failed, no recovery possible"); // keep it simple, we cannot affort much anymore
}

//----------------------------------------------------------------------
// cachable MemoryAllocFailure
//----------------------------------------------------------------------

MemoryAllocFailure::MemoryAllocFailure()
	: DmsException(std::make_shared<ErrMsg>(memoryAllocFailureMsg))
{
	s_BlockNewAllocations = true;
}

//----------------------------------------------------------------------
// Various exception constructors and reporting
//----------------------------------------------------------------------

extern "C" RTC_CALL void DMS_CONV DMS_ReportError(CharPtr msg)
{
	DMS_CALL_BEGIN

		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_Error);
		*g_DebugStream << msg;

	DMS_CALL_END
}

extern "C" RTC_CALL void DMS_CONV DMS_DisplayError(CharPtr msg)
{
	DMS_CALL_BEGIN

		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_DispError);
		*g_DebugStream << msg;

	DMS_CALL_END
}

RTC_CALL void reportD_without_cancellation_check(MsgCategory msgCat, SeverityTypeID st, CharPtr msg)
{
	if (!g_DebugStream)
		return;

	DebugOutStream::scoped_lock lock(g_DebugStream, st, msgCat);
	*g_DebugStream << msg;
}

RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg)
{
	DMS_ASyncContinueCheck();
	reportD_without_cancellation_check(msgCat, st, msg);
}

RTC_CALL void reportD_impl(MsgCategory msgCat, SeverityTypeID st, const CharPtrRange& msg)
{
	if (!g_DebugStream)
		return;

	DMS_ASyncContinueCheck();
	DebugOutStream::scoped_lock lock(g_DebugStream, st, msgCat);

	*g_DebugStream << msg;
}

RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg1, CharPtr msg2)
{
	if (!g_DebugStream)
		return;

	DMS_ASyncContinueCheck();
	DebugOutStream::scoped_lock lock(g_DebugStream, st, msgCat);

	*g_DebugStream << msg1 << msg2;
}


RTC_CALL void ReportSuspension()
{
	reportD(SeverityTypeID::ST_MinorTrace, "Suspension that might result in the recalculation of intermediate results");
}

SharedStr ErrLoc(CharPtr sourceFile, int line, bool isInternal)
{
	SharedStr result;
	if (sourceFile && *sourceFile)
		result = mySSPrintF("%s(%d):\n", sourceFile, line);
	if (isInternal)
		result += "\nThis seems to be a GeoDms internal error; contact Object Vision or report this as issue at https://github.com/ObjectVision/GeoDMS/issues";
	return result;
}

#define ERR_TXT " Error: "

[[noreturn]] RTC_CALL void throwErrorD(CharPtr type, CharPtr msg)
{
	dms_assert(type && *type && (strncmp(type, msg, StrLen(type)) || strncmp(ERR_TXT, msg + StrLen(type), sizeof(ERR_TXT) - 1)));
	DmsException::throwMsgF("%s" ERR_TXT "%s", type, msg);
}

[[noreturn]] RTC_CALL void throwErrorD(const TokenID& type, CharPtr msg)
{
	auto typeStr = SharedStr(type);
	throwErrorD(typeStr.c_str(), msg);
}


[[noreturn]] RTC_CALL void  throwDmsErrD(CharPtr msg)
{
	DmsException::throwMsgF("Error: %s", msg);
}

[[noreturn]] RTC_CALL void throwIllegalAbstract(CharPtr sourceFile, int line, const PersistentSharedObj* obj, CharPtr method)
{
	obj->throwItemErrorF("Illegal Abstract %s called.\n%s", method, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void throwIllegalAbstract(CharPtr sourceFile, int line, CharPtr method)
{
	throwErrorF("Illegal Abstract", "%s called.\n%s", method, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void throwNYI(CharPtr sourceFile, int line, CharPtr method)
{
	throwErrorF("NYI", "Function %s is not yet implemented\n%s", method, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void  throwPreconditionFailed(CharPtr sourceFile, int line, CharPtr msg)
{
	throwErrorF("Precondition Exception", "%s\n%s", msg, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void throwCheckFailed(CharPtr sourceFile, int line, CharPtr msg)
{
	throwErrorF("Check Failed", "%s\n%s", msg, ErrLoc(sourceFile, line, true));
}

//----------------------------------------------------------------------
// GlobalCppTranslator
//----------------------------------------------------------------------

namespace { // local defs

	THREAD_LOCAL TCppExceptionTranslator s_cppTrFunc = nullptr;
	TCppExceptionTranslator s_SeTrGlobalFunc = nullptr;
} // end anonymous namespace

extern "C" RTC_CALL void DMS_CONV DMS_SetGlobalCppExceptionTranslator(TCppExceptionTranslator trFunc)
{
	dms_assert(IsMainThread());
	s_cppTrFunc = trFunc;
}

extern "C" RTC_CALL void DMS_CONV DMS_SetGlobalSeTranslator(TCppExceptionTranslator trFunc)
{
	s_SeTrGlobalFunc = trFunc;
}

TCppExceptionTranslator SetCppTranslator(TCppExceptionTranslator trFunc)
{
	omni::swap(trFunc, s_cppTrFunc);
	return trFunc;
}

//----------------------------------------------------------------------
// process exception before returning to DMS client;
// should be called from a catch (...) block
//----------------------------------------------------------------------

ErrMsgPtr catchExceptionImpl(bool rethrowCancelation)
{
	try {
 		throw; // dispatch caught exception based on its type
	}
	catch (const DmsException& x)
	{
		return x.AsErrMsg();
	}
	catch (const concurrency::task_canceled&)
	{
		if (rethrowCancelation)
			throw;
		return std::make_shared<ErrMsg>( SharedStr("Task cancelation") );
	}
	catch (const std::exception& x)
	{
		return std::make_shared<ErrMsg>( SharedStr(x.what()) );
	}
	catch (...)
	{
		return std::make_shared<ErrMsg>(SharedStr("Unknown Error") );
	}
}

RTC_CALL ErrMsgPtr catchException(bool rethrowCancelation)
{
	auto result = catchExceptionImpl(rethrowCancelation);
	return result;
}

RTC_CALL ErrMsgPtr catchAndReportException()
{
	auto result = catchException(false);
	reportD(SeverityTypeID::ST_Warning, "\n", result->GetAsText().c_str());
	return result;
}

RTC_CALL void catchAndProcessException()
{
	dms_assert(IsMainThread());
	static ErrMsgPtr msgPtr; // static to avoid the need to destroy when a Structured Exception will be thrown.
	msgPtr = catchException(false);

	if (s_cppTrFunc)
		s_cppTrFunc(msgPtr->GetAsText().c_str()); // may throw a Borland Structured Exception
//	else
//		throw;
}


//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

SharedStr GetLastErrorMsgStr()
{
	dms_assert(IsMainThread());
	if (!IsMainThread())
		return {};

	return g_LastHandledErrMsgPtr->Why();
}

extern "C" RTC_CALL CharPtr DMS_CONV DMS_GetLastErrorMsg()
{
	return GetLastErrorMsgStr().c_str();
}

ErrMsgPtr GetUnrollingErrorMsgPtr()
{
	if (std::uncaught_exceptions() > 0)
	{
		return g_TopmostUnrollingErrMsgPtr;
	}
	return {};
}

//----------------------------------------------------------------------
// C structured exception handling (convert WinNT structured exception)
//----------------------------------------------------------------------

#include <windows.h>
#define EXCEPTION_BORLAND_ERROR 0x0eedfade

SharedStr GetExceptionText(unsigned int exceptionCode, _EXCEPTION_POINTERS* pExp )
{
	CharPtr result;
	switch(exceptionCode)
	{
		case EXCEPTION_BORLAND_ERROR:
			result = "Delphi Exception raised in callback function"; 
			break;
		case EXCEPTION_ACCESS_VIOLATION:
			if (pExp && pExp->ExceptionRecord &&  pExp->ExceptionRecord->NumberParameters >= 2)
				return mySSPrintF("The thread tried to %s virtual address 0x%X for which it does not have the appropriate access.",
				(pExp->ExceptionRecord->ExceptionInformation[0]
					? "write to"
					: "read from"),
					pExp->ExceptionRecord->ExceptionInformation[1]);
			result = "Invalid ExceptionRecord pointer";
			break;

		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			result = "The thread tried to access an array element that is out of bounds.";
			break;
		case EXCEPTION_BREAKPOINT: 
			result = "A breakpoint was encountered.";
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT: 
			result = "The thread tried to read or write data that is misaligned on hardware that does not provide alignment.\n"
					"For example, 16-bit values must be aligned on 2-byte boundaries; "
					"32-bit values on 4-byte boundaries, and so on.";
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND: 
			result = "One of the operands in a floating-point operation is denormal.\n"
				"A denormal value is one that is too small to represent as a standard floating-point value.";
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			result = "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			result = "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			result = "Unspecified invalid floating-point operation.";
			break;
		case EXCEPTION_FLT_OVERFLOW:
			result = "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			result = "The stack overflowed or underflowed as the result of a floating-point operation.";
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			result = "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			result = "The thread tried to execute an invalid instruction.";
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			result = "The thread tried to access a page that was not present, and the system was unable to load the page.\n"
				"Possible causes: lost network connection or full disc volume when writing/changing data in a compressed CalcCache entry or other pagefiles.";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			result = "The thread tried to divide an integer value by an integer divisor of zero.";
			break;
		case EXCEPTION_INT_OVERFLOW:
			result = "The result of an integer operation caused a carry out of the most significant bit of the result.";
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			result = "An exception handler returned an invalid disposition to the exception dispatcher.\n"
				"Programmers using a high-level language such as C should never encounter this exception.";
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			result = "The thread tried to continue execution after a noncontinuable exception occurred.";
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			result = "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
			break;
		case EXCEPTION_SINGLE_STEP:
			result = "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
			break;
		case EXCEPTION_STACK_OVERFLOW:
			result = "The thread used up its stack.";
			break;
		case 0xc06d007e:
			result = "Error in delayed loading a dll";
			break;
		default: result = "Unknown ExceptionCode";
	}
	dms_assert(result);
	return SharedStr(result);
}


#define DMS_SE_CPP 0xE06D7363

THREAD_LOCAL unsigned int g_StructuredExceptionCode = 0;
THREAD_LOCAL _EXCEPTION_POINTERS* g_pExp;

RTC_CALL unsigned int GetLastExceptionCode()
{
	return g_StructuredExceptionCode;
}

int signalHandling(unsigned int u, _EXCEPTION_POINTERS* pExp, bool passBorlandException)
{
//	if ((u == DMS_SE_CPP) || (u == EXCEPTION_BORLAND_ERROR))
	if (passBorlandException && u == EXCEPTION_BORLAND_ERROR)
			return EXCEPTION_CONTINUE_SEARCH;
	g_StructuredExceptionCode = u;
	g_pExp = pExp;

	return EXCEPTION_EXECUTE_HANDLER;
}


[[noreturn]] void  trans_SE2DMSfunc( unsigned int u, _EXCEPTION_POINTERS* pExp, bool mustTerminate)
{
#if defined(DMS_32)
	__asm fninit;
#endif
	dms_assert(u != DMS_SE_CPP);

	auto exceptionText = GetExceptionText(u, pExp);

	if (mustTerminate)
	{
		MessageBox(reinterpret_cast<HWND>(GetGlobalMainWindowHandle()), exceptionText.c_str(), "Fatal OS Structured Exception raised", MB_OK);
		std::terminate();
	}
	DmsException::throwMsgF( "%s Structured Exception: 0x%X raised:\n%s"
	,	(u == EXCEPTION_BORLAND_ERROR) ? "Borland" : "OS"
	,	u
	,	exceptionText.c_str()
	);
}

[[noreturn]] RTC_CALL void call_trans_SE2DMSfunc()
{
	trans_SE2DMSfunc(g_StructuredExceptionCode, g_pExp, false);
}

[[noreturn]] RTC_CALL void call_HaltOnSE()
{
	trans_SE2DMSfunc(g_StructuredExceptionCode, g_pExp, true);
}


//----------------------------------------------------------------------
// dms_assertion_failed
//----------------------------------------------------------------------

CppTranslatorContext::CppTranslatorContext(TCppExceptionTranslator trFunc)
		: m_PrevCppTranslator(SetCppTranslator(trFunc))
	{}

CppTranslatorContext::~CppTranslatorContext()
{
	if (IsMainThread() && !m_PrevCppTranslator)
	{
		dms_assert(g_DebugStream);
//		ProcessMainThreadOpers();
	}
	SetCppTranslator(m_PrevCppTranslator);
}

//----------------------------------------------------------------------
// dms_assertion_failed
//----------------------------------------------------------------------

#if defined(MG_DEBUG)

THREAD_LOCAL DebugOnlyLock* g_CurrAssertLock = nullptr;
THREAD_LOCAL CharPtr g_LastAssertStr = nullptr;
THREAD_LOCAL CharPtr g_LastFile = nullptr;
THREAD_LOCAL UInt32 g_LastLine = -1;

DebugOnlyLock::DebugOnlyLock(CharPtr assertStr, CharPtr file, UInt32 line)
	:	m_AssertStr(assertStr)
	,	m_File(file)
	,	m_Line(line)
	,	m_PrevLock(g_CurrAssertLock)
{
	g_CurrAssertLock = this;
}

DebugOnlyLock::~DebugOnlyLock()
{
	g_CurrAssertLock = m_PrevLock;
	g_LastAssertStr = m_AssertStr;
	g_LastFile = m_File;
	g_LastLine = m_Line;
}

bool DebugOnlyLock::IsLocked()
{
	return g_CurrAssertLock;
}

void DebugOnlyLock::CheckNoLocks()
{
	if (IsLocked())
		dms_assertion_failed(g_CurrAssertLock->m_AssertStr, g_CurrAssertLock->m_File, g_CurrAssertLock->m_Line);
}

#endif

RTC_CALL void dms_check_failed(CharPtr msg, CharPtr fileName, unsigned line)
{
	reportF_without_cancellation_check(SeverityTypeID::ST_MajorTrace, "check failure: %s\n%s(%u)", msg, fileName, line);
#if defined(MG_DEBUG)
	__debugbreak();
#endif
}

RTC_CALL void dms_assertion_failed(CharPtr msg, CharPtr fileName, unsigned line)
{
#if defined(MG_DEBUG)
	__debugbreak();
#endif
}

CharPtr GetContextPtr(WeakStr msg)
{
	return Search(CharPtrRange(msg), CharPtrRange("\n# "));
}

RTC_CALL bool HasContext(WeakStr msg)
{
	return GetContextPtr(msg) != msg.csend();
}

RTC_CALL SharedStr GetFirstLine(WeakStr msg)
{
	CharPtr eolPtr = msg.find('\n');
	if (eolPtr == msg.csend())
		return msg;
	return SharedStr(msg.begin(), eolPtr);
}

