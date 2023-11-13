The **[[cdf]]** [[property]] is used to configure a default [[classification scheme|Classification]] for a [[values unit]].

Refer the cdf property to the [[data item]] with the class breaks of a classification. The values unit of this data item need to be the same as the values unit for which the cdf property is configured.

If a default classification scheme is configured for a values unit, it is by default applied in the Map view of the [[GeoDMS GUI]] for each data item with this values unit.

The default classification can be overruled for a data item, by configuring the cdf property for the data item for which the classification scheme needs to be overruled.

## example
<pre>
unit&lt;uint32&gt; meter := BaseUnit(Meter,float32), cdf  = "m_dist/ClassBreaks";

unit&lt;uint8&gt; m_dist: nrofrows = 3
{
   attribute&lt;meter&gt; ClassBreaks: DialogType = "Classification",
      [   0, 1000, 2500];
   attribute&lt;uint32&gt; BrushColor:  DialogType = "BrushColor",
      [rgb(  0,192,  0),rgb( 51,205,  0),rgb(102,217,  0)];
   attribute&lt;string&gt; Label: DialogType = "LabelText",
      ['0 .. 1 km','1 .. 2 km',,'>=2.5 km'];
}
</pre>