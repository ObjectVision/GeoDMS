*[[Geometric functions]] dist(ance)*

## syntax

- dist(*destination*, *origin*)

## definition

dist(*destination*, *origin*) calculates the **distance (as the crow flies) between *origin* and *destination* points** of the same [[domain unit]].

The resulting [[data item]] has a float32 [[value type]] without [[metric]]. Use the [[value]] function to convert the result to the requested [[values unit]].

## description

Use the [[sqrdist]] function if only the distance order is relevant as the sqrdist function calculates faster.

## applies to

- *destination* and *origin* are [[data items|data item]] with Point value type

## conditions

The values unit and domain unit of the *destination* and *origin* [[arguments|argument]] must match.

## example

<pre>
attribute&lt;meter&gt; distOD (ADomain) := value(<B>dist(</B>destination, origin<B>)</B>, meter);
</pre>

| destination      | origin           | **distOD** |
|------------------|------------------|-----------:|
| {401331, 115135} | {401331, 115135} |  **0**     |
| {399501, 111793} | {399476, 111803} |  **26.9**  |
| {399339, 114883} | {399289, 114903} |  **53.9**  |
| {401804, 111323} | {401729, 111353} |  **80.8**  |
| {398796, 111701} | {398696, 111741} |  **107.7** |

*ADomain, nr of rows = 5*

## see also

- [[sqrdist]]

## extra: distance between points of different domains

If you want to calculate the distance to the nearest point between to different domain, the following example script can be used.

```
unit<uint32> from_domain  := plan 
{
   attribute<rdc_meter> geometry      := plan/geometry;
   attribute<to_domain> to_domain_rel := connect(to_domain/geometry, geometry);
   attribute<rdc_meter> geometry_to   := to_domain/geometry[to_domain_rel];

   attribute<meter> dist              := value(dist(geometry_to, geometry), meter);
}

unit<uint32> to_domain  := city
{
   attribute<rdc_meter> geometry := city/geometry;
}  
```