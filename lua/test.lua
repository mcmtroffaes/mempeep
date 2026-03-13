local T = require("mempeep.T")

assert(T.array(T.i32, 5)["kind"] == "array", "unexpected kind for T.array")
assert(T.array(T.i32, 5)["type_ref"] == T.i32, "unexpected type_ref for T.array")
assert(T.array(T.i32, 5)["count"] == 5, "unexpected count for T.array")
assert(T.circular_list(T.struct("Item"))["kind"] == "circular_list", "unexpected kind for T.circular_list")
assert(T.i8["kind"] == "i8", "unexpected kind for T.i8")
assert(T.optional_ref(T.i32)["kind"] == "ptr", "unexpected kind for T.optional_ref")
assert(T.optional_ref(T.i32)["type_ref"] == T.i32, "unexpected ref_type for T.optional_ref")
assert(T.struct("Item")["kind"] == "struct", "unexpected kind for T.struct")
assert(T.struct("Item")["name"] == "Item", "unexpected name for T.struct")
assert(T.vector(T.i32)["kind"] == "vector", "unexpected kind for T.vector")
assert(T.vector(T.i32)["type_ref"] == T.i32, "unexpected type_ref for T.vector")

