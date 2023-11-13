*[[Configuration examples]] BAG relate attributes and make grids*

This script uses a [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG) snapshot, made with the [BAG Toolkit](https://github.com/ObjectVision/BAG-Tools/wiki/home) and presents examples on

- how to relate attributes between different BAG objects
- how to make [[grid]] maps with [[potential]] calculations.

In the download you will find a configuration and an example BAG snapshot data set for the municipality of Nieuwegein. *Disclaimer: This dataset is only to be used for example purposes.*

Change the *SnapshotBaseDir* [[parameter]] (in the root file of the configuration) to refer to another (national) shapshot.

## contents

The example has two sections:

1) in the container: *RelateerAttributen,* you will find examples of how attributes are related to the vbo and pand [[domain units|domain unit]]. The
configuration contains:

-   simple examples, like how to relate the building years from panden to the vbo's related to these panden
-   more complex examples like how to relate adres ranges to panden.

2) In the [[container]]: *AttributenNaarGrid*, you will find examples on how to visualise [[continous|Numerical]] (vbo surfaces) or categorical data (building year classes) in grids of 250, 100 and 25 meter. It also includes focal statistics ([[potential]]) calculations) with kernels of 3*3, 5*5 and 25*25 cells.

## download

- [configuration/data](https://www.geodms.nl/downloads/GeoDMS_Academy/geodms_academy_bag_relate_and_grid_attributes_20210524.zip)

## concepts

- [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG)
- [grid/raster analysis](https://geogra.uah.es/patxi/gisweb/GISModule/GIST_Raster.htm)

## functions

- [[poly2grid]]
- [[potential]]
- [[point_in_polygon]]
- [[rlookup]]
- [[invert]]
- [[gridset]]
- [[dist2]]
- [[centroid]]
- [[for_each]]
- [[sum]], [[min]], [[max]], [[first]], [[modus]], [[pcount]]
- [[AsList]]
