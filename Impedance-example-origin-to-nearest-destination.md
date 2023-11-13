*[[Network functions]] Impedance example: origin to nearest destination*

## syntax

-   impedance_table(*bidirectional;startPoint(Node_rel)*, *Impedance*, *Network/F1*, *Network/F2*, *nr_destnode*);

## definition

This example describes how a [[impedance function|impedance functions]] can be used to calculate the [[impedance]] per origin to the nearest destination.

The function results in an [[attribute]] with the [[values unit]] of the *impedance* [[argument]] and the [[domain unit]] of the nodeset used.

This variant requires four arguments:

1.  *impedance*: a [[data item]] with the impedance of the links in the linkset;
2.  *F1*: [[relation]] to the first node of a linkset;
3.  *F2*: relation to the last node of a linkset;
4.  *nr_destnode*: relation to the destination node in the destination domain unit.

## description

The function results in an impedance for the nodes in the nodeset. Use the [[lookup]] function to relate these impedances to a set of destinations (see example).

## options

Different options are available for this impedance variant. They are configured with extra options in the first argument, for instance:

1. **flow calculations**: Use as first argument: *'bidirectional;startPoint(Node_rel);node:TraceBack'*. The function now also results in an attribute named: <B>traceback</B>, indicating for each node which link brings it back one step further back towards the nearest start node, used to calculate flows.
2. **directed graph**: The option bidirectional in the first argument is used to apply the [dijkstra algorithm](https://en.wikipedia.org/wiki/Dijkstra's_algorithm) in an [undirected graph](https://en.wikipedia.org/wiki/Graph_(discrete_mathematics)#Undirected_graph). All links can be traversed in both directions. Use as first argument: *'directed;startPoint(Node_rel)*; for [directed graphs](https://en.wikipedia.org/wiki/Directed_graph).
3. **maximum impedance**: A maximum impedance can be configured if distances above a threshold are not relevant for instance to increase the calculation speed. To configure a maximum impedance, configure as first argument: *'bidirectional;startPoint(Node_rel);cut(OrgZone_max_imp)'* and as fifth argument a parameter with the values unit of the impedance argument and the cut value. All resulting values for the nodeset above the maximum impedance will get the maximum value for the value type of the impedance argument.

## applies to

- data item *impedance* with float32 or float64 [[value type]]
- data items *F1*, *F2* and *nr_destnode* with unit32 value type

## conditions

The [[domain unit]] of attributes *impedance*, *F1* and *F2* must match.

## since version

7.115

## example
<pre>
attribute&lt;meter&gt; dist_nodeset (nodeset):= <B>impedance_table(</B>
       'bidirectional;startPoint(node_rel)'
      , LinkSet/length
      , Network/F1
      , Network/F2
      , destnoder_rel
   <B>)</B>;

attribute&lt;meter&gt; distance (origin) := dist_nodeset[network/OrgNode_rel];
</pre>

| length  | F1  | F2  |
|--------:|----:|----:|
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

*domain Destination, nr of rows = 4*


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