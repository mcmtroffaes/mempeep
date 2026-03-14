local T = require("mempeep.T")
local D = require("mempeep.D")
local schema = require("mempeep.schema")
local decoder = require("mempeep.decoder")

-- nil reading -> nil, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.i32, nil)
  assert(v == nil)
  assert(#errs == 0)
end

-- reading with nil value and no error -> nil, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.i32, { addr = 0x100, value = nil })
  assert(v == nil)
  assert(#errs == 0)
end

-- reading with nil value and error -> nil, one error
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.i32, { addr = 0x100, value = nil, error = "Could not read i32" })
  assert(v == nil)
  assert(#errs == 1)
  assert(errs[1] == "Could not read i32")
end

-- scalars
do
  local decode = decoder.new(schema.new(4, {}))

  local v, errs = decode(T.i32, { addr = 0x100, value = 42 })
  assert(v == 42 and #errs == 0)

  local v, errs = decode(T.i8, { addr = 0x100, value = -1 })
  assert(v == -1 and #errs == 0)

  local v, errs = decode(T.i16, { addr = 0x100, value = 1000 })
  assert(v == 1000 and #errs == 0)

  local v, errs = decode(T.i64, { addr = 0x100, value = 123456789 })
  assert(v == 123456789 and #errs == 0)

  local v, errs = decode(T.f32, { addr = 0x100, value = 3 })
  assert(v == 3 and #errs == 0)
end

-- string
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.string(16), { addr = 0x500, value = "hello" })
  assert(v == "hello" and #errs == 0)
end

-- weak ptr -> raw address integer, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.ptr(T.i32, { weak = true }), { addr = 0x900, value = 0xABCD })
  assert(v == 0xABCD and #errs == 0)
end

-- optional weak ptr with nil value -> nil, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.ptr(T.i32, { optional = true, weak = true }), { addr = 0x920, value = nil })
  assert(v == nil and #errs == 0)
end

-- followed ptr -> decodes pointee
do
  local decode = decoder.new(schema.new(4, {}))
  local inner_reading = { addr = 0xA00, value = 77 }
  local v, errs = decode(T.ptr(T.i32), { addr = 0x940, value = inner_reading })
  assert(v == 77 and #errs == 0)
end

-- followed ptr with null reading -> nil, error surfaced
do
  local decode = decoder.new(schema.new(4, {}))
  local null_reading = { addr = 0x950, value = nil, error = "Null pointer" }
  local v, errs = decode(T.ptr(T.i32), { addr = 0x940, value = null_reading })
  assert(v == nil)
  assert(#errs == 1)
  assert(errs[1] == "Null pointer")
end

-- optional followed ptr with nil value -> nil, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.ptr(T.i32, { optional = true }), { addr = 0x960, value = nil })
  assert(v == nil and #errs == 0)
end

-- struct -> flat table of plain values, no errors
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local decode = decoder.new(schema.new(4, { point }))
  local reading = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = 20 },
    },
  }
  local v, errs = decode(T.struct("Point"), reading)
  assert(v ~= nil)
  assert(v.x == 10 and v.y == 20)
  assert(#errs == 0)
end

-- struct with a failing field -> partial result, error is context-prefixed
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local decode = decoder.new(schema.new(4, { point }))
  local reading = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = nil, error = "Could not read i32" },
    },
  }
  local v, errs = decode(T.struct("Point"), reading)
  assert(v ~= nil)
  assert(v.x == 10 and v.y == nil)
  assert(#errs == 1)
  assert(errs[1]:find("y"), errs[1])
  assert(errs[1]:find("Could not read i32"), errs[1])
end

-- partial reading (both error and value) -> value decoded, error surfaced
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.i32, { addr = 0x100, value = 55, error = "partial" })
  assert(v == 55)
  assert(#errs == 1)
  assert(errs[1] == "partial")
end

-- partial reading on a struct -> value decoded, top-level error prepended
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local decode = decoder.new(schema.new(4, { point }))
  local reading = {
    addr = 0x600,
    error = "truncated",
    value = {
      x = { addr = 0x600, value = 7 },
      y = { addr = 0x604, value = 8 },
    },
  }
  local v, errs = decode(T.struct("Point"), reading)
  assert(v ~= nil)
  assert(v.x == 7 and v.y == 8)
  assert(#errs == 1)
  assert(errs[1] == "truncated")
end

-- unknown struct errors
do
  local decode = decoder.new(schema.new(4, {}))
  local ok, err = pcall(decode, T.struct("Ghost"), { addr = 0x100, value = {} })
  assert(not ok)
  assert(err:find("Ghost"), err)
end

-- array -> plain table of values
do
  local decode = decoder.new(schema.new(4, {}))
  local reading = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = 20 },
      { addr = 0xC08, value = 30 },
    },
  }
  local v, errs = decode(T.array(T.i32, 3), reading)
  assert(#v == 3)
  assert(v[1] == 10 and v[2] == 20 and v[3] == 30)
  assert(#errs == 0)
end

-- array with one failing element -> error prefixed with 0-based index
do
  local decode = decoder.new(schema.new(4, {}))
  local reading = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = nil, error = "Could not read i32" },
      { addr = 0xC08, value = 30 },
    },
  }
  local v, errs = decode(T.array(T.i32, 3), reading)
  assert(v[1] == 10 and v[2] == nil and v[3] == 30)
  assert(#errs == 1)
  assert(errs[1]:find("%[1%]"), errs[1])
end

-- vector -> plain table of values
do
  local decode = decoder.new(schema.new(4, {}))
  local reading = {
    addr = 0xE00,
    value = {
      { addr = 0xE10, value = 100 },
      { addr = 0xE14, value = 200 },
      { addr = 0xE18, value = 300 },
    },
  }
  local v, errs = decode(T.vector(T.i32), reading)
  assert(#v == 3)
  assert(v[1] == 100 and v[2] == 200 and v[3] == 300)
  assert(#errs == 0)
end

-- empty vector -> empty table, no errors
do
  local decode = decoder.new(schema.new(4, {}))
  local v, errs = decode(T.vector(T.i32), { addr = 0xF00, value = {} })
  assert(type(v) == "table" and #v == 0 and #errs == 0)
end

-- circular_list -> plain table of values
do
  local node = D.struct("CNode", {
    D.field("val", T.i32),
    D.field("next", T.ptr(T.struct("CNode"), { weak = true })),
  })
  local decode = decoder.new(schema.new(4, { node }))
  local reading = {
    addr = 0x3000,
    value = {
      { addr = 0x3010, value = { val = { addr = 0x3010, value = 1 }, next = { addr = 0x3014, value = 0x3018 } } },
      { addr = 0x3018, value = { val = { addr = 0x3018, value = 2 }, next = { addr = 0x301C, value = 0x3020 } } },
      { addr = 0x3020, value = { val = { addr = 0x3020, value = 3 }, next = { addr = 0x3024, value = 0x3010 } } },
    },
  }
  local v, errs = decode(T.circular_list(T.struct("CNode")), reading)
  assert(#v == 3)
  assert(v[1].val == 1 and v[2].val == 2 and v[3].val == 3)
  assert(#errs == 0)
end

-- empty circular_list -> empty table, no errors
do
  local node = D.struct("ENode", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("ENode"), { weak = true })),
  })
  local decode = decoder.new(schema.new(4, { node }))
  local v, errs = decode(T.circular_list(T.struct("ENode")), { addr = 0x4000, value = {} })
  assert(type(v) == "table" and #v == 0 and #errs == 0)
end

-- unknown type kind errors
do
  local decode = decoder.new(schema.new(4, {}))
  local ok, err = pcall(decode, { kind = "bogus" }, { addr = 0x100, value = 0 })
  assert(not ok)
  assert(err:find("bogus"), err)
end

-- nested struct -> nested plain tables
do
  local inner = D.struct("Inner", {
    D.field("a", T.i32),
    D.field("b", T.i32),
  })
  local outer = D.struct("Outer", {
    D.field("inner", T.struct("Inner")),
    D.field("c", T.i32),
  })
  local decode = decoder.new(schema.new(4, { inner, outer }))
  local reading = {
    addr = 0x5000,
    value = {
      inner = {
        addr = 0x5000,
        value = {
          a = { addr = 0x5000, value = 11 },
          b = { addr = 0x5004, value = 22 },
        },
      },
      c = { addr = 0x5008, value = 33 },
    },
  }
  local v, errs = decode(T.struct("Outer"), reading)
  assert(v.inner.a == 11 and v.inner.b == 22 and v.c == 33)
  assert(#errs == 0)
end

-- deeply nested error -> prefixed at each level
do
  local inner = D.struct("DeepInner", {
    D.field("x", T.i32),
  })
  local outer = D.struct("DeepOuter", {
    D.field("inner", T.struct("DeepInner")),
  })
  local decode = decoder.new(schema.new(4, { inner, outer }))
  local reading = {
    addr = 0x6000,
    value = {
      inner = {
        addr = 0x6000,
        value = {
          x = { addr = 0x6000, value = nil, error = "Could not read i32" },
        },
      },
    },
  }
  local v, errs = decode(T.struct("DeepOuter"), reading)
  assert(#errs == 1)
  assert(errs[1]:find("inner"), errs[1])
  assert(errs[1]:find("x"), errs[1])
  assert(errs[1]:find("Could not read i32"), errs[1])
end
