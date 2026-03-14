local T = require("mempeep.T")
local D = require("mempeep.D")
local schema = require("mempeep.schema")

-- sizeof: scalars
local s = schema.new(4, {})
assert(s.sizeof(T.i8) == 1)
assert(s.sizeof(T.i16) == 2)
assert(s.sizeof(T.i32) == 4)
assert(s.sizeof(T.i64) == 8)
assert(s.sizeof(T.f32) == 4)
assert(s.sizeof(T.f64) == 8)

-- sizeof: strings
assert(s.sizeof(T.string(16)) == 16)
assert(s.sizeof(T.string(1)) == 1)

-- sizeof: pointer-sized types, pointer_size = 4
-- all ptr variants are pointer_size regardless of opts
local s4 = schema.new(4, {})
assert(s4.sizeof(T.ptr(T.i32)) == 4)
assert(s4.sizeof(T.ptr(T.i32, { weak = true })) == 4)
assert(s4.sizeof(T.ptr(T.i32, { optional = true, weak = true })) == 4)
assert(s4.sizeof(T.ptr(T.i32, { optional = true })) == 4)
assert(s4.sizeof(T.circular_list(T.struct("X"))) == 4)
assert(s4.sizeof(T.vector(T.i32)) == 8)

-- sizeof: pointer-sized types, pointer_size = 8
local s8 = schema.new(8, {})
assert(s8.sizeof(T.ptr(T.i32)) == 8)
assert(s8.sizeof(T.ptr(T.i32, { weak = true })) == 8)
assert(s8.sizeof(T.ptr(T.i32, { optional = true, weak = true })) == 8)
assert(s8.sizeof(T.ptr(T.i32, { optional = true })) == 8)
assert(s8.sizeof(T.circular_list(T.struct("X"))) == 8)
assert(s8.sizeof(T.vector(T.i32)) == 16)

-- sizeof: arrays
local s = schema.new(4, {})
assert(s.sizeof(T.array(T.i32, 5)) == 20)
assert(s.sizeof(T.array(T.i8, 8)) == 8)
assert(s.sizeof(T.array(T.i64, 3)) == 24)

-- sizeof: inline struct
local point = D.struct("Point", {
  D.field("x", T.i32),
  D.field("y", T.i32),
})
local s = schema.new(4, { point })
assert(s.sizeof(T.struct("Point")) == 8)

-- sizeof: inline struct with pad
local padded = D.struct("Padded", {
  D.field("x", T.i32),
  D.pad(4),
  D.field("y", T.i32),
})
local s = schema.new(4, { padded })
assert(s.sizeof(T.struct("Padded")) == 12)

-- sizeof: inline struct with offset
local with_offset = D.struct("WithOffset", {
  D.field("x", T.i32),
  D.offset(16),
  D.field("y", T.i32),
})
local s = schema.new(4, { with_offset })
assert(s.sizeof(T.struct("WithOffset")) == 20)

-- sizeof: nested inline struct
local inner = D.struct("Inner", {
  D.field("a", T.i32),
  D.field("b", T.i32),
})
local outer = D.struct("Outer", {
  D.field("inner", T.struct("Inner")),
  D.field("c", T.i32),
})
local s = schema.new(4, { inner, outer })
assert(s.sizeof(T.struct("Inner")) == 8)
assert(s.sizeof(T.struct("Outer")) == 12)

-- sizeof: unknown struct errors
local s = schema.new(4, {})
local ok, err = pcall(s.sizeof, T.struct("Ghost"))
assert(not ok)
assert(err:find("Ghost"), err)

-- sizeof: unresolved struct (forward reference)
local ok, err = pcall(function()
  local a = D.struct("A", { D.field("b", T.struct("B")) })
  local b = D.struct("B", { D.field("x", T.i32) })
  schema.new(4, { a, b })
end)
assert(not ok)
assert(err:find("B"), err)

-- sizeof: unknown type kind
local s = schema.new(4, {})
local ok, err = pcall(s.sizeof, { kind = "unknown" })
assert(not ok)
assert(err:find("unknown"), err)

-- fields: basic
local point = D.struct("Point", {
  D.field("x", T.i32),
  D.field("y", T.i32),
})
local s = schema.new(4, { point })
local f = s.fields("Point")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].typ.kind == "primitive")
assert(f[1].typ.name == "i32")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].typ.kind == "primitive")
assert(f[2].typ.name == "i32")
assert(f[2].offset == 4)

-- fields: pad is excluded
local padded = D.struct("Padded", {
  D.field("x", T.i32),
  D.pad(4),
  D.field("y", T.i32),
})
local s = schema.new(4, { padded })
local f = s.fields("Padded")
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
local s = schema.new(4, { with_offset })
local f = s.fields("WithOffset")
assert(#f == 2)
assert(f[1].name == "x")
assert(f[1].offset == 0)
assert(f[2].name == "y")
assert(f[2].offset == 16)

-- fields: pointer-typed field
local node = D.struct("Node", {
  D.field("value", T.i32),
  D.field("next", T.ptr(T.struct("Node"), { weak = true })),
})
local s = schema.new(4, { node })
local f = s.fields("Node")
assert(#f == 2)
assert(f[1].name == "value")
assert(f[1].offset == 0)
assert(f[2].name == "next")
assert(f[2].offset == 4)

-- fields: opts are preserved
local with_opts = D.struct("WithOpts", {
  D.field("visible", T.i32),
  D.field("hidden", T.i32, { print = false }),
})
local s = schema.new(4, { with_opts })
local f = s.fields("WithOpts")
assert(f[1].opts == nil or f[1].opts.print ~= false)
assert(f[2].opts and f[2].opts.print == false)

-- fields: unknown struct errors
local s = schema.new(4, {})
local ok, err = pcall(s.fields, "Ghost")
assert(not ok)
assert(err:find("Ghost"), err)

-- D.offset backwards error
local ok, err = pcall(function()
  schema.new(4, {
    D.struct("Bad", {
      D.field("x", T.i32),
      D.offset(2),
      D.field("y", T.i32),
    }),
  })
end)
assert(not ok)
assert(err:find("forwards"), err)
