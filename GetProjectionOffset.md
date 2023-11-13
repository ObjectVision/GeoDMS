*[[Unit functions]] GetProjectionOffset*

## syntax

- *GetProjectionOffset(*gridunit*)*

## definition

GetProjectionOffset(*gridunit*) results in a [[parameter]] with a dpoint [[value type]], indicating the coordinate of the top left grid cell in the [[coordinate system|grid domain]].

## description

The GetProjectionOffset function, in combination with [[GetProjectionFactor]] was used to calculate [[grid]] [[relations|relation]] for a point [[vector|vector data]] domain.

Since GeoDMS version 7.015 this is done with the [[value]] function, see [[Configuration examples]] Grid.

## applies to

- [[unit]] *gridunit* with Point value type of the group CanBeDomainUnit

## since version

5.44

## example

<pre>
unit&lt;fpoint&gt; rdc_meter: range = "[{300000, 0}, {625000, 280000})";
unit&lt;spoint&gt; rdc_100 :=
   range(
      gridset(
          rdc_meter
         ,point(  -100f,   100f), rdc_meter)
         ,point(625000f, 10000f), rdc_meter)
         ,'spoint'
      ), point(0s,0s), point(3250s, 2700s)
   );

unit&lt;fpoint&gt; projBase := <B>GetProjectionOffset(</B>rdc_100<B>)</B>;
</pre>

*result: projOffSet = [(625000.0, 10000.0)]*.

## see also

- [[GetProjectionBase]]
- [[GetProjectionFactor]]