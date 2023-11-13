*[[Network functions]] connected_parts*

## syntax

- connected_parts(*F1*, *F2*)

## definition

The connected_parts function is used to find out**how many connected (sub)networks exist** in a set of [links](https://en.wikipedia.org/wiki/Network_topology)

connected_parts(*F1*, *F2*) results in a new [[domain unit]] with a as [[subitem]] the [[attribute]]: Partnr.

The arguments *F1* and *F2* are the [[relations|relation]] towards the the first and last node of a link set.

The Partnr item contains the relation towards the connected network(s).

Each value indicates a group of connected nodes. If all Partnr values are zero, the network is fully connected.

## applies to

[[data items|data item]] *F1* and *F2* with uint32 [[value type]]

## conditions

The domain units of [[arguments|argument]] *F1* and *F2* must match.

## example

<pre>
unit&lt;uint32&gt; connected_parts := <B>connected_parts(</B>F1, F2<B>)</B>;
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

| **PartNr** |
|-----------:|
| **0**      |
| **1**      |
| **0**      |
| **0**      |
| **1**      |
| **1**      |
| **1**      |
| **1**      |
| **1**      |
| **0**      |
| **0**      |
| **1**      |
| **1**      |
| **1**      |
| **0**      |
| **0**      |
| **0**      |
| **1**      |
| **1**      |
| **1**      |
| **1**      |
| **1**      |

*domain nodeSet, nr of rows = 21*