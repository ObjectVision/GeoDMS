*[[Network functions]] obsolete: Dijkstra OD*

<pre>
Starting from GeoDMS version 7.115, a new set of dijkstra functions replace the ones described below.
The documentation of the old versions is still available for backward compatibility,
the old functions will become obsolete in future versions.
</pre>

## syntax

- dijkstra_od(*link_impedance,link_node1_rel, lin_node2_rel, originloc_node_rel, originloc_impedance, originloc_origin_rel,destloc_node, destloc_impedance, destloc_dest_rel*)

- dijkstra_od_maxdist(*link_impedance,link_node1_rel, lin_node2_rel,originloc_node_rel, originloc_impedance, originloc_origin_rel,destloc_node, destloc_impedance, destloc_dest_rel,origin_impedance_max*)

## definition

The dijkstra_od functions apply the [dijkstra algorithm](https://en.wikipedia.org/wiki/Dijkstra's_algorithm) to calculate the shortest network impedance from a set of origin points to a set of destination points.**The result is an O(rigin) D(estination) matrix,** in Dutch: Herkomst/Bestemmingsmatrix.

- dijkstra_od(*link_impedance,link_node1_rel, lin_node2_rel,originloc_node_rel, originloc_impedance, originloc_origin_rel,destloc_node, destloc_impedance, destloc_dest_rel*)

or

- dijkstra_od_maxdist(*link_impedance,link_node1_rel, lin_node2_rel,originloc_node_rel, originloc_impedance, originloc_origin_rel,destloc_node, destloc_impedance, destloc_dest_rel,origin_impedance_max*)

with:

- *link_impedance*: a [[data item]] with the [[impedance]] of the links of a linkset; *link_node1_rel*: a [[relation]] to the first node of a link set;
- *link_node2_rel*: a relation to the last node of a link set;

- *originloc_node_rel*: a relation to the origin nodes;
- *originloc_impedance*: start impedance for each origin, from the startpoint to the startnode;
- *originloc_origin_rel*: a relation to each origin;

- *destloc_node*: a relation to destination node;
- *destloc_impedance*: end impedance for each destination, from the endnode to the end point;
- *destloc_dest_rel*: a relation to each destination;

- *origin_impedance_max*: maximum impedance for the resulting sparse matrix of od_pairs

## description

To apply any dijkstra function, first configure a nodeset (based on the origin and destination points) and a linkset (based on the segments in an arc set, usually with roads).

These variants of the dijkstra function result in a new [[domain unit]] with a [[subitem]] called impedance. This subitem indicates the impedance from each origin point towards each destination point. The [[values unit]] is the values unit of the three impedance
[[arguments|argument]]. The [[cardinality]] of the new domain unit is the product of the cardinality of the origin and destination domain units.

The relations the to origin(originn_nr) and destination(destination_nr) domain units can be configured with the [[expressions|expression]] presented in the example.

## applies to

- [[attributes|attribute]] *impedance_link*, *impedance_origin* and *impedance_destination* with float32 or float64 [[value type]]
- attributes *F1*, *F2*, *nr_originnode*, *nr_destnode*, *indexnumber_origin* and *indexnumber_destination* with uint32 value type

## conditions

1. The domain unit of arguments *impedance_link*, *F1* and *F2* must match.
2. The domain unit of arguments *nr_originnode*, *impedance_origin*  and *indexnumber_origin* must match.
3. The domain unit of arguments *nr_destnode*, *impedance_destination* and *indexnumber_destination* must match.
4. The values unit of arguments *impedance_link*, *impedance_origin* and *impedance_destination* must match.

## since version

5.95

## example

<pre>
unit&lt;uint32&gt; od := <B>dijkstra_od(</B>dist, F1, F2
       , nr_OrgNode,  const(0, ODomain, m), id(ODomain)
       , nr_DestNode, const(0, DDomain, m), id(DDomain)
   <B>)</B>;
<I>// replace this obsolete syntax to</I>

unit&lt;uint32&gt; odd := <B>dijkstra_m(</B>
      'bidirectional;startPoint(Node_rel);endPoint(Node_rel);od:impedance
      ,OrgZone_rel,DstZone_rel
     , dist
     , F1
     , F2
     , network/nr_OrgNode
     , network/nr_DestNode
   <B>)</B>
{
   attribute&lt;ODomain&gt; originn_nr      :=     id(OD) / #DDomain;
   attribute&lt;DDomain&gt; destination_rel := mod(id(OD) , #DDomain);
}
</pre>

| dist    | F1  | F2  |
|--------:|----:|----:|
| 713.44  | 9   | 10  |
| 907.33  | 0   | 3   |
| 1913.96 | 3   | 19  |
| 1907.31 | 19  | 26  |
| ..      | ..  | ..  |
| ..      | ..  | ..  |

*domain Linkset, nr of rows = 34*

| nr_orgnode |
|-----------:|
| 22         |
| 17         |
| 14         |
| 29         |
| 8          |

*domain ODomain, nr of rows = 5*

| nr_destnode |
|------------:|
| 22          |
| 13          |
| 6           |
| 30          |

*domain Destination, nr of rows = 4*

| **Impedance** | origin_nr | destination_nr |
|--------------:|----------:|---------------:|
| **0**         | 0         | 0              |
| **5093.28**   | 0         | 1              |
| **5596.24**   | 0         | 2              |
| **5410**      | 0         | 3              |
| **5896.14**   | 1         | 0              |
| **1383.17**   | 1         | 1              |
| **5272.83**   | 1         | 2              |
| **5086.39**   | 1         | 3              |
| **4901.69**   | 2         | 0              |
| **3775.21**   | 2         | 1              |
| **1393.9**    | 2         | 2              |
| **4091.94**   | 2         | 3              |
| **6181.57**   | 3         | 0              |
| **5055.09**   | 3         | 1              |
| **5558.26**   | 3         | 2              |
| **1457.7**    | 3         | 3              |
| **5956.03**   | 4         | 0              |
| **1443.05**   | 4         | 1              |
| **5332.71**   | 4         | 2              |
| **5146.27**   | 4         | 3              |

*domain OD, nr of rows = 20*
