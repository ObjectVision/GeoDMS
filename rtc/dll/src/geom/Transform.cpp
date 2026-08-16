// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif

// Transformation<T> implementation: composition and streaming.

#include "geom/Transform.h"
#include "ser/FormattedStream.h"
#include "ser/PointStream.h"

FormattedOutStream& operator << (FormattedOutStream& os, const CrdTransformation& t)
{
	if (t.IsAxisSeparable())
		os << "[ *" << t.Factor() << " + " << t.Offset() << "]";
	else
	{
		auto h = t.Matrix();
		os << "[ H";
		for (int i = 0; i != 9; ++i)
			os << (i ? " " : "= ") << h[i];
		os << "]";
	}
	return os;
}

template class Transformation<CrdType>;

