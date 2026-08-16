// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// General: miscellaneous gen-subsystem services, version components and
// MG_DEBUG self-tests of core rtc facilities.

#include <boost/config/helper_macros.hpp> // BOOST_STRINGIZE (needed by MSVC and GCC; boost/format no longer provides it transitively)

#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Environment.h"
#include "utl/TimeFmt.h"
#include "utl/StrFormat.h"

#include "rtctypemodel.h"
#include "RtcInterface.h"

#include "RtcVersionNumbers.h" // tracked single source of truth; see its header comment
#include "buildstamp.h"        // generated per build (date/time only)


#ifdef MG_DEBUG
#	define	DMS_CONFIGURATION_NAME "Debug"
#else
#	define	DMS_CONFIGURATION_NAME "Release"
#endif

// Platform string in vcpkg-triplet-like form: <arch>-<os>[-<toolset>], e.g.
// "x64-windows-cmake", "x64-windows-msbuild", "x64-linux-cmake". Surfaces in
// the GUI title bar (DMS_GetVersion) and via DMS_GetPlatform.
#if defined(DMS_64)
#	define DMS_PLATFORM_ARCH "x64"
#else
#	define DMS_PLATFORM_ARCH "x86"
#endif

#if defined(_WIN32)
#	define DMS_PLATFORM_OS "windows"
#elif defined(__linux__)
#	define DMS_PLATFORM_OS "linux"
#elif defined(__APPLE__)
#	define DMS_PLATFORM_OS "osx"
#else
#	define DMS_PLATFORM_OS "unknown"
#endif

// Toolset distinction: cmake defines DMS_TOOLSET_CMAKE in CMakeLists.txt; in
// its absence we are being built by MSBuild (the .vcxproj/.sln path). Both are
// surfaced explicitly so the GUI title bar makes the toolset unambiguous.
#if defined(DMS_TOOLSET_CMAKE)
#	define DMS_PLATFORM_TOOLSET "-cmake"
#else
#	define DMS_PLATFORM_TOOLSET "-msbuild"
#endif

#define DMS_PLATFORM DMS_PLATFORM_ARCH "-" DMS_PLATFORM_OS DMS_PLATFORM_TOOLSET

Float64 DMS_CONV DMS_GetVersionNumber()
{
	return DMS_VERSION_MAJOR + 0.01 * DMS_VERSION_MINOR + DMS_VERSION_PATCH * 0.0001;
}

UInt32 DMS_CONV DMS_GetMajorVersionNumber()
{
	return DMS_VERSION_MAJOR;
}

UInt32 DMS_CONV DMS_GetMinorVersionNumber()
{
	return DMS_VERSION_MINOR;
}

UInt32 DMS_CONV DMS_GetPatchNumber()
{
	return DMS_VERSION_PATCH;
}

CharPtr DMS_CONV DMS_GetPlatform()
{
	return DMS_PLATFORM;
}

CharPtr DMS_CONV DMS_GetBuildConfig()
{
	return DMS_CONFIGURATION_NAME;
}

#include <boost/config.hpp>

#if defined(__EDG_VERSION__)
#  define MG_EDG_VERSION __EDG_VERSION__
#  define MG_EDG_VERSION_STR " [EDG: " BOOST_STRINGIZE( MG_EDG_VERSION ) "]"
#else
#  define MG_EDG_VERSION -1
#  define MG_EDG_VERSION_STR ""
#endif // defined(__EDG_VERSION__)

#pragma message( "==========:---------------" )
#pragma message( "Compiler  : " BOOST_COMPILER )
#if defined(_MSC_VER)
#pragma message( "MSC_VER   : " BOOST_STRINGIZE( _MSC_VER ) )
#endif
#if defined(__EDG_VERSION__)
#pragma message( "EdgVersion: " BOOST_STRINGIZE( MG_EDG_VERSION ) )
#endif
#pragma message( "StdVersion: " BOOST_STDLIB )
#pragma message( "Platform  : " BOOST_PLATFORM )
#pragma message( "DmsVersion: " BOOST_STRINGIZE( DMS_VERSION_MAJOR ) "." BOOST_STRINGIZE( DMS_VERSION_MINOR ) "." BOOST_STRINGIZE( DMS_VERSION_PATCH ) )
#pragma message( "DmsDate   : " DMS_BUILD_DATE )
#pragma message( "DmsTime   : " DMS_BUILD_TIME )
#pragma message( "==========:---------------" )

