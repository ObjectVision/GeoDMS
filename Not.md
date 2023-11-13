*[[Logical functions]] not (!)*

## syntax

- not(*condition*)
- !*condition*

## definition

not(*condition*) or !*condition* results in **true values if *condition* is false and vice versa**.

## applies to

- condition [[data item]] with bool [[value type]]

## example

<pre>
1. attribute&lt;bool&gt; notA (LDomain) := <B>not(</B>condA<B>)</B>;
2. attribute&lt;bool&gt; notA (LDomain) := <B>!</B>condA;
</pre>

| condA | **notA**  |
|-------|-----------|
| False | **True**  |
| True  | **False** |

*DDomain, nr of rows = 2*

## see also

- [[and]] (&&)
- [[or]] (||)