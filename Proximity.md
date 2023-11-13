*[[Grid functions]] proximity*

## syntax

- proximity(*grid_data_item*, *kernel*)

## definition

proximity(*grid_data_item*, *kernel*) results in a **[[convolution]]** of a *grid_data_item* and a *[[kernel]]*.

For each cell in the *grid_data_item*, the values of the cell and it's neighbourhood cells are multiplied with the corresponding values in the kernel taking that cell as the focal point in the kernel. The resulting cell value is the **maximum** of these multiplications.

The resulting [[value type]] is float32 or float64, based on the value type of the *grid_data_item*. The resulting [[domain unit]] is the domain unit of the *grid_data_item* [[attribute]].

Because the proximity function uses a [Fast Fourier Transformation](https://nl.wikipedia.org/wiki/Fast_Fourier_transform),
[[calculating results is fast|expression]] even with large kernels.

In other GIS software this operation is also known as the [focal statistics](http://webhelp.esri.com/arcgisdesktop/9.3/index.cfm?TopicName=Focal%20Statistics), statistic type: MAXIMUM

## description

See [[potential with kernel]] for more information on how to configure different kernels.

## applies to

- attribute *grid_data_item* with float32 or float64 [[value type]]

## conditions

The domain unit of *grid_data_item* must be a Point value type of the group CanBeDomainUnit.

## example
<pre>
attribute&lt;float32&gt; proxgrid (GridDomain) := <B>proximity(</B>float32(sourcegrid), pot3Range/RelWeight<B>)</B>;
</pre>

*sourcegrid* 
|            |  |  |  |  |
|-----------:|-:|-:|-:|-:|
| null       |0 |0 |0 |1 |
| 0          |0 |2 |1 |1 |
| 0          |2 |3 |3 |3 |
| 1          |1 |1 |3 |0 |
| 0          |1 |0 |1 |3 |

*GridDomain, nr of rows = 5, nr of cols = 5*

**proxgrid**
|          |          |          |          |          |
|---------:|---------:|---------:|---------:|---------:|
| **0**    | **0.18** | **0.23** | **0.18** | **0.18** |
| **0.18** | **0.26** | **0.35** | **0.35** | **0.35** |
| **0.24** | **0.35** | **0.53** | **0.53** | **0.53** |
| **0.18** | **0.26** | **0.35** | **0.53** | **0.35** |
| **0.12** | **0.18** | **0.26** | **0.35** | **0.53** |

*GridDomain, nr of rows = 5, nr of cols = 5*