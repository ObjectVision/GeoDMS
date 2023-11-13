*[[Grid functions]] griddist*

## syntax

- griddist(*resistance*, *destpoint_grid_rel*, *startvalues*)

## definition

griddist(*resistance*, *destpoint_grid_rel*, *startvalues*) results in an **[[impedance]]** value for each cell of a [[grid]] towards the nearest point in a pointset, summing the *resistance* of each cell part of the shortest path to the nearest point.

The function has three [[arguments|argument]]:
-   *resistance*, an [[attribute]] of a [[grid domain]] with the resistance value for each cell (e.g. based on slope or land use type);
-   *destpoint_grid_rel*, a [[relation]] from the pointset towards the grid domain. The pointset contains the destination locations, to which impedances are calculated. See [[point 2 grid]] for how to configure such a relation.
-   *startvalues*, the initial impedance values for the point set.

The resulting [[values unit]] of the impedance item has a float32 or float64 [[value type]], based on the value type of the *resistance* and *startvalues* 
 arguments.

The resulting [[domain unit]] is the domain unit of the *resistance* argument.

## description

The griddist function is often used to calculate off road travel resistances, where e.g. the land use type and the slope have an important influence on the travel time/costs.

## conditions

- The values unit of the arguments *resistance* and *startvalues* must match.
- The domain unit of the arguments *destpoint_gridid* and *startvalues* must match.

## since version

5.85

Not Yet Implemented:

- an aspect ratio is required to make griddist useful on a non-square rectangular raster
- a row specific aspect ratio is required to make griddist useful on a lat-long raster
- a cell specific (2x2) metric is required to make griddist useful on a generic projection

## example

<pre>
attribute&lt;GridDomain&gt; GridDomain_rel (destination);
attribute&lt;float64&gt;    griddist       (GridDomain) := <B>griddist(</B>resistance, GridDomain_rel, const(0.0, destination)<B>)</B>;
</pre>

*resistance* 
|  |  |  |  |  | 
|-:|-:|-:|-:|-:|
| 1| 1| 1| 1| 1|
| 2| 3| 0| 0| 1|
| 2| 4| 5| 1| 1|
| 5| 6| 3| 2| 2|
| 2| 2| 1| 1| 1|

*GridDomain, nr of rows = 5, nr of cols = 5*

| GridDomain_rel |
|----------------|
| {1, 1}         |
| {1, 3}         |
| {4, 1}         |
| {4, 4}         |

*domain destination, nr of rows = 4*

**griddist**
|           |           |           |           |           |
|----------:|----------:|----------:|----------:|----------:|
| **1.71**  |  **0.71** | **0.50**  | **0.50**  | **0.71**  |
| **2.50**  |  **0**    | **0**     |    **0**  | **0.50**  |
| **3.54**  |  **2.83** | **2.50**  | **0.50**  | **0.71**  |
| **4.95**  |  **4**    | **3.33**  | **2**     | **0.71**  |
| **2**     |  **0**    | **1.50**  | **1**     | **0**     |

*GridDomain, nr of rows = 5, nr of cols = 5*