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
/*
 *  Name        : per\persistent.cpp
 *  SubSystem   : RTL
 *  Author      : M. Hilferink
 *  Company     : YUSE GSO Object Vision B.V.
 */

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

	DECL_RTTI(RTC_CALL, Class);
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

