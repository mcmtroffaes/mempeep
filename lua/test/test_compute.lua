local T = require("mempeep.T")
local D = require("mempeep.D")
local compute = require("mempeep.compute")

-- sizeof: scalars
local c = compute.new(4, {})
assert(c.sizeof(T.i8) == 1)
assert(c.sizeof(T.i16) == 2)
assert(c.sizeof(T.i32) == 4)
assert(c.sizeof(T.i64) == 8)
assert(c.sizeof(T.float) == 4)

-- sizeof: strings
assert(c.sizeof(T.string(16)) == 16)
assert(c.sizeof(T.string(1)) == 1)

-- sizeof: pointer-sized types, pointer_size = 4
local c4 = compute.new(4, {})
assert(c4.sizeof(T.ptr(T.i32)) == 4)
assert(c4.sizeof(T.ref(T.i32)) == 4)
assert(c4.sizeof(T.optional_ptr(T.i32)) == 4)
assert(c4.sizeof(T.optional_ref(T.i32)) == 4)
assert(c4.sizeof(T.circular_list(T.struct("X"))) == 4)
assert(c4.sizeof(T.vector(T.i32)) == 8)

-- sizeof: pointer-sized types, pointer_size = 8
local c8 = compute.new(8, {})
assert(c8.sizeof(T.ptr(T.i32)) == 8)
assert(c8.sizeof(T.ref(T.i32)) == 8)
assert(c8.sizeof(T.optional_ptr(T.i32)) == 8)
assert(c8.sizeof(T.optional_ref(T.i32)) == 8)
assert(c8.sizeof(T.circular_list(T.struct("X"))) == 8)
assert(c8.sizeof(T.vector(T.i32)) == 16)

-- sizeof: arrays
local c = compute.new(4, {})
assert(c.sizeof(T.array(T.i32, 5)) == 20)
assert(c.sizeof(T.array(T.i8(), 8)) == 8)
assert(c.sizeof(T.array(T.i64(), 3)) == 24)

-- sizeof: inline struct
local point = D.struct("Point", {
  D.field("x", T.i32),
  D.field("y", T.i32),
})
local c = compute.new(4, { point })
assert(c.sizeof(T.struct("Point")) == 8)

-- sizeof: inline struct with pad
local padded = D.struct("Padded", {
  D.field("x", T.i32),
  D.pad(4),
  D.field("y", T.i32),
})
local c = compute.new(4, { padded })
assert(c.sizeof(T.struct("Padded")) == 12)

-- sizeof: inline struct with offset
local with_offset = D.struct("WithOffset", {
  D.field("x", T.i32),
  D.offset(16),
  D.field("y", T.i32),
})
local c = compute.new(4, { with_offset })
assert(c.sizeof(T.struct("WithOffset")) == 20)

-- sizeof: nested inline struct
local inner = D.struct("Inner", {
  D.field("a", T.i32),
  D.field("b", T.i32),
})
local outer = D.struct("Outer", {
  D.field("inner", T.struct("Inner")),
  D.field("c", T.i32),
})
local c = compute.new(4, { inner, outer })
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
  local b = D.struct("B", { D.field("x", T.i32) })
  compute.new(4, { a, b })
end)
assert(not ok)
assert(err:find("B"), err)

-- sizeof: unknown type kind
local ok, err = pcall(c.sizeof, { kind = "unknown" })
assert(not ok)
assert(err:find("unknown"), err)

-- fields: basic
local point = D.struct("Point", {
  D.field("x", T.i32),
  D.field("y", T.i32),
})
local c = compute.new(4, { point })
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
  D.field("x", T.i32),
  D.pad(4),
  D.field("y", T.i32),
})
local c = compute.new(4, { padded })
local f = c.fields("Padded")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].offset == 8)

-- fields: offset directive is excluded
local with_offset = D.struct("WithOffset", {
  D.field("x", T.i32),
  D.offset(16),
  D.field("y", T.i32),
})
local c = compute.new(4, { with_offset })
local f = c.fields("WithOffset")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].offset == 16)

-- fields: pointer-typed field
local node = D.struct("Node", {
  D.field("value", T.i32),
  D.field("next", T.ptr(T.struct("Node"))),
})
local c = compute.new(4, { node })
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
      D.field("x", T.i32),
      D.offset(2),
      D.field("y", T.i32),
    }),
  })
end)
assert(not ok)
assert(err:find("forwards"), err)
