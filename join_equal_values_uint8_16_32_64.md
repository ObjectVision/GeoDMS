*[[Relational functions]] join_equal_values_uint8, join_equal_values_uint16, join_equal_values_uint32, join_equal_values_uint64*

## syntax

- join_equal_values_uint8(first_rel, second_rel)
- join_equal_values_uint16(first_rel, second_rel)
- join_equal_values_uint32(first_rel, second_rel)
- join_equal_values_uint64(first_rel, second_rel)

## definition

These functions work in the same way as the [[join_equal_values]] function, only they result in [[domain unit|domain unit]] of respectively uint8, uint16,
uint32 or uint64 [[value types|value type]].