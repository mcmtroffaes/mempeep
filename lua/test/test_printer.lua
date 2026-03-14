local T = require("mempeep.T")
local D = require("mempeep.D")
local printer = require("mempeep.printer")

--- Capture lines printed to stdout by fn().
-- Temporarily replaces the global print function.
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

--- Return true if any line in lines contains the plain substring s.
local function any_line(lines, s)
  for _, l in ipairs(lines) do
    if l:find(s, 1, true) then
      return true
    end
  end
  return false
end

-- print_rawval: nil rawval -> prints "nil"
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, nil, 0, "field")
  end)
  assert(#lines == 1)
  assert(any_line(lines, "nil"))
end

-- print_rawval: rawval with nil value and no error -> prints "nil"
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = nil }, 0, "field")
  end)
  assert(any_line(lines, "nil"))
end

-- print_rawval: rawval with error and nil value -> prints "ERROR:"
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = nil, error = "Could not read i32" }, 0, "v")
  end)
  assert(any_line(lines, "ERROR:"))
  assert(any_line(lines, "Could not read i32"))
end

-- print_rawval: address is formatted as [XXXXXXXX] (uppercase hex, 8 digits)
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0xABCD1234, value = 42 }, 0, nil)
  end)
  assert(any_line(lines, "ABCD1234"))
end

-- print_rawval: nil addr -> placeholder "[--------]"
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, nil, 0, nil)
  end)
  assert(any_line(lines, "--------"))
end

-- print_rawval: label appears in output followed by ": "
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = 7 }, 0, "my_label")
  end)
  assert(any_line(lines, "my_label"))
end

-- print_rawval: no label -> no ": " prefix in value portion
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = 7 }, 0, nil)
  end)
  assert(#lines == 1)
  -- line should not contain a spurious ": "
  assert(not lines[1]:find(": ", 1, true), lines[1])
end

-- print_rawval: scalars print their value as a string
do
  local print_rawval = printer.new({})

  for _, kind in ipairs({ "i8", "i16", "i32", "i64", "float" }) do
    local lines = capture(function()
      print_rawval({ kind = kind }, { addr = 0x100, value = 99 }, 0, "v")
    end)
    assert(any_line(lines, "99"))
  end
end

-- print_rawval: string prints value in double quotes
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.string(16), { addr = 0x500, value = "hello" }, 0, "s")
  end)
  assert(any_line(lines, '"hello"'))
end

-- print_rawval: weak pointer prints hex address with "0x" prefix
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    -- T.ptr is weak=true
    print_rawval(T.ptr(T.i32), { addr = 0x900, value = 0xDEAD }, 0, "p")
  end)
  -- string.format("0x%X", 0xDEAD) -> "0xDEAD"
  assert(any_line(lines, "0xDEAD"))
end

-- print_rawval: optional weak pointer prints hex address
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.optional_ptr(T.i32), { addr = 0x930, value = 0x1234 }, 0, "p")
  end)
  assert(any_line(lines, "0x1234"))
end

