*[[Constant functions]] null*

The keyword Null is used in the GeoDMS to indicate [missing data](https://en.wikipedia.org/wiki/Missing_data).

The constant: null_<I>suffix</I> can be used in expressions, it is important to use the version with the correct [[value type]], see the following table:

<table>
<thead>
<tr class="header">
<th><sup>Value Type</sup></th>
<th><sup>Name</sup></th>
</tr>
</thead>
<tbody>
<tr class="even">
<td><sup>[[uint8]]</sup></td>
<td><sup>null_b</sup></td>
</tr>
<tr class="odd">
<td><sup>[[uint16]]</sup></td>
<td><sup>null_w</sup></td>
</tr>
<tr class="even">
<td><sup>[[uint32]]</sup></td>
<td><sup>null_u</sup></td>
</tr>
<tr class="odd">
<td><sup>[[uint64]]</sup></td>
<td><sup>null_u64</sup></td>
</tr>
<tr class="even">
<td><sup>[[int8]]</sup></td>
<td><sup>null_c</sup></td>
</tr>
<tr class="odd">
<td><sup>[[int16]]</sup></td>
<td><sup>null_s</sup></td>
</tr>
<tr class="even">
<td><sup>[[int32]]</sup></td>
<td><sup>null_i</sup></td>
</tr>
<tr class="odd">
<td><sup>[[int64]]</sup></td>
<td><sup>null_i64</sup></td>
</tr>
<tr class="even">
<td><sup>[[float32]]</sup></td>
<td><sup>null_f</sup></td>
</tr>
<tr class="odd">
<td><sup>[[float64]]</sup></td>
<td><sup>null_d</sup></td>
</tr>
<tr class="even">
<td><sup>[[spoint]]</sup></td>
<td><sup>null_sp</sup></td>
</tr>
<tr class="odd">
<td><sup>[[wpoint]]</sup></td>
<td><sup>null_wp</sup></td>
</tr>
<tr class="even">
<td><sup>[[ipoint]]</sup></td>
<td><sup>null_ip</sup></td>
</tr>
<tr class="odd">
<td><sup>[[upoint]]</sup></td>
<td><sup>null_up</sup></td>
</tr>
<tr class="even">
<td><sup>[[fpoint]]</sup></td>
<td><sup>null_fp</sup></td>
</tr>
<tr class="odd">
<td><sup>[[dpoint]]</sup></td>
<td><sup>null_dp</sup></td>
</tbody>
</table>  

## syntax

- null_<I>suffix</I>

## definition

null_<I>suffix</I> results in the null data value for the [[value type]] defined by the suffix.

## applies to

[[data item]] with the value type defined by the suffix.  

## since

version 14.3.0

## example

<pre>
parameter&lt;float32&gt; param_null_f := <B>null_f</B>;
</pre>

result: param_null_f = null