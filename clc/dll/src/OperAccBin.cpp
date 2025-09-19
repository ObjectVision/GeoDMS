// Copyright (C) 1998-2025 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

#include "ser/StringStream.h"

#include "OperAccBin.h"

#include "prototypes.h"

#include "AggrBinStructNum.h"

// *****************************************************************************
//											INSTANTIATION
// *****************************************************************************

namespace 
{
	CommonOperGroup cogCov("cov");
	CommonOperGroup cogCorr("corr");

	tl_oper::inst_tuple<typelists::num_objects, tl::bind_placeholders<OperAccTotBin, covariance_total <ph::_1>>> s_CovT(&cogCov);
	tl_oper::inst_tuple<typelists::num_objects, tl::bind_placeholders<OperAccTotBin, correlation_total<ph::_1>>> s_CorrT(&cogCorr);

	tl_oper::inst_tuple<typelists::num_objects, tl::bind_placeholders<OperAccPartBin, covariance_partial_best <ph::_1>>> s_CovP(&cogCov);
	tl_oper::inst_tuple<typelists::num_objects, tl::bind_placeholders<OperAccPartBin, correlation_partial_best<ph::_1>>> s_CorrP(&cogCorr);
}
