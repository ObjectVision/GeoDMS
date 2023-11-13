*[[Network functions]] service_area*

## syntax

- service_area(*F1,* *F2*, *dijkstra/traceback*)

## definition

service_area(*F1*, *F2*, *dijkstra/traceback*) calculates **service areas** for nodes in the network.

A service area is defined as the [[relation]] to the nearest destination node for each node in the node set. The results are based on the results of [[dijkstra|dijkstra general]] calculations.

The service_area function requests three [[arguments|argument]]:
- *F1*: [[relation]] to the first node of a linkset;
- *F2*: relation to the last node of a linkset;
- *dijkstra/traceback*: the traceback item, a generated subitem by the dijkstra function.

## description

The function results in a relation to the nodes in the nodeset. Use the [[lookup]] function to relate these numbers to the origin locations (see example).

## applies to

[[attributes|attribute]] *F1*, *F2* and *dijkstra/traceback* with uint32 [[value type]]

## conditions

The [[domain units|domain unit]] of attributes *F1* and *F2* must match.

## example
<pre>
attribute&lt;meter&gt;       dist           (nodeset) := dijkstra(dist, F1, F2, nr_DestNode);
attribute&lt;destination&gt; servicearea_ns (nodeset) := 
   rlookup(
      <B>service_area(</B>
          F1 
         ,F2
         ,dist/TraceBack
       <B>)</B>
       , nr_DestNode
   ); 
attribute&lt;destination&gt; servicearea (origin) := servicearea_ns[nr_OrgNode];
</pre>

| F1  | F2  |
|----:|----:|
| 8   | 7   |
| 1   | 4   |
| 16  | 15  |
| 2   | 3   |
| 5   | 4   |
| 6   | 7   |
| 9   | 10  |
| 12  | 11  |
| 14  | 15  |
| 18  | 20  |
| 19  | 17  |
| 3   | 0   |
| 4   | 11  |
| 7   | 1   |
| 10  | 3   |
| 11  | 13  |
| 13  | 17  |
| 15  | 10  |
| 20  | 21  |
| 17  | 20  |

*domain linkset, nr of rows = 19*

| nr_DestNode |
|------------:|
| 14          |
| 6           |
| 2           |
| 19          |

*domain destination, nr of rows = 5*

| **servicearea** |
|----------------:|
| **0**           |
| **1**           |
| **2**           |
| **3**           |
| **1**           |

*domain origin, nr of rows = 5*