## viewports

The main Viewports of MapViews (the part of a MapView that contains the map, excluding the LayerControl and overview area) can also be exported as (a collection of) [bitmap files](https://en.wikipedia.org/wiki/BMP_file_format).

The center of the ROI and the set of layers and their aspects are defined by each exported MapView. You can specify the desired PaperSize, and Scale (or DotWorldSize), to overrule the use of the ROI and Scale of the exported ViewPort and specify the given dot-resolution ([DPI](https://en.wikipedia.org/wiki/Dots_per_inch)) of the final medium and a number of sub-dots for added accuracy.

A ViewFactor may be given to scale Symbols and Text Objects to make them readable from distances that significantly differ from typical screen view distances.

## further processing

We advice that you will further process these exports with other software tools (Word, PowerPoint, Impress) to include them in printable publications, web-pages, and/or slide presentations. Therefore, Legends and Map Titles are not included here.

As the GeoDMS and many of these tools (especially their 32 bit builds) cannot process very large individual bitmaps as they often utilize the
[GPU](https://nl.wikipedia.org/wiki/Graphics_processing_unit) which often limit bitmap capacity to approximately 3.000 x 3.000 pixels, the
GeoDMS divides an export over multiple bitmap files when the number of subdots exceeds the given MaxNrSubDotsPerTile.

For example, we were able to make-up a [A0](https://en.wikipedia.org/wiki/ISO_216#A_series) map in PowerPoint by tiling the individual high resolution bitmap images (and added boxes for title and legend), which we could not have handled as one bitmap (an portrait [A0](https://en.wikipedia.org/wiki/ISO_216#A_series) page of 33.1 " x 46.8 " with 600 [dpi](https://en.wikipedia.org/wiki/Dots_per_inch) and 4x4 subdots has
112.320 subdot rows and 79.440 subdot columns, which doesn't even fit in the RAM of most PC's).

## exporting MapViews

MapView Viewports can be exported with the [[GUI|GeoDMS GUI]]) in two ways:

1. With the menu option: File \> Export Viewports or the tool: [[images/Export_viewports.png]]. These options present a dialog with the open MapViews. The views to be exported can be selected, the path for the resulting file names are presented. With the OK button the resulting files are made.

2. With the tool: [[images/Copy_text.png]] only the main ViewPort of the active MapView will be exported to the file name indicated.

The settings for the export need to be configured in the ExportSettings container. See the following example script:

<pre>
container ExportSettings
{ 
   parameter&lt;string&gt;  FileNameBase := "'resultfolder/resultfile'";
   parameter&lt;float64&gt; width  := 24; 
   parameter&lt;float64&gt; height := 18;

   parameter&lt;dpoint&gt;  PaperSize := Point(height / 100.0, width / 100.0)
   ,  url = "about: papersize is defined as height * width";

   parameter&lt;float64&gt; ScaleDenom:   [50000];
   parameter&lt;float64&gt; Scale := 1.0 / ScaleDenom;
   parameter&lt;float64&gt; dwsc : [null];
   parameter&lt;dpoint&gt;  DotWorldSize := point(dwsc, dwsc);
   
   parameter&lt;float64&gt; dpi: = 600;
   parameter&lt;dpoint&gt;  DotSize := Point(0.0254 / dpi,0.0254 / dpi);
   
   parameter&lt;float64&gt; ViewFactor: = 1.0;
   parameter&lt;ipoint&gt;  MaxNrSubDotsPerTile: [(4096, 4096)];

   parameter&lt;uint32&gt;  NrSubDotsPerDot := 1;
}
</pre>

## script parameters

In this script the following parameters can be set:

- **FileNameBase**: The path within the %LocalDataProjDir% and the base name for the files that will be generated. The actual filenames are composed by the base name and the id of the bitmap tile.
- **width**: the width in \[cm\] of the paper for which the export file(s) is/are made
- **height**: the heigth in \[cm\] of the paper for which the export file(s) is/are made

If no width or height parameter is configured or any of them is defined as <null>, these are derived from the map window size. The resulting PaperSize is calculated and does not have to be configured.

- **ScaleDenom**: the scale denominator is used to calculate the scale of the exported file. The value 50.000 means the scale is 1 to 50.000 so 1 \[cm\] in the map is 500 \[meter\] in the real world. If no scale denominator is configured, the scale is based on the scale of the main ViewPort of the MapWindow.
- **dwsc**: the dot world size coordinate can be configured to specify the size of a dot in world coordinates. This is an alternative for configuring the scale of the export. If the ScaleDenom is configured, the dwsc does not have to be configured or can be set to <null> (as in the example). The DotWorldSize [[parameter]] itself is calculated, using the configured dot world size coordinate.
- **dpi:** [Dots Per Inch](https://en.wikipedia.org/wiki/Dots_per_inch). This parameter indicates the number of dots per inch in the resulting file. Increasing this number results in better quality maps but also in more/larger bitmaps. MS Office products such as PowerPoint usually assume images to be 96 [DPI](https://en.wikipedia.org/wiki/Dots_per_inch) (small Fonts), 120 DPI (medium Fonts) or 144 DPI (larger Fonts). Image sizes need to be     adjusted if files with a larger DPI are imported. The actual Dotsize is calculated based on the configured DPI.
- **ViewFactor**: A factor indicating if the final result is to be  seen at the distance of a screen of e.g. of a poster, which is usually looked at from a larger distance. The default value for the ViewFactor is 1, corresponding to a ViewDistance of approximately 50 [cm]. A larger VIewFactor will increase the number of dots per symbol and text object while leaving the number of dots per geographic object unchanged. Use a ViewFactor of 8 for exporting to posters that are assumed to be seen from a distance of 4 [m].
- **maxNrSubDotsPerTile**: The result of the export is a bitmap file or a set of bitmap files(tiling). The maxNrSubDitsPerTile indicates the maximum size of a seperate file, if the resulting file would contain more dots, it will be split up into a set of tiles. To export bitmaps that can be used in other software packages, it is advised to limit the size of an individual bitmap file to a maximum of 4096 * 4096 dots. If the [GPU](https://en.wikipedia.org/wiki/Graphics_processing_unit) of your system does not support writing large bitmaps, it can be necessary to limit this size even further.
- **NrSubDotsPerDot**: This parameter determines the number of subdots in the export file(s) for every dot. Use 1 as a default value and higher values for higher quality images. A larger number increases the number of subdots per object (both symbols and geographic objects). The GeoDMS does not aggregate sub dot color values to average dot color values. The software used for printing and/or displaying the export results should do this to get an anti-aliased version of the original image.

N.B.: to get exports with the same number of subdots as the main ViewPort of a MapView, one should not configure Width, Height, ScaleDenom, and dwsc, or set the to <null>, set ViewFactor and NrSubDotsPerDot to 1, and set the DPI to 127, which corresponds to 0.2 [mm] per dot, the screen resolution that the
GeoDMS assumes when calculating the screen scale of the main ViewPort.