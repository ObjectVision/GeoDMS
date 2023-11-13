[[Geographic|Geography]] data describes objects/subjects/processes on the earth surface for which the location on this surface is relevant. As the earth is a [sphere/ellips](wikipedia:Sphere "wikilink"), a [map projection](wikipedia:Map_projection "wikilink") is needed to project these locations in a two-dimensional plane. This two-dimensional plane can be described by a [geographic coordinate system](wikipedia:Geographic_coordinate_system "wikilink").  

The coordinate system of a GeoDMS project is defined by it's [[coordinate unit|How to configure a coordinate system]], defining how the coordinates need to be interpreted, in terms of their [[metric]] (meters, degrees) and their origin.

In a GeoDMS project usually one coordinate system is used, although source data can be available in different coordinate systems. But most functions only support calculating with data in the same coordinate system, which also applies to visualization data in a map view.

Therefore [[coordinate conversions|coordinate conversions]] can be used to convert data between and within coordinate systems.

In working with coordinate systems, it is important to make the following distinction:

## a cartesian system

Most GeoDMS applications use a cartesian coordinate system, like in the Netherlands the Rijksdriehoekcoördinaten.

A Cartesian coordinate system is a [orthogonal](wikipedia:Orthogonality "wikilink") coordinate system in which the [distance](wikipedia:Distance "wikilink") between two coördinate lines is constant in a [length](wikipedia:Length "wikilink") unit.

This means the [Pythagorean theorem](wikipedia:Pythagorean_theorem "wikilink") can be used to calculate distances, which is an important assumption in
GeoDMS functions like [[dist]] and [[connect_info]].

## or a geographic coordinate systems based on longitude and latitude

Within the GeoDMS is possible to configure coordinate systems based on [lat](wikipedia:Latitude "wikilink") / [long](wikipedia:Longitude "wikilink") coordinates.

Be carefull in interpreting the results of functions that assume the [Pythagorean theorem](wikipedia:Pythagorean_theorem "wikilink") can be used like [[dist]] and [[connect_info]].

## see also

- [[How to configure a coordinate system]]
- [[coordinate conversions|coordinate conversions]]