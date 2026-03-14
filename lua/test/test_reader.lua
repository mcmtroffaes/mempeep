local T = require("mempeep.T")
local D = require("mempeep.D")
local compute = require("mempeep.compute")
local reader = require("mempeep.reader")

--- Make a mock read_bytes function backed by a flat string.
-- Addresses are byte offsets into data (0-based).
local function make_read_bytes(data)
  return function(addr, size)
    if addr < 0 or addr + size > #data then
      return nil
    end
    return data:sub(addr + 1, addr + size)
  end
end

-- i32 basic read
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x44\x33\x22\x11"))

  local r = read(0, T.i32)
  assert(r.value == 0x11223344)
  assert(r.addr == 0)
  assert(r.error == nil)
end

-- i32 at non-zero offset
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\xFF\xFF\x44\x33\x22\x11\xFF\xFF"))

  local r = read(2, T.i32)
  assert(r.value == 0x11223344)
  assert(r.addr == 2)
  assert(r.error == nil)
end

-- i8 (signed, -1)
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\xFF"))

  local r = read(0, T.i8)
  assert(r.value == -1)
end

-- i16
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\xE8\x03")) -- 1000 little-endian

  local r = read(0, T.i16)
  assert(r.value == 1000)
end

-- i64
do
  local c = compute.new(8, {})
  local read = reader.new(c, make_read_bytes("\x15\xCD\x5B\x07\x00\x00\x00\x00")) -- 123456789

  local r = read(0, T.i64)
  assert(r.value == 123456789)
end

-- Scalar with unreadable address returns error
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes(""))

  local r = read(0, T.i32)
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- nil address returns error
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes(""))

  local r = read(nil, T.i32)
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- String
-- ---------------------------------------------------------------------------

do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("hello\0\0\0\0\0\0\0\0\0\0\0"))

  local r = read(0, T.string(16))
  assert(r.value == "hello")
  assert(r.error == nil)
end

-- String with no null terminator reads up to size
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("ABC"))

  local r = read(0, T.string(3))
  assert(r.value == "ABC")
end

-- Unreadable string address
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes(""))

  local r = read(0, T.string(8))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Struct
-- ---------------------------------------------------------------------------

-- Point{x=10, y=20}
do
  local point = D.struct("Point", {
    D.field("x", T.i32),
    D.field("y", T.i32),
  })
  local c = compute.new(4, { point })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x0A\x00\x00\x00" -- x = 10
        .. "\x14\x00\x00\x00" -- y = 20
    )
  )

  local r = read(0, T.struct("Point"))
  assert(r.error == nil)
  assert(r.value ~= nil)
  assert(r.value.x.value == 10)
  assert(r.value.y.value == 20)
end

-- Struct with pad: a=1 at offset 0, 4 pad bytes, b=2 at offset 8
do
  local padded = D.struct("Padded", {
    D.field("a", T.i32),
    D.pad(4),
    D.field("b", T.i32),
  })
  local c = compute.new(4, { padded })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x01\x00\x00\x00" -- a = 1
        .. "\x00\x00\x00\x00" -- pad
        .. "\x02\x00\x00\x00" -- b = 2
    )
  )

  local r = read(0, T.struct("Padded"))
  assert(r.value.a.value == 1)
  assert(r.value.b.value == 2)
end

-- Struct with D.offset: a=7 at offset 0, b=99 at offset 8
do
  local s = D.struct("Sparse", {
    D.field("a", T.i32),
    D.offset(8),
    D.field("b", T.i32),
  })
  local c = compute.new(4, { s })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x07\x00\x00\x00" -- a = 7
        .. "\x00\x00\x00\x00" -- gap
        .. "\x63\x00\x00\x00" -- b = 99
    )
  )

  local r = read(0, T.struct("Sparse"))
  assert(r.value.a.value == 7)
  assert(r.value.b.value == 99)
end

-- ---------------------------------------------------------------------------
-- Pointers (pointer_size = 4)
-- ---------------------------------------------------------------------------

-- T.ptr (weak, non-optional): returns raw address value 12
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x0C\x00\x00\x00"))

  local r = read(0, T.ptr(T.i32))
  assert(r.value == 12)
  assert(r.error == nil)
