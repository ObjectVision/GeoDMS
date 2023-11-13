*[[File, Folder and Read functions]] parse_xml*

## syntax

- parse_xml(*string_dataitem*, *xml_scheme*)

## definition

parse_xml(*string_dataitem*, *xml_scheme*) **parses** the contents of the [[argument]] *string_dataitem* with **XML data** into a set of configured [[attributes|attribute]], based on the *xml_scheme* argument.

## applies to

- [[data item]] *string_dataitem* with [[value type]] string
- [[template]] *xml_scheme*, with xml scheme information for the *string_dataitem*.

## condition

The template used as second argument of the xml_parse function may not contain any [[expression]]. If calculations are needed, configure them outside the scope of the template.

## example

<pre>
attribute&lt;string&gt; FileName(File): ['9999OPR08012016-000001.xml','9999OPR08012016-000003.xml'];
attribute&lt;string&gt; XmlData(File
:   StorageType = "strfiles"
,   StorageName = "%SourceDataDir%/BAG";

template scheme
{
   unit&lt;uint32&gt; bag_LVC_OpenbareRuimte
   {
      attribute&lt;string&gt; bag_LVC_identificatie;
      attribute&lt;string&gt; bag_LVC_OpenbareRuimteNaam;
      attribute&lt;string&gt; bag_LVC_openbareRuimteType;

      unit&lt;uint32&gt; bag_LVC_gerelateerdeWoonplaats
      {
         attribute&lt;string&gt; bag_LVC_identificatie;
      }

      attribute&lt;string&gt; bag_LVC_openbareruimteStatus;
      attribute&lt;string&gt; bag_LVC_aanduidingRecordInactief;
      attribute&lt;string&gt; bag_LVC_aanduidingRecordCorrectie;
      attribute&lt;string&gt; bag_LVC_officieel;
      attribute&lt;string&gt; bag_LVC_inOnderzoek;
      attribute&lt;string&gt; bagtype_begindatumTijdvakGeldigheid;
      attribute&lt;string&gt; bagtype_einddatumTijdvakGeldigheid;
      attribute&lt;string&gt; bagtype_documentdatum;
      attribute&lt;string&gt; bagtype_documentnummer;
   }
}

container ParsedXML := <B>parse_xml(</B>XmlData, scheme<B>)</B>;

unit&lt;uint32&gt; gerelateerdeWoonplaats := ParsedXML/bag_LVC_OpenbareRuimte/bag_LVC_gerelateerdeWoonplaats
{
   attribute&lt;string&gt;                 identificatie      := bag_LVC_identificatie;
   attribute&lt;gerelateerdeWoonplaats&gt; openbareruimte_rel := value(Parent_rel, ParsedXML/bag_LVC_OpenbareRuimte);
 }

</pre>

*This example parses two OpenbareRuimte files from the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG) into a set of attributes for GeoDMS domain units.*