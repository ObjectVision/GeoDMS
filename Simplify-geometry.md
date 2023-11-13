*[[Configuration examples]] Simplify geometry*

The script presents an example how you can **simplify the geometry of a polygon**. It will delete convex points from the geometry. Compare it with putting a rubber band around the shape, see also [convex hull](https://en.wikipedia.org/wiki/Convex_hull)

To what extent the simplifying is executed depends on the amount of iterations implemented.

*Tip: use the [[sequence2points]] function and take the maximum value for ordinal, you'll get the maximum amount of points for a single [[polygon]]. This is the theoretical maximum amount of iterations necessary.*

Since GeoDMS version 7.408 you can use the [[bg_simplify_polygon]] function to simplify a [[polygon]] and since 8.031 the [[bg_simplify_linestring]] function to simplify an [[arc]].

We advice to use these functions, the following example can still be used but is not advised.

## example

<pre>
container Simplified := Simplify(iter_domain, 50);  <I>// domain and number of iterations </I>
 
template Simplify
{
   <I>// begin case parameters</I>
   unit&lt;uint32&gt; iter_domain;
   parameter&lt;uint32&gt; nrIterations;
   <I>// end case parameters</I>
   
   unit&lt;uint32&gt; iter := range(uint32, 0, nrIterations)
   {
      attribute&lt;string&gt; name     := 'I' + string(id(.));
      attribute&lt;string&gt; PrevName := MakeDefined(name[id(.)-1], 'StartingState');
   }

   container StartingState 
   {
      unit&lt;uint32&gt; NextValue := sequence2points(iter_domain/geometry_rd);
   }

   container Iters     := for_each_ne(Iter/name, 'IterT('+Iter/PrevName+', iter_domain)');
   container LastIterI := =last('Iters/'+Iter/name);
           
  template IterT 
  {
       &lt;I&gt;//  begin case parameters&lt;/I&gt;
       container PrevIterator;
       unit&lt;uint32&gt; domain;
       &lt;I&gt;//  end case parameters&lt;/I&gt;

       unit&lt;uint32&gt;  PrevValue   := PrevIterator/NextValue;
       container     ConvexHullT := MakeConvexHullT(domain, PrevValue);
       unit&lt;uint32&gt;  NextValue   := ConvexHullT/sequence/Convex_hull_points;
  }

  container MakeFinal 
   {
      unit &lt;uint32&gt; domain_with_endpoints := union_unit(iter_domain, LastIter/NextValue)
      {
         attribute&lt;geometries/rdc&gt; point := 
           union_data(., LastIter/NextValue/point, first(LastIter/NextValue/point, LastIter/NextValue/SequenceNr));
         attribute&lt;uint32&gt;         SequenceNr := union_data(., LastIter/NextValue/SequenceNr, ID(Iteratie_domain));
      }
      attribute&lt;geometries/rdc&gt; convex_hull (iter_domain, poly) := 
         points2sequence(domain_with_endpoints /point, domain_with_endpoints /SequenceNr);
    }
   
  template MakeConvexHullT
   {
       &lt;I&gt;//  begin case parameters&lt;/I&gt;
        unit&lt;uint32&gt; domain;
        unit&lt;uint32&gt; seq;
       &lt;I&gt;//  end case parameters&lt;/I&gt;

        unit&lt;uint32&gt; sequence := seq
        {
           attribute&lt;geometries/rdc&gt;  point              := seq/point;
           attribute&lt;uint32&gt;          SequenceNr         := seq/SequenceNr;

           attribute&lt;uint32&gt;          min_index (domain) := min_index(id(.), SequenceNr);
           attribute&lt;uint32&gt;          max_index (domain) := max_index(id(.), SequenceNr);

           attribute&lt;bool&gt;            IsFirst  := id(.) == min_index[SequenceNr];
           attribute&lt;bool&gt;            IsLast   := id(.) == max_index[SequenceNr];
           attribute&lt;uint32&gt;          prev_id  := IsFirst ? rjoin(SequenceNr, id(domain), max_index) : ID(.)-1;
           attribute&lt;uint32&gt;          next_id  := Islast  ? rjoin(SequenceNr, id(domain), min_index) : ID(.)+1;
           attribute&lt;geometries/rdc&gt;  A        := point[prev_id];
           attribute&lt;geometries/rdc&gt;  B        := point;
           attribute&lt;geometries/rdc&gt;  C        := point[next_id];
           attribute&lt;geometries/rdc&gt;  p        := B - A;
           attribute&lt;geometries/rdc&gt;  q        := C - B;

           attribute&lt;float32&gt;         Px       := pointcol(P);
           attribute&lt;float32&gt;         Py       := pointrow(P);
           attribute&lt;float32&gt;         Qx       := pointcol(Q);
           attribute&lt;float32&gt;         Qy       := pointrow(Q);
           
           attribute&lt;float32&gt;         det      := Px*Qy - Py*Qx;
           
           unit&lt;uint32&gt; Convex_hull_points := 
              select_with_org_rel(det &lt;= 0f && (point != rjoin(prev_id, id(.), point)))
           {
              attribute&lt;geometries/rdc&gt; point       := ../point[org_rel];
              attribute&lt;uint32&gt;         SequenceNr  := ../SequenceNr[org_rel];
           }
      }
   }
}
</pre>

## see also:

- [[bg_simplify_polygon]]
