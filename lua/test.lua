local T = require("mempeep.T")

local i32 = T.i32()
assert(i32["kind"] == "i32")

local arr = T.array(i32, 5)
T.validate(arr)
assert(arr["kind"] == "array")
assert(arr["type_ref"] == i32)
assert(arr["count"] == 5)

local stru = T.struct("Item")
T.validate(stru)
assert(stru["kind"] == "struct")
assert(stru["name"] == "Item")

local circ = T.circular_list(stru)
T.validate(circ)
assert(circ["kind"] == "circular_list")
assert(circ["type_ref"] == stru)

local opt_ptr = T.optional_ptr(i32)
T.validate(opt_ptr)
assert(opt_ptr["kind"] == "ptr")
assert(opt_ptr["type_ref"] == i32)
assert(opt_ptr["optional"] == true)
assert(opt_ptr["weak"] == true)

local opt_ref = T.optional_ref(i32)
T.validate(opt_ref)
assert(opt_ref["kind"] == "ptr")
assert(opt_ref["type_ref"] == i32)
assert(opt_ref["optional"] == true)
assert(opt_ref["weak"] == false)

local ptr = T.ptr(i32)
T.validate(ptr)
assert(ptr["kind"] == "ptr")
assert(ptr["type_ref"] == i32)
assert(ptr["optional"] == false)
assert(ptr["weak"] == true)

local ref = T.ref(i32)
T.validate(ref)
assert(ref["kind"] == "ptr")
assert(ref["type_ref"] == i32)
assert(ref["optional"] == false)
assert(ref["weak"] == false)

local str = T.string(32)
T.validate(str)
assert(str["kind"] == "string")
assert(str["max_length"] == 32)

local vec = T.vector(i32)
T.validate(vec)
assert(vec["kind"] == "vector")
assert(vec["type_ref"] == i32)

local ok, err = pcall(T.validate, {})
assert(not ok)

local ok, err = pcall(T.validate, T.array(i32, 0))
assert(not ok)

local ok, err = pcall(T.validate, T.string(0))
assert(not ok)

local ok, err = pcall(T.validate, {kind = "unknown"})
assert(not ok)

local ok, err = pcall(T.validate, T.circular_list(T.i32()))
assert(not ok)

