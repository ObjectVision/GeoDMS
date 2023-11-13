*[[MetaScript functions]] SubTree_PropValues*

## syntax

- SubTree_PropValues(*item*, *property*)
- SubTree_PropValues(*item*, *attribute*)

## definition

- SubTree_PropValues(*item*, *property*) results in a new uint32 [[domain unit]] with a string [[attribute]] containing the [[property]] values of the [[subitems|subitem]] of the *item* [[argument]]. The name of this attribute is the name of the [[property]].
- SubTree_PropValues(*item*, *attribute*) results in a new uint32 domain unit with a set of string attributes containing multiple property values of the subitems of the *item* argument. The names of the resulting attributes are the names of the properties.

## applies to

- argument *[[item|tree item]]* can be any tree item.
- argument *property* needs to be a valid property name, the list of all properties can be found here.
- argument *attribute* needs to a string [[data item]].

## since version

7.103

## example

<pre>
1.
container PropValueSource
{
   parameter&lt;string&gt; label                     : ['ABC'];
   parameter&lt;uint32&gt; sumNrInhabitants (ADomain): [2,5,3,2,0];

   unit&lt;uint32&gt;  domainA: nrofrows = 9;
   unit&lt;float64&gt; valuesB:= baseunit('b',float64);
   container sublevel
   {
      attribute&lt;uint32&gt; meanNrInhabitants(PropValueSource): [1,2,1,1,0];
   }
}

unit&lt;uint32&gt; name := <B>SubTree_PropValues(</B>PropValueSource,'name'<B>)</B>;
unit&lt;uint32&gt; expr := <B>SubTree_PropValues(</B>PropValueSource,'Expr'<B>)</B>;
</pre>

<pre>
2.

unit&lt;uint32&gt; property : nrofrows = 2
{
   parameter&lt;string&gt; name: ['name','expr'];
}

unit&lt;uint32&gt; name_and_expr := <B>SubTree_PropValues(</B>PropValueSource, property/name<B>)</B>;
</pre>

| name              |
|-------------------|
| label             |
| sumNrInhabitants  |
| domainA           |
| valuesB           |
| sublevel          |
| meanNrInhabitants |

*domain name, nr of rows = 6*

| expr                  |
|-----------------------|
|                       |
|                       |
|                       |
| baseunit('b',float64) |
|                       |
|                       |

*domain expr, nr of rows = 6*

| name              | expr                  |
|-------------------|-----------------------|
| label             |                       |
| sumNrInhabitants  |                       |
| domainA           |                       |
| valuesB           | baseunit('b',float64) |
| sublevel          |                       |
| meanNrInhabitants |                       |

*domain name_and_expr, nr of rows = 6*

## see also

- [[PropValue]]
- [[SubItem_PropValues]]
- [[Inherited_PropValues]]