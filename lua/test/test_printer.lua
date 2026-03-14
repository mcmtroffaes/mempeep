local T = require("mempeep.T")
local D = require("mempeep.D")
local schema = require("mempeep.schema")
local printer = require("mempeep.printer")

local function capture(fn)
  local lines = {}
  local orig = print
  print = function(s)
    lines[#lines + 1] = tostring(s)
  end
  local ok, err = pcall(fn)
  print = orig
  if not ok then
    error(err, 2)
  end
  return lines
end

local function any_line(lines, s)
  for _, l in ipairs(lines) do
    if l:find(s, 1, true) then
      return true
    end
  end
  return false
end

-- nil reading -> prints "nil"
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, nil, 0, "field")
  end)
  assert(#lines == 1)
  assert(any_line(lines, "nil"))
end

-- reading with nil value and no error -> prints "nil"
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = nil }, 0, "field")
  end)
  assert(any_line(lines, "nil"))
end

-- reading with error and nil value -> prints "ERROR:"
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = nil, error = "Could not read i32" }, 0, "v")
  end)
  assert(any_line(lines, "ERROR:"))
  assert(any_line(lines, "Could not read i32"))
end

-- address formatted as [XXXXXXXX]
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0xABCD1234, value = 42 }, 0, nil)
  end)
  assert(any_line(lines, "ABCD1234"))
end

-- nil addr -> "[--------]"
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, nil, 0, nil)
  end)
  assert(any_line(lines, "--------"))
end

-- label appears in output
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = 7 }, 0, "my_label")
  end)
  assert(any_line(lines, "my_label"))
end

-- no label -> no spurious ": "
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = 7 }, 0, nil)
  end)
  assert(#lines == 1)
  assert(not lines[1]:find(": ", 1, true), lines[1])
end

-- scalars print their value
do
  local pr = printer.new(schema.new(4, {}))
  for _, typ in ipairs({ T.i8, T.i16, T.i32, T.i64, T.f32 }) do
    local lines = capture(function()
      pr(typ, { addr = 0x100, value = 99 }, 0, "v")
    end)
    assert(any_line(lines, "99"))
  end
end

-- string prints value
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.string(16), { addr = 0x500, value = "hello" }, 0, "s")
  end)
  assert(any_line(lines, "hello"))
end

-- weak ptr prints hex address with "0x" prefix
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.ptr(T.i32, { weak = true }), { addr = 0x900, value = 0xDEAD }, 0, "p")
  end)
  assert(any_line(lines, "0xDEAD"))
end

-- optional weak ptr prints hex address
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.ptr(T.i32, { optional = true, weak = true }), { addr = 0x930, value = 0x1234 }, 0, "p")
  end)
  assert(any_line(lines, "0x1234"))
end

-- followed ptr prints "ptr" then child on next line
do
  local pr = printer.new(schema.new(4, {}))
  local inner_reading = { addr = 0xA00, value = 77 }
  local lines = capture(function()
    pr(T.ptr(T.i32), { addr = 0x940, value = inner_reading }, 0, "r")
  end)
  assert(any_line(lines, "ptr"))
  assert(any_line(lines, "77"))
  assert(#lines >= 2)
end

-- struct prints name then one line per field
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local pr = printer.new(schema.new(4, { point }))
  local reading = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = 20 },
    },
  }
  local lines = capture(function()
    pr(T.struct("Point"), reading, 0, nil)
  end)
  assert(any_line(lines, "Point"))
  assert(any_line(lines, "x"))
  assert(any_line(lines, "10"))
  assert(any_line(lines, "y"))
  assert(any_line(lines, "20"))
  assert(#lines >= 3)
end

-- struct field with opts.print == false is suppressed
do
  local s = D.struct("HideField", {
    D.field("visible", T.i32),
    D.field("hidden", T.i32, { print = false }),
  })
  local pr = printer.new(schema.new(4, { s }))
  local reading = {
    addr = 0x700,
    value = {
      visible = { addr = 0x700, value = 1 },
      hidden = { addr = 0x704, value = 2 },
    },
  }
  local lines = capture(function()
    pr(T.struct("HideField"), reading, 0, nil)
  end)
  assert(any_line(lines, "visible"))
  assert(not any_line(lines, "hidden"))
end

-- array prints "array(N)" header and 0-based element labels
do
  local pr = printer.new(schema.new(4, {}))
  local reading = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = 20 },
      { addr = 0xC08, value = 30 },
    },
  }
  local lines = capture(function()
    pr(T.array(T.i32, 3), reading, 0, "arr")
  end)
  assert(any_line(lines, "array(3)"))
  assert(any_line(lines, "10"))
  assert(any_line(lines, "20"))
  assert(any_line(lines, "30"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[2]"))
end

-- empty array prints "array(0)" with no child lines
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.array(T.i32, 0), { addr = 0xC00, value = {} }, 0, nil)
  end)
  assert(any_line(lines, "array(0)"))
  assert(#lines == 1)
end

-- vector prints "vector(N)" header and 0-based element labels
do
  local pr = printer.new(schema.new(4, {}))
  local reading = {
    addr = 0xE00,
    value = {
      { addr = 0xE10, value = 100 },
      { addr = 0xE14, value = 200 },
    },
  }
  local lines = capture(function()
    pr(T.vector(T.i32), reading, 0, "vec")
  end)
  assert(any_line(lines, "vector(2)"))
  assert(any_line(lines, "100"))
  assert(any_line(lines, "200"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[1]"))
end

-- empty vector prints "vector(0)" with no child lines
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.vector(T.i32), { addr = 0xF00, value = {} }, 0, nil)
  end)
  assert(any_line(lines, "vector(0)"))
  assert(#lines == 1)
end

-- circular_list prints "circular_list(N)" header and elements
do
  local node = D.struct("PNode", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("PNode"), { weak = true })),
  })
  local pr = printer.new(schema.new(4, { node }))
  local reading = {
    addr = 0x3000,
    value = {
      {
        addr = 0x3010,
        value = {
          value = { addr = 0x3010, value = 1 },
          next = { addr = 0x3014, value = 0x3018 },
        },
      },
      {
        addr = 0x3018,
        value = {
          value = { addr = 0x3018, value = 2 },
          next = { addr = 0x301C, value = 0x3010 },
        },
      },
    },
  }
  local lines = capture(function()
    pr(T.circular_list(T.struct("PNode")), reading, 0, "list")
  end)
  assert(any_line(lines, "circular_list(2)"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[1]"))
end

-- reading with both error and value -> prints ERROR then the value
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = 55, error = "partial" }, 0, "v")
  end)
  assert(any_line(lines, "ERROR:"))
  assert(any_line(lines, "55"))
  assert(#lines >= 2)
end

-- unknown type kind raises an error mentioning "print:"
do
  local pr = printer.new(schema.new(4, {}))
  local ok, err = pcall(function()
    capture(function()
      pr({ kind = "bogus" }, { addr = 0x100, value = 0 }, 0, nil)
    end)
  end)
  assert(not ok)
  assert(err:find("bogus"), err)
  assert(err:find("print:"), err)
end

-- unknown struct name raises an error
do
  local pr = printer.new(schema.new(4, {}))
  local ok, err = pcall(function()
    capture(function()
      pr(T.struct("Ghost"), { addr = 0x100, value = {} }, 0, nil)
    end)
  end)
  assert(not ok)
  assert(err:find("Ghost"), err)
end

-- indent and label can be omitted
do
  local pr = printer.new(schema.new(4, {}))
  local lines = capture(function()
    pr(T.i32, { addr = 0x100, value = 1 })
  end)
  assert(#lines == 1)
end
