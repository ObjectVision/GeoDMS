*[[Configuration examples]] Distances in LatLong coordinate system*

[Great-circle distances](wikipedia:Great-circle_distance "wikilink") between two points on a sphere given their [longitudes](wikipedia:Longitude "wikilink") and [latitudes](wikipedia:Latitude "wikilink") can be calculated with the [haversine formula.](wikipedia:Haversine_formula "wikilink")

The following example shows an implementation in the GeoDMS configuration. We thank [PBL](https://www.pbl.nl/) for making this example available.

## example haversine function

<pre>
template DistanceMtrFromDegrees
{
  <I>// begin case parameters</I>
  attribute&lt;float64&gt; lat1_degrees (NL);
  attribute&lt;float64&gt; lat2_degrees (NL);
  attribute&lt;float64&gt; lon1_degrees (NL);
  attribute&lt;float64&gt; lon2_degrees (NL);
  <I>// end case parameters</I>

  attribute&lt;float64&gt; lat1_radian (NL) := lat1_degrees * pi() / 180.0;
  attribute&lt;float64&gt; lat2_radian (NL) := lat2_degrees * pi() / 180.0;
  attribute&lt;float64&gt; lon1_radian (NL) := lon1_degrees * pi() / 180.0;
  attribute&lt;float64&gt; lon2_radian (NL) := lon2_degrees * pi() / 180.0;           `

  attribute&lt;float64&gt; deltaLon_radian (NL) := lon1_radian - lon2_radian;`
  attribute&lt;float64&gt; deltaLat_radian (NL) := lat1_radian - lat2_radian;`
  attribute&lt;float64&gt; a               (NL) := 
    sqr(sin(deltaLat_radian/2d)) + (((cos(lat1_radian) * cos(lat2_radian))) * sqr(sin(deltaLon_radian/2d)));
  attribute&lt;units/meter&gt; distance    (NL) := (2d * 6371000d * atan(sqrt(a) / (sqrt(1d - a))))[Units/Meter];
}
</pre>