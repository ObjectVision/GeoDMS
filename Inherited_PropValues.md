*[[MetaScript functions]] Inherited_PropValues*

## syntax

- Inherited_PropValues(*item*, *property*)
- Inherited_PropValues(*item*, *attribute*)

## definition

- Inherited_PropValues(*item*, *property*) results in a new uint32 [[domain unit]] with a string [[attribute]] containing the property values of the direct [[subitems|subitem]] of the *item* [[argument]] and of all referred items in the namespace. The name of this [[attribute]] is the name of the [[property]].
- Inherited_PropValues(*item*, *attribute*) results in a new uint32 domain unit with a set of string attributes containing multiple property values of the direct subitems of the *item* argument and of all referred items in the namespace. The names of the resulting attributes are the names of the properties.

## applies to

- [[argument]] *[[item|tree item]]* can be any tree item.
- [[argument]] *property* needs to be a valid property name, the list of all properties can be found here.
- [[argument]] *attribute* needs to a string [[data item]].

## since version

7.102

## example

<pre>
1.
unit&lt;uint32&gt; Region: nrofrows = 5;
{
   attribute&lt;uint32&gt; RegionNr         : [0,1,2,3,4];
   attribute&lt;string&gt; MetaScriptName   : ['NH','ZH','UT','NB','GE'];
   attribute&lt;uint32&gt; sumNrInhabitants : [550,1025,300,200,0];
   attribute&lt;string&gt; RegionLabel      : ['hs: Ams','hs: DB','hs: Ut','hs:DH',null];
   attribute&lt;string&gt; RegionDescr      : ['Texel..Gooi','Carnaval','Dom',null,null];
}
unit&lt;uint32&gt; PropValueSource := Region
{
   parameter&lt;string&gt; label            : ['ABC'];
   parameter&lt;uint32&gt; sumNrInhabitants : [2,5,3,2,0];

   unit&lt;uint32&gt;      domainA: nrofrows = 9;
   unit&lt;float64&gt;     valuesB := baseunit('b', float64);

   container sublevel
   {
      attribute&lt;uint32&gt; meanNrInhabitants (PropValueSource): [1,2,1,1,0];
   }
}

unit&lt;uint32&gt; name := <B>Inherited_PropValues(</B>PropValueSource,'name'<B>)</B>;
unit&lt;uint32&gt; expr := <B>Inherited_PropValues(</B>PropValueSource,'expr'<B>)</B>;
</pre>

<pre>
2.
unit&lt;uint32&gt; property : nrofrows = 2
{
   parameter&lt;string&gt; name: ['name','expr'];
}
unit&lt;uint32&gt; name_and_expr := <B>Inherited_PropValues(</B>PropValueSource, property/name<B>)</B>;
</pre>

| name             |
|------------------|
| label            |
| sumNrInhabitants |
| domainA          |
| valuesB          |
| sublevel         |
| RegionNr         |
| MetaScriptName   |
| sumNrInhabitants |
| RegionLabel      |
| RegionDescr      |

*domain name, nr of rows = 10*

| expr                  |
|-----------------------|
|                       |
|                       |
|                       |
| baseunit('b',float64) |
|                       |
|                       |
|                       |
|                       |
|                       |
|                       |

*domain expr, nr of rows = 10*

| name             | expr                  |
|------------------|-----------------------|
| label            |                       |
| sumNrInhabitants |                       |
| domainA          |                       |
| valuesB          | baseunit('b',float64) |
| sublevel         |                       |
| RegionNr         |                       |
| MetaScriptName   |                       |
| sumNrInhabitants |                       |
| RegionLabel      |                       |
| RegionDescr      |                       |

*domain name_and_expr, nr of rows = 10*

## See Also

- [[PropValue]]
- [[SubItem_PropValues]]
- [[SubTree_PropValues]]