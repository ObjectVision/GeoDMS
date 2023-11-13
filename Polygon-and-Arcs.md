*[[Planned Developments]]: Polygon and Arcs*

More and more the GeoDMS is also used to work with [[vector data]]. We are planning to:

-   improve the performance of [[polygon functions|Geometric functions]], see [Mantis issue 1208](http://mantis.objectvision.nl/view.php?id=1221).

Some preliminary test results:

1.  use polygon_d16D and not polygon_deflated_d16D
2.  I've added some tests. polygon_i4HV(geometry_mm, 200.0) takes 5 minutes, polygon_i4HV(geometry_mm, 2550d) takes long. A bit more testing might clarify the effect of the scale parameter for i4HV, d4HV and d16HV. However, this is not likely to pointer a resolution of this issue.

Plan/workaround:

\- filter out big objects as being irrelevant

\- test intersection with convolved copies of polygons. Check that lakes
are properly inflated.

links:

\- <http://masc.cs.gmu.edu/wiki/ReducedConvolution>

[-
<http://masc.cs.gmu.edu/wiki/uploads/ReducedConvolution/iros11-mksum2d.pdf>](http://masc.cs.gmu.edu/wiki/uploads/ReducedConvolution/iros11-mksum2d.pdf)

-   add more functions to work with [[arcs|arc]], for instance buffering.
-   improve the rendering of [[vector data]].