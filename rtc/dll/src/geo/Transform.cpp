// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "geo/Transform.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"

FormattedOutStream& operator << (FormattedOutStream& os, const CrdTransformation& t)
{
	os << "[ *" << t.Factor() << " + " << t.Offset() << "]";
	return os;
}

template class Transformation<CrdType>;

