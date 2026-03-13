local T = require("mempeep.T")

local flt = T.float()
assert(flt.kind == "float")
local i8 = T.i8()
assert(i8.kind == "i8")
local i16 = T.i16()
assert(i16.kind == "i16")
local i32 = T.i32()
assert(i32["kind"] == "i32")
local i64 = T.i64()
assert(i64.kind == "i64")

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

local D = require("mempeep.D")

local foo_struct = D.struct("Foo", {
    D.field("x", T.i32()),
    D.pad(4),
    D.field("y", T.i32()),
    D.offset(24),
    D.field("z", T.float()),
})
D.validate({foo_struct})

-- D: struct field referencing another struct
local bar_struct = D.struct("Bar", {
    D.field("foo", T.struct("Foo")),
})
D.validate({foo_struct, bar_struct})

-- D: circular_list happy path
local node_struct = D.struct("Node", {
    D.field("value", T.i32()),
    D.field("next", T.ptr(T.struct("Node"))),
})
D.validate({node_struct})

local container_struct = D.struct("Container", {
    D.field("nodes", T.circular_list(T.struct("Node"))),
})
D.validate({node_struct, container_struct})

-- D: unknown struct reference
local ok, err = pcall(D.validate, {
    D.struct("Bad", { D.field("x", T.struct("Ghost")) })
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list referencing unknown struct
local ok, err = pcall(D.validate, {
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("Ghost"))) })
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list next field missing
local ok, err = pcall(D.validate, {
    D.struct("NoNext", { D.field("value", T.i32()) }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("NoNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next field is not a ptr
local ok, err = pcall(D.validate, {
    D.struct("BadNext", {
        D.field("value", T.i32()),
        D.field("next", T.i32()),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("BadNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next ptr points to wrong struct
local ok, err = pcall(D.validate, {
    D.struct("Other", { D.field("v", T.i32()) }),
    D.struct("WrongNext", {
        D.field("v", T.i32()),
        D.field("next", T.ptr(T.struct("Other"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("WrongNext"))) })
})
assert(not ok)
assert(err:find("WrongNext"), err)

-- D: field with invalid type_ref propagates T.validate error
local ok, err = pcall(D.validate, {
    D.struct("Bad", { D.field("x", {kind = "unknown"}) })
})
assert(not ok)

-- D: struct missing descriptors field
local ok, err = pcall(D.validate, { {name = "Bad"} })
assert(not ok)

-- D: circular_list next T.ref: fails
local ok, err = pcall(D.validate, {
    D.struct("RefNext", {
        D.field("value", T.i32()),
        D.field("next", T.ref(T.struct("RefNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("RefNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ptr: fails
local ok, err = pcall(D.validate, {
    D.struct("OptPtrNext", {
        D.field("value", T.i32()),
        D.field("next", T.optional_ptr(T.struct("OptPtrNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptPtrNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ref: fails
local ok, err = pcall(D.validate, {
    D.struct("OptRefNext", {
        D.field("value", T.i32()),
        D.field("next", T.optional_ref(T.struct("OptRefNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptRefNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

