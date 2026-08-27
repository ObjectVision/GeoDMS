// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////
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

