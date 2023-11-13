*[[Configuration examples]] Convert OD long to wide format*

If you would create a full OD with a Dijkstra algorithm, you will get the result in a long data format. This is often not convenient when exporting to other applications. The following piece of code can be helpful in converting that output into a wide date format. The following code could be inserted in the Dijkstra unit:

<pre>
attribute&lt;string&gt; impedance_min := string(round(impedance / 60f)); 
<I>//Convert the impedance from seconds to minutes, round it off, and store it as a string value.</I>

unit&lt;uint32&gt; Matrix_Array := org
{
    attribute&lt;string&gt; org_name           := org/name;
    attribute&lt;string&gt; impedance_min_list := AsList(impedance_min, ';', OrgZone_rel);
}

unit&lt;uint32&gt; Header : nrofrows = 1
{
    attribute&lt;string&gt; dest_name          := AsList(dest/name, ';', const(0[Header],dest));
}

unit&lt;uint32&gt; Matrix_met_header := union_unit(Header, Matrix_Array) 
<I>//This unit can easily be exported to csv and used in other applications.</I>
{
    attribute&lt;string&gt; org_name           := union_data(., const('',Header), Matrix_Array/org_name);
    attribute&lt;string&gt; values             := union_data(., Header/dest_name, Matrix_Array/impedance_min_list);
}