end

-- T.ptr (weak, non-optional) with null pointer: error
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x00\x00\x00\x00"))

  local r = read(0, T.ptr(T.i32))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- T.optional_ptr with null pointer: no error, nil value
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x00\x00\x00\x00"))

  local r = read(0, T.optional_ptr(T.i32))
  assert(r.value == nil)
  assert(r.error == nil)
end

-- T.optional_ptr with non-null: returns raw address (weak)
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x08\x00\x00\x00"))

  local r = read(0, T.optional_ptr(T.i32))
  assert(r.value == 8)
end

-- T.ref: follows pointer to offset 4, reads i32 value 77
do
  local c = compute.new(4, {})
  local read = reader.new(
    c,
    make_read_bytes(
      "\x04\x00\x00\x00" -- pointer -> offset 4
        .. "\x4D\x00\x00\x00" -- i32 = 77
    )
  )

  local r = read(0, T.ref(T.i32))
  assert(r.error == nil)
  assert(r.value ~= nil)
  local inner = r.value
  assert(type(inner) == "table")
  assert(inner.value == 77)
end

-- T.ref null pointer: error
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x00\x00\x00\x00"))

  local r = read(0, T.ref(T.i32))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- T.optional_ref with null: nil value, no error
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x00\x00\x00\x00"))

  local r = read(0, T.optional_ref(T.i32))
  assert(r.value == nil)
  assert(r.error == nil)
end

-- T.optional_ref non-null: follows pointer to offset 4, reads i32 value 55
do
  local c = compute.new(4, {})
  local read = reader.new(
    c,
    make_read_bytes(
      "\x04\x00\x00\x00" -- pointer -> offset 4
        .. "\x37\x00\x00\x00" -- i32 = 55
    )
  )

  local r = read(0, T.optional_ref(T.i32))
  assert(r.error == nil)
  assert(r.value ~= nil)
  assert(r.value.value == 55)
end

-- ---------------------------------------------------------------------------
-- Array
-- ---------------------------------------------------------------------------

