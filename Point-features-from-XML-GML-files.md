This example shows how point data can be read and processed from a [[GML|https://en.wikipedia.org/wiki/Geography_Markup_Language]] data source. The example reads and processes verblijfsobject geometries from the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG).

<pre>
container vbo
{
   attribute&lt;string&gt; FileName(File):
      ['9999VBO08012016-000001.xml','9999VBO008012016-000003.xml'];
   attribute&lt;String&gt; XmlData (File)
   :   StorageType = "strfiles"
   ,   StorageName = "%SourceDataDir%/BAG";

   container ParsedXML := parse_xml(XmlData, scheme);
  
   unit&lt;uint32&gt; vbo := ParsedXML/bag_LVC_Verblijfsobject
   {
      attribute&lt;string&gt;  gml_pos := ParsedXML/bag_LVC_Verblijfsobject/gml_pos;
      attribute&lt;float64&gt; x       := ReadElems(gml_pos, float64, const(0,.))
      {
          attribute&lt;uint32&gt; ReadPos (vbo); 
          <I>// explict configuration of ReadPos item</I>
          <I>// is necessary to find vbo unit</I>
      }
      attribute&lt;float64&gt; y := ReadElems(gml_pos, float64, x/ReadPos)
      {
         attribute&lt;uint32&gt; ReadPos (vbo);
      }
      attribute&lt;rdc_mm&gt; geometry_mm := point(round(y * 1000.0), round(x * 1000.0), rdc_mm);

      template scheme
      {
         unit&lt;uint32&gt; bag_LVC_Verblijfsobject 
         {
            attribute&lt;string&gt; bag_LVC_identificatie;
            attribute&lt;string&gt; gml_pos;
         }
      }
   }
}
</pre>
