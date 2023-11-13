*[[Network functions]] Impedance general*

## Dijkstra algorithm

The [dijkstra algorithm](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm) determines the minimum impedance of the routes through a
[network/graph](https://en.wikipedia.org/wiki/Graph_(abstract_data_type)) of [nodes](https://en.wikipedia.org/wiki/Vertex_(graph_theory)) and
[links](https://en.wikipedia.org/wiki/Glossary_of_graph_theory_terms#edge) with given link-impedance for each combination of given origins and
destinations.

[[images/Dijkstra_Animation.gif]]

## Impedance functions in the GeoDMS

In the GeoDMS a [[family of functions|Impedance functions]] is implemented using this algorithm to find the minimum impedance in a [network/graph](https://en.wikipedia.org/wiki/Graph_(abstract_data_type)).

These functions are used to calculate shortest paths in directed/bidirectional graphs, OD matrices, and multiple impedances, with or without filters.

The user can filter the set of origin-destination combinations (further: od-pairs) in two ways:

-   by setting a maximum impedance (per origin) and/or
-   a maximum amount of visited destination mass per origin.

Furthermore, the user can specify the production of additional results per origin, destination, link, or od-pair such as:

-   the specification of an alternative link impedance measure, to be aggregated over the found routes.
-   interaction results: trip generation, distribution, and network flow assignment, following Alonso's Theory of Movements.

Origin and destination zones can be specified as a set of start points or endpoints that relate to the network nodes with an optional additional
impedance per start point and/or endpoint.

When no od-pair-related output is requested, the memory usage remains proportional to the memory size of the given network and calculation
time remains proportional to the number of actually visited nodes (plus some initialization time proportional to the size of the network).

## see also

- [[Impedance key entities]]
- [[Impedance functions]]
- [[Impedance options]]
- [[Impedance warning]]
- [[Impedance interaction potential]]
- [[Impedance additional]]
- [[Impedance future]]
- [[Impedance links]]