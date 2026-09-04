// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3

#include "RtcPCH.h"
#include "LockLevels.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "RtcInterface.h"
#include "DbgInterface.h" // DBG_WriteFatalLine, DBG_FlushLogs

#include "xct/DmsException.h"

#include "act/ActorEnums.h"
#include "dbg/debug.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "vt/iterrangefuncs.h"
#include "mci/Object.h"
#include "ser/DebugOutStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/IncrementalLock.h"
#include "utl/StrFormat.h"
#include "utl/Quotes.h"
#include "xml/XMLOut.h"
#include "Parallel.h"

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
	if (IsMetaThread())
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

		if (!IsMetaThread())
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
	return SharedStr(CharPtrRange(osb.GetData(), osb.GetDataEnd()));
}

CharPtr FailTypeStr(FailType ft)
{
	switch (ft) {
	case FailType::Determine: return "Determining State";
	case FailType::MetaInfo: return "Deriving item properties and subitems";
	case FailType::Data: return "Deriving primary data";
	case FailType::Validate: return "Validation";
	case FailType::Committed: return "Committing data to storage";
	}
	return "<Unexpected FailType>";
}

#include "utl/SourceLocation.h"

ErrMsg::ErrMsg(WeakStr msg, const Object* ptr)
	:	m_Why(msg)
{
	TellWhere(ptr);

	m_Context = GenerateContext();
}

void ErrMsg::TellExtra(CharPtrRange msg)
{
	if (msg.empty())
		return;
	if (m_Context.empty())
		m_Context = SharedStr(msg);
	else
		m_Context = mySSPrintF("{}\n{}", m_Context, msg);
}

void ErrMsg::TellWhere(const Object* ptr)
{
	if (!ptr)
		return;

	if (m_FullName.empty())
	{
		m_FullName = ptr->GetFullCfgName();
		if (!m_FullName.empty())
			m_HasBeenReported = false; // we like to see this reported again with m_FullName
	}
}

SharedStr ErrMsg::GetAsText() const
{
	return mgFormat2SharedStr("{}\n{}\n{}"
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

RTC_CALL const char* DmsException::what() const noexcept
{
	return get()->m_Why.c_str();
}

[[noreturn]] void DmsException::throwMsg(ErrMsgPtr msg)
{
	throw DmsException(msg);
}

[[noreturn]] RTC_CALL void DmsException::throwMsgD(WeakStr str)
{
	throwMsg(std::make_shared<ErrMsg>(str ));
}

[[noreturn]] RTC_CALL void DmsException::throwMsgD(CharPtr msg)
{
	throwMsgD(SharedStr(msg MG_DEBUG_ALLOCATOR_SRC("DmsException::throwMsgD")));
}

namespace {
	SharedStr memoryAllocFailureMsg("memory allocation failed"); // keep it simple, we cannot affort much anymore
}

//----------------------------------------------------------------------
// cachable MemoryAllocFailure
//----------------------------------------------------------------------

MemoryAllocFailure::MemoryAllocFailure()
	: DmsException(std::make_shared<ErrMsg>(memoryAllocFailureMsg))
{
	DMS_ENTERS(ord_level_type::DebugOutStream, dms_exclusive_v);
	s_BlockNewAllocations = true;
}

//----------------------------------------------------------------------
// Various exception constructors and reporting
//----------------------------------------------------------------------

extern "C" RTC_CALL void DMS_CONV DMS_ReportError(CharPtr msg)
{
	DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	DMS_CALL_BEGIN

		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_Error);
		*g_DebugStream << msg;

	DMS_CALL_END
}

extern "C" RTC_CALL void DMS_CONV DMS_DisplayError(CharPtr msg)
{
	DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	DMS_CALL_BEGIN

		DebugOutStream::scoped_lock lock(g_DebugStream, SeverityTypeID::ST_DispError);
		*g_DebugStream << msg;

	DMS_CALL_END
}

RTC_CALL auto GetReportingItemName() -> SharedStr
{
	if (!g_IsTerminating)
		for (auto ch = ContextHandle::GetLast(); ch; ch = ch->GetPrev())
			if (ch->HasItemContext())
				return ch->ItemAsStr();

	return {};
}

auto getContext(SeverityTypeID st) -> SharedStr
{
	if (st >= SeverityTypeID::ST_MajorTrace)
		return GetReportingItemName();

	return {};
}

// #795: a calculating operator puts the item name in FRONT of its progress messages itself,
// because that name has to reach the tile worker threads, which do not carry the reporting
// context of the thread that started them. Appending the same name here would double it.
static bool BeginsWith(CharPtrRange msg, WeakStr prefix)
{
	return msg.size() >= prefix.ssize() && std::equal(prefix.begin(), prefix.send(), msg.begin());
}

void reportD_without_cancellation_check_impl(MsgCategory msgCat, SeverityTypeID st, auto&& payload, CharPtrRange msgTextForContextCheck = {})
{
	DMS_ENTERS(ord_level_type::IndexedString, dms_shared_v);
	if (!g_DebugStream)
		return;

	auto contextStr = getContext(st);
	if (!contextStr.empty() && BeginsWith(msgTextForContextCheck, contextStr))
		contextStr = SharedStr();
	DebugOutStream::scoped_lock lock(g_DebugStream, st, msgCat);

	payload();

	if (contextStr.empty())
		return;
	*g_DebugStream << " " << contextStr;
}

RTC_CALL void reportD_without_cancellation_check(MsgCategory msgCat, SeverityTypeID st, CharPtr msg)
{
	reportD_without_cancellation_check_impl(msgCat, st, [=] { *g_DebugStream << msg; }, CharPtrRange(msg));
}

RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg)
{
	ASyncContinueCheck();

	reportD_without_cancellation_check_impl(msgCat, st, [=] {*g_DebugStream << msg;  }, CharPtrRange(msg));
}


