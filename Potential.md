*[[Grid functions]] potential*

## syntax

- potential(*grid_data_item*, *kernel*)

## definition

potential(*grid_data_item*, *kernel*) results in a **[[convolution]]** of a *grid_data_item* and a *[[kernel]]*.

For each cell in the *grid_data_item*, the values of the cell and it's neighbourhood cells are multiplied with the corresponding values in the kernel taking that cell as the focal point in the kernel. The resulting cell value is the **sum** of these multiplications.

The resulting [[value type]] is float32 or float64, based on the value type of the *grid_data_item*. The resulting [[domain unit]] is the domain unit of the *grid_data_item* [[attribute]].

Because the potential function uses a [Fast Fourier Transformation](https://nl.wikipedia.org/wiki/Fast_Fourier_transform),
[[calculating results is fast|expression]] even with large kernels.

In other GIS software this operation is also known as the [focal statistics](http://webhelp.esri.com/arcgisdesktop/9.3/index.cfm?TopicName=Focal%20Statistics), statistic type: SUM

## description

See [[potential with kernel]] for more information on how to configure different kernels.

## applies to

- attribute *grid_data_item* with float32 or float64 [[value type]]

## conditions

The domain unit of *grid_data_item* must be a Point value type of the group CanBeDomainUnit.

## example
<pre>
attribute&lt;float32&gt; potgrid (GridDomain) := <B>potential(</B>float32(sourcegrid), pot3Range/RelWeight<B>)</B>;
</pre>

*sourcegrid*
|       |  |  |  |  |
|------:|-:|-:|-:|-:|
| null  | 0| 0| 0| 1|
| 0     | 0| 2| 1| 1|
| 0     | 2| 3| 3| 3|
| 1     | 1| 1| 3| 0|
| 0     | 1| 0| 1| 3|

*GridDomain, nr of rows = 5, nr of cols = 5*

**potgrid**
|          |          |          |          |          |
|---------:|---------:|---------:|---------:|---------:|
| **0**    | **0.18** | **0.32** | **0.50** | **0.38** |
| **0.18** | **0.74** | **1.26** | **1.50** | **1.03** |
| **0.44** | **1.18** | **1.91** | **2.06** | **1.35** |
| **0.56** | **1.03** | **1.62** | **1.91** | **1.41** |
| **0.32** | **0.47** | **0.71** | **0.97** | **0.91** |

*GridDomain, nr of rows = 5, nr of cols = 5*