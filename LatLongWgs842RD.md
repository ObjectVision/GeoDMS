*[[Geometric functions]] LatLongWgs842RD*

## syntax

- LatLongWgs842RD(*geometry*, *value type*)

## definition

LatLongWgs842RD(*geometry*, *value type*) converts the [[geometry]] [[argument]], expressed in the <B>[LatLong coordinate system](https://en.wikipedia.org/wiki/Geographic_coordinate_system) towards the [Dutch RD coordinate system](http://nl.wikipedia.org/wiki/RijksdriehoekscoB6rdinaten)</B>

The second optional argument is the *[[value type]]* of the resulting [[data item]].

## applies to

data item *geometry* with Point value type

## since version

5.44

## example

<pre>
attribute&lt;dpoint&gt; district_rd (DDomain, polygon) := <B>LatLongWgs842RD(</B>district_LatLong, dpoint<B>)</B>;
</pre>

| district_LatLong                |          **district_rd**    |
|---------------------------------|-----------------------------|
| {21: {51.6145901, 4.7924961}{51 | **{21: {403025, 113810}{4** |
| {17: {51.5962592, 4.7849284}{51 | **{17: {400990, 113269}{4** |
| {19: {51.5986213, 4.8113099}{51 | **{19: {401238, 115099}{4** |

*DDomain, nr of rows = 3*

## see also

- [[RD2LatLongWgs84]]
- [[RD2LatLongGE]]
- [[RD2LatLongEd50]]
- [[RD2LatLong]]