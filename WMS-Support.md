*[[Recent Developments]]: WMS Support*

Since version 7.163 the GeoDMS supports [web mapping services (WMS)](https://nl.wikipedia.org/wiki/Web_Map_Service) as an alternative [[background layer]]. In more recent GeoDMS versions some issues are solved.

With a WMS scale dependent and fast background layers can be presented in the map view, without having to add/update data for a project. See [[background layer]] for examples on how to configure.

[[images/WMS_w700.png]]

WMS services work with fixed scale levels, in the GeoDMS the user is free to choose the area of interest (and derived scale). With the pop-up menu option Zoom 1 Grid to 1 Pixel it's easy to select a scale level with an optimal rendering quality.

The GeoDMS supports at the moment the WMTS protocol and images in [png](https://nl.wikipedia.org/wiki/Portable_network_graphics) and since 7.198 also in [jpeg](https://nl.wikipedia.org/wiki/JPEG) format. We have experience with multiple layers from the host server: geodata.nationaalgeoregister.nl.

## related issues

-   [issue 819](http://mantis.objectvision.nl/view.php?id=819)