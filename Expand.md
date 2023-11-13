*[[String functions]] expand*

## syntax

- expand(*item*, *placeholderString*)

## definition

expand(*item*, *placeholderString*) results in a string [[parameter]] with the **expanded value** of the *placeholderString* in the context of the *item* [[argument]].

## description

Expansion means placeholders are replaced by (sub)folders on the local machine. See [[folders and placeholders]] for more information on placeholders.

## applies to

- [[tree item]]
- string [[parameter]] *placeholderString*

## since version

5.60

## example

<pre>
parameter&lt;string&gt; LocalDataProjDir := <B>expand(</B>. ,'%localDataProjDir%'<B>)</B>;
</pre>

*result: LocalDataDir = 'C:/LocalData/operator' (or based on the value of another %localDataProjDir% configured)*