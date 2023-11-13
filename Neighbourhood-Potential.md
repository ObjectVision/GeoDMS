A Neighbourhood Potential is the combination of a raster with a kernel.

**r** := **Potential(d,K)** implies that $r_{ij} = \\sum\\limits_{kl} d_{i-k, j-l} \\times K_{kl}$

This is AKA [[Convolution]]. See also the [[Potential]] function and the [[Potential with kernel]].

If we ignore the terms that are cut off at the borders, then $\\sum\\limits_{ij} R = \\sum\\limits_{ij} d \\times \\sum\\limits_{kl} K$.
The values unit of *R* is defined as the product of the values units of *d* and *K*.

Note the similarity of this operation with the multiplication of two polynomials in two unknowns, take:

$d(x,y) := \\sum\\limits_{ij} d_{ij} \\times x^i \\times y^j$

$K(x,y) := \\sum\\limits_{kl} K_{kl} \\times x^k \\times y^l$

$R(x,y) := \\sum\\limits_{ij} R_{ij} \\times x^i \\times y^j$

It follows that

*R*(*x*,*y*) = *d*(*x*,*y*) \* *K*(*x*,*y*) if we ignore the terms that are cut off at the borders.