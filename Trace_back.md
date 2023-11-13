*[[Network functions]] trace_back*

## syntax

- trace_back(*F1*, *F2*, *dijkstra/traceback*, *capacity*)

## definition

trace_back(*F1*, *F2*, *dijkstra/traceback*, *capacity*) calculates **flows in a network**.

Based on the results of [[dijkstra|dijkstra general]] calculations, the trace_back function results in the amount of flow for each link in the network.

The trace_back function requests four arguments:

- *F1*: [[relation]] to the from node of each link;
- *F2*: relation to the to node of each link;
- *dijkstra/traceback*: the node to node traceback item, a generated [[subitem]] by the [[dijkstra function|dijkstra functions]]
- *capacity*: capacity indicator for each node.

## applies to

- [[attributes|attribute]] *F1*, *F2* and *dijkstra/traceback* with uint32 [[value type]]
- attribute *capacity* with Numeric value type

## conditions

The [[domain unit]] of attributes *F1* and *F2* must match.

## example

<pre>
attribute&lt;meter&gt;   dist (NodeSet) := dijkstra(dist, F1, F2, nr_DestNode);
attribute&lt;nr_pers&gt; flow (LinkSet) :=
   value(
      <B>trace_back(</B>
           F1
         , F2
         , dist/TraceBack
         , pcount(nr_OrgNode)
      <B>)</B>
      , nr_pers
   );
</pre>

| F1  | F2  |**flow**|
|----:|----:|-------:|
| 8   | 7   | **0**  |
| 1   | 4   | **2**  |
| 16  | 15  | **0**  |
| 2   | 3   | **1**  |
| 5   | 4   | **1**  |
| 6   | 7   | 2      |
| 9   | 10  | **1**  |
| 12  | 11  | **1**  |
| 14  | 15  | **0**  |
| 18  | 20  | **1**  |
| 19  | 17  | **1**  |
| 3   | 0   | **0**  |
| 4   | 11  | **1**  |
| 7   | 1   | **2**  |
| 10  | 3   | 1      |
| 11  | 13  | **0**  |
| 13  | 17  | **0**  |
| 15  | 10  | **0**  |
| 20  | 21  | **0**  |
| 17  | 20  | **1**  |

*domain Linkset, nr of rows = 19*

| nr_OrgNode |
|-----------:|
| 14         |
| 12         |
| 9          |
| 18         |
| 5          |

*domain Origin, nr of rows = 5*