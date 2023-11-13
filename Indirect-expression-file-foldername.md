_[[Configuration examples]] Indirect expression file/foldername_

The following example shows how to configure a direct or [[indirect expression]] to make a foldername, that can be used to read e.g. multiple files from a data source like the BAG. 

The main difference between the configuration of the item _folder_direct_expression_ versus _folder_indirect_expression_ is that the calculation rule for the _folder_indirect_expression_ is evaluated first to a direct expression, before the actual data is calculated. In the [[GeoDMS GUI]] the resulting direct calculation rule is presented, in this case just the string: '20220101'. 

The advantage of the indirect expression is that the then or else part of the if function does not have to be evaluated/calculated as it is not a part of the resulting direct expression anymore. This is mainly an advantage if the calculation of this part is time or memory consuming.  

## configuration

<pre>
container in_direct_expressions
{
   parameter&lt;string&gt; date       := '2022';
   parameter&lt;bool&gt;   isDate2022 := date == '2022';

   parameter&lt;string&gt; folder_direct_expression   :=   isDate2022 ? '20220101' : '20230101';
   parameter&lt;string&gt; folder_indirect_expression := = isDate2022 ? '''20220101''' : '''20230101''';

   unit&lt;uint32&gt; bag_pand_direct_expression
   : StorageName = "= 'D:/SourceData/BAG20/snapshots/' + folder_direct_expression + '/pand.fss'"
   {
      attribute&lt;string&gt; identificatie;
   }
   unit&lt;uint32&gt; bag_pand_indirect_expression
   : StorageName = "= 'D:/SourceData/BAG20/snapshots/' + folder_indirect_expression + '/pand.fss'"
   {
      attribute&lt;string&gt; identificatie;
   }
}