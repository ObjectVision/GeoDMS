*[[Network functions]] obsolete: dijkstra_directed*
 
<pre>
Starting from GeoDMS version 7.115, a new set of dijkstra functions replace the ones described below. 
The documentation of the old versions is still available for backward compatibility, 
the old functions will become obsolete in future versions.
</pre>

## syntax

- dijkstra_directed(*impedance*: *LinkSet->Measure*, *F1: LinkSet->NodeSet*, *F2: LinkSet->NodeSet*, *startNodes*: *startNode->NodeSet*)

## definition

The dijkstra_directed function is a variant of the [[dijkstra obsolete: dijkstra|dijkstra functions]]. It is used to apply the [[Dijkstra algorithm|https://en.wikipedia.org/wiki/Dijkstra's_algorithm dijkstra algorithm]] in a [[directed graph|https://en.wikipedia.org/wiki/Directed_graph]]. [[Impedances|impedance]] are calculated taken into account the direction of the links. The *F1* [[argument]] defines the start index number for each link, the *F2* argument the end [[index numbers]]. To apply the dijkstra algorithm in an [[undirected graph|https://en.wikipedia.org/wiki/Graph_(discrete_mathematics)#Undirected_graph]], use the [[dijkstra obsolete: dijkstra|dijkstra functions]].


## description
Undirected links in a directed graph should be added twice to the linkset, in both directions.

The [[connect]] function to add new nodes to the network is at the moment not supported for directed graphs. 

Previous users have partially surpassed this problem by first connecting to an undirected version of the graph before directing the graph for the dijkstra_directed function.