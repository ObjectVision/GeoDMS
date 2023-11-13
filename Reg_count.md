*[[Aggregation functions]] reg_count*

## syntax

- reg_count(*a*, *b*, *c*, *d*)

## definition

reg_count(*a*, *b*, *c*, *d*) results in a set of [[attributes|attribute]] with the **count** of the non [[null]] values of attribute *a*, for each value class in a named by attribute *b* per region towards the regional level defined in the string attribute *d*.

The labels in this string attribute *d* need to refer to the names of the [[relations|relation]] in [[container]] *c*.

The [[values unit]] for each resulting [[data item]] is uint32 and the [[domain units|domain unit]] depend on the values unit of the corresponding relations.

The *reg_count_uint16*/*reg_count_uint8* versions can be used in the same manner as the reg_count function, resulting in data items with a [[uint16]] or [[uint8]] values unit.

## applies to

- attribute *a* with uint32 [[value type]]
- attribute *b* with string value type
- container *c*
- attribute *d* with string value type

## conditions

1. The domain unit of attribute *a* must match with the domain of the attribute in container *c*.
2. The values unit of attribute *a* has to be the domain of attributes *b* and *d*.
3. The values of attribute *d* need to refer to [[subitem]] attributes in container *c*.
4. The value type of the attribute in container *c* must be of the group CanBeDomainUnit.

## since version

5.85

## example

<pre>
container reg_count
{
   container units
   {
      unit&lt;uint32&gt; Domain: nrofrows = 10;
      unit&lt;uint8&gt;  RegioA: nrofrows = 5;
      unit&lt;uint8&gt;  RegioB: nrofrows = 3;
      unit&lt;uint8&gt;  Class:  nrofrows = 4
      {
         attribute&lt;string&gt; label: ['Class1','Class2','Class3','Class4'];
      }
   }
   container regios
   {
      attribute&lt;units/RegioA&gt; A (units/Domain): [0,0,1,2,3,0,4,2,1,0];
      attribute&lt;units/RegioB&gt; B (units/Domain): [1,0,2,2,0,0,1,2,1,0];   
   }
   attribute&lt;units/Class&gt; src  (units/Domain): [0,1,3,2,0,1,2,1,0,3];
   attribute&lt;string&gt;      refs (units/Class):  ['A','A','B','A'];

   container attributes := 
      <B>reg_count</B>(
           src
         , units/Class/label
         , Regios
         , refs
     );
};
</pre>

| **attributes/class1** | **attributes/class2** | **attributes/class4** |
|----------------------:|----------------------:|----------------------:|
| **1**                 | **2**                 | **1**                 |
| **1**                 | **0**                 | **1**                 |
| **0**                 | **1**                 | **0**                 |
| **1**                 | **0**                 | **0**                 |
| **0**                 | **0**                 | **0**                 |

*domain RegioA, nr of rows = 5*

| **attributes/class3** |
|----------------------:|
| **0**                 |
| **1**                 |
| **1**                 |

*domain RegioB, nr of rows = 3*