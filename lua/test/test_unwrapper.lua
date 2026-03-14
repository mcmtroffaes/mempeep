local T = require("mempeep.T")
local D = require("mempeep.D")
local unwrapper = require("mempeep.unwrapper")

-- unwrap: nil rawval -> nil, no errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.i32, nil)
  assert(v == nil)
  assert(#errs == 0)
end

-- unwrap: rawval with nil value and no error -> nil, no errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.i32, { addr = 0x100, value = nil })
  assert(v == nil)
  assert(#errs == 0)
end

-- unwrap: rawval with nil value and error string -> nil, one error
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.i32, { addr = 0x100, value = nil, error = "Could not read i32" })
  assert(v == nil)
  assert(#errs == 1)
  assert(errs[1] == "Could not read i32")
end

-- unwrap: scalars (i8, i16, i32, i64, float)
do
  local structs = {}
  local unwrap = unwrapper.new(structs)

  local v, errs = unwrap(T.i32, { addr = 0x100, value = 42 })
  assert(v == 42)
  assert(#errs == 0)

  local v, errs = unwrap(T.i8(), { addr = 0x100, value = -1 })
  assert(v == -1)
  assert(#errs == 0)

  local v, errs = unwrap(T.i16(), { addr = 0x100, value = 1000 })
  assert(v == 1000)
  assert(#errs == 0)

  local v, errs = unwrap(T.i64(), { addr = 0x100, value = 123456789 })
  assert(v == 123456789)
  assert(#errs == 0)

  local v, errs = unwrap(T.float(), { addr = 0x100, value = 3 })
  assert(v == 3)
  assert(#errs == 0)
end

-- unwrap: string
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.string(16), { addr = 0x500, value = "hello" })
  assert(v == "hello")
  assert(#errs == 0)
end

-- unwrap: weak pointer -> raw address integer, no errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  -- T.ptr is weak=true
  local v, errs = unwrap(T.ptr(T.i32), { addr = 0x900, value = 0xABCD })
  assert(v == 0xABCD)
  assert(#errs == 0)
end

-- unwrap: optional weak pointer with nil value -> nil, no errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.optional_ptr(T.i32), { addr = 0x920, value = nil })
  assert(v == nil)
  assert(#errs == 0)
end

-- unwrap: followed pointer (T.ref) -> unwraps pointee
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  -- T.ref is weak=false; value is the inner rawval for the pointee
  local inner_rawval = { addr = 0xA00, value = 77 }
  local v, errs = unwrap(T.ref(T.i32), { addr = 0x940, value = inner_rawval })
  assert(v == 77)
  assert(#errs == 0)
end

-- unwrap: followed pointer with null (nil value rawval) -> nil, error surfaced
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local null_rawval = { addr = 0x950, value = nil, error = "Null pointer" }
  local v, errs = unwrap(T.ref(T.i32), { addr = 0x940, value = null_rawval })
  assert(v == nil)
  assert(#errs == 1)
  assert(errs[1] == "Null pointer")
end

-- unwrap: optional_ref with nil value -> nil, no errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local v, errs = unwrap(T.optional_ref(T.i32), { addr = 0x960, value = nil })
  assert(v == nil)
  assert(#errs == 0)
end

-- unwrap: struct -> flat table of plain values, no errors
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local structs = { point }
  local unwrap = unwrapper.new(structs)

  local rawval = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = 20 },
    },
  }
  local v, errs = unwrap(T.struct("Point"), rawval)
  assert(v ~= nil)
  assert(v.x == 10)
  assert(v.y == 20)
  assert(#errs == 0)
end

-- unwrap: struct with a failing field -> partial result, error is context-prefixed
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local structs = { point }
  local unwrap = unwrapper.new(structs)

  local rawval = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = nil, error = "Could not read i32" },
    },
  }
  local v, errs = unwrap(T.struct("Point"), rawval)
  assert(v ~= nil)
  assert(v.x == 10)
  assert(v.y == nil)
  assert(#errs == 1)
  assert(errs[1]:find("y"), errs[1])
  assert(errs[1]:find("Could not read i32"), errs[1])
end

-- unwrap: unknown struct errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local rawval = { addr = 0x100, value = {} }
  local ok, err = pcall(unwrap, T.struct("Ghost"), rawval)
  assert(not ok)
  assert(err:find("Ghost"), err)
end

-- unwrap: array -> plain table of values
do
  local structs = {}
  local unwrap = unwrapper.new(structs)

  local rawval = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = 20 },
      { addr = 0xC08, value = 30 },
    },
  }
  local v, errs = unwrap(T.array(T.i32, 3), rawval)
  assert(#v == 3)
  assert(v[1] == 10)
  assert(v[2] == 20)
  assert(v[3] == 30)
  assert(#errs == 0)
end

-- unwrap: array with one failing element -> error prefixed with index
do
  local structs = {}
  local unwrap = unwrapper.new(structs)

  local rawval = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = nil, error = "Could not read i32" },
      { addr = 0xC08, value = 30 },
    },
  }
  local v, errs = unwrap(T.array(T.i32, 3), rawval)
  assert(v[1] == 10)
  assert(v[2] == nil)
  assert(v[3] == 30)
  assert(#errs == 1)
  assert(errs[1]:find("%[1%]"), errs[1]) -- 0-indexed: element index 1
end

-- unwrap: vector -> plain table of values
do
  local structs = {}
  local unwrap = unwrapper.new(structs)

  local rawval = {
    addr = 0xE00,
    value = {
      { addr = 0xE10, value = 100 },
      { addr = 0xE14, value = 200 },
      { addr = 0xE18, value = 300 },
    },
  }
  local v, errs = unwrap(T.vector(T.i32), rawval)
  assert(#v == 3)
  assert(v[1] == 100)
  assert(v[2] == 200)
  assert(v[3] == 300)
  assert(#errs == 0)
end

-- unwrap: empty vector -> empty table, no errors
do
  local unwrap = unwrapper.new({})

  local rawval = { addr = 0xF00, value = {} }
  local v, errs = unwrap(T.vector(T.i32), rawval)
  assert(type(v) == "table")
  assert(#v == 0)
  assert(#errs == 0)
end

-- unwrap: circular_list -> plain table of values
do
  local node = D.struct("CNode", {
    D.field("val", T.i32),
    D.field("next", T.ptr(T.struct("CNode"))),
  })
  local unwrap = unwrapper.new({ node })

  -- Simulate three-element circular list as unwrap sees it:
  -- value is a Lua array of rawvals, each rawval.value is the struct field table.
  -- The "next" field is a weak ptr rawval whose value is the raw address integer.
  local rawval = {
    addr = 0x3000,
    value = {
      { addr = 0x3010, value = { val = { addr = 0x3010, value = 1 }, next = { addr = 0x3014, value = 0x3018 } } },
      { addr = 0x3018, value = { val = { addr = 0x3018, value = 2 }, next = { addr = 0x301C, value = 0x3020 } } },
      { addr = 0x3020, value = { val = { addr = 0x3020, value = 3 }, next = { addr = 0x3024, value = 0x3010 } } },
    },
  }
  local v, errs = unwrap(T.circular_list(T.struct("CNode")), rawval)
  assert(#v == 3)
  assert(v[1].val == 1)
  assert(v[2].val == 2)
  assert(v[3].val == 3)
  assert(#errs == 0)
end

-- unwrap: empty circular_list -> empty table, no errors
do
  local node = D.struct("ENode", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("ENode"))),
  })
  local structs = { node }
  local unwrap = unwrapper.new(structs)

  local rawval = { addr = 0x4000, value = {} }
  local v, errs = unwrap(T.circular_list(T.struct("ENode")), rawval)
  assert(type(v) == "table")
  assert(#v == 0)
  assert(#errs == 0)
end

-- unwrap: unknown type kind errors
do
  local structs = {}
  local unwrap = unwrapper.new(structs)
  local ok, err = pcall(unwrap, { kind = "bogus" }, { addr = 0x100, value = 0 })
  assert(not ok)
  assert(err:find("bogus"), err)
end

-- unwrap: nested struct -> nested plain tables
do
  local inner = D.struct("Inner", {
    D.field("a", T.i32),
    D.field("b", T.i32),
  })
  local outer = D.struct("Outer", {
    D.field("inner", T.struct("Inner")),
    D.field("c", T.i32),
  })
  local structs = { inner, outer }
  local unwrap = unwrapper.new(structs)

  local rawval = {
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
  local v, errs = unwrap(T.struct("Outer"), rawval)
  assert(v.inner.a == 11)
  assert(v.inner.b == 22)
  assert(v.c == 33)
  assert(#errs == 0)
end

-- unwrap: error prefixes are nested for deeply nested failures
do
  local inner = D.struct("DeepInner", {
    D.field("x", T.i32),
  })
  local outer = D.struct("DeepOuter", {
    D.field("inner", T.struct("DeepInner")),
  })
  local structs = { inner, outer }
  local unwrap = unwrapper.new(structs)

  local rawval = {
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
  local v, errs = unwrap(T.struct("DeepOuter"), rawval)
  assert(#errs == 1)
  -- error should be prefixed: "inner: x: Could not read i32"
  assert(errs[1]:find("inner"), errs[1])
  assert(errs[1]:find("x"), errs[1])
  assert(errs[1]:find("Could not read i32"), errs[1])
end
