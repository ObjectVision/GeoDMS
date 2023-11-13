Numeric data items (see [[value types|value type]] Numerical) can be [[Numerical]] or [[Categorical]].

1.  Numerical data need to be classified before it can presented in a map view. Classify means grouping the values of the original distribution in a limited set of classes. A class unit and a class break [[attribute]] are used for this purpose, see example 1.
2.  Categorical data does not have to be classified before it can be presented in a map view, as it is already available in a limited set of classes. Still it can be useful to classify also this type of data, e.g. to reclassify groups to Low, Middle and High, see example 2.

For each class of a class unit, [[visualisation styles|visualisation style]] (like colors) and labels can be configured. Labels describe the classes in the map view legend or in the table. The combination of class breaks (if necessary), the set of [[visualisation styles|visualisation style]] and labels is called a Classification Scheme. Classification schemes in the GeoDMS are usually configured in a Classifications Container.

## classification schemes

The following examples show the configuration of Classification Schemes for Numerical (1.1 and 1.2) and Categorical (2) data:

*example 1.1: Numerical data, explicitly defined colors*
<pre>
unit&lt;float32&gt; meter := baseunit('m', float32)
,   <B>cdf</B> = "Classifications/m_4K/ClassBreaks";

unit&lt;uint8&gt; m_4K: NrofRows = 4
{
  attribute&lt;meter&gt; ClassBreaks: DialogType = "Classification",
      [0, 200, 400, 800];
  attribute&lt;uint32&gt; SymbolColor: DialogType = "SymbolColor",
      [rgb(128,255,0), rgb(0,128,0), rgb(0,64,128), rgb(255,0,0)];
  attribute&lt;string&gt; Labels: DialogType = "LabelText",
      ['0 - 200 meter','200 - 400 meter','400 - 800 meter','>= 800 meter'];
}
</pre>

*example 1.2: Numerical data, multiple color ramp*
<pre>
unit<uint8> Ratio_12K: nrofrows = 12
{
   attribute ClassBreaks: DialogType = "Classification", [ 0,.5,.55,.575,.6,.625,.65,.675,.7,.725,.75,.8];
   attribute&lt;int32&gt; b := max_elem(int32(0),min_elem(int32(255),( ramp(-120,int32(250), .))));
   attribute&lt;int32&gt; r := max_elem(int32(0),min_elem(int32(255),( ramp(int32(250),-120, .))));
   attribute&lt;int32&gt; g := max_elem(int32(0),min_elem(int32(255),( (int32(250) - b - r) *int32(2))));
	
   attribute&lt;uint32&gt;  BrushColor := rgb(r,g,b) ,DialogType = "BrushColor";
)
</pre>

*example 2: Categorical data*

<pre>
unit&lt;uint8&gt; LS_pres: nrofrows = 3
{
   attribute&lt;uint32&gt; BrushColor: DialogType = "BrushColor",
      [rgb(255,255,198),rgb(0,0,200),rgb(0,215,0)];
   attribute&lt;string&gt; Label: DialogType = "LabelText",
      ['Low','Medium','High'];
}
</pre>

In the 1.1 example, a [[values unit]] meter is configured with [[metric]]: meter. Such quantity values units are usually configured in a Units container. For this values unit a [[cdf]] [[property]] is configured, referring to a class break [[attribute]], usually called ClassBreaks (or
Classes), of a Classification Scheme. By configuring this cdf property, the GeoDMS will use this Classification Scheme by default for all [[data items|data item]] with this values unit, to classify the data for the map view.

By configuring the cdf property to a data item, the default Classification Scheme configured for the values unit is overruled.

The class values unit is a uint8 [[unit]] called *m_4K*. Class units are usually configured in a Classification container. For the class unit, the [[nrofrows]] property need to be configured. In the example the class unit has 4 entries.

In the 1.2 example, a color ramp is configured with calculation rules. Some notes to this configuration:
<li>the red and blue are off (value =0) for a reasonable part of the palette. Therefore, they ramp from -120 till the point where they fade in</li>
<li>values red and blue are clipped before green is calculated</li>
<li>the green part in the middle is brighter for easier recognition. Hence that green is not only the difference in brightness from one color, but *int32(2)</li><BR>

In example 2 the class unit is LS_pres (LandScapePressure). This class unit is also the values unit for items describing the landscape pressure in two classes.

## subitems

The class units have the following subitems:

1.  **ClassBreaks item**: The first data item is the item with the class breaks (only for example 1), needed if data must be classified. The values unit     of this ClassBreaks item must be the same values unit as the original data item to be classified. In the example this is the values_unit meter. The [[DialogType]] property for this item needs to be set to Classification. For each class, the start value is configured as primary data (or read from an external storage). In the example the first class starts with the value 0. The class continues until the start value of the next class, in this case 200.     The last class is always open ended. The syntax for the data of the ClassBreaks item is: [*start value class 1, .., start value class n* ]
2.  **Visualisation style items, like SymbolColor and BrushColor** : The second (set of) data items is used for the visualisation of each class. The DialogType needs to be configured to the specific [[visualisation style|Visualisation Style]]. The possible style items are dependent on the type of data. The current Classification and Palette editor in the [[GeoDMS GUI]] only supports adapting colors. Other visualisation style items can at the moment only be configured in the GeoDMS syntax.
3.  **Labels item**: The last data item is the item with the labels shown in the (map) legend for each class. A labels item need to be configured as an attribute with a string values type. The DialogType property for labels items needs to be set to "LabelText". For each class a label needs to be     configured. The syntax for the data of the label item is: [*label 1, .., label n*]. Labels are also shown in the table to refer the data visualized. The current Classification and Palette editor in the GeoDMS GUI supports editing label items.