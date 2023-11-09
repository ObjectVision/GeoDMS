// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#pragma once

#ifndef __SYM_BASE_H
#define __SYM_BASE_H

#ifdef DMSYM_EXPORTS
#	define SYM_CALL __declspec(dllexport)
#elif DMSYM_STATIC
#	define SYM_CALL
#else
#	define SYM_CALL __declspec(dllimport)
#endif

#include "RtcBase.h"
#include "set/Token.h"

using Number_t = double;

struct Number
{
	explicit Number(Number_t v)
		: m_Value(v)
	{}
	operator Number_t() const { return m_Value;  }

	Number_t m_Value;
};

#include "LispRef.h"


#endif // __SYM_BASE_H
