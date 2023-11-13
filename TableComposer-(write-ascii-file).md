*[[Configuration examples]] TableComposer*

The TableComposer is a scriptblock using the [[Str StorageManager]] and a set of GeoDMS functions to **write** data from a delimited 
[[ASCII file|ASCII Files]].

The TableComposer is used to write data, use the [[TableChopper|TableChopper (read ascii file)]] to read data from delimited ASCII Files.

## example
1) container for exporting header with names and values in .csv file

<pre>
container TableComposer
{
   // assume ExportDomain is a configured domain unit with two attributes as subitems: id & dist</I>
  unit&lt;uint32&gt; context := ExportDomain; 
  parameter&lt;string&gt; fieldlist      := 'id;Dist';
  parameter&lt;string&gt; fieldseparator := ';';
  parameter&lt;string&gt; filename       := '%LocalDataProjDir%/dist.csv';

  unit&lt;uint32&gt; field := range(uint32, 0,strcount(fieldlist, ';') +1)
  {
      attribute&lt;string&gt; name := ReadArray(FieldList, ., string, 0);
  }

  parameter&lt;string&gt; newline := '\n';
  parameter&lt;string&gt; header  := FieldList;
  attribute&lt;string&gt; body (context) := = AsList(
        +'string(context/' + field/name + ')',' + '+quote(fieldseparator)+' +'
     );
 
  parameter&lt;string&gt; result := header + newline + AsList(body, newline)
  ,  StorageName = "= filename"
  ,  StorageType = "str";
}
</pre>

2) template for exporting header with names and metric and values in .csv file

<pre>
 template TableComposerWithMetric
 {
    <I>// begin case parameters </I>;
    unit&lt;uint32&gt; table;
    parameter&lt;string&gt; fieldlist;
    parameter&lt;string&gt; filename;
    <I> // end case parameters </I>;

     container impl: isHidden = "True"
     {
         unit&lt;uint32&gt; field := range(uint32,0,strcount(fieldlist,';')+1)
         {
            attribute&lt;string&gt; FieldDescr  := ReadArray(fieldlist,.,string,0);
            attribute&lt;uint32&gt; SepPos      := strpos(FieldDescr, ':');
            attribute&lt;string&gt; name        := IsDefined(SepPos) ? substr(FieldDescr, 0, SepPos): FieldDescr;
            attribute&lt;string&gt; NameInTable 
               := IsDefined(SepPos) ? substr(FieldDescr, SepPos+1, strlen(FieldDescr)) : FieldDescr;
            attribute&lt;string&gt; metric      :=  
               ='union_data(.,'+
                    AsList('+PropValue(ValuesUnit(table/'+impl/Field/NameInTable+'),'+quote('metric') + ')', ',') +
                ')';
           }
        }
        parameter&lt;string&gt; header 
           := AsList(Quote(impl/Field/name +(impl/field/metric==*?* : ' [' + impl/Field/metric +']')), ';');

        parameter&lt;string&gt; body_expr    
           := AsList('AsExprList(table/' + impl/field/NameInTable + ', id(table))',' + *;* + ');
        attribute&lt;string&gt; body (table) := = body_expr;
        parameter&lt;string&gt; result := header + '\n' + AsList(body+'\n', '')
        , StorageName  = "=filename"
        , StorageType  = "str";
 }
</pre>

## Explanation

-   The *context* [[unit]] refers to the configured [[domain unit]] *ExportDomain*. All [[attributes|attribute]] to be exported need to be configured as direct [[subitems|subitem]] of this domain unit.
-   The *fieldlist* parameter configures the names of the attributes to be exported, semicolon delimited.
-   The *fieldseparator* parameter configures the separator used in the ASCII file between the fields. In most cases the semicolon is used.
-   The *filename* parameter configures the file name to be exported. In the example a [[placeholder|Folders and Placeholders]] for the file path is used.

When the TableComposer is used as a template to write multiple files, these first three items are often used as [[case parameters|case parameter]].

-   The *field* unit is the domain unit of the set of fields to be exported. In this example the [[cardinality]] of this domain is two, derived from the number of  semicolons in the fieldlist string value. 
The field unit has one subitem: the *name* of the field (in this example: *id* and *Dist*). These names are read with the [[ReadArray]] function from the fieldlist string value.
-   A parameter newline is configured with as [[expression]] : \n. This code indicates a new line character and is used to split the contents of the resulting file over multiple lines.
-   The *header* parameter refers to the fieldlist parameter.
-   The *body* parameter will contain the actual primary data from the attributes to be exported. The contents of the attributes are concatenated, with the fieldseperator as delimiter (in the example a semicolon is used). 
With the [[AsList]] function, the concatenated values are combined to one string per element.
-   The *result* parameter combines the header, a new line and the body as a string parameter, separated by new lines. The StorageName and the StorageType properties are configured for the *result* parameter.

## Optional extra lines
Other [[properties|property]] can also be exported to the delimited ASCII file. The example shows how to add an extra line between the header and the body with the [[metric]] of the attributes to be exported.

<pre>
parameter&lt;string&gt; metric := = AsList(
       quote('[')+'+PropValue(
         ValuesUnit(context/' + field/name +')
         ,' + quote('metric') + '
      ) + ' + quote(']' )
      , '+' + quote(fieldseparator) + '+' 
   );
</pre>

This parameter results in the metric of each exported attribute between square brackets, separated by the fieldseparator parameter.

To add this parameter to the resulting file, replace the configuration of the resulting parameter result by:

<pre>
parameter&lt;string&gt; result := header + newline + metric + newline + AsList(body, newline)
,  StorageName = "=FileName"
,  StorageType = "str";
</pre>
