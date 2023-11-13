A configuration file is an [ASCII file](https://nl.wikipedia.org/wiki/ASCII_(tekenset)), used in the GeoDMS to store logical parts of the modelling logic.

A configuration file in the GeoDMS always has one and only one [[parent item]].

Configuration file can include other configuration files by using the [[include]] statement.

Configuration files can be viewed or edited with a [[configuration file editor]].

Configuration files always use the [extension](https://en.wikipedia.org/wiki/Filename_extension): .dms

## contents

In a configuration file can be configured:

-   which [[units|unit]] are in use
-   which [[classification scheme|classification]] are in use
-   which source data is read 
-   which calculations are made
-   how data is visualised with [[visualisation styles|visualisation style]]
-   how data is exported with [[export]] settings


## primary data

Configuration files can also contain (small parts) of primary data.

### small data items

For small data items like case [[parameters|parameter]] or class boundaries/labels in classifications it is common to store these values in configuration files, see example:

<pre>
&lt;uint32&gt; ClassUnit : nrofrows = 4 
{
   attribute&lt;meter&gt;  ClassBreaks : DialogType = "Classification", [0,200,400,800];
   attribute&lt;string&gt; Label       : DialogType = "LabelText",      ['0 - 200','200 - 400','400 - 800','> 800'];
}
</pre>

_Since version 7.199, is it also possible to configure data in such a way for domains with 0 entries, see [[empty domain]]_

Primary data for [[data items|data_item]] is configured between square brackets [], comma separated. String values need to be single quoted. Configure a value for each entry of the domain unit.

### two dimensional data items

To configure two dimensional data items (coodinates), use the following syntax

<pre>
unit&lt;uint32&gt; PointSet : nrofrows = 4 
{
   attribute&lt;fpoint&gt; geometry : 
   [
      {0,0},{1,1},{2,2},{3,3}
   ];
}
</pre>

### tabular data

Configuring attribute values as in the earlier example is not so user-friendly for tabular data with multiple attributes. With the following configuration example, primary data can be stored in a configuration file and be edited as a table:

<pre>
unit&lt;uint32> supermarket: NrOfRows = 3
{
    parameter&lt;uint32&gt; nrAttr := 5;
    unit&lt;uint32&gt; elem := range(uint32, 0, nrAttr *#supermarkt) // domain of all cells in a table`
    {
        attribute&lt;string&gt; values: [
        <I>// 'name'      ,'street'           ,'housenr' ,'zipcode'  ,'place'</I>
           'Plusmarkt' ,'Amerikaplein'     , '2'      ,'6269DA'   ,'Margraten',
          ,'Lidl'      ,'Wilhelminastraat' ,'63'      ,'6245AV'   ,'Eijsden',
          ,'Spar'      ,'Dalestraat'       ,'23'      ,'6262NP'   ,'Banholt'
        ];
    }
    attribute&lt;.&gt; id := id(.);

    attribute&lt;string&gt; name    := elem/values[value(id * nrAttr + 0, elem)];
    attribute&lt;string&gt; street  := elem/values[value(id * nrAttr + 1, elem)];
    attribute&lt;uint32&gt; housenr := uint32(elem/values[value(id * nrAttr + 2, elem)]);
    attribute&lt;string&gt; zipcode := elem/values[value(id * nrAttr + 3, elem)];
    attribute&lt;string&gt; place   := elem/values[value(id * nrAttr + 4, elem)];
   }
}
</pre>

In this example the primary data is configured for the supermarket/elem/values data item. The values are presented and can be edited as a table. All data values are configured as string values.

The trick here is that the data is not configured per attribute but for a new domain unit: elem, which is the domain unit of all cells in a table. This elem domain can be formatted in a tabular way (see example). Later on the correct values per attribute are selected from the elem domain with [[lookup]]
functions. [[Conversion|Conversion functions]] functions are used to convert the original string values to their requested [[value type]].