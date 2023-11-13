_[[WMS background layer]]_

## script

```
unit<dpoint> LatLong : SpatialReference = "EPSG:4326", DialogData = "wms_layer_llh"
{
	parameter<float32> ViewPortMinSize := 100f / 3600f;
	parameter<float32> PenWorldWidth   := 10f / 3600f, DialogType = "PenWorldWidth";
	parameter<float32> LabelWorldSize  := 10f / 3600f, DialogType = "LabelWorldSize";
	parameter<float32> SymbolWorldSize := 10f / 3600f, DialogType = "SymbolWorldSize";
}


container wms_layer_llh 
{
	parameter<uint32> nr_att := 8;
	parameter<uint32> nr_row := 16;
	
	unit<uint32> TileMatrixElems := range(uint32, 0, nr_att * nr_row)
	{
		attribute<string> values:
		//name          , ScaleDen ,             Left     , Top     ,Width ,Height, MatrixWidth,MatrixHeight
		[
		 'EPSG:4326:0' , '279541132.0143589',      '-180.0', '90.0', '256' ,'256' ,       '2'     ,       '1'
		,'EPSG:4326:1' , '139770566.00717944',     '-180.0', '90.0', '256' ,'256' ,       '4'     ,       '2'
		,'EPSG:4326:2' ,  '69885283.00358972',     '-180.0', '90.0', '256' ,'256' ,       '8'     ,       '4'
		,'EPSG:4326:3' ,  '34942641.50179486',     '-180.0', '90.0', '256' ,'256' ,      '16'     ,       '8'
		,'EPSG:4326:4' ,  '17471320.75089743',     '-180.0', '90.0', '256' ,'256' ,      '32'     ,      '16'
		,'EPSG:4326:5' ,   '8735660.375448715',    '-180.0', '90.0', '256' ,'256' ,      '64'     ,      '32'
		,'EPSG:4326:6' ,   '4367830.1877243575',   '-180.0', '90.0', '256' ,'256' ,     '128'     ,      '64'
		,'EPSG:4326:7' ,   '2183915.0938621787',   '-180.0', '90.0', '256' ,'256' ,     '256'     ,     '128'
		,'EPSG:4326:8' ,   '1091957.5469310894',   '-180.0', '90.0', '256' ,'256' ,     '512'     ,     '256'
		,'EPSG:4326:9' ,    '545978.7734655447',   '-180.0', '90.0', '256' ,'256' ,    '1024'     ,     '512'
		,'EPSG:4326:10',    '272989.38673277234',  '-180.0', '90.0', '256' ,'256' ,    '2048'     ,    '1024'
		,'EPSG:4326:11',    '136494.69336638617',  '-180.0', '90.0', '256' ,'256' ,    '4096'     ,    '2048'
		,'EPSG:4326:12',     '68247.34668319309',  '-180.0', '90.0', '256' ,'256' ,    '8192'     ,    '4096'
		,'EPSG:4326:13',     '34123.67334159654',  '-180.0', '90.0', '256' ,'256' ,   '16384'     ,    '8192'
		,'EPSG:4326:14',     '17061.83667079827',  '-180.0', '90.0', '256' ,'256' ,   '32768'     ,   '16384'
		,'EPSG:4326:15',      '8530.918335399136', '-180.0', '90.0', '256' ,'256' ,   '65536'     ,   '32768'
		];
	}

	unit<uint32> TileMatrix := range(uint32, 0, nr_row)
	{
		attribute<.>       id                := id(.);

		attribute<string>  name              :=         TileMatrixElems/values[value(id * nr_att + 0, TileMatrixElems)];
		attribute<float64> ScaleDenominator  := float64(TileMatrixElems/values[value(id * nr_att + 1, TileMatrixElems)]);
		attribute<float64> LeftCoord         := float64(TileMatrixElems/values[value(id * nr_att + 2, TileMatrixElems)]);
		attribute<float64> TopCoord          := float64(TileMatrixElems/values[value(id * nr_att + 3, TileMatrixElems)]);
		attribute<uint16>  TileWidth         :=  uint16(TileMatrixElems/values[value(id * nr_att + 4, TileMatrixElems)]);
		attribute<uint16>  TileHeight        :=  uint16(TileMatrixElems/values[value(id * nr_att + 5, TileMatrixElems)]);
		attribute<uint32>  MatrixWidth       :=  uint32(TileMatrixElems/values[value(id * nr_att + 6, TileMatrixElems)]);
		attribute<uint32>  MatrixHeight      :=  uint32(TileMatrixElems/values[value(id * nr_att + 7, TileMatrixElems)]);
	}

	parameter<string> host   := 'tiles.geoservice.dlr.de';
	parameter<string> layer := 'eoc:basemap';
	parameter<string> url    := 'https://' + host;
	parameter<string> unit   := "degree";

	parameter<string> target := 
		'/service/wmts/?SERVICE=WMTS&VERSION=1.0.0&REQUEST=GetTile&LAYER=' + layer +
		'&STYLE=_empty&TileMatrixSet=EPSG%3A4326&TILEMATRIX=EPSG%3A4326%3A@TM@'+
		'&TILEROW=@TR@&TILECOL=@TC@&FORMAT=image%2Fpng';
}
```