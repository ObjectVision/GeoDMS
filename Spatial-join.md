
To sum population per origin over all destiantions within 120 km Euclidian:

```
unit<uint64> Combine_OrgDest := combine_unit_uint64(Orgs, Dests)
{
	attribute<Orgs>    Orgs_rel         := convert(id(.) / uint64(#Dests), Orgs);
	attribute<uint32>  Dests_rel        := convert(id(.) % uint64(#Dests), Dests);
	
	attribute<float64> Distance2        := sqrdist(Orgs/geometry[Orgs_rel], Dests/geometry[Dests_rel]);
	attribute<uint32>  NearbyPopulation := Dests/Population[Dests_rel] * uint32(Distance2 < sqr(120000d));
}

attribute<uint32> Population_within_120km (Orgs) := sum(Combine_OrgDest/NearbyPopulation, Combine_OrgDest/Orgs_rel);
```