RTC_CALL CharPtr DMS_CONV DMS_GetVersion()
{
	return "GeoDms "
		BOOST_STRINGIZE( DMS_VERSION_MAJOR ) "." BOOST_STRINGIZE( DMS_VERSION_MINOR )  "." BOOST_STRINGIZE(DMS_VERSION_PATCH)
#if defined(MG_DEBUG)
		" [" DMS_CONFIGURATION_NAME "]"
#endif
		" [" DMS_PLATFORM "]"
		" [" DMS_BUILD_DATE "]"
		MG_DEBUGCODE( MG_EDG_VERSION_STR )
	;
}


// =================================  BEGIN VersionComponent

#include "VersionComponent.h"
#include "set/VectorFunc.h"

// Function-local static (Meyers singleton): components register from OTHER translation units' static
// initializers (e.g. Environment.cpp's s_ExeComponent), whose order relative to this TU is unspecified.
// A namespace-scope vector could be pushed into BEFORE its own constructor ran; the constructor then
// re-zeroed it, leaking the first heap buffer (the unit suite's per-test 8-byte CRT leak {&s_ExeComponent}).
static std::vector<const AbstrVersionComponent*>& GetVersionComponents()
{
	static std::vector<const AbstrVersionComponent*> s_VersionComponents;
	return s_VersionComponents;
}

AbstrVersionComponent::AbstrVersionComponent()
{
	GetVersionComponents().push_back(this);
}

AbstrVersionComponent::~AbstrVersionComponent()
{
	vector_erase(GetVersionComponents(), this);
}

VersionComponent::VersionComponent(CharPtr name)
	:	m_Name(name)
{
}

VersionComponent::~VersionComponent()
{
}

void VersionComponent::Visit(ClientHandle cHandle, VersionComponentCallbackFunc callBack, UInt32 componentLevel) const
{
	callBack(cHandle, componentLevel, m_Name);
}

void DMS_CONV DMS_VisitVersionComponents(ClientHandle clientHandle, VersionComponentCallbackFunc callBack)
{
	for (auto c: GetVersionComponents())
		c->Visit(clientHandle, callBack, 1);
}

VersionComponent s_Compiler  (CC_COMPILER_NAME " ( _MSC_VER = " BOOST_STRINGIZE(_MSC_VER) " ) ");
VersionComponent s_Platform("Platform : " BOOST_PLATFORM);
static SharedStr s_PtrSizeC = mySSPrintF("ptr size : {} bits", sizeof(void*)*8);
static SharedStr s_IntSizeC = mySSPrintF("int size : {} bits", sizeof(int  )*8);
VersionComponent s_PtrSize(s_PtrSizeC.c_str());
VersionComponent s_IntSize(s_IntSizeC.c_str());

VersionComponent s_StrVersion( BOOST_STDLIB );
#if defined(__EDG_VERSION__)
VersionComponent s_EdgVersion("EdgVersion: " BOOST_STRINGIZE(MG_EDG_VERSION));
#endif

// =================================  END VersionComponent

SharedStr g_sessionStartTime;

SharedStr GetCurrentTimeStr()
{
	VectorOutStreamBuff outBuff;
	FormattedOutStream fout(&outBuff, FormattingFlags::None);
	fout << StreamableDateTime();
	return SharedStr(CharPtrRange(outBuff.GetData(), outBuff.GetDataEnd()));
}

RTC_CALL void DMS_CONV DMS_Rtc_Load()
{
	g_sessionStartTime = GetCurrentTimeStr();
}

SharedStr GetSessionStartTimeStr()
{
	return g_sessionStartTime;
}

#if defined(MG_DEBUG)

#include "geom/Range.h"
#include "vt/iterrange.h"

bool RangeTest()
{
	DBG_START("Range", "TEST", true);

	bool result = true;
	result &= DBG_TEST("EmptyRange",     IsDefined(Range<UInt32>()) );
	result &= DBG_TEST("EmptyRange",     Range<UInt32>().empty() );
	result &= DBG_TEST("UndefRange",    !IsDefined(Range<UInt32>(Undefined())) );
	result &= DBG_TEST("UndefRange",     Range<UInt32>(Undefined()).empty() );
	result &= DBG_TEST("ZeroRange",      IsDefined(Range<UInt32>(0, 0)) );
	result &= DBG_TEST("ZeroRange",      Range<UInt32>(0, 0).empty() );

	return result;
}

bool IterRangeTest()
{
	DBG_START("IterRange", "TEST", true);

	bool result = true;
	result &= DBG_TEST("EmptyIterRange",!IterRange<CharPtr>().size() );
	result &= DBG_TEST("ZeroIterRange",  IterRange<CharPtr>().empty() );

	return result;
}

bool DMS_RTC_Test()
{
	DBG_START("RTC", "TEST", true);

	bool result = true;
	result &= DBG_TEST("Range",     RangeTest());
	result &= DBG_TEST("IterRange", IterRangeTest());

	return result;
}

#endif
