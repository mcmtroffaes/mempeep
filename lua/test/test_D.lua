local T = require("mempeep.T")
local D = require("mempeep.D")

local foo_struct = D.struct("Foo", {
  D.field("x", T.i32),
  D.pad(4),
  D.field("y", T.i32),
  D.offset(24),
  D.field("z", T.f32),
})
D.assert_valid({ foo_struct })

-- D: struct field referencing another struct
local bar_struct = D.struct("Bar", {
  D.field("foo", T.struct("Foo")),
})
D.assert_valid({ foo_struct, bar_struct })

-- D: circular_list happy path
local node_struct = D.struct("Node", {
  D.field("value", T.i32),
  D.field("next", T.ptr(T.struct("Node"))),
})
D.assert_valid({ node_struct })

local container_struct = D.struct("Container", {
  D.field("nodes", T.circular_list(T.struct("Node"))),
})
D.assert_valid({ node_struct, container_struct })

-- D: unknown struct reference
local ok, err = pcall(D.assert_valid, {
  D.struct("Bad", { D.field("x", T.struct("Ghost")) }),
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list referencing unknown struct
local ok, err = pcall(D.assert_valid, {
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("Ghost"))) }),
})
assert(not ok)
assert(err:find("Ghost"), err)

-- D: circular_list next field missing
local ok, err = pcall(D.assert_valid, {
  D.struct("NoNext", { D.field("value", T.i32) }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("NoNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next field is not a ptr
local ok, err = pcall(D.assert_valid, {
  D.struct("BadNext", {
    D.field("value", T.i32),
    D.field("next", T.i32),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("BadNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next ptr points to wrong struct
local ok, err = pcall(D.assert_valid, {
  D.struct("Other", { D.field("v", T.i32) }),
  D.struct("WrongNext", {
    D.field("v", T.i32),
    D.field("next", T.ptr(T.struct("Other"))),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("WrongNext"))) }),
})
assert(not ok)
assert(err:find("WrongNext"), err)

-- D: field with invalid type_ref propagates T.assert_valid error
local ok, err = pcall(D.assert_valid, {
  D.struct("Bad", { D.field("x", { kind = "unknown" }) }),
})
assert(not ok)

-- D: struct missing descriptors field
local ok, err = pcall(D.assert_valid, { { name = "Bad" } })
assert(not ok)

-- D: circular_list next T.ref: fails
local ok, err = pcall(D.assert_valid, {
  D.struct("RefNext", {
    D.field("value", T.i32),
    D.field("next", T.ref(T.struct("RefNext"))),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("RefNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ptr: fails
local ok, err = pcall(D.assert_valid, {
  D.struct("OptPtrNext", {
    D.field("value", T.i32),
    D.field("next", T.optional_ptr(T.struct("OptPtrNext"))),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptPtrNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next T.optional_ref: fails
local ok, err = pcall(D.assert_valid, {
  D.struct("OptRefNext", {
    D.field("value", T.i32),
    D.field("next", T.optional_ref(T.struct("OptRefNext"))),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptRefNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)
