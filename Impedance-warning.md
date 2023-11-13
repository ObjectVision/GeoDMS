*[[Network functions]] Impedance warning*

## memory and disk issues

The od variants of the Impedance functions result in a [[domain unit]] which is a subset of the cartesian product of the origin and destination domain units.

Be aware that this could result in a very large array, causing memory and disk issues. Be aware of the [[cardinality]] of these resulting domain units, especially when a full od table is requested.

For full symmetric od matrices, keep the number of elements of the origin/destination domain units limited (we do have experience with domains of more than 10.000 elements, but calculation times increase exponentially), use filters to cut the route search, or consider asymmetric variants with a lower number of origin zones.

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance options]]
- [[Impedance interaction potential]]
- [[Impedance additional]]
- [[Impedance future]]
- [[Impedance links]]