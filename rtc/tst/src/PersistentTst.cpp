// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
#include "RtcPCH.h"
#include "mci/Persistent.h"
#include "ser/polyStream.h"

/**********  TEST CODE ********************/

struct PTestje : Object 
{

//	typedef PTestje this_t;
//	typedef Object inherited_t;

	virtual void ReadObj (PolymorphInpStream& in)
	{
		in >> i;
	}
	virtual void WriteObj(PolymorphOutStream& out) const
	{
		out << i;
	}

	int i;

	DECL_RTTI(RTC_CALL, Class)
};

IMPL_DYNC(PTestje, Class)

#include <iostream.h>

void PTestCode()
{
	PTestje t, *u;
	t.i = 3;
//	std::vector<char> buff;
	VectorOutStreamBuff os;
	PolymorphOutStream ops(&os);
	ops << &t;

	MemoInpStreamBuff is(os.GetData(), os.GetData() + os.CurrPos() );
	PolymorphInpStream ips(&is);
	ips >> u;

	cout << u->i; // must be 3.
}

