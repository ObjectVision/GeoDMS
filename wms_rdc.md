_[[WMS background layer]]_

## description

The configuration below describes examples for WMTS layers from the [PDOK](http://pdok.nl). Often the container wmts_layer and it's [[subitems|subitem]] are configured in a separate dms file and included in a Geography container.

The example presents how a [[coordinate system is configured|How to configure a coordinate system]], with as base unit: point_rd_base. The [[DialogData]] for this [[unit]] need to refer to the container in which a WMS layer is configured. This container need to contain a target
[[parameter]], used to make the relevant requests to the WMTS server.

## script

<pre>
unit&lt;fpoint&gt; point_rd_base  
   : DialogData = "wmts_layer" 
   , Format     = "EPSG:28992";

unit&lt;fpoint&gt; point_rd := range(point_rd_base, point(300000[m],0[m]), point(625000[m],280000[m]));

container wmts_layer 
{
  parameter &lt;uint32&gt; nr_att :=  8;
  parameter &lt;uint32&gt; nr_row := 15;

  unit&lt;uint32&gt; TileMatrixEl := range(uint32, 0, nr_att * nr_row)
   {
      attribute&lt;string&gt; values:      
        //name         , ScaleDen    , Top        , Left      ,Width ,Height , MatrixWidth, MatrixHeight</span>
      [
        'EPSG:28992:0' ,'12288000'   ,'-285401.92', '903402.0', '256' ,'256' ,    '1'  ,    '1'
       ,'EPSG:28992:1' , '6144000'   ,'-285401.92', '903402.0', '256' ,'256' ,    '2'  ,    '2'
       ,'EPSG:28992:2' , '3072000'   ,'-285401.92', '903402.0', '256' ,'256' ,    '4'  ,    '4'
       ,'EPSG:28992:3' , '1536000'   ,'-285401.92', '903402.0', '256' ,'256' ,    '8'  ,    '8'
       ,'EPSG:28992:4' ,  '768000'   ,'-285401.92', '903402.0', '256' ,'256' ,   '16'  ,   '16'
       ,'EPSG:28992:5' ,  '384000'   ,'-285401.92', '903402.0', '256' ,'256' ,   '32'  ,   '32'
       ,'EPSG:28992:6' ,  '192000'   ,'-285401.92', '903402.0', '256' ,'256' ,   '64'  ,   '64'
       ,'EPSG:28992:7' ,   '96000'   ,'-285401.92', '903402.0', '256' ,'256' ,  '128'  ,  '128'
       ,'EPSG:28992:8' ,   '48000'   ,'-285401.92', '903402.0', '256' ,'256' ,  '256'  ,  '256'
       ,'EPSG:28992:9' ,   '24000'   ,'-285401.92', '903402.0', '256' ,'256' ,  '512'  ,  '512'
       ,'EPSG:28992:10',   '12000'   ,'-285401.92', '903402.0', '256' ,'256' , '1024'  , '1024'
       ,'EPSG:28992:11',    '6000'   ,'-285401.92', '903402.0', '256' ,'256' , '2048'  , '2048'
       ,'EPSG:28992:12',    '3000'   ,'-285401.92', '903402.0', '256' ,'256' , '4096'  , '4096'
       ,'EPSG:28992:13',    '1500'   ,'-285401.92', '903402.0', '256' ,'256' , '8192'  , '8192'
       ,'EPSG:28992:14',     '750'   ,'-285401.92', '903402.0', '256' ,'256' ,'16384'  ,'16384'
     ] ;
   }
    unit&lt;uint32&gt; TileMatrix := range(uint32, 0, nr_row)
   {
      attribute&lt;.&gt;       id               := id(.);
      attribute&lt;string&gt;  name             := TileMatrixEl/values[value(id * nr_att + 0, TileMatrixEl)];
      attribute&lt;float64&gt; ScaleDenominator := float64(TileMatrixEl/values[value(id * nr_att + 1, TileMatrixEl)]);
      attribute&lt;float64&gt; LeftCoord        := float64(TileMatrixEl/values[value(id * nr_att + 2, TileMatrixEl)]);
      attribute&lt;float64&gt; TopCoord         := float64(TileMatrixEl/values[value(id * nr_att + 3, TileMatrixEl)]);
      attribute&lt;uint16&gt;  TileWidth        :=  uint16(TileMatrixEl/values[value(id * nr_att + 4, TileMatrixEl)]);
      attribute&lt;uint16&gt;  TileHeight       :=  uint16(TileMatrixEl/values[value(id * nr_att + 5, TileMatrixEl)]);
      attribute&lt;uint32&gt;  MatrixWidth      :=  uint32(TileMatrixEl/values[value(id * nr_att + 6, TileMatrixEl)]);
      attribute&lt;uint32&gt;  MatrixHeight     :=  uint32(TileMatrixEl/values[value(id * nr_att + 7, TileMatrixEl)]);
   }

    // Different examples of tested layers </span>
    parameter&lt;string&gt; layer  := 'grijs';
    // parameter&lt;string&gt; layer  := 'pastel'; </span>
    // parameter&lt;string&gt; layer  := 'standaard'; </span>
    // parameter&lt;string&gt; layer  := 'water'; </span>
  
    // parameter&lt;string&gt; layer  := 'Actueel_ortho25'; </span> 
    // parameter&lt;string&gt; image_format := 'jpeg'; </span>

    parameter&lt;string&gt; image_format := 'png8';
    parameter&lt;string&gt; host := 'service.pdok.nl';

    parameter&lt;string&gt; target := '/brt/achtergrondkaart/wmts/v2_0'
       '?SERVICE=WMTS'
       '&REQUEST=GetTile'
       '&VERSION=1.0.0'
       '&LAYER=' + layer +
       '&TILEMATRIXSET=EPSG:28992'
       '&TILEMATRIX=@TM@'
       '&TILEROW=@TR@'
       '&TILECOL=@TC@'
       '&FORMAT=image/'+image_format
  ;
}
</pre>

## explanation

In this example the brtachtergrondkaart layer: grijs is used as background layer, but other layers can be selected by (un)commenting other layer lines.

If the layer serves .png files, no image_format needs to be configured. If the layer serves .jpeg files (often used for photo's), such as Actueel_ortho25, uncomment also the image_format = 'jpeg' configuration rule.