*[[Unit functions]] GetProjectionFactor*

## syntax

- *GetProjectionFactor(*gridunit*)*

## definition

GetProjectionFactor(*gridunit*) results in a [[parameter]] with a dpoint [[value type]], indicating the gridsize in both X and Y directions.

The grid size is expressed in the [[unit]] of the [[coordinate system|Grid Domain]].

## description

The GetProjectionFactor function in combination with the [[GetProjectionFactor]] function was used to calculate [[grid]] [[relations|relation]] for a set of points.

Since GeoDMS version 7.015 this is done with the [[value]] function, see [[Configuration examples]] Grid.

## applies to

- unit *gridunit* with Point value type of the group CanBeDomainUnit

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

unit&lt;fpoint&gt; projFactor := <B>GetProjectionFactor(</B>rdc_100<B>)</B>;
</pre>

*result: projFactor = [(-100.0, 100.0)]*.

## see also

- [[GetProjectionBase]]
- [[GetProjectionOffset]]