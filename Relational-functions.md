Relational [[functions|Operators and functions]] are used to relate [[data items|data item]] of different [[domain units|domain unit]] like [[lookup]] or [[rjoin]] or create new domain units like [[unique]]. 

Selection functions van be found on the separate page: [[Selection functions]].

- [[id]] - *index numbers*
- [[mapping]] - *to map attributes between domain units*

<!-- -->

- [[lookup]] - *using a relation to find the relevant entries*
- [[rlookup]] - *using a foreign key attribute to make a relation*
- [[rjoin]] - *using a foreign key to find the relevant entries, combining lookup and rlookup*
- [[join_equal_values]] - *find all combinations of A and B wherfe A.x_rel is equal to B.x_rel*
- [[join_equal_values_uint8_16_32_64]] - *find all combinations of A and B wherfe A.x_rel is equal to B.x_rel, resulting in a domain unit with the explicit value type*

<!-- -->

- [[index]] - *an index number based on a sort order*
- [[direct_index]]

<!-- -->

- [[invert]] - *inverts a relation*

<!-- -->
- [[collect_attr_by_org_rel]] - *collects a set of attributes to a new domain unit, using an org_rel attribute (lookup)*
- [[collect_attr_by_cond]] - *collects a set of attributes to a new domain unit, using a condition*
- [[collect_by_cond]] - *collects an attributes to a new domain unit, using a condition*
- [[recollect_by_cond]] -  *recollects attribute values of a selection back to an original set using the condition for the selection.*

<!-- -->
- [[unique]] - *configures a new unit, based on the unique values*
- [[unique_uint8_16_32_64]] - *configures a new unit, based on the unique values, with a uint8_16_32_64 values unit*
- [[union]] - *obsolete, only in use for backward compatibility*
- [[union_unit]] - *configures a new unit, based on the union of other units*
- [[union_unit_uint8_16_32_64]] - *configures a new unit, based on the union of other units, with a uint8_16_32_64 values unit*
- [[union_data]] - *configures a new attribute, based on the union of other data items*
- [[combine]] - *configures a new unit, based on the Cartesian product of other units*
- [[combine_unit_uint8_16_32_64]] - *configures a new unit, without subitems, based on the Cartesian product of other units, with a uint8_16_32_64 values unit*
- [[combine_uint8_16_32_64]] - *configures a new unit, based on the Cartesian product of other units, with a uint8_16_32_64 values unit*
- [[combine_data]] - *configures a new attribute, based on the Cartesian product of other data items*

<!-- -->

- [[merge]] - *merges the values of a data item, based on an option data item*

<!-- -->

- [[overlay]] *- an uint16 domain unit with the unique values of multiple grid attributes*
- [[overlay32]] *- an uint32 domain unit with the unique values of multiple grid attributes*
- [[overlay64]] *- an uint64 domain unit with the unique values of multiple grid attributes*