void reportD_impl(MsgCategory msgCat, SeverityTypeID st, CharPtrRange&& msg)
{
	ASyncContinueCheck();

	reportD_without_cancellation_check_impl(msgCat, st, [=] { *g_DebugStream << msg; }, msg);
}

RTC_CALL void reportD(MsgCategory msgCat, SeverityTypeID st, CharPtr msg1, CharPtr msg2)
{
	ASyncContinueCheck();

	reportD_without_cancellation_check_impl(msgCat, st, [=] { *g_DebugStream << msg1 << msg2; }, CharPtrRange(msg1));
}

void ReportSuspension()
{
	reportD(SeverityTypeID::ST_MinorTrace, "Suspension that might result in the recalculation of intermediate results");
}

SharedStr ErrLoc(CharPtr sourceFile, int line, bool isInternal)
{
	SharedStr result;
	if (sourceFile && *sourceFile)
		result = mySSPrintF("{}({}):\n", sourceFile, line);
	if (isInternal)
		result += "\nThis seems to be a GeoDms internal error; contact Object Vision or report this as issue at https://github.com/ObjectVision/GeoDMS/issues";
	return result;
}

#define ERR_TXT " Error: "

[[noreturn]] RTC_CALL void throwErrorD(CharPtr type, CharPtr msg)
{
	dms_assert(type && *type && (strncmp(type, msg, StrLen(type)) || strncmp(ERR_TXT, msg + StrLen(type), sizeof(ERR_TXT) - 1)));
	DmsException::throwMsgF("{}" ERR_TXT "{}", type, msg);
}

[[noreturn]] RTC_CALL void throwErrorD(TokenID type, CharPtr msg)
{
	auto typeStr = SharedStr(type);
	throwErrorD(typeStr.c_str(), msg);
}


[[noreturn]] RTC_CALL void  throwDmsErrD(CharPtr msg)
{
	DmsException::throwMsgF("Error: {}", msg);
}

// ---------------------------------------------------------------------------
// The internal-error family: throwCheckFailed (MG_CHECK), throwPreconditionFailed
// (MG_PRECONDITION), throwIllegalAbstract and throwNYI. They report a violated invariant of the
// GeoDms code itself, which is why each of them names a source file and line and why ErrLoc adds
// the line telling the reader the problem is not in their configuration.
//
// #1202: they mark their ErrMsg as well. Actor::DoFail derives severity from the FailType, so a
// failure that surfaces again as a consumer's Validate or Committed failure is reported as a
// warning -- and for an internal error that second report is the only one that names the item.
// The mark carries "this is not a configuration error" to that decision. It is set here, in the
// one place all four of these throwers pass through, rather than at each of them.
// ---------------------------------------------------------------------------

namespace { // local defs

