*[[Network functions]] Impedance future*

## possible extensions

1. Log-logistic distance decay: **d -> 1/(1+e<sup>a+blnd</sup>) = 1 / (1+e<sup>a</sup>d<sup>b</sup>)**.

## possible speedup optimizations

1. Parallel processing of different OrgZones, as most time is now spent in growing trees, which can be done per origin zone separately, a speedup factor is expected close to the number of available cores.
2. Pre-processing, overview: <http://algo2.iti.kit.edu/schultes/hwy/dynamic.pdf> using:
    - **highway hierarchies** along the lines of <http://algo2.iti.kit.edu/schultes/hwy/distTable.pdf>, and <http://algo2.iti.kit.edu/schultes/hwy/esaHwyHierarchiesSlides.pdf>
    - **edge reduction**
    - **transit node routing** and separators
    - **A\* methods**, geometric containment and goal direction search, useful only for individual OD-pairs.

## see also

- [[Impedance general (formerly known as Dijkstra)]]
- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance options]]
- [[Impedance warning]]
- [[Impedance interaction potential]]
- [[Impedance additional]]
- [[Impedance links]]