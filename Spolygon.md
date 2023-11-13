*[[Conversion functions]] spolygon*

## syntax

- spolygon(*a*)

## definition

spolygon(*a*) converts the coordinates of a point [[item|tree item]] *a* with a sequence of points ([[arc]] or [[polygon]]) to the **spoint (int16 coordinates)** [[value type]].

## applies to

- [[data item|data item]] with Point [[value type]] and [[composition type|composition]] arc or polygon

## example

<pre>
attribute&lt;spoint&gt; spolygonA (SDomain, polygon) := <B>spolygon(</B>A<B>)</B>;
</pre>

| A(*fpolygon*)                                |  **spolygonA**                       |
|----------------------------------------------|--------------------------------------|
| {2:{0,0},{1,1}}                              | **{2:{0,0},{1,1}}**                  |
| {3: {1E+007,1E+007},{-2.5,-2.5},{99.9,99.9}} | **{3: {null,null},{-2,-2},{99,99}}** |