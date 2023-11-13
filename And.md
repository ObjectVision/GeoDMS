*[[Logical functions]] and (&&)*

## syntax

- and(*condition1* ,.., *conditionn*)
- *condition1* && *condition2*

## definition

and(*condition1* ,.., *conditionn*) or *condition1* && *condition2* combines two or more conditions and results in **true values if all conditions are true** and in false values if any condition is false.

## applies to

- *condition1 .. conditionn* [[data items|data item]] with bool [[value type]]

## conditions

The conditions need to match with regard to their [[domain unit]] or be [[void]] ([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be combined with data itemsof any [[domain|domain unit]]).

## example

<pre>
1. attribute&lt;bool&gt; condA_and_condB (LDomain) := <B>and(</B>condA, condB<B>)</B>;
2. attribute&lt;bool&gt; condA_and_condB (LDomain) := condA <B>&&</B> condB;
</pre>

| condA | condB |**condA_and_condB**|
|-------|-------|-------------------|
| False | False | **False**         |
| False | True  | **False**         |
| True  | False | **False**         |
| True  | True  | **True**          |

*LDomain, nr of rows = 4*

## see also

- [[or]] (||)
- [[not]] (!)