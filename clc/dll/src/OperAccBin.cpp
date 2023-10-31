// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "ClcPCH.h"
#pragma hdrstop

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

	tl_oper::inst_tuple<typelists::num_objects, OperAccTotBin<covariance_total <_>>, AbstrOperGroup*> s_CovT(&cogCov);
	tl_oper::inst_tuple<typelists::num_objects, OperAccTotBin<correlation_total<_>>, AbstrOperGroup*> s_CorrT(&cogCorr);

	tl_oper::inst_tuple<typelists::num_objects, OperAccPartBin<covariance_partial_best <_>>, AbstrOperGroup*> s_CovP(&cogCov);
	tl_oper::inst_tuple<typelists::num_objects, OperAccPartBin<correlation_partial_best<_>>, AbstrOperGroup*> s_CorrP(&cogCorr);
}
