*[[Grid functions]] district*

## syntax

- district(*grid_data_item*)

## definition

The district function is used to find **adjacent** (**horizontal & vertical, not diagonal**) grid cell values with the **same values**.

district(*grid_data_item*) results in a new uint32 [[domain unit]], with a generated [[subitem]]: Districts.

If two adjacent cells have the same value in the *grid_data_item* [[argument]], they will get the same district value.

The Districts [[attribute]] has the the same domain unit as the *grid_data_item* and contains a [[relation]] to each district (zero based).

## description

In earlier versions of the GeoDMS adjacency was also defined in diagonal directions, the [[district_8]] function defines adjacent also in diagonal directions.

## applies to

- attribute *grid_data_item* with uin8 or unit32 [[value type]]

## conditions

The domain unit of the *grid_data_item* argument must be a Point value type of the group CanBeDomainUnit.

## example

<pre>
unit&lt;uint32&gt; unit_district := <B>district(</B>sourcegrid<B>)</B>;
</pre>

*sourcegrid*
|      |      |      |      |      |
|-----:|-----:|-----:|-----:|-----:|
| null | 0    | 0    | 0    | 1    |
| 0    | 0    | 2    | 1    | 1    |
| 0    | 2    | 3    | 3    | 3    |
| 1    | 1    | 1    | 3    | 0    |
| 0    | 1    | 0    | 1    | 3    |

*Table GridDomain, nr of rows = 5, nr of cols = 5*

**unit_district/districts**
|          |       |       |       |       |
|---------:|------:|------:|------:|------:|
| **null** | **0** | **0** | **0** | **1** |
| **0**    | **0** | **2** | **1** | **1** |
| **0**    | **3** | **4** | **4** | **4** |
| **5**    | **5** | **5** | **4** | **6** |
| **7**    | **5** | **8** | **9** | **10**|

*Table GridDomain, nr of rows = 5, nr of cols = 5*

## see also

- [[district_8]]