	[[noreturn]] void throwInternalError(ErrMsgPtr errMsg)
	{
		errMsg->m_IsInternalError = true;
		DmsException::throwMsg(errMsg);
	}

	// the internal counterpart of throwErrorD / throwErrorF: same "<type> Error: <msg>" text
	[[noreturn]] void throwInternalErrorD(CharPtr type, CharPtr msg)
	{
		dms_assert(type && *type && (strncmp(type, msg, StrLen(type)) || strncmp(ERR_TXT, msg + StrLen(type), sizeof(ERR_TXT) - 1)));
		throwInternalError(std::make_shared<ErrMsg>(mgFormat2SharedStr("{}" ERR_TXT "{}", type, msg)));
	}

	template<typename ...Args>
	[[noreturn]] void throwInternalErrorF(CharPtr type, CharPtr format, Args&&... args)
	{
		throwInternalErrorD(type, mgFormat2string<Args...>(format, std::forward<Args>(args)...).c_str());
	}

} // end anonymous namespace

[[noreturn]] RTC_CALL void throwIllegalAbstract(CharPtr sourceFile, int line, const Object* obj, CharPtr method)
{
	assert(0);
	throwInternalError(std::make_shared<ErrMsg>(
		mgFormat2SharedStr("Illegal Abstract {} called.\n{}", method, ErrLoc(sourceFile, line, true)), obj));
}

