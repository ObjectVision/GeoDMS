_[[WMS background layer]]_

## script

<pre>
container tms_layer_osm_webmercator 
{
	parameter&lt;uint32&gt; nr_att := 8;
	parameter&lt;uint32&gt; nr_row := 20;
	
	unit&lt;uint32&gt; TileMatrixElems := range(uint32, 0, nr_att * nr_row)
	{
		attribute&lt;string&gt; values:
		//name                   , ScaleDen                 , Left                  , Top                  ,Width ,Height, MatrixWidth,MatrixHeight
		[
		 'GLOBAL_WEBMERCATOR:0'  , '559082264.0287176'      ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,      '1'     ,      '1'
		,'GLOBAL_WEBMERCATOR:1'  , '279541132.0143588'      ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,      '2'     ,      '2'
		,'GLOBAL_WEBMERCATOR:2'  , '139770566.0071794'      ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,      '4'     ,      '4'
		,'GLOBAL_WEBMERCATOR:3'  ,  '69885283.0035897'      ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,      '8'     ,      '8'
		,'GLOBAL_WEBMERCATOR:4'  ,  '34942641.50179485'     ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,     '16'     ,     '16'
		,'GLOBAL_WEBMERCATOR:5'  ,  '17471320.750897426'    ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,     '32'     ,     '32'
		,'GLOBAL_WEBMERCATOR:6'  ,   '8735660.375448713'    ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,     '64'     ,     '64'
		,'GLOBAL_WEBMERCATOR:7'  ,   '4367830.187724357'    ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,    '128'     ,    '128'
		,'GLOBAL_WEBMERCATOR:8'  ,   '2183915.0938621783'   ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,    '256'     ,    '256'
		,'GLOBAL_WEBMERCATOR:9'  ,   '1091957.5469310891'   ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,    '512'     ,    '512'
		,'GLOBAL_WEBMERCATOR:10' ,    '545978.7734655446'   ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,   '1024'     ,   '1024'
		,'GLOBAL_WEBMERCATOR:11' ,    '272989.3867327723'   ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,   '2048'     ,   '2048'
		,'GLOBAL_WEBMERCATOR:12' ,    '136494.69336638614'  ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,   '4096'     ,   '4096'
		,'GLOBAL_WEBMERCATOR:13' ,     '68247.34668319307'  ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,   '8192'     ,   '8192'
		,'GLOBAL_WEBMERCATOR:14' ,     '34123.673341596535' ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,  '16384'     ,  '16384'
		,'GLOBAL_WEBMERCATOR:15' ,     '17061.836670798268' ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,  '32768'     ,  '32768'
		,'GLOBAL_WEBMERCATOR:16' ,      '8530.918335399134' ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' ,  '65536'     ,  '65536'
		,'GLOBAL_WEBMERCATOR:17' ,      '4265.459167699567' ,'-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' , '131072'     , '131072'
		,'GLOBAL_WEBMERCATOR:18' ,      '2132.7295838497835','-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' , '262144'     , '262144'
		,'GLOBAL_WEBMERCATOR:19' ,      '1066.3647919248917','-20037508.342789244 .0','20037508.342789244.0', '256' ,'256' , '524288'     , '524288'
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
    parameter&lt;string&gt; layer         := 'osm-default';
    parameter&lt;string&gt; STYLE         := 'default';
    parameter&lt;string&gt; FORMAT        := 'png';
	parameter&lt;string&gt; host          := 'tile.openstreetmap.org';
    parameter&lt;string&gt; url           := 'https://' + host;
    parameter&lt;string&gt; unit          := "metre";                     

    parameter&lt;string&gt; target := '/@TM@/@TC@/@TR@.' + format ;
}
</pre>