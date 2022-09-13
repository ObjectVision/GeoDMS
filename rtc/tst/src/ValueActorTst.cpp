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
#include "act/CalculatedValueActor.h"
#include <ostream>
#include <iostream>
#include "ptr/RefPtr.h"
#include "utl/StackProtector.h"
#include <functional>
#include "geo/PairOrder.h"
//#include "geo/Polygon.h"
#include "ser/VectorStream.h"
#include "ser/TupleStream.h"
#include "ser/AsString.h"

int main()
{
  ValueActor<int> a = 1;
  ValueActor<int> b = 2; 
  StackProtecter<ValueActor<int> > sa(a), sb(b);

  RefPtr<CalculatedValueActor<std::plus      <int> > > c = new CalculatedValueActor<std::plus      <int> >(&a, &b);
  RefPtr<CalculatedValueActor<std::multiplies<int> > > d = new CalculatedValueActor<std::multiplies<int> >(c, c);

  // calculate (a+b)*(a+b) = (1+2)*(1+2) = 9
  std::cout << *d << std::endl; 

  // reset a and calculate (a+b)*(a+b) = (3+2)*(3+2) = 25
  a = 3;
  std::cout << *d << std::endl;

  std::cout << "\nPolygonTest:";
  SPoint data[4] = { SPoint( 1, 2), SPoint(3,4), SPoint(5,6),SPoint(7,8)};
  SPolygon poly(data, data+4);
  std::cout << AsString(poly);
  return 0;
}

