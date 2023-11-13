## description

A cdf [[property]] is used to configure a [[classification scheme|classification]] for a [[values unit]] or [[data item]].

cdf properties can be configured to both values units and data items:

1.  If configured to a values unit, all data items with this values unit will use the classification (by default) in map and graph views.
2.  If a cdf property is configured to a data item, it is used in a graph or map view of this data item and overrules a cdf property configured to the 
 values unit of the data item.

The cdf property always needs to refer to the ClassBreaks item of a Classification scheme.

## example


```
unit<float32>       s               := baseunit('s', float32)   , label = "second", cdf = "seconden/ClassBreaks";

unit<uint8> seconden : nrofrows = 3
{
	attribute<string>   Label        :  ['treated', 'control', 'other'];
	attribute<uint32>   PenColor     := Brushcolor, DialogType = "PenColor";
	attribute<uint32>   BrushColor   :  [rgb(200,0,0),rgb(0,200,0), rgb(128,128,128)], DialogType = "BrushColor";
	attribute<s>        ClassBreaks  :  [0,1080,3600], DialogType = "Classification";
}
```