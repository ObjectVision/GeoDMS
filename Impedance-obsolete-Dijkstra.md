*[[Network functions]] obsolete: dijkstra *

<pre>
Starting from GeoDMS version 7.115, a new set of dijkstra functions replace the ones described below. 
The documentation of the old versions is still available for backward compatibility, 
the old functions will become obsolete in future versions.
</pre>

## syntax

- dijkstra(*impedance*: LinkSet-\>Measure, *F1*: LinkSet-\>NodeSet, *F2*: LinkSet-\>NodeSet, *startNodes*: startNode-\>NodeSet)

## definition

The dijkstra(*impedance*, *F1*, *F2*, *startNodes*) function results in the minimum [[impedance]] for each node with which a route is available from any of the given startNodes through a network that is defined by the dual link to node relations: F1 and F2. For this, the [dijkstra algorithm](https://en.wikipedia.org/wiki/Dijkstra's_algorithm) is applied.

To apply the dijkstra function, first configure a nodeset (based on the origin and destination points) and a linkset (usually based on the segments in an arc set, usually with roads).

The dijkstra function requires four [[arguments|argument]]:

1. *impedance*: a [[data item]] with the impedance of the links in the linkset;
2. *F1*: relation to the first node of a link set;
3. *F2*: relation to the last node of a link set;
4. *nr_destnode*: relation to the destination node in the destination domain unit.

## description

The function results in an impedance for the nodes in the nodeset.

Use the [[lookup]] function to relate these impedances to a set of destinations (see example).

The dijkstra function result also contains a [[subitem]]] named traceback which indicates for each node which link brings it back one step further back towards the nearest start-node. This traceback item can be used to calculate a flow. Use the [[trace_back]] function for this purpose.

The dijkstra function is used to apply the [dijkstra algorithm](https://en.wikipedia.org/wiki/Dijkstra's_algorithm) in a [undirected
graph](https://en.wikipedia.org/wiki/Graph_(discrete_mathematics)#Undirected_graph).
All links can be traversed in both directions. Use the dijkstra_directed function for directed [graphs](https://en.wikipedia.org/wiki/Directed_graph).

## applies to

- data item *impedance* with float32 or float64 [[value type]]
- data items *F1*, *F2* and *nr_destnode* with unit32 value type

## conditions

The domain units of attributes *impedance*, *F1* and *F2* must match.

## example

<pre>
attribute&lt;meter&gt; dist_nodeset (nodeset) := <B>dijkstra(</B>dist ,F1 ,F2, DestNode_rel<B>)</B>;  
<I>// replace this obsolete syntax for the new dijkstra functions to:   
   // dijkstra_s(  
   //    'bidirectional;startPoint(Node_rel);node:TraceBack'  
   //   , LinkSet/length, Network/F1, Network/F2, destnoder_rel  
   // )</I>  
  
attribute&lt;meter&gt; distance (origin) := dist_nodeset[network/OrgNode_rel];
</pre>

| length  | F1  | F2  |
|---------|----:|----:|
| 92.66   | 8   | 7   |
| 88.72   | 1   | 4   |
| 190.76  | 16  | 15  |
| 79.42   | 2   | 3   |
| 131.01  | 5   | 4   |
| 63.08   | 6   | 7   |
| 56.50   | 9   | 10  |
| 119.52  | 12  | 11  |
| 220.47  | 14  | 15  |
| 180.79  | 18  | 20  |
| 610.39  | 19  | 17  |
| 18.67   | 3   | 0   |
| 782.38  | 4   | 11  |
| 909.26  | 7   | 1   |
| 688.6   | 10  | 3   |
| 26.78   | 11  | 13  |
| 2161.49 | 13  | 17  |
| 2048.6  | 15  | 10  |
| 41.71   | 20  | 21  |
| 137.24  | 17  | 20  |

*domain linkset, nr of rows = 19*  

| destnoder_rel |
|--------------:|
| 14            |
| 6             |
| 2             |
| 9             |

*domain destination, nr of rows = 4*  

| **dist_nodeset** |
|-----------------:|
| **98.09**        |
| **972.34**       |
| **0**            |
| **79.43**        |
| **1061.05**      |
| **1192.06**      |
| **0**            |
| **63.08**        |
| **155.74**       |
| **824.54**       |
| **768.03**       |
| **1843.44**      |
| **1962.96**      |
| **1870.21**      |
| **0**            |
| **220.47**       |
| **411.23**       |
| **610.39**       |
| **928.43**       |
| **0**            |
| **747.63**       |
| **789.34**       |

*domain nodeset, nr of rows = 21*  

| **distance** |
|-------------:|
| **0**        |
| **1962.96**  |
| **824.54**   |
| **928.43**   |
| **1192.06**  |

*domain origin, nr of rows = 5* 