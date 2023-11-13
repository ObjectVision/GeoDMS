*[[Geometric functions]] RD2LatLongGE*

## syntax

- RD2LatLongGE(*geometry*, *value type*)

## definition

RD2LatLongGE(*geometry*, *value type*) converts the [[geometry]] [[argument]], expressed in the <B>[Dutch RD coordinate system](http://nl.wikipedia.org/wiki/RijksdriehoekscoB6rdinaten) towards the [Wgs84](http://en.wikipedia.org/wiki/World_Geodetic_System)
projection</B> used in [Google Earth](https://en.wikipedia.org/wiki/Google_Earth) ([Wgs84 projection](https://en.wikipedia.org/wiki/World_Geodetic_System) with a lineair correction)

The second optional argument is the *[[value type]]* of the resulting [[data item]].

## applies to

data item *geometry* with Point value type

## since version

5.44

## example

<pre>
attribute&lt;dpoint&gt; district_LatLongGE(DDomain, polygon) := <B>RD2LatLongGE(</B>district_rd, dpoint<B>)</B>;
</pre>

| district_rd             |  **district_LatLongGE**             |
|-------------------------|-------------------------------------|
| {21: {403025, 113810}{4 | **{21: {51.6146385, 4.7925546}{51** |
| {17: {400990, 113269}{4 | **{17: {51.5963092, 4.7849878}{51** |
| {19: {401238, 115099}{4 | **{19: {51.5986712, 4.8113673}{51** |

*DDomain, nr of rows = 3*

## see also

- [[RD2LatLongWgs84]]
- [[LatLongWgs842RD]]
- [[RD2LatLongEd50]]
- [[RD2LatLong]]