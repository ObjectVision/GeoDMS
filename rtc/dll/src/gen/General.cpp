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
#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "ser/FormattedStream.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"

#include "rtctypemodel.h"
#include "RtcVersion.h"
#include "RtcInterface.h"


#ifdef MG_DEBUG
#	define	DMS_CONFIGURATION_NAME "Debug"
#else
#	define	DMS_CONFIGURATION_NAME "Release"
#endif

#if defined(DMS_64)
#	define DMS_PLATFORM "x64"
#else
#	define DMS_PLATFORM "Win32"
#endif

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
#pragma message( "MSC_VER   : " BOOST_STRINGIZE( _MSC_VER ) )
#if defined(__EDG_VERSION__)
#pragma message( "EdgVersion: " BOOST_STRINGIZE( MG_EDG_VERSION ) )
#endif
#pragma message( "StdVersion: " BOOST_STDLIB )
#pragma message( "Platform  : " BOOST_PLATFORM )
#pragma message( "DmsVersion: " BOOST_STRINGIZE( DMS_VERSION_MAJOR ) "." BOOST_STRINGIZE( DMS_VERSION_MINOR ) "." BOOST_STRINGIZE( DMS_VERSION_PATCH ) )
#pragma message( "DmsDate   : " DMS_VERSION_DATE )
#pragma message( "DmsTime   : " DMS_VERSION_TIME )
#pragma message( "==========:---------------" )

RTC_CALL CharPtr DMS_CONV DMS_GetVersion()
{
	return "GeoDms "
		BOOST_STRINGIZE( DMS_VERSION_MAJOR ) "." BOOST_STRINGIZE( DMS_VERSION_MINOR )  "." BOOST_STRINGIZE(DMS_VERSION_PATCH)
#if defined(MG_DEBUG)
		" [" DMS_CONFIGURATION_NAME "]"
#endif
#if !defined(DMS_64)
		" [" DMS_PLATFORM "]"
#endif
		" [" DMS_VERSION_DATE " " DMS_VERSION_TIME "]"
		MG_DEBUGCODE( MG_EDG_VERSION_STR )
	;
}


// =================================  BEGIN VersionComponent

#include "VersionComponent.h"
#include "set/VectorFunc.h"

std::vector<const AbstrVersionComponent*> s_VersionComponents;

AbstrVersionComponent::AbstrVersionComponent()
{
	s_VersionComponents.push_back(this);
}

AbstrVersionComponent::~AbstrVersionComponent()
{
	vector_erase(s_VersionComponents, this);
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
	for (auto c: s_VersionComponents)
		c->Visit(clientHandle, callBack, 1);
}

VersionComponent s_Compiler  (CC_COMPILER_NAME " ( _MSC_VER = " BOOST_STRINGIZE(_MSC_VER) " ) ");
VersionComponent s_Platform("Platform : " BOOST_PLATFORM);
static SharedStr s_PtrSizeC = mySSPrintF("ptr size : %d bits", sizeof(void*)*8);
static SharedStr s_IntSizeC = mySSPrintF("int size : %d bits", sizeof(int  )*8);
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
	return SharedStr(outBuff.GetData(), outBuff.GetDataEnd());
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

#include "RtcInterface.h"
#include "geo/Range.h"
#include "geo/IterRange.h"

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
