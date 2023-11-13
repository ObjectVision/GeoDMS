_[[WMS background layer]]_

## script

<pre>
container ngr_layer_lea 
{
	parameter&lt;uint32&gt; nr_att := 8;
	parameter&lt;uint32&gt; nr_row := 13;
	
	unit&lt;uint32&gt; TileMatrixElems := range(uint32, 0, nr_att * nr_row)
	{
		attribute&lt;string&gt; values:
		//name          , ScaleDen , Left        , Top     ,Width ,Height, MatrixWidth,MatrixHeight
		[
		 'EU:3035:0' , '62779017','2000000.0','5500000.0', '256' ,'256' ,    '1'     ,    '1'
		,'EU:3035:1' , '31389508','2000000.0','5500000.0',  '256' ,'256' ,    '2'     ,    '2'
		,'EU:3035:2' , '15694754','2000000.0', '5500000.0',  '256' ,'256' ,    '4'     ,    '4'
		,'EU:3035:3' ,  '7847377','2000000.0', '5500000.0',  '256' ,'256' ,    '8'     ,    '8'
		,'EU:3035:4' ,  '3923688','2000000.0', '5500000.0',  '256' ,'256' ,   '16'     ,   '16'
		,'EU:3035:5' ,  '1961844','2000000.0', '5500000.0',  '256' ,'256' ,   '32'     ,   '32'
		,'EU:3035:6' ,   '980922','2000000.0', '5500000.0',  '256' ,'256' ,   '64'     ,   '64'
		,'EU:3035:7' ,   '490461', '2000000.0','5500000.0',  '256' ,'256' ,  '128'     ,  '128'
		,'EU:3035:8' ,   '245230', '2000000.0','5500000.0',  '256' ,'256' ,  '256'     ,  '256'
		,'EU:3035:9' ,   '122615', '2000000.0','5500000.0',  '256' ,'256' ,  '512'     ,  '512'
		,'EU:3035:10',    '61307.6', '2000000.0','5500000.0', '256' ,'256' , '1024'     , '1024'
		,'EU:3035:11',    '30653.8', '2000000.0','5500000.0',  '256' ,'256' , '2048'     , '2048'
		,'EU:3035:12',    '15326.9', '2000000.0','5500000.0',  '256' ,'256' , '4096'     , '4096'
		];
	}

	unit&lt;uint32&gt; TileMatrix := range(uint32, 0, nr_row)
	{
		attribute&lt;.&gt;       id                := id(.);

		attribute&lt;string&gt;  name              :=         TileMatrixElems/values[value(id * nr_att + 0, TileMatrixElems)];
		attribute&lt;float64&gt; ScaleDenominator  := float64(TileMatrixElems/values[value(id * nr_att + 1, TileMatrixElems)]);
		attribute&lt;float64&gt; LeftCoord         := float64(TileMatrixElems/values[value(id * nr_att + 2, TileMatrixElems)]);
		attribute&lt;float64&gt; TopCoord          := float64(TileMatrixElems/values[value(id * nr_att + 3, TileMatrixElems)]);
		attribute&lt;uint16&gt;  TileWidth         :=  uint16(TileMatrixElems/values[value(id * nr_att + 4, TileMatrixElems)]);
		attribute&lt;uint16&gt;  TileHeight        :=  uint16(TileMatrixElems/values[value(id * nr_att + 5, TileMatrixElems)]);
		attribute&lt;uint32&gt;  MatrixWidth       :=  uint32(TileMatrixElems/values[value(id * nr_att + 6, TileMatrixElems)]);
		attribute&lt;uint32&gt;  MatrixHeight      :=  uint32(TileMatrixElems/values[value(id * nr_att + 7, TileMatrixElems)]);
	}

    // wmts request params
    parameter&lt;string&gt; layer         := 'eoc:basemap';
    parameter&lt;string&gt; TileMatrixSet := 'EU:3035';
    parameter&lt;string&gt; VERSION       := '1.0.0';
    parameter&lt;string&gt; REQUEST       := 'GetTile';
    parameter&lt;string&gt; STYLE         := '_empty';
    parameter&lt;string&gt; FORMAT        := 'image/png';
	parameter&lt;string&gt; host          := 'tiles.geoservice.dlr.de';
    parameter&lt;string&gt; url           := 'https://' + host;
    parameter&lt;string&gt; unit          := "metre";                     
    
    parameter&lt;string&gt; target := 
        '/service/wmts?SERVICE=WMTS&VERSION=' + VERSION + '&REQUEST=' + REQUEST + '&LAYER=' + layer + 
        '&STYLE=' + STYLE + '&TileMatrixSet=' + TileMatrixSet + '&TILEMATRIX=' + TileMatrixSet + ':@TM@'+
        '&TILEROW=@TR@&TILECOL=@TC@&FORMAT=' + FORMAT;
}



</pre>