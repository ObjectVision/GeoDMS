*[[Configuration Examples]] Explicit Tiling*

For some time/memory intensive operations, like determining the dwelling type of each pand in the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG), an explicit tiling of polygons is used to limit the number of objects worked on simultaneously.

## example

<pre>
unit&lt;uint32&gt; TileDomain := 
 ='union_unit('+AsItemList('CreateTiles/LoopPolygons/'+string(CreateTiles/TileSet/name)+'/shape')+')'
{  
    attribute&lt;rdc&gt; geometry (poly) := 
     ='union_data(.,'+AsItemList(
         'CreateTiles/LoopPolygons/'+string(CreateTiles/TileSet/name)+'/shape/geometry')+
      ')';
}

container CreateTiles
{
    parameter&lt;float32&gt; NrofTiles_horizontal := 5f; 
    parameter&lt;float32&gt; NrofTiles_vertical   := 5f;
   
    <I>// bounding box of The Netherlands</I>
    parameter&lt;meter> x_min := 0[meter];
    parameter&lt;meter> x_max := 280000[meter]; 
    parameter&lt;meter> y_min := 300000[meter];
    parameter&lt;meter> y_max := 625000[meter];
   
    parameter&lt;meter> x_div := (x_max - x_min) / NrofTiles_horizontal;
    parameter&lt;meter> y_div := (y_max - y_min) / NrofTiles_vertical;
   
    unit&lt;uint32&gt; TileSet := range(uint32, 0, uint32(NrofTiles_horizontal * NrofTiles_vertical))
    {
       attribute&lt;uint32&gt; values     := id(.);
       attribute&lt;string&gt; name       := 'Tile'+ string(values);
       attribute&lt;uint32&gt; row_number := values / NrofTiles_horizontal[uint32];
       attribute&lt;uint32&gt; col_number := mod(values, NrofTiles_horizontal[uint32]);
    }
   
    container LoopPolygons := for_each_ne(
      TileSet/name
     ,'CreatePolygons('+string(TileSet/values)+','+string(TileSet/row_number)+','+string(TileSet/col_number)+')'
    );
   
    template CreatePolygons
    {
	   parameter&lt;uint32&gt; TileNumber;
	   parameter&lt;uint32&gt; row_number;
	   parameter&lt;uint32&gt; col_number;
	       
	   unit&lt;uint32&gt; shape: nrofrows = 1
	   {
	      attribute&lt;rdc&gt; left_top     := 
               const(point(x_min + col_number[float32]     *x_div, y_max-row_number[float32]     *y_div, rdc), .);
              attribute&lt;rdc&gt; right_top    :=
               const(point(x_min + (col_number[float32]+1f)*x_div, y_max-row_number[float32]     *y_div, rdc), .);
	      attribute&lt;rdc&gt; right_bottom := 
               const(point(x_min + (col_number[float32]+1f)*x_div, y_max-(row_number[float32]+1f)*y_div, rdc), .);
	      attribute&lt;rdc&gt; left_bottom  := 
               const(point(x_min + col_number[float32]     *x_div, y_max-(row_number[float32]+1f)*y_div, rdc), .);
	           
	      attribute&lt;rdc&gt; geometry (poly) := 
                 points2sequence(pointset/point, pointset/sequence, pointset/order);
	   }
	   unit&lt;uint32&gt; pointset: nrofrows = 5
	   {
	      attribute&lt;rdc&gt;    point    := 
                 union_data(
                  ., shape/left_top, shape/right_top, shape/right_bottom, shape/left_bottom, shape/left_top
                 );
	      attribute&lt;shape&gt;  sequence := const(0,., shape);
	      attribute&lt;uint32&gt; order    := id(.);
	   }
    }
}
</pre>