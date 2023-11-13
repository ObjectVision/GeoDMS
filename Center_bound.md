*[[Geometric functions]] center_bound*

## syntax

- center_bound(*polygon_data_item*)

## definition

center_bound(*polygon_data_item*) results in a [[point]] [[data item]] with the <B>center of the bounding box</B> of the *polygon_data_item*.

## applies to

data item *polygon_data_item* with fpoint or dpoint [[value type]] and [[composition type|composition]] polygon

## example

<pre>
attribute&lt;point_rd&gt; cb (district) := <B>center_bound(</B>district/geometry<B>)</B>;
</pre>

| district/geometry      | **cb**               |
|------------------------|----------------------|
| {21 {403025, 113810}{4 | **{402990, 113061}** |
| {17 {400990, 113269}{4 | **{401207, 112734}** |
| {19 {401238, 115099}{4 | **{401265, 114026}** |

*domain district, nr of rows = 3*

## see also

- [[lower_bound]]
- [[upper_bound]]