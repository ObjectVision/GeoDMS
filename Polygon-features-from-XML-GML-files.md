## GML polygon

This example shows how polygon data can be read and processed from a [[GML|https://en.wikipedia.org/wiki/Geography_Markup_Language]] data source. The example reads and processes pand geometries from the [BAG](https://github.com/ObjectVision/BAG-Tools/wiki/BAG).

We advice to put the templates in a separate configuration file.

<pre>
container pand
{
attribute&lt;string&gt; FileName(File):
   ['9999PND08012016-000001.xml','9999PND008012016-000003.xml'];
attribute&lt;String&gt; XmlData (File)
:   StorageType = "strfiles"
,   StorageName = "%SourceDataDir%/BAG";

container ParsedXML  := parse_xml(XmlData, scheme);
container geoBuilder := ProcessGmlPolygon(ParsedXML/bag_LVC_pand,false);

template scheme
{
  unit&lt;uint32&gt; bag_LVC_pand 
  {
     attribute&lt;string&gt; bag_LVC_identificatie;

     unit&lt;uint32&gt; gml_Polygon 
     {
        unit&lt;uint32&gt; gml_posList
        {
           attribute&lt;string&gt; srsDimension;
           attribute&lt;string&gt; count;
        }
     }
     unit&lt;uint32&gt; gml_Interior 
     {
        unit&lt;uint32&gt; gml_posList
        {
           attribute&lt;string&gt; srsDimension;
           attribute&lt;string&gt; count;
        }
     }
 }
}
<I>// templates for processing polygon geometries</I>
template ProcessGmlPolygon
{
<I>// begin case parameters</I>
unit&lt;uint32&gt;    gmlContext;
parameter&lt;bool&gt; hasMultiplePolygons;
<I>// end case parameters</I>

container impl := ProcessGmlPolygonImpl(gmlContext/gml_Polygon);
parameter&lt;string&gt; geometry_expr:= hasMultiplePolygons
   ?   'templates/optimized_union(
           impl/gmlPolygon
         , gmlContext
         , impl/Polygon/result
         , impl/gmlPolygon/Parent_rel
       )'
   :   'templates/one2one_union(
          impl/gmlPolygon
        , gmlContext
        , impl/Polygon/result
        , impl/gmlPolygon/Parent_rel
       )';
container geometry := = geometry_expr;
attribute&lt;rdc_mm&gt; result (gmlContext, polygon) := geometry/result;
}

template ProcessGmlPolygonImpl
{
<I>// begin case parameters</I>
unit&lt;uint32&gt; gmlPolygon;
<I>// end case parameters</I>

container Exterior := ProcessLinearRing(gmlPolygon, true);
container Interior := ProcessLinearRing(gmlPolygon/gml_Interior, false);
container Polygon  := ProcessPolygon(
   gmlPolygon,              Exterior/geometry_mm, 
   gmlPolygon/gml_Interior, Interior/geometry_mm,
   gmlPolygon/gml_Interior/Parent_rel
   );
}

template ProcessLinearRing
{
<I>// begin case parameters </I>
unit&lt;uint32&gt; parsedXMLsrc;
parameter&lt;bool&gt; isExt; // exterior
<I>// end case parameters </I>

container impl 
{
   container posList := ProcessPosList(parsedXMLsrc/gml_posList, isExt);
   container union   := one2one_union(
       parsedXMLsrc/gml_posList
     , parsedXMLsrc
     , posList/result
     , parsedXMLsrc/gml_posList/Parent_rel
     );
 }
 attribute&lt;rdc_mm&gt; geometry_mm(poly,parsedXMLsrc) := impl/union/result;
}

template ProcessPosList
{
<I>// begin case parameters </I>
unit&lt;uint32&gt; posList;
parameter&lt;bool&gt; isExterior;
<I>// end case parameters </I>

unit&lt;uint32&gt; impl := posList
{
   attribute&lt;string&gt;  values := _ValuesTable/Values[value_rel];
   attribute&lt;string&gt;  str_sequence := 
      '{'+string(uint32(count) * uint32(srsDimension)) +':'+ values +'}'
   ,  IntegrityCheck = "srsDimension == '2' || srsDimension == '3'";
   attribute&lt;Float64&gt; f64_sequence(poly) := Float64Seq(str_sequence);

   unit&lt;uint32&gt; posListunit := range(uint32, 0, #posList)
   {
      attribute&lt;uint32&gt; nrCoordPerPoint := 
         union_data(posListunit, uint32(srsDimension));
   }
   unit&lt;uint32&gt; coordinates := 
      sequence2points(union_data(posListunit, f64_sequence));

   unit&lt;uint32&gt; p := 
      select_with_org_rel(
               coordinates/ordinal 
             % posListunit/nrCoordPerPoint[coordinates/SequenceNr] == 0
            )
   {
      attribute&lt;float64&gt;      x    := coordinates/point[org_rel];
      attribute&lt;float64&gt;      y    := coordinates/point[org_rel + 1];
      attribute&lt;rdc_mm&gt;       p_mm := 
         point(Round(y * 1000.0), Round(x * 1000.0), rdc_mm);
      attribute&lt;posListunit&gt;  s    := 
         coordinates/SequenceNr[org_rel];
      attribute&lt;uint32&gt;       fo   := 
           coordinates/ordinal[org_rel]
         / posListunit/nrCoordPerPoint[s];
      attribute&lt;uint32&gt;       ro   := 
         pcount(s)[s]- fo - 1;
   }
   attribute&lt;rdc_mm&gt; geometry_mm (poly) := 
      union_data(
                  posList
                , points2sequence_pso(
                      p/p_mm, p/s
                    , isExterior 
                      ?   p/ro 
                      :   p/fo
                  )
                );
}
attribute&lt;rdc_mm&gt; result (posList, poly) := impl/geometry_mm[rdc_mm];
}

template ProcessPolygon
{
<I>// begin case parameters </I>
unit&lt;uint32&gt;        Exterior;
attribute&lt;rdc_mm&gt;   ExtGeometry(Exterior, poly);
unit&lt;uint32&gt;        Interior;
attribute&lt;rdc_mm&gt;   IntGeometry(Interior, poly);
attribute&lt;Exterior&gt; Parent_rel(Interior);
<I>// end case parameters</I>
container impl
{
   container IntUnion := optimized_union(
      Interior, Exterior, IntGeometry, Parent_rel
      );
   attribute&lt;uint32&gt; count(Exterior) := pcount(parent_rel);

   unit&lt;uint32&gt; ExtCopy := range(Exterior, 0, #Exterior)
   {
      attribute&lt;uint32&gt; count2 := union_data(., count);
   }
   unit&lt;uint32&gt; nonTrivialExterior := select_with_org_rel(ExtCopy/count2 &gt; 0)
   {
      attribute&lt;Exterior&gt; Exterior_rel := 
         value(org_rel, Exterior);
      attribute&lt;rdc_mm&gt;   diff(poly)   := 
        ExtGeometry[Exterior_rel]- IntUnion/result[Exterior_rel];
      attribute&lt;rdc_mm&gt; result(poly,Exterior) := 
         impl/nonTrivialExterior/diff[
            invert(impl/nonTrivialExterior/Exterior_rel)
         ];
   }
attribute&lt;rdc_mm&gt; result(poly,Exterior) := impl/count == 0 
   ? ExtGeometry 
   : impl/result;
attribute&lt;int32&gt;  area  (Exterior)      := area(result, Int32);
}

template union 
{
<I>// begin case parameters </I>
unit&lt;uint32&gt; child;
unit&lt;uint32&gt; parent;
attribute&lt;rdc_mm&gt; geometry(child, poly);
attribute&lt;parent&gt; parent_rel(child);
<I>// end case parameters </I>

attribute&lt;rdc_mm&gt; result(poly,parent) := 
   partitioned_union_polygon(geometry, parent_rel);
}

template optimized_union
{
<I>// begin case parameters </I>
unit&lt;uint32&gt; child;
unit&lt;uint32&gt; parent;
attribute&lt;rdc_mm&gt; geometry(child, poly);
attribute&lt;parent&gt; parent_rel(child);
<I>// end case parameters </I>

container impl 
{
   attribute&lt;uint32&gt; count(parent) := pcount(parent_rel);
   unit&lt;uint32&gt; childCopy := range(child, 0, #child);

   unit&lt;uint32&gt; nonTrivialChild := 
      select_with_org_rel((count != 1)[union_data(childCopy, parent_rel)])
   {
      attribute&lt;child&gt; child_rel := value(org_rel, child);
      attribute&lt;rdc_mm&gt; union(poly,parent) := 
         partitioned_union_polygon(
              geometry[child_rel]
            , parent_rel[child_rel]
         );
   }
}
attribute&lt;rdc_mm&gt; result (poly,parent) := impl/count &lt;= 1 
   ? geometry[invert(parent_rel)] 
   : impl/nonTrivialChild/union;
attribute&lt;Int32&gt;  area   (parent)      := area(result, Int32);
}

template one2one_union 
{
<I>// begin case parameters </I>
unit&lt;uint32&gt; child;
unit&lt;uint32&gt; parent;
attribute&lt;rdc_mm&gt; geometry(child, poly);
attribute&lt;parent&gt; parent_rel(child);
<I>// end case parameters </I>

container impl 
{
   parameter&lt;bool&gt; Check := 
      (#child == #parent) && all(parent_rel == ID(child));
}
attribute&lt;rdc_mm&gt; result(poly,parent) := union_data(parent, geometry)
, IntegrityCheck = "impl/Check";
}
</pre>
