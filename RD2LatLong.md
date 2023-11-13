*[[Geometric functions]] RD2LatLong*

## syntax

- RD2LatLong(*geometry*, *value type*)

## definition

RD2LatLong(*geometry*, *value type*) converts the [[geometry]] [[argument]], expressed in the <B>[Dutch RD coordinate system](http://nl.wikipedia.org/wiki/RijksdriehoekscoB6rdinaten) towards the [LatLong coordinate system](https://en.wikipedia.org/wiki/Geographic_coordinate_system)</B>.

The second optional argument is the *[[value type]]* of the resulting [[data item]].

## applies to

data item *geometry* with Point value type

## since version

5.44

## example

<pre>
attribute&lt;dpoint&gt; district_LatLong (DDomain, polygon) := <B>RD2LatLong(</B>district_rd, dpoint<B>)</B>;
</pre>

| district_rd             | **district_LatLong**                |
|-------------------------|-------------------------------------|
| {21: {403025, 113810}{4 | **{21: {51.6155134, 4.7928460}{51** |
| {17: {400990, 113269}{4 | **{17: {51.5971804, 4.7852773}{51** |
| {19: {401238, 115099}{4 | **{19: {51.5995428, 4.8116626}{51** |

*DDomain, nr of rows = 3*

## see also

- [[RD2LatLongWgs84]]
- [[LatLongWgs842RD]]
- [[RD2LatLongGE]]
- [[RD2LatLongEd50]]