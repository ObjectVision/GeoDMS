*[[Unit functions]] TiledUnit*

## syntax

-   TiledUnit(*tilesize*)

## definition

The TiledUnit function results in a **[[segmented/tiled domain unit|Tiled Domain]]**, the *tilesize* [[argument]] defines the size of each tile.

The [[values unit]] of the "tilesize" argument must be the original [[domain unit]] from which the tiled unit is made.

TiledUnits are often used for [[grid domains|grid domain]]. The *tilesize* argument for these two-dimensional domain is also two-dimensional Point value type.

For one-dimensional (tables) domains, the *tilesize* argument is one-dimensional. For these domains the concept segmented domain is also used as synonym of tiled domain.

## description

Tiled units are useful if the original domain has a very large number of items (think for example of a grid unit of 100 meter cells for the whole of Europe).

Calculating with data items might cause memory problems (in a win32 environment). Tiling the original units can solve these problems.

If a tiled unit is configured as domain unit, calculations are processed on each tile separately.

The results are presented to the user in the same manner as untiled
domains.

## applies to

-   [[parameter]] or [literal](https://en.wikipedia.org/wiki/Literal_(computer_programming)) *tilesize*

## conditions

-   the values unit of the *tilesize* argument must match be the original domain unit, from which the tiled unit is derived.

## since version

6.024

## example

1: uint32 domain unit
<pre>
unit&lt;uint32&gt; building_untiled: nrofrows = 100000000
unit&lt;uint32&gt; building := <B>TiledUnit</B>(25000[building])
{
   attribute&lt;float32&gt; a_attribute := union_data(.,building_untiled/a_attribute);
}
</pre>
*result: a tiled/segmented domain of buildings, with tiles of 25000 entries each.*

2: spoint (grid) domain
<pre>
unit&lt;spoint&gt; gridunit_untiled := range(gridset(
         point/Source/point_rd
        ,point(-1f    , 1f     , point/Source/point_rd)
        ,point(405600f, 111300f, point/Source/point_rd)
        ,spoint"
     ),point( 0s, 0s), point(500s, 400s)
  );

unit&lt;spoint&gt; gridunit := <B>TiledUnit</B>(point(100s, 200s, gridunit_untiled));
</pre>

*result: a tiled grid unit, called TiledUnit with 10 tiles of 100 * 200 cells each.*