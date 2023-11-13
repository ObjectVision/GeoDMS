*[[Grid functions]] diversity*

## syntax

- diversity(*grid_data_item*, *radius*, *square_or_circle*)

## definition

diversity(*grid_data_item*, *radius*, *square_or_circle*) results in a new [[attribute]] with the **number of different occurences** in the neighbourhood of each cell of the *grid_data_item*.

The [[argument]] *radius* defines the neighbourhood size, in number of cells.

The argument *square_or_circle* indicates if a square (value 0) or a circle (value 1) is used to define the shape of the neighbourhood.

The resulting item has the same [[domain unit]] as the *grid_data_item* and a uint8 or unit32 [[value type]] (based on the value type of the *grid_data_item*).

## applies to

- attribute *grid_data_item* with uint8 or uint32 [[value type]]
- [[parameter]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *radius* with uint16 value type
- parameter or literal *square_or_circle* with uint16 value type

## conditions

The domain unit of the *grid_data_item* argument must be a Point value type of the group CanBeDomainUnit.

## example

<pre>
attribute&lt;uint32&gt; divgrid (GridDomain) := <B>diversity(</B>sourcegrid, 2w, 1w<B>)</B>;
</pre>

*sourcegrid*
|      |      |      |      |      |
|-----:|-----:|-----:|-----:|-----:|
| null | 0    | 0    | 0    | 1    |
| 0    | 0    | 2    | 1    | 1    |
| 0    | 2    | 3    | 3    | 3    |
| 1    | 1    | 1    | 3    | 0    |
| 0    | 1    | 0    | 1    | 3    |

*GridDomain, nr of rows = 5, nr of cols = 5*

**divgrid**
|          |       |       |       |       |
|---------:|------:|------:|------:|------:|
| **null** | **0** | **0** | **0** | **1** |
| **0**    | **0** | **2** | **1** | **1** |
| **0**    | **2** | **3** | **3** | **3** |
| **1**    | **1** | **1** | **3** | **0** |
| **0**    | **1** | **0** | **1** | **3** |

*GridDomain, nr of rows = 5, nr of cols = 5*