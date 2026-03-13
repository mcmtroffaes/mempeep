local T = require("mempeep.T")

local flt = T.assert_valid(T.float())
assert(flt.kind == "float")
local i8 = T.assert_valid(T.i8())
assert(i8.kind == "i8")
local i16 = T.assert_valid(T.i16())
assert(i16.kind == "i16")
local i32 = T.assert_valid(T.i32())
assert(i32["kind"] == "i32")
local i64 = T.assert_valid(T.i64())
assert(i64.kind == "i64")

local arr = T.assert_valid(T.array(i32, 5))
assert(arr["kind"] == "array")
assert(arr["type_ref"] == i32)
assert(arr["count"] == 5)

local stru = T.assert_valid(T.struct("Item"))
assert(stru["kind"] == "struct")
assert(stru["name"] == "Item")

local circ = T.assert_valid(T.circular_list(stru))
assert(circ["kind"] == "circular_list")
assert(circ["type_ref"] == stru)

local opt_ptr = T.assert_valid(T.optional_ptr(i32))
assert(opt_ptr["kind"] == "ptr")
assert(opt_ptr["type_ref"] == i32)
assert(opt_ptr["optional"] == true)
assert(opt_ptr["weak"] == true)

local opt_ref = T.assert_valid(T.optional_ref(i32))
assert(opt_ref["kind"] == "ptr")
assert(opt_ref["type_ref"] == i32)
assert(opt_ref["optional"] == true)
assert(opt_ref["weak"] == false)

local ptr = T.assert_valid(T.ptr(i32))
assert(ptr["kind"] == "ptr")
assert(ptr["type_ref"] == i32)
assert(ptr["optional"] == false)
assert(ptr["weak"] == true)

local ref = T.assert_valid(T.ref(i32))
assert(ref["kind"] == "ptr")
assert(ref["type_ref"] == i32)
assert(ref["optional"] == false)
assert(ref["weak"] == false)

local str = T.assert_valid(T.string(32))
assert(str["kind"] == "string")
assert(str["max_length"] == 32)

local vec = T.assert_valid(T.vector(i32))
assert(vec["kind"] == "vector")
assert(vec["type_ref"] == i32)

local ok, err = pcall(T.assert_valid, {})
assert(not ok)

local ok, err = pcall(T.assert_valid, T.array(i32, 0))
assert(not ok)

local ok, err = pcall(T.assert_valid, T.string(0))
assert(not ok)

local ok, err = pcall(T.assert_valid, {kind = "unknown"})
assert(not ok)

local ok, err = pcall(T.assert_valid, T.circular_list(T.i32()))
assert(not ok)
