// Copyright (C) 1998-2026 Object Vision B.V. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "mem/HeapSequenceProvider.h"

// =================================================== class heap_sequence_provider : public abstr_sequence_provider<V>

void throwInsertError(SizeT seqSize, SizeT n)
{
	throwErrorF(" heap_sequence", 
		seqSize == 0
		?	"cannot insert %I64u elements to an empty heap_sequence" 
		:	"cannot insert %I64u elements to a heap_sequence that already contained %I64u elements"
	,	(UInt64)n
	,	(UInt64)seqSize
	);
}

