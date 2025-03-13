// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif


#ifndef __RTC_GEO_RNDPOINT_H
#define __RTC_GEO_RNDPOINT_H

#include "geo/SelectPoint.h"

//----------------------------------------------------------------------
// Section      : wrapper around boost::random
//----------------------------------------------------------------------

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>

using rnd_engine_type = boost::mt19937;
using rnd_seed_type   = rnd_engine_type::result_type;

#endif // __RTC_GEO_RNDPOINT_H


