*[[Network functions]] Impedance functions*

There are three preferred Impedance operations:

- **impedance_table(...)**: when all given startPoints have to be considered as a **single** origin zone, resulting in:
    - an `attribute` DstZone->Impedance with the lowest route Impedance per destination zone.
    - optional attributes as sub-items depending on the used options.
    - so the result is a table with for each origin the impedance to the nearest destination.
- **impedance_matrix(...)** when startPoints can relate to **multiple** origin zones, resulting in
    - a `unit<uint32>` reflecting the set of all or all found od-pairs
    - optional attributes as sub-items depending on the used options.
- **impedance_matrix_od64(...)**, similar to **impedance_matrix_od(...)**, but resulting in a `unit<uint64>` in order to accommodate more than $2^{32} − 2$ od-pairs.

All these operations require at least the following four arguments:

- options : {∅} → String, a String parameter indicating function options, described below.
- impedance : Link → Impedance, a Measure (Float32 or Float64) attribute of Links with the Impedance per Link.
- f1 : Link → Node, a **Node** attribute of **Link** indicating the from node of each link.
- f2 : Link → Node, a **Node** attribute of **Link** indicating the to node of each link.

Note that Impedance functions can run different OrgZones in parallel, up to the number of cores of the running machine, as their tree growth is independent. This does not affect impedance_table, as that only grows one tree.

## phased out variants
- dijkstra_s(...) to be replaced by impedance_table(...)
- dijkstra_m(...) to be replaced by impedance_matrix(...)
- dijkstra_m64(...) to be replaced by impedance_matrix_od64(...)

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance options]]
- [[Impedance warning]]
- [[Impedance interaction potential]]
- [[Impedance additional]]
- [[Impedance future]]
- [[Impedance links]]