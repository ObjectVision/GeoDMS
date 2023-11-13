The [[TableChopper|TableChopper (read ascii file)]] can be used to read ASCII files, both with a **point** (default) and a **comma** as decimal separator.

The next example script assumes a point as decimal separator.

<pre>
container data := 
   for_each_nedv(
       Field/Name
      ,'ReadElems(
          BodyLines
         ,string
         ,'+ MakeDefined(
             Field/Name[ID(Field)-1] + '/ReadPos'
            ,'const(0, Domain)'
         ) + '
      )'
      ,Domain
      ,string
   );
</pre>
To read files with a comma as separator, edit the following code:

<pre>
container data :=
   for_each_nedv(
        Field/Name
       ,'ReadElems(
           BodyLines
          ,string
          ,'+MakeDefined(
              Field/Name[ID(Field)-1] + '/ReadPos'
             ,'const(0,Domain)'
          )+',1
       )'
       ,Domain
       ,string
  );
</pre>

Be aware of the last [[argument]] of the [[ReadElems]] function (in bold and italic). This last argument is optional, with two possible
values:
- 0 (default value): indicating a point is considered as decimal separator.
- 1: indicating a comma is considered as decimal separator.