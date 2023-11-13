*[[Configuration examples]] Dijkstra examples*

## example

<pre>
unit&lt;uint64&gt; Dijkstra_cut := 
   dijkstra_m64('bidirectional(link_flag);startPoint(Node_rel);endPoint(Node_rel)
   ;cut(OrgZone_max_imp);od:OrgZone_rel,DstZone_rel'
       , NetworkSpec/OrgToDest/impedance
       , NetworkSpec/OrgToDest/F1
       , NetworkSpec/OrgToDest/F2
       , NetworkSpec/OrgToDest/LinkSet/roadtype    != classifications/OSM/roadtype/V/motorway  
	 && NetworkSpec/OrgToDest/LinkSet/roadtype != classifications/OSM/roadtype/V/motorway_link 
	 // motor way en motor way link, snelwegen zijn niet bidirectional
       , NetworkSpec/OrgToDest/nr_orgNode
       , NetworkSpec/OrgToDest/nr_destNode
       , MaxTravelTime
   ), FreeData = "false"
{
   attribute&lt;int32&gt; Inhabitants           := dest/Inhabitants_2019[DstZone_rel];
   attribute&lt;int32&gt; Inhabitants_org (org) := sum(Inhabitants, OrgZone_rel);
}
</pre>

<pre>
unit&lt;uint64&gt; Dijkstra_fullOD := //calculation a full origin-destination matrix
   dijkstra_m64('bidirectional(link_flag);startPoint(Node_rel);endPoint(Node_rel)
      ;od:impedance,OrgZone_rel,DstZone_rel'
       , NetworkSpec/OrgToDest/impedance
       , NetworkSpec/OrgToDest/F1
       , NetworkSpec/OrgToDest/F2
       , NetworkSpec/OrgToDest/LinkSet/roadtype    != classifications/OSM/roadtype/V/motorway 
         && NetworkSpec/OrgToDest/LinkSet/roadtype != classifications/OSM/roadtype/V/motorway_link 
         // motor way en motor way link, snelwegen zijn niet bidirectional
       , NetworkSpec/OrgToDest/nr_orgNode
       , NetworkSpec/OrgToDest/nr_destNode
   ), FreeData = "false"
{
   attribute&lt;string&gt; impedance_min := string(round(impedance / 60f)); 
   //Convert the impedance from seconds to minutes, round it off, and store it as a string value.

   unit&lt;uint32&gt; Matrix_Array := org
   {
       attribute&lt;string&gt; org_name           := org/label;
       attribute&lt;string&gt; impedance_min_list := AsList(impedance_min, ';', OrgZone_rel);
   }

   unit&lt;uint32&gt; Header : nrofrows = 1
   {
       attribute&lt;string&gt; values := AsList(dest/name, ';', const(0[Header],dest));
   }

   unit&lt;uint32&gt; Matrix_met_header := union_unit(Header, Matrix_Array) 
   //This unit can easily be exported to csv and used in other applications.
   {
       attribute&lt;string&gt; org_name := union_data(., const('',Header), Matrix_Array/org_name);
       attribute&lt;string&gt; values   := union_data(., Header/values, Matrix_Array/impedance_min_list) ;
   }
}
</pre>

<pre>

unit&lt;uint64&gt; Dijkstra_limit :=
   dijkstra_m64('bidirectional(link_flag);startPoint(Node_rel):max_imp
      ;endPoint(Node_rel);limit(OrgZone_max_mass,DstZone_mass)'
       , NetworkSpec/OrgToDest/impedance
       , NetworkSpec/OrgToDest/F1
       , NetworkSpec/OrgToDest/F2
       , NetworkSpec/OrgToDest/LinkSet/roadtype    != classifications/OSM/roadtype/V/motorway  
         && NetworkSpec/OrgToDest/LinkSet/roadtype != classifications/OSM/roadtype/V/motorway_link 
	 // motor way en motor way link, snelwegen zijn niet bidirectional
       , NetworkSpec/OrgToDest/nr_orgNode
       , NetworkSpec/OrgToDest/nr_destNode
       , MaxInhabitants, dest/Inhabitants_2019[float32]
   );

attribute&lt;float32&gt; DistanceToNearestDest (NodeSet) := 
   dijkstra_s('bidirectional(link_flag);startPoint(Node_rel)'
       , LinkSet/length
       , LinkSet/F1
       , LinkSet/F2
       , NetwerkSpec/OrgToDest/LinkSet/wegtype    != classifications/OSM/wegtype/v/motorway 
         && NetwerkSpec/OrgToDest/LinkSet/wegtype != classifications/OSM/wegtype/v/motorway_link
       , NetwerkSpec/OrgToDest/nr_destNode
   );
</pre>