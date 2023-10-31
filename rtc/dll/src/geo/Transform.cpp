// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(_MSC_VER)
#pragma hdrstop
#endif

#include "geo/Transform.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"

FormattedOutStream& operator << (FormattedOutStream& os, const CrdTransformation& t)
{
	os << "[ *" << t.Factor() << " + " << t.Offset() << "]";
	return os;
}

template class Transformation<CrdType>;

