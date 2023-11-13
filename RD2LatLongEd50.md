*[[Geometric functions]] RD2LatLongEd50*

## syntax

- RD2LatLongEd50(*geometry*, *value type*)

## definition

RD2LatLongEd50(*geometry*, *value type*) converts the [[geometry]] [[argument]], expressed in the <B>[Dutch RD coordinate system](http://nl.wikipedia.org/wiki/RijksdriehoekscoB6rdinaten) towards the [Ed50 projection](https://en.wikipedia.org/wiki/ED50)</B> of the
[LatLong coordinate system](https://en.wikipedia.org/wiki/Geographic_coordinate_system).

The second optional argument is the *[[value type]]* of the resulting [[data item]].

## applies to

data item *geometry* and with Point value type

## since version

5.44

## example

<pre>
attribute&lt;dpoint&gt; district_LatLongEd50(DDomain, polygon) := <B>RD2LatLongEd50(</B>district_rd, dpoint<B>)</B>;
</pre>

| district_rd             | **district_LatLongEd50**            |
|-------------------------|-------------------------------------|
| {21: {403025, 113810}{4 | **{21: {51.6153922, 4.7937585}{51** |
| {17: {400990, 113269}{4 | **{17: {51.5970619, 4.7861904}{51** |
| {19: {401238, 115099}{4 | **{19: {51.5994237, 4.8125713}{51** |

*DDomain, nr of rows = 3*

## see also

- [[RD2LatLongWgs84]]
- [[LatLongWgs842RD]]
- [[RD2LatLongGE]]
- [[RD2LatLong]]