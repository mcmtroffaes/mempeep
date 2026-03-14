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

-- D: circular_list happy path (default next_field = "next")
local node_struct = D.struct("Node", {
  D.field("value", T.i32),
  D.field("next", T.ptr(T.struct("Node"), { weak = true })),
})
D.assert_valid({ node_struct })

local container_struct = D.struct("Container", {
  D.field("nodes", T.circular_list(T.struct("Node"))),
})
D.assert_valid({ node_struct, container_struct })

-- D: circular_list with explicit next_field
local sib_struct = D.struct("Sib", {
  D.field("value", T.i32),
  D.field("sibling", T.ptr(T.struct("Sib"), { weak = true })),
})
local sib_container = D.struct("SibContainer", {
  D.field("sibs", T.circular_list(T.struct("Sib"), "sibling")),
})
D.assert_valid({ sib_struct, sib_container })

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
    D.field("next", T.ptr(T.struct("Other"), { weak = true })),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("WrongNext"))) }),
})
assert(not ok)
assert(err:find("WrongNext"), err)

-- D: field with invalid type propagates T.assert_valid error
local ok, err = pcall(D.assert_valid, {
  D.struct("Bad", { D.field("x", { kind = "unknown" }) }),
})
assert(not ok)

-- D: struct missing descriptors field
local ok, err = pcall(D.assert_valid, { { name = "Bad" } })
assert(not ok)

-- D: circular_list next is a followed (non-weak) ptr: fails
local ok, err = pcall(D.assert_valid, {
  D.struct("RefNext", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("RefNext"))), -- weak=false, optional=false
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("RefNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next is optional weak ptr: fails (optional=true)
local ok, err = pcall(D.assert_valid, {
  D.struct("OptPtrNext", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("OptPtrNext"), { optional = true, weak = true })),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptPtrNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)

-- D: circular_list next is optional followed ptr: fails (optional=true)
local ok, err = pcall(D.assert_valid, {
  D.struct("OptRefNext", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("OptRefNext"), { optional = true })),
  }),
  D.struct("Bad", { D.field("xs", T.circular_list(T.struct("OptRefNext"))) }),
})
assert(not ok)
assert(err:find("next"), err)
