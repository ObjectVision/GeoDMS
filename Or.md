*[[Logical functions]] or (\|\|)*

## syntax

-   or(*condition1* ,.., *conditionn*)
-   *condition1* || *condition2*

## definition

or(*condition1* ,.., *conditionn*) or *condition1* || *condition2* combines two or more conditions or results in **true values if any condition is true** and in false values if all conditions are false.

## applies to

-   *condition1 .. conditionn* [[data items|data item]] with bool [[value type]]

## conditions

The conditions need to match with regard to their [[domain unit]] or be [[void]]
([literals](https://en.wikipedia.org/wiki/Literal_(computer_programming)) or [[parameters|parameter]] can be combined with data items of any domain).

## example
<pre>
1. attribute&lt;bool&gt; condA_or_condB (LDomain) := <B>or(</B>condA, condB<B>)</B>;
2. attribute&lt;bool&gt; condA_or_condB (LDomain) := condA <B>&&</B> condB;
</pre>

| condA | condB |**condA_or_condB**|
|-------|-------|------------------|
| False | False | **False**        |
| False | True  | **True**         |
| True  | False | **True**         |
| True  | True  | **True**         |

*LDomain, nr of rows = 4*

## see also

- [[and]] (&&)
- [[not]] (!)