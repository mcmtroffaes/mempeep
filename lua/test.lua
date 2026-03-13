-- ----------------------------------------------------------------------------
-- mempeep.T
-- ----------------------------------------------------------------------------

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

-- ----------------------------------------------------------------------------
-- mempeep.D
-- ----------------------------------------------------------------------------

local D = require("mempeep.D")

local foo_struct = D.struct("Foo", {
    D.field("x", T.i32()),
    D.pad(4),
    D.field("y", T.i32()),
    D.offset(24),
    D.field("z", T.float()),
})
D.assert_valid({foo_struct})

-- D: struct field referencing another struct
local bar_struct = D.struct("Bar", {
    D.field("foo", T.struct("Foo")),
})
D.assert_valid({foo_struct, bar_struct})

-- D: circular_list happy path
local node_struct = D.struct("Node", {
    D.field("value", T.i32()),
    D.field("next", T.ptr(T.struct("Node"))),
})
D.assert_valid({node_struct})

local container_struct = D.struct("Container", {
    D.field("nodes", T.circular_list(T.struct("Node"))),
})
D.assert_valid({node_struct, container_struct})

-- D: unknown struct reference
local ok, err = pcall(D.assert_valid, {
    D.struct("Bad", { D.field("x", T.struct("Ghost")) })
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list referencing unknown struct
local ok, err = pcall(D.assert_valid, {
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("Ghost"))) })
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list next field missing
local ok, err = pcall(D.assert_valid, {
    D.struct("NoNext", { D.field("value", T.i32()) }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("NoNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next field is not a ptr
local ok, err = pcall(D.assert_valid, {
    D.struct("BadNext", {
        D.field("value", T.i32()),
        D.field("next", T.i32()),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("BadNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next ptr points to wrong struct
local ok, err = pcall(D.assert_valid, {
    D.struct("Other", { D.field("v", T.i32()) }),
    D.struct("WrongNext", {
        D.field("v", T.i32()),
        D.field("next", T.ptr(T.struct("Other"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("WrongNext"))) })
})
assert(not ok)
assert(err:find("WrongNext"), err)

-- D: field with invalid type_ref propagates T.assert_valid error
local ok, err = pcall(D.assert_valid, {
    D.struct("Bad", { D.field("x", {kind = "unknown"}) })
})
assert(not ok)

-- D: struct missing descriptors field
local ok, err = pcall(D.assert_valid, { {name = "Bad"} })
assert(not ok)

-- D: circular_list next T.ref: fails
local ok, err = pcall(D.assert_valid, {
    D.struct("RefNext", {
        D.field("value", T.i32()),
        D.field("next", T.ref(T.struct("RefNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("RefNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ptr: fails
local ok, err = pcall(D.assert_valid, {
    D.struct("OptPtrNext", {
        D.field("value", T.i32()),
        D.field("next", T.optional_ptr(T.struct("OptPtrNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptPtrNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ref: fails
local ok, err = pcall(D.assert_valid, {
    D.struct("OptRefNext", {
        D.field("value", T.i32()),
        D.field("next", T.optional_ref(T.struct("OptRefNext"))),
    }),
    D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptRefNext"))) })
})
assert(not ok)
assert(err:find("next"), err)

-- ----------------------------------------------------------------------------
-- mempeep.compute
-- ----------------------------------------------------------------------------

local T = require("mempeep.T")
local D = require("mempeep.D")
local compute = require("mempeep.compute")

-- sizeof: scalars

local c = compute.new(4, {})
assert(c.sizeof(T.i8())    == 1)
assert(c.sizeof(T.i16())   == 2)
assert(c.sizeof(T.i32())   == 4)
assert(c.sizeof(T.i64())   == 8)
assert(c.sizeof(T.float()) == 4)

assert(c.sizeof(T.string(16)) == 16)
assert(c.sizeof(T.string(1))  == 1)

-- sizeof: pointer-sized types, pointer_size = 4

local c4 = compute.new(4, {})
assert(c4.sizeof(T.ptr(T.i32()))           == 4)
assert(c4.sizeof(T.ref(T.i32()))           == 4)
assert(c4.sizeof(T.optional_ptr(T.i32()))  == 4)
assert(c4.sizeof(T.optional_ref(T.i32()))  == 4)
assert(c4.sizeof(T.circular_list(T.struct("X"))) == 4)
assert(c4.sizeof(T.vector(T.i32()))        == 8)

-- sizeof: pointer-sized types, pointer_size = 8

local c8 = compute.new(8, {})
assert(c8.sizeof(T.ptr(T.i32()))           == 8)
assert(c8.sizeof(T.ref(T.i32()))           == 8)
assert(c8.sizeof(T.optional_ptr(T.i32()))  == 8)
assert(c8.sizeof(T.optional_ref(T.i32()))  == 8)
assert(c8.sizeof(T.circular_list(T.struct("X"))) == 8)
assert(c8.sizeof(T.vector(T.i32()))        == 16)

-- sizeof: arrays

local c = compute.new(4, {})
assert(c.sizeof(T.array(T.i32(), 5))   == 20)
assert(c.sizeof(T.array(T.i8(),  8))   == 8)
assert(c.sizeof(T.array(T.i64(), 3))   == 24)

-- sizeof: inline struct

local point = D.struct("Point", {
    D.field("x", T.i32()),
    D.field("y", T.i32()),
})
local c = compute.new(4, {point})
assert(c.sizeof(T.struct("Point")) == 8)

-- sizeof: inline struct with pad
local padded = D.struct("Padded", {
    D.field("x", T.i32()),
    D.pad(4),
    D.field("y", T.i32()),
})
local c = compute.new(4, {padded})
assert(c.sizeof(T.struct("Padded")) == 12)

-- sizeof: inline struct with offset
local with_offset = D.struct("WithOffset", {
    D.field("x", T.i32()),
    D.offset(16),
    D.field("y", T.i32()),
})
local c = compute.new(4, {with_offset})
assert(c.sizeof(T.struct("WithOffset")) == 20)

-- sizeof: nested inline struct
local inner = D.struct("Inner", {
    D.field("a", T.i32()),
    D.field("b", T.i32()),
})
local outer = D.struct("Outer", {
    D.field("inner", T.struct("Inner")),
    D.field("c",     T.i32()),
})
local c = compute.new(4, {inner, outer})
assert(c.sizeof(T.struct("Inner")) == 8)
assert(c.sizeof(T.struct("Outer")) == 12)

-- sizeof: unknown struct errors

local c = compute.new(4, {})
local ok, err = pcall(c.sizeof, T.struct("Ghost"))
assert(not ok)
assert(err:find("Ghost"), err)

-- sizeof: unresolved struct (forward reference)
local ok, err = pcall(function()
    local a = D.struct("A", { D.field("b", T.struct("B")) })
    local b = D.struct("B", { D.field("x", T.i32()) })
    compute.new(4, {a, b})
end)
assert(not ok)
assert(err:find("B"), err)

-- sizeof: unknown type kind
local ok, err = pcall(c.sizeof, {kind = "unknown"})
assert(not ok)
assert(err:find("unknown"), err)

-- fields: basic

local point = D.struct("Point", {
    D.field("x", T.i32()),
    D.field("y", T.i32()),
})
local c = compute.new(4, {point})
local f = c.fields("Point")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].type_ref.kind == "i32")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].type_ref.kind == "i32")
assert(f[2].offset == 4)

-- fields: pad is excluded
local padded = D.struct("Padded", {
    D.field("x", T.i32()),
    D.pad(4),
    D.field("y", T.i32()),
})
local c = compute.new(4, {padded})
local f = c.fields("Padded")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].offset == 8)

-- fields: offset directive is excluded
local with_offset = D.struct("WithOffset", {
    D.field("x", T.i32()),
    D.offset(16),
    D.field("y", T.i32()),
})
local c = compute.new(4, {with_offset})
local f = c.fields("WithOffset")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].offset == 16)

-- fields: pointer-typed field
local node = D.struct("Node", {
    D.field("value", T.i32()),
    D.field("next",  T.ptr(T.struct("Node"))),
})
local c = compute.new(4, {node})
local f = c.fields("Node")
assert(#f == 2)
assert(f[1].name == "value")
assert(f[1].offset == 0)
assert(f[2].name == "next")
assert(f[2].offset == 4)

-- fields: unknown struct errors
local c = compute.new(4, {})
local ok, err = pcall(c.fields, "Ghost")
assert(not ok)
assert(err:find("Ghost"), err)

-- D.offset backwards error
local ok, err = pcall(function()
    compute.new(4, {
        D.struct("Bad", {
            D.field("x", T.i32()),
            D.offset(2),
            D.field("y", T.i32()),
        })
    })
end)
assert(not ok)
assert(err:find("backwards"), err)
print("all tests passed")