-- array of 3 x i32: 10, 20, 30
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes("\x0A\x00\x00\x00" .. "\x14\x00\x00\x00" .. "\x1E\x00\x00\x00"))

  local r = read(0, T.array(T.i32, 3))
  assert(r.error == nil)
  assert(#r.value == 3)
  assert(r.value[1].value == 10)
  assert(r.value[2].value == 20)
  assert(r.value[3].value == 30)
end

-- array of 2 x Point2{i16 x, i16 y}: (1,2), (3,4)
do
  local point = D.struct("Point2", {
    D.field("x", T.i16),
    D.field("y", T.i16),
  })
  local c = compute.new(4, { point })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x01\x00\x02\x00" -- x=1, y=2
        .. "\x03\x00\x04\x00" -- x=3, y=4
    )
  )

  local r = read(0, T.array(T.struct("Point2"), 2))
  assert(r.error == nil)
  assert(#r.value == 2)
  assert(r.value[1].value.x.value == 1)
  assert(r.value[1].value.y.value == 2)
  assert(r.value[2].value.x.value == 3)
  assert(r.value[2].value.y.value == 4)
end

-- ---------------------------------------------------------------------------
-- Vector
-- ---------------------------------------------------------------------------

-- vector of 3 x i32: begin/end ptrs at offset 0 (8 bytes), data at offset 8
-- begin=8, end=20 (8 + 3*4); elements 100, 200, 300
do
  local c = compute.new(4, {})
  local read = reader.new(
    c,
    make_read_bytes(
      "\x08\x00\x00\x00" -- begin = 8
        .. "\x14\x00\x00\x00" -- end   = 20
        .. "\x64\x00\x00\x00" -- [0] = 100
        .. "\xC8\x00\x00\x00" -- [1] = 200
        .. "\x2C\x01\x00\x00" -- [2] = 300
    )
  )

  local r = read(0, T.vector(T.i32))
  assert(r.error == nil)
  assert(#r.value == 3)
  assert(r.value[1].value == 100)
  assert(r.value[2].value == 200)
  assert(r.value[3].value == 300)
end

-- Empty vector (begin == end)
do
  local c = compute.new(4, {})
  local read = reader.new(
    c,
    make_read_bytes(
      "\x08\x00\x00\x00" -- begin = 8
        .. "\x08\x00\x00\x00" -- end   = 8
    )
  )

  local r = read(0, T.vector(T.i32))
  assert(r.error == nil)
  assert(#r.value == 0)
end

-- Vector begin > end: error
do
  local c = compute.new(4, {})
  local read = reader.new(
    c,
    make_read_bytes(
      "\x10\x00\x00\x00" -- begin = 16
        .. "\x08\x00\x00\x00" -- end   = 8 (inverted)
    )
  )

  local r = read(0, T.vector(T.i32))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- Unreadable vector pointers
do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes(""))

  local r = read(0, T.vector(T.i32))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Circular list
-- ---------------------------------------------------------------------------

-- Three-node circular list: A -> B -> C -> A
-- Each node: value(i32) at +0, next(ptr) at +4 => 8 bytes/node
-- head ptr at offset 0; node A at 4, B at 12, C at 20
do
  local node = D.struct("Node", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("Node"))),
  })
  D.assert_valid({ node })

  local container = D.struct("Container", {
    D.field("nodes", T.circular_list(T.struct("Node"))),
  })
  local c = compute.new(4, { node, container })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x04\x00\x00\x00" -- head ptr = 4
        .. "\x01\x00\x00\x00\x0C\x00\x00\x00" -- A: value=1, next=12
        .. "\x02\x00\x00\x00\x14\x00\x00\x00" -- B: value=2, next=20
        .. "\x03\x00\x00\x00\x04\x00\x00\x00" -- C: value=3, next=4
    )
  )

  local r = read(0, T.circular_list(T.struct("Node")))
  assert(r.error == nil)
  assert(#r.value == 3)
  assert(r.value[1].value.value.value == 1)
  assert(r.value[2].value.value.value == 2)
  assert(r.value[3].value.value.value == 3)
end

-- Empty circular list (head pointer is null)
do
  local node = D.struct("NodeE", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("NodeE"))),
  })
  local c = compute.new(4, { node })
  local read = reader.new(c, make_read_bytes("\x00\x00\x00\x00"))

  local r = read(0, T.circular_list(T.struct("NodeE")))
  assert(r.error == nil)
  assert(#r.value == 0)
end

-- Unreadable head pointer
do
  local node = D.struct("NodeU", {
    D.field("value", T.i32),
    D.field("next", T.ptr(T.struct("NodeU"))),
  })
  local c = compute.new(4, { node })
  local read = reader.new(c, make_read_bytes(""))

  local r = read(0, T.circular_list(T.struct("NodeU")))
  assert(r.value == nil)
  assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Nested struct
-- ---------------------------------------------------------------------------

-- Outer2{ inner: Inner2{a=11, b=22}, c=33 } => 12 contiguous bytes
do
  local inner = D.struct("Inner2", {
    D.field("a", T.i32),
    D.field("b", T.i32),
  })
  local outer = D.struct("Outer2", {
    D.field("inner", T.struct("Inner2")),
    D.field("c", T.i32),
  })
  local c = compute.new(4, { inner, outer })
  local read = reader.new(
    c,
    make_read_bytes(
      "\x0B\x00\x00\x00" -- a = 11
        .. "\x16\x00\x00\x00" -- b = 22
        .. "\x21\x00\x00\x00" -- c = 33
    )
  )

  local r = read(0, T.struct("Outer2"))
  assert(r.error == nil)
  assert(r.value.inner.value.a.value == 11)
  assert(r.value.inner.value.b.value == 22)
  assert(r.value.c.value == 33)
end

-- ---------------------------------------------------------------------------
-- Unknown type kind
-- ---------------------------------------------------------------------------

do
  local c = compute.new(4, {})
  local read = reader.new(c, make_read_bytes(""))

  local ok, err = pcall(read, 0, { kind = "bogus" })
  assert(not ok)
  assert(err:find("bogus"))
end