[[noreturn]] RTC_CALL void throwIllegalAbstract(CharPtr sourceFile, int line, CharPtr method)
{
	assert(0);
	throwInternalErrorF("Illegal Abstract", "{} called.\n{}", method, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void throwNYI(CharPtr sourceFile, int line, CharPtr method)
{
	throwInternalErrorF("NYI", "Function {} is not yet implemented\n{}", method, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void  throwPreconditionFailed(CharPtr sourceFile, int line, CharPtr msg)
{
	assert(0);
	throwInternalErrorF("Precondition Exception", "{}\n{}", msg, ErrLoc(sourceFile, line, true));
}

[[noreturn]] RTC_CALL void throwCheckFailed(CharPtr sourceFile, int line, CharPtr msg)
{
	assert(0);
	throwInternalErrorF("Check Failed", "{}\n{}", msg, ErrLoc(sourceFile, line, true));
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
	assert(IsMetaThread());
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
	catch (const task_canceled&)
	{
		if (rethrowCancelation)
			throw;
		return std::make_shared<ErrMsg>( SharedStr("Task cancelation") );
	}
	catch (const std::exception& x)
	{
		CharPtr xWhat = x.what();
		auto xLength = StrLen(xWhat);
		MakeMin<SizeT>(xLength, 32767);
		return std::make_shared<ErrMsg>( SharedStr(CharPtrRange(xWhat, xWhat+xLength)) );
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
	reportD(SeverityTypeID::ST_Error, "\n", result->GetAsText().c_str());
	return result;
}

RTC_CALL void catchAndProcessException()
{
	assert(IsMetaThread());
	if (!s_cppTrFunc)
	{
		catchAndReportException();
		return;
	}
	static ErrMsgPtr msgPtr; // static to avoid the need to destroy when a Structured Exception will be thrown.
	msgPtr = catchException(false);

	assert(s_cppTrFunc);
	s_cppTrFunc(msgPtr->GetAsText().c_str()); // may throw a Borland Structured Exception
}


//----------------------------------------------------------------------
// DMS interface functions
//----------------------------------------------------------------------

SharedStr GetLastErrorMsgStr()
{
	assert(IsMetaThread());
	if (!IsMetaThread())
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
#if defined(WIN32)

#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib") // keeps the dependency with the code that needs it, out of the project files
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
			{
				auto kind = pExp->ExceptionRecord->ExceptionInformation[0]; // 0 read, 1 write, 8 execute (DEP)
				return mySSPrintF("The thread tried to {} virtual address 0x{:X} for which it does not have the appropriate access.",
					kind == 0 ? "read from" : kind == 1 ? "write to" : "execute code at",
					pExp->ExceptionRecord->ExceptionInformation[1]);
			}
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
				"Possible causes: a lost network connection to a memory-mapped storage, or a full disc volume while writing memory-mapped tile data or the system pagefile.";
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
		case 0xc06d007f:
			result = "Error in delayed loading a dll: a procedure was not found in it";
			break;
		default: result = "Unknown ExceptionCode";
	}
	dms_assert(result);
	return SharedStr(result MG_DEBUG_ALLOCATOR_SRC("GetExceptionText"));
}


#define DMS_SE_CPP 0xE06D7363

THREAD_LOCAL unsigned int g_StructuredExceptionCode = 0;
THREAD_LOCAL _EXCEPTION_POINTERS* g_pExp;
THREAD_LOCAL EXCEPTION_RECORD     g_ExceptionRecordCopy;   // taken in the filter, read in the handler; see signalHandling
THREAD_LOCAL EXCEPTION_POINTERS   g_ExceptionPointersCopy;

unsigned int GetLastExceptionCode()
{
	return g_StructuredExceptionCode;
}

//----------------------------------------------------------------------
// crash dump (#1241)
//----------------------------------------------------------------------
// A structured exception can only be debugged while the faulting frames are still on the
// stack, and that is true in the __except FILTER but no longer in the handler body: Windows
// unwinds everything between the fault and the __try before call_HaltOnSE runs, so a dump
// taken there shows the handler and g_pExp points into stack memory that has been released.
// Hence WriteCrashDump is called from signalHandling; because that filter is shared, the one
// call covers the GUI main thread, GeoDmsRun and the GDAL callbacks alike. A fault outside any
// __try, on a tile worker or a Qt thread say, reaches the same code through the unhandled
// exception filter installed below.
//
// The rules below all follow from "the process is already broken":
// - the crash path allocates, looks up and converts nothing: the folder is captured at startup,
//   the writer thread and its events are created at startup, and the file name is formatted
//   into a static buffer. The heap may be the very thing that broke;
// - the dump is written by that separate, parked thread, on a stack of its own. That is what
//   makes EXCEPTION_STACK_OVERFLOW dumpable at all: the filter runs on the stack that is already
//   exhausted, with about a page to spare, where MiniDumpWriteDump would fault again, and the
//   t720 regression project fills the stack on purpose. Creating the thread at that moment
//   would need that stack as well, hence at startup;
// - one dump per process, so a fault while dumping cannot recurse, and an SE that is not a crash
//   (a C++ throw travelling as an SE, a failed delay-load) does not spend it;
// - never MiniDumpWithFullMemory: a GeoDMS process routinely holds tens of gigabytes and would
//   try to write all of it. Normal plus the memory the stacks point at stays in the megabytes.

namespace { // crash dump state; all of it prepared by InitCrashDumpSupport while the process is still healthy

	wchar_t s_CrashDumpFolder[MAX_PATH]   = L""; // empty means: not initialised, no dumps
	wchar_t s_CrashDumpPathW[MAX_PATH]    = L""; // the file being written, so that a failed attempt can be removed
	char    s_CrashDumpPath[MAX_PATH * 3] = "";  // utf8 path of the dump that was written, for the message and the log
	LONG    s_CrashDumpTaken = 0;

	struct CrashDumpRequest
	{
		unsigned int         code = 0;
		_EXCEPTION_POINTERS* pExp = nullptr;
		DWORD                threadId = 0; // the thread that faulted, whose context the dump carries; not the one that writes it
	};
	CrashDumpRequest s_CrashDumpRequest; // filled by the faulting thread before it signals s_CrashDumpRequested

	HANDLE s_CrashDumpThread    = nullptr;
	HANDLE s_CrashDumpRequested = nullptr; // set once, by the faulting thread
	HANDLE s_CrashDumpDone      = nullptr; // set once, by the writer thread, dump or no dump
	HANDLE s_CrashDumpFile      = INVALID_HANDLE_VALUE;

	LPTOP_LEVEL_EXCEPTION_FILTER s_PrevUnhandledExceptionFilter = nullptr;

	bool IsCrashCode(unsigned int code)
	{
		switch (code)
		{
			case DMS_SE_CPP:              // a C++ throw travelling as an SE
			case EXCEPTION_BORLAND_ERROR: // the same, raised by a Delphi client
			case 0xC06D007E:              // delay-load: module not found
			case 0xC06D007F:              // delay-load: procedure not found
				return false;             // reported as an error, after which the process carries on
		}
		return true;
	}

	void WriteCrashDumpImpl(const CrashDumpRequest& req)
	{
		CreateDirectoryW(s_CrashDumpFolder, nullptr); // usually already there; CreateFileW reports what matters

		SYSTEMTIME st;
		GetLocalTime(&st);
		if (swprintf_s(s_CrashDumpPathW, L"%s\\GeoDms_%u_%04u%02u%02u_%02u%02u%02u.dmp"
			,	s_CrashDumpFolder, GetCurrentProcessId()
			,	st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond) < 0)
			return;

		s_CrashDumpFile = CreateFileW(s_CrashDumpPathW, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (s_CrashDumpFile == INVALID_HANDLE_VALUE)
			return;

		MINIDUMP_EXCEPTION_INFORMATION mei = { req.threadId, req.pExp, FALSE }; // FALSE: pExp is in this process
		auto dumpType = MINIDUMP_TYPE(MiniDumpNormal
			|	MiniDumpWithIndirectlyReferencedMemory // what the stacks point at: the objects being worked on
			|	MiniDumpWithDataSegs                   // the globals of every module
			|	MiniDumpWithHandleData                 // files, events and mutexes, for a fault in a wait or a write
			|	MiniDumpWithProcessThreadData          // PEB and TEBs, hence the thread-local state
			|	MiniDumpWithThreadInfo
			|	MiniDumpWithUnloadedModules
			|	MiniDumpIgnoreInaccessibleMemory);     // an unreadable page, such as the one just faulted on, must not fail the whole dump
		bool hasDump = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), s_CrashDumpFile, dumpType, &mei, nullptr, nullptr) != FALSE;
		CloseHandle(s_CrashDumpFile);
		s_CrashDumpFile = INVALID_HANDLE_VALUE;

		if (!hasDump)
		{
			DeleteFileW(s_CrashDumpPathW); // an empty or truncated file only misleads whoever finds it
			return;
		}
		WideCharToMultiByte(CP_UTF8, 0, s_CrashDumpPathW, -1, s_CrashDumpPath, sizeof(s_CrashDumpPath), nullptr, nullptr);
		OutputDebugStringW(L"GeoDMS wrote a crash dump: "); // takes no lock of ours, unlike the trace log
		OutputDebugStringW(s_CrashDumpPathW);
	}

	// The writer thread: created at startup and parked until a fault posts a request, see WriteCrashDump.
	DWORD WINAPI CrashDumpThreadFunc(LPVOID)
	{
		if (WaitForSingleObject(s_CrashDumpRequested, INFINITE) != WAIT_OBJECT_0)
			return 0;
		__try
		{
			WriteCrashDumpImpl(s_CrashDumpRequest);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// a fault while dumping: report no dump rather than hand out a broken one
			if (s_CrashDumpFile != INVALID_HANDLE_VALUE)
				CloseHandle(s_CrashDumpFile);
			DeleteFileW(s_CrashDumpPathW);
			s_CrashDumpPath[0] = 0;
		}
		SetEvent(s_CrashDumpDone);
		return 0;
	}

	void WriteCrashDump(unsigned int code, _EXCEPTION_POINTERS* pExp)
	{
		if (!s_CrashDumpFolder[0])                     return; // InitCrashDumpSupport has not run
		if (!IsCrashCode(code))                        return; // not a crash: keep the one dump for a real one
		if (InterlockedExchange(&s_CrashDumpTaken, 1)) return; // one per process, which also stops recursion

		s_CrashDumpRequest = CrashDumpRequest{ code, pExp, GetCurrentThreadId() };

		// Hand the request to the parked writer and wait for it, so that the faulting thread sits
		// here in the filter, its frames intact, until the dump has them. Bounded, and satisfied by
		// the writer ending as well: a crash path that never ends is worse than one without a dump.
		if (s_CrashDumpThread)
		{
			SetEvent(s_CrashDumpRequested);
			HANDLE doneOrGone[2] = { s_CrashDumpDone, s_CrashDumpThread };
			WaitForMultipleObjects(2, doneOrGone, FALSE, 120000);
		}
		else if (code != EXCEPTION_STACK_OVERFLOW)
			WriteCrashDumpImpl(s_CrashDumpRequest); // no writer thread: from here is better than not at all, except on a stack that is already gone
	}

	// The trace log gets the same lines the fatal handlers in MsgDispatch.cpp write, by the same
	// route: not through the message dispatch, whose lock this very thread may hold if it faulted
	// while reporting, and flushed at once, because neither ExitProcess nor WER runs a destructor
	// and the buffered tail of the log would be lost with them.
	void ReportFatalSE(CharPtr how, unsigned int code, _EXCEPTION_POINTERS* pExp)
	{
		DBG_WriteFatalLine(mySSPrintF("FATAL: {}OS Structured Exception 0x{:X} on thread {}: {}", how, code, GetThreadID(), GetExceptionText(code, pExp).c_str()).c_str());
		if (s_CrashDumpPath[0])
			DBG_WriteFatalLine(mySSPrintF("FATAL: a minidump of this process was written to {}", s_CrashDumpPath).c_str());
		else
			DBG_WriteFatalLine("FATAL: no minidump was written; InitCrashDumpSupport and WriteCrashDump in DmsException.cpp list the conditions");
		DBG_FlushLogs();
	}

	// The backstop: a fault on a thread that is inside no DMS_SE_CALL(BACK) region, a tile worker
	// or a Qt thread for instance, or main() before it enters its guarded part. Windows calls this
	// only when no frame handled the exception and no debugger is attached. It takes the dump and
	// says so, then passes the exception on to whatever filter was installed before, WER as a rule,
	// which ends the process the way it always did.
	LONG WINAPI DmsUnhandledExceptionFilter(_EXCEPTION_POINTERS* pExp)
	{
		unsigned int code = (pExp && pExp->ExceptionRecord) ? pExp->ExceptionRecord->ExceptionCode : 0;
		WriteCrashDump(code, pExp);
		ReportFatalSE("unhandled ", code, pExp);
		return s_PrevUnhandledExceptionFilter ? s_PrevUnhandledExceptionFilter(pExp) : EXCEPTION_CONTINUE_SEARCH;
	}

} // end anonymous namespace

// Called once at startup, from DMS_Rtc_Load, while there is a working heap to do it with; the
// crash path itself then needs no registry lookup, no allocation and no thread creation.
RTC_CALL void InitCrashDumpSupport()
{
	if (s_CrashDumpFolder[0])
		return;
	auto folder = Utf8_2_wchar(mySSPrintF("{}/CrashDumps", GetLocalDataDir().c_str()));
	if (!folder || wcslen(folder.get()) >= MAX_PATH)
		return;
	wcscpy_s(s_CrashDumpFolder, folder.get());
	for (wchar_t* p = s_CrashDumpFolder; *p; ++p) // LocalDataDir comes with forward slashes; the dump path is read by people
		if (*p == L'/')
			*p = L'\\';

	s_CrashDumpRequested = CreateEventW(nullptr, TRUE, FALSE, nullptr); // manual reset: each is set once, for the one dump
	s_CrashDumpDone      = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (s_CrashDumpRequested && s_CrashDumpDone)
		s_CrashDumpThread = CreateThread(nullptr, 0, CrashDumpThreadFunc, nullptr, 0, nullptr);
	if (s_CrashDumpThread)
	{
		// named, so that it can be told apart in a debugger and in the dump itself; resolved at run time
		// because the declaration depends on the _WIN32_WINNT the SDK headers were given
		using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
		if (auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")))
			setThreadDescription(s_CrashDumpThread, L"GeoDMS crash dump writer");
	}

	s_PrevUnhandledExceptionFilter = SetUnhandledExceptionFilter(DmsUnhandledExceptionFilter);
}

int signalHandling(unsigned int u, _EXCEPTION_POINTERS* pExp, bool passBorlandException)
{
//	if ((u == DMS_SE_CPP) || (u == EXCEPTION_BORLAND_ERROR))
	if (passBorlandException && u == EXCEPTION_BORLAND_ERROR)
			return EXCEPTION_CONTINUE_SEARCH;
	g_StructuredExceptionCode = u;

	// The handler runs after the unwind, when the EXCEPTION_RECORD that pExp names lies in released
	// stack memory. Its fields, the faulting address above all, go into the message, so keep a copy
	// for the handler to read; copying a fixed-size struct is all the filter may afford.
	if (pExp && pExp->ExceptionRecord)
	{
		g_ExceptionRecordCopy = *pExp->ExceptionRecord;
		g_ExceptionPointersCopy = { &g_ExceptionRecordCopy, nullptr };
		g_pExp = &g_ExceptionPointersCopy;
	}
	else
		g_pExp = pExp;

	WriteCrashDump(u, pExp); // here, and not in the handler: the faulting stack is still intact

	return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void  trans_SE2DMSfunc( unsigned int u, _EXCEPTION_POINTERS* pExp, bool mustTerminate)
{
#if defined(DMS_32)
	__asm fninit;
#endif
	assert(u != DMS_SE_CPP);

	if (mustTerminate)
	{
		DMS_Terminate();
		_CrtDbgBreak(); // try to get the debugger's attention
		ReportFatalSE("", u, pExp);
	}

	auto exceptionText = GetExceptionText(u, pExp);
	if (s_CrashDumpPath[0]) // a dump nobody knows about is a dump nobody reads
		exceptionText = mySSPrintF("{}\n\nA minidump of this process was written to\n{}\nPlease attach it when reporting this at\nhttps://github.com/ObjectVision/GeoDMS/issues"
		,	exceptionText.c_str(), s_CrashDumpPath);

	if (mustTerminate)
	{
		MG_CHECK(exceptionText.ssize() < 2000); // the exception texts are short; the dump path was appended to it
		char msgBuffer[5000];
		SilentMemoOutStreamBuff smosb(IterRange(msgBuffer, msgBuffer+5000));
		FormattedOutStream fos(&smosb);
		fos << "\"" << GetExeDir();
		fos << "\\GeoDmsGuiQt.exe\" \"/F";
		DoubleQuoteMiddle(smosb, exceptionText.begin(), exceptionText.end());
		fos << char(0);

		StartChildProcess(nullptr, msgBuffer);
		ExitProcess(GetLastExceptionCode());
	}

	// The recoverable path: about to throw a DmsException that the caller reports on its own terms
	// (an item failure, a message box, GeoDmsRun's stderr). The dump path travels in that message,
	// but the issue asks for it in the trace log itself as well, so record it there directly. Safe
	// here, unlike in the terminating branch above: this thread unwinds normally after the throw.
	if (s_CrashDumpPath[0])
		reportF_without_cancellation_check(MsgCategory::other, SeverityTypeID::ST_Error, "A minidump of this process was written to {}", s_CrashDumpPath);

	DmsException::throwMsgF( "{} Structured Exception: 0x{:X} raised:\n{}"
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

#endif //defined(WIN32)

//----------------------------------------------------------------------
// dms_assertion_failed
//----------------------------------------------------------------------

#if defined(_MSC_VER)

CppTranslatorContext::CppTranslatorContext(TCppExceptionTranslator trFunc)
		: m_PrevCppTranslator(SetCppTranslator(trFunc))
	{}

CppTranslatorContext::~CppTranslatorContext()
{
	if (IsMetaThread() && !m_PrevCppTranslator)
	{
		dms_assert(g_DebugStream);
	}
	SetCppTranslator(m_PrevCppTranslator);
}

#endif //defined(_MSC_VER)

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

void debugBreak()
{
#if defined(_MSC_VER)
	__debugbreak();
#else
	__builtin_trap();
#endif //defined(_MSC_VER)

}

void dms_check_failed(CharPtr msg, CharPtr fileName, unsigned line)
{
	reportF_without_cancellation_check(SeverityTypeID::ST_MajorTrace, "check failure: {}\n{}({})", msg, fileName, line);

#if defined(MG_DEBUG)
	debugBreak();
#endif

}

void dms_assertion_failed(CharPtr msg, CharPtr fileName, unsigned line)
{
#if defined(MG_DEBUG)
	debugBreak();
#endif
}

CharPtr GetContextPtr(WeakStr msg)
{
	return Search(CharPtrRange(msg), CharPtrRange("\n# "));
}

bool HasContext(WeakStr msg)
{
	return GetContextPtr(msg) != msg.csend();
}

RTC_CALL SharedStr GetFirstLine(WeakStr msg)
{
	CharPtr eolPtr = msg.find('\n');
	if (eolPtr == msg.csend())
		return msg;
	return SharedStr(CharPtrRange(msg.begin(), eolPtr));
}


//======================================= FileResult
#include "FileResult.h"

[[noreturn]] void FileResult::Throw(CharPtr context) const
{
	if (this->has_value())
		throwErrorF(context, "Operation succeeded but was expected to fail");
	else
		throwErrorF(context, "Operation failed with error: {}", this->error().c_str());
}

