Polygon convolution is the [[convolution]] of mappings of *F*<sup>2</sup> → *Bool*, that are represented by polygons that indicate the boundary of the closed subset(s) of *F*<sup>2</sup> that are mapped onto TRUE; were addition is replaced by the or operation. The convolution operator is here represented as * and has type (*F*<sup>2</sup>→*Bool*) × (*F*<sup>2</sup>→*Bool*) → (*F*<sup>2</sup>→*Bool*).

For_each *q* ∈ *F*<sup>2</sup> and *M*<sub>*a*</sub>, *M*<sub>*b*</sub> ∈ *F*<sup>2</sup> → *Bool*:

(*M*<sub>*a*</sub>\**M*<sub>*b*</sub>)*q* := ⋁<sub>*p* ∈ *F*<sup>2</sup></sub> : *M*<sub>*a*</sub>(*p*) ∧ *M*<sub>*b*</sub>(*q*−*p*)

Further let *P*<sub>*i*</sub> := *δ**M*<sub>*i*</sub>, it seems that *δ*(*M*<sub>*a*</sub>\**M*<sub>*b*</sub>) = *δ**M*<sub>*a*</sub> \* *M**b* ∧ *M*<sub>*a*</sub> \* *δ**M**b* similar to the diffential of the product of two functions.

See also: [Boost Polygon's minkowski tutorial](http://www.boost.org/doc/libs/1_49_0/libs/polygon/doc/gtl_minkowski_tutorial.htm)