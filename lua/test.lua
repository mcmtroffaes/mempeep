local T = require("mempeep.T")

local arr = T.array(T.i32, 5)
T.validate(arr)
assert(arr["kind"] == "array", "unexpected kind for T.array")
assert(arr["type_ref"] == T.i32, "unexpected type_ref for T.array")
assert(arr["count"] == 5, "unexpected count for T.array")

local stru = T.struct("Item")
T.validate(stru)
assert(stru["kind"] == "struct", "unexpected kind for T.struct")
assert(stru["name"] == "Item", "unexpected name for T.struct")

local circ = T.circular_list(stru)
T.validate(circ)
assert(circ["kind"] == "circular_list", "unexpected kind for T.circular_list")
assert(circ["type_ref"] == stru, "unexpected type_ref for T.circular_list")

assert(T.i8["kind"] == "i8", "unexpected kind for T.i8")

local opt_ref = T.optional_ref(T.i32)
T.validate(opt_ref)
assert(opt_ref["kind"] == "ptr", "unexpected kind for T.optional_ref")
assert(opt_ref["type_ref"] == T.i32, "unexpected ref_type for T.optional_ref")

local vec = T.vector(T.i32)
T.validate(vec)
assert(vec["kind"] == "vector", "unexpected kind for T.vector")
assert(vec["type_ref"] == T.i32, "unexpected type_ref for T.vector")