-- print_rawval: followed pointer (T.ref) prints "ptr" then child on next line
do
  local print_rawval = printer.new({})

  local inner_rawval = { addr = 0xA00, value = 77 }
  local lines = capture(function()
    print_rawval(T.ref(T.i32), { addr = 0x940, value = inner_rawval }, 0, "r")
  end)
  assert(any_line(lines, "ptr"))
  assert(any_line(lines, "77"))
  -- "ptr" and the child value are on separate lines
  assert(#lines >= 2)
end

-- print_rawval: struct prints name then one line per field
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local print_rawval = printer.new({ point })

  local rawval = {
    addr = 0x600,
    value = {
      x = { addr = 0x600, value = 10 },
      y = { addr = 0x604, value = 20 },
    },
  }
  local lines = capture(function()
    print_rawval(T.struct("Point"), rawval, 0, nil)
  end)
  assert(any_line(lines, "Point"))
  assert(any_line(lines, "x"))
  assert(any_line(lines, "10"))
  assert(any_line(lines, "y"))
  assert(any_line(lines, "20"))
  -- struct name line + two field lines
  assert(#lines >= 3)
end

-- print_rawval: struct field with opts.print == false is suppressed
do
  local s = D.struct("HideField", {
    D.field("visible", T.i32),
    D.field("hidden", T.i32, { print = false }),
  })
  local print_rawval = printer.new({ s })

  local rawval = {
    addr = 0x700,
    value = {
      visible = { addr = 0x700, value = 1 },
      hidden = { addr = 0x704, value = 2 },
    },
  }
  local lines = capture(function()
    print_rawval(T.struct("HideField"), rawval, 0, nil)
  end)
  assert(any_line(lines, "visible"))
  assert(not any_line(lines, "hidden"))
end

-- print_rawval: array prints "array(N)" header and 0-based element labels
do
  local print_rawval = printer.new({})

  local rawval = {
    addr = 0xC00,
    value = {
      { addr = 0xC00, value = 10 },
      { addr = 0xC04, value = 20 },
      { addr = 0xC08, value = 30 },
    },
  }
  local lines = capture(function()
    print_rawval(T.array(T.i32, 3), rawval, 0, "arr")
  end)
  assert(any_line(lines, "array(3)"))
  assert(any_line(lines, "10"))
  assert(any_line(lines, "20"))
  assert(any_line(lines, "30"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[2]"))
end

-- print_rawval: empty array prints "array(0)" with no child lines
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.array(T.i32, 0), { addr = 0xC00, value = {} }, 0, nil)
  end)
  assert(any_line(lines, "array(0)"))
  assert(#lines == 1)
end

-- print_rawval: vector prints "vector(N)" header and 0-based element labels
do
  local print_rawval = printer.new({})

  local rawval = {
    addr = 0xE00,
    value = {
      { addr = 0xE10, value = 100 },
      { addr = 0xE14, value = 200 },
    },
  }
  local lines = capture(function()
    print_rawval(T.vector(T.i32), rawval, 0, "vec")
  end)
  assert(any_line(lines, "vector(2)"))
  assert(any_line(lines, "100"))
  assert(any_line(lines, "200"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[1]"))
end

-- print_rawval: empty vector prints "vector(0)" with no child lines
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.vector(T.i32), { addr = 0xF00, value = {} }, 0, nil)
  end)
  assert(any_line(lines, "vector(0)"))
  assert(#lines == 1)
end

-- print_rawval: circular_list prints "circular_list(N)" header and elements
do
  local node = D.struct("PNode", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("PNode"))),
  })
  local print_rawval = printer.new({ node })

  local rawval = {
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
    print_rawval(T.circular_list(T.struct("PNode")), rawval, 0, "list")
  end)
  assert(any_line(lines, "circular_list(2)"))
  assert(any_line(lines, "[0]"))
  assert(any_line(lines, "[1]"))
end

-- print_rawval: rawval with both error and value -> prints ERROR then the value
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = 55, error = "partial" }, 0, "v")
  end)
  assert(any_line(lines, "ERROR:"))
  assert(any_line(lines, "55"))
  assert(#lines >= 2)
end

-- print_rawval: unknown type kind raises an error (message says "print:")
do
  local print_rawval = printer.new({})

  local ok, err = pcall(function()
    capture(function()
      print_rawval({ kind = "bogus" }, { addr = 0x100, value = 0 }, 0, nil)
    end)
  end)
  assert(not ok)
  assert(err:find("bogus"), err)
  assert(err:find("print:"), err)
end

-- print_rawval: unknown struct name raises an error (message says "print:")
do
  local print_rawval = printer.new({})

  local ok, err = pcall(function()
    capture(function()
      print_rawval(T.struct("Ghost"), { addr = 0x100, value = {} }, 0, nil)
    end)
  end)
  assert(not ok)
  assert(err:find("Ghost"), err)
  assert(err:find("print:"), err)
end

-- print_rawval: indent and label can be omitted (default indent = 0, no crash)
do
  local print_rawval = printer.new({})

  local lines = capture(function()
    print_rawval(T.i32, { addr = 0x100, value = 1 })
  end)
  assert(#lines == 1